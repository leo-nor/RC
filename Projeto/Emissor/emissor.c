/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "../utils.c"

#define CMD_SEND 0x03
#define RES_SEND 0x01
#define CMD_REC 0x01
#define RES_REC 0x03

volatile int STOP = FALSE, ERROR = FALSE;

int flag=1, timeout=1, trama_num = 1;

void atende() {
	printf("alarme # %d\n", timeout);
	flag=1;
	timeout++;
  ERROR = TRUE;
}

int main(int argc, char** argv) {
  (void) signal(SIGALRM, atende);

  int fd;
  struct termios oldtio,newtio;

  if ( (argc < 2) ||
        ((strcmp("/dev/ttyS0", argv[1])!=0) &&
        (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
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

  newtio.c_cc[VTIME]    = 30;   /* inter-character timer unused */
  newtio.c_cc[VMIN]     = 0;   /* blocking read until 5 chars received */

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

  while(timeout <= RETRY_ATTEMPTS) {
    send_SET(fd);
    
    alarm(3);

    char new_buf[255];
    int counter = 0;

    while (STOP==FALSE && ERROR==FALSE) {
      flag = 0;
      read(fd, new_buf + counter, 1);
      switch (counter) {
        case 0:
          if(new_buf[0] != FLAG) ERROR = TRUE;
          break;
        case 1:
          if(new_buf[1] != RES_REC) ERROR = TRUE;
          break;
        case 2:
          if(new_buf[2] != UA) ERROR = TRUE;
          break;
        case 3:
          if(new_buf[3] != (RES_REC^UA)) ERROR = TRUE;
          break;
        case 4:
          if(new_buf[4] != FLAG) ERROR = TRUE;
          break;
      }
      if(counter == 4) STOP = TRUE;
      else counter++;
    }

    if(STOP == TRUE) break;
    else {
      ERROR = FALSE;
      counter = 0;
    }
  }

  if(timeout > RETRY_ATTEMPTS) printf("Failed to receive UA response\n");
  else printf("UA received successfully\n");

  /*
    O ciclo FOR e as instru��es seguintes devem ser alterados de modo a respeitar
    o indicado no gui�o
  */

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}