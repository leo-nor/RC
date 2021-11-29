#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include "../utils.c"

#define CMD_SEND 0x01
#define RES_SEND 0x03
#define CMD_REC 0x03
#define RES_REC 0x01

void takeAlarm();

int llopen(int fd, int flag);

int llread(int fd, unsigned char *buf);

int llclose(int fd);

void createFile();
