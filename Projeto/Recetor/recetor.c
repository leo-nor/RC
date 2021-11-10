/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../utils.c"

#define CMD_SEND 0x01
#define RES_SEND 0x03
#define CMD_REC 0x03
#define RES_REC 0x01

volatile int STOP = FALSE, ERROR = FALSE;

int trama_num = 1;

int main(int argc, char** argv) {
  int fd;
  struct termios oldtio,newtio;

  if ( (argc < 2) ||
        ((strcmp("/dev/ttyS0", argv[1])!=0) &&
        (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS11\n");
    exit(1);
  }

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

  fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd <0) {perror(argv[1]); exit(-1); }

  if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME]    = 5;   /* inter-character timer unused */
  newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */

  /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) pr�ximo(s) caracter(es)
  */

  tcflush(fd, TCIOFLUSH);

  if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");

  char buf[255];
  int counter = 0;

  while (STOP==FALSE && ERROR==FALSE) {
    read(fd, buf + counter, 1);   /* returns after 5 chars have been input */
    switch (counter) {
      case 0:
        if(buf[0] != FLAG) ERROR = TRUE;
        break;
      case 1:
        if(buf[1] != CMD_REC) ERROR = TRUE;
        break;
      case 2:
        if(buf[2] != SET) ERROR = TRUE;
        break;
      case 3:
        if(buf[3] != (CMD_REC^SET)) ERROR = TRUE;
        break;
      case 4:
        if(buf[4] != FLAG) ERROR = TRUE;
        break;
    }
    if(counter == 4) STOP = TRUE;
    else counter++;
  }

  if(ERROR == TRUE) printf("Command received is not a SET\n");
  else send_UA(fd);

  /*
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no gui�o
  */

  tcsetattr(fd,TCSANOW,&oldtio);
  close(fd);
  return 0;
}