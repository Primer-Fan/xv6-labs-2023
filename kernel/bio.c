// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf bufbucket[NBUCKET];
  struct spinlock bucketlock[NBUCKET];
} bcache;

void
binit(void)
{
  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKET; ++i) {
    initlock(&bcache.bucketlock[i], "bcache.bucket");
    bcache.bufbucket[i].next = 0;
  }

  for (int i = 0; i < NBUF; ++i) {
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->refcnt = 0;
    b->lastuse = 0;
    b->next = bcache.bufbucket[0].next;
    bcache.bufbucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint key = HASHMAP(dev, blockno);

  acquire(&bcache.bucketlock[key]);

  // Is the block already cached?
  for(b = bcache.bufbucket[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucketlock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.

  release(&bcache.bucketlock[key]);
  acquire(&bcache.lock);

  for(b = bcache.bufbucket[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucketlock[key]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  struct buf *before_least = 0;
  uint holding_bucket = -1;
  for (int i = 0; i < NBUCKET; ++i) {
    acquire(&bcache.bucketlock[i]);
    int flag = 0;
    for (b = &bcache.bufbucket[i]; b->next; b = b->next) {
      if (b->next->refcnt == 0 && (!before_least || b->next->lastuse < before_least->next->lastuse)) {
        before_least = b;
        flag = 1;
      }
    }
    if (flag) {
      if (holding_bucket != -1) release(&bcache.bucketlock[holding_bucket]);
      holding_bucket = i;
    } else {
      release(&bcache.bucketlock[i]);
    }
  }

  if (!before_least)
    panic("bget: no buffers");

  b = before_least->next;

  if (holding_bucket != key) {
    before_least->next = b->next;
    release(&bcache.bucketlock[holding_bucket]);
    acquire(&bcache.bucketlock[key]);
    b->next = bcache.bufbucket[key].next;
    bcache.bufbucket[key].next = b;
  }

  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  release(&bcache.bucketlock[key]);
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint key = HASHMAP(b->dev, b->blockno);

  acquire(&bcache.bucketlock[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lastuse = ticks;
  }
  
  release(&bcache.bucketlock[key]);
}

void
bpin(struct buf *b) {
  uint key = HASHMAP(b->dev, b->blockno);
  acquire(&bcache.bucketlock[key]);
  b->refcnt++;
  release(&bcache.bucketlock[key]);
}

void
bunpin(struct buf *b) {
  uint key = HASHMAP(b->dev, b->blockno);
  acquire(&bcache.bucketlock[key]);
  b->refcnt--;
  release(&bcache.bucketlock[key]);
}


