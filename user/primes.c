#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void prime(int r_fd) {
  int p[2];
  pipe(p);

  int n;
  if (read(r_fd, &n, 4) == 0)
    return;
  printf("prime %d\n", n);

  if (fork() == 0) {
    close(p[1]);
    prime(p[0]);
    close(p[0]);
  } else {
    close(p[0]);
    int num;
    while (read(r_fd, &num, 4)) {
      if (num % n)
        write(p[1], &num, 4);
    }
    close(p[1]);
    wait(0);
  }
}

int
main(int argc, char *argv[])
{
  int p[2];
  pipe(p);

  if (fork() == 0) {
    close(p[1]);
    prime(p[0]);
    close(p[0]);
  } else  {
    close(p[0]);
    for (int num = 2; num <= 35; ++num) {
      write(p[1], &num, 4);
    }
    close(p[1]);
    wait(0);
  }
  exit(0);
}