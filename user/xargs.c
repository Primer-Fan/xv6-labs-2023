#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXLEN 128

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(2, "usage: xargs commands ...\n");
    exit(1);
  }

  char *args[MAXARG];
  int idx = 0;
  for (int i = 1; i < argc; ++i)
    args[idx++] = argv[i];
  for (int i = idx; i < MAXARG; ++i)
    args[i] = (char*) malloc(sizeof(char) * MAXLEN);

  char buf;
  int m = 0;
  while (read(0, &buf, 1)) {
    if (buf == ' ' || buf == '\n') {
      args[idx++][m] = 0;
      m = 0;
    } else {
      args[idx][m++] = buf;
    }
  }
  args[idx] = 0;
  idx = argc - 1;
  if (fork() == 0) {
    exec(args[0], args);
  }
  wait(0);
  exit(0);
}