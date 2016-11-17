#include "../include/t2fs.h"
#include "../include/apidisk.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, const char *argv[])
{
  unsigned char *buffer = malloc(SECTOR_SIZE);
  read_sector(0, buffer);
  int i;
  for (i = 0; i < SECTOR_SIZE; i++) {
    printf("%c\n", buffer[i]);
  }
  return 0;
}
