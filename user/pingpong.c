#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p[2];
  pipe(p);

  int pid;
  char buf[10];

  if (fork() == 0) {
    pid = getpid();
    read(p[0], buf, 4);
    printf("%d: received %s\n", pid, buf);
    close(p[0]);
    write(p[1], "pong", 4);
    close(p[1]);
  } else {
    write(p[1], "ping", 4);
    close(p[1]);
    pid = getpid();
    read(p[0], buf, 4);
    close(p[0]);
    printf("%d: received %s\n", pid, buf);
  }
  exit(0);
}