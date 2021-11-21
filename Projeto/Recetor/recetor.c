#include "recetor.h"

struct termios oldtio,newtio;

volatile int STOP = FALSE, ERROR = FALSE;

int flag = 1, timeout = 1, trama_num = 1;

unsigned char *fileName, *fileData;
off_t fileSize;

void takeAlarm() {
	printf("alarm # %d\n", timeout);
	flag=1;
	timeout++;
  ERROR = TRUE;
}

int main(int argc, char** argv) {
  int fd;

  if ( (argc < 2) ||
        ((strcmp("/dev/ttyS10", argv[1])!=0) &&
        (strcmp("/dev/ttyS11", argv[1])!=0) )) {
    printf("Usage:\tSerialPort\n\tex: /dev/ttyS1\n");
    exit(1);
  }

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

  fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd <0) {perror(argv[1]); exit(-1); }

  (void) signal(SIGALRM, takeAlarm);

  if(llopen(fd, RECEIVER) == -1) {
		errorMsg("llopen() falhou!");
		return -1;
	}

	char buf[255];
  if(llread(fd, buf) == -1) {
		errorMsg("llopen() falhou!");
		return -1;
	}

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	close(fd);
  return 0;
}

int llopen(int fd, int flag) {
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

  unsigned char buf[255];
  int counter = 0;

  while (STOP==FALSE && ERROR==FALSE) {
    read(fd, buf + counter, 1);
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

	if(ERROR == TRUE) {
		errorMsg("Failed to receive SET command!");
		return -1;
  } else {
    printf("SET received successfully\n");
    if(send_trama_S(fd, UA, RES_SEND, trama_num))
      printf("UA response sent\n");
    else
      errorMsg("Failed to send UA response!");
		return fd;
  }
}

int llread(int fd, unsigned char *buf) {
	STOP = FALSE, ERROR = FALSE;
  int counter = -1, parametro = 0, packetIsData = FALSE, packetIsRead = FALSE;
	int fileNameSize = 0, fileSizeSize = 0;
	unsigned char BCC2 = 0x00;

  while (STOP==FALSE && ERROR==FALSE) {
		counter++;
    read(fd, buf + counter, 1);
    switch (counter) {
      case 0:
        if(buf[0] != FLAG) ERROR = TRUE;
        break;
      case 1:
        if(buf[1] != CMD_REC) ERROR = TRUE;
        break;
      case 2:
        if(buf[2] != get_Ns(trama_num) << 6) ERROR = TRUE;
        break;
      case 3:
        if(buf[3] != (CMD_REC^get_Ns(trama_num) << 6)) ERROR = TRUE;
        break;
			case 4:
				if(buf[4] == CONTROLDATA) packetIsData = TRUE;
				break;
    }
    if(counter >= 5) {
			if(packetIsRead) {
				for(int i = 4; i < counter; i++)
					BCC2 ^= buf[i];
				if(buf[counter] != BCC2) ERROR = TRUE;
				else {
					counter++;
			    read(fd, buf + counter, 1);
					if(buf[counter] != FLAG) ERROR = TRUE;
					else STOP = TRUE;
				}
			} else {
				if(packetIsData) {

				} else {
					switch (parametro) {
						case 0:
							if(buf[counter] == 0x00) parametro++;
							else ERROR = TRUE;
							break;
						case 1:
							fileSizeSize = buf[counter];
							parametro++;
							break;
						case 2:
							for(int i = 0; i < fileSizeSize; i++) {
								fileSize = fileSize | (buf[counter] << (8*((fileSizeSize-i)-1)));
								if(!(i == fileSizeSize-1)) {
									counter++;
									read(fd, buf + counter, 1);
								}
							}
							parametro++;
							break;
						case 3:
							if(buf[counter] == 0x01) parametro++;
							else ERROR = TRUE;
							break;
						case 4:
							fileName = (unsigned char *) malloc(buf[counter]);
							fileNameSize = buf[counter];
							parametro++;
							break;
						case 5:
							for(int i = 0; i < fileNameSize; i++) {
								fileName[i] = buf[counter];
								if(!(i == fileNameSize-1)) {
									counter++;
									read(fd, buf + counter, 1);
								}
							}
							parametro = 0;
							packetIsRead = TRUE;
							break;
					}
				}
			}
		}
	}

	if(ERROR == TRUE) {
		if(send_trama_S(fd, REJ, RES_SEND, trama_num))
			printf("REJ response sent\n");
	  else
			errorMsg("Failed to send REJ response!");
			return -1;
	} else {
	  printf("DATA received successfully\n");
	  if(send_trama_S(fd, RR, RES_SEND, trama_num))
	    printf("RR response sent\n");
	  else
			errorMsg("Failed to send RR response!");
			return counter;
	}
}
