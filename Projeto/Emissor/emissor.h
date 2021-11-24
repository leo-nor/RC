#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include "../utils.c"

#define CMD_SEND 0x03
#define RES_SEND 0x01
#define CMD_REC 0x01
#define RES_REC 0x03

void takeAlarm();

void registerFileData(unsigned char *fname);

unsigned char *create_trama_I(unsigned char type, unsigned char SEND);

int send_trama_I(int fd, unsigned char *buf, int length);

unsigned char *createDataPacket();

unsigned char *createControlPacket(unsigned char type);

int llopen(int fd, int flag);

int llwrite(int fd, unsigned char *buf, int length);
