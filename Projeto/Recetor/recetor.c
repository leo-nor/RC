#include "recetor.h"

struct termios oldtio,newtio;

volatile int STOP = FALSE, ERROR = FALSE;

int flag = 1, timeout = 1, trama_num = 1, state = CONTROLSTART;

int allFinished = FALSE;

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

	while(!allFinished) {
		switch (state) {
			case CONTROLSTART: {
				unsigned char buf[255];
				if(llread(fd, buf) == -1) {
					errorMsg("Failed to receive valid CONTROLSTART!");
					return -1;
				} else state = CONTROLDATA;
			}
			case CONTROLDATA: {
				unsigned char buf[255];
				if(llread(fd, buf) == -1) {
					errorMsg("Failed to receive valid CONTROLDATA!"); // A PRIMEIRA TRAMA ESTÁ A CHEGAR MAL, IMPRIMI LA E VERIFICAR PORQUÊ
					return -1;
				} else {
					if(fileSize % MINK > 0)
						if(trama_num == ((fileSize / MINK) + 1)) allFinished = TRUE;
					else
						if(trama_num == (fileSize / MINK)) allFinished = TRUE;
				}
			}
		}
	}

	//printf("GOT HERE\n");

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
    if(send_trama_S(fd, RECEIVER, UA, RES_SEND, trama_num))
      printf("UA response sent\n");
    else
      errorMsg("Failed to send UA response!");
		return fd;
  }
}

int llread(int fd, unsigned char *buf) {
	STOP = FALSE, ERROR = FALSE;
  int counter = -1, parametro = 0, packetIsData = FALSE, packetIsRead = FALSE;
	int fileNameSize = 0, fileSizeSize = 0, actualSize, l1, l2;
	unsigned char BCC2 = 0x00;
	unsigned char *data;

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
					//printf("GOT HERE1: %i\n", trama_num);
					//printf("GOT HERE1: %i\n", buf[2]);
        if(buf[2] != get_Ns(trama_num) << 6) ERROR = TRUE;
					//printf("GOT HERE2: %i\n", trama_num);
					//printf("GOT HERE2: %i\n", get_Ns(trama_num) << 6);
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
				if(packetIsData)
					for(int i = 0; i < MINK; i++)
						BCC2 ^= (fileData + (((trama_num - 1) * MINK)))[i];
				else
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
					switch (parametro) {
						case 0: //Número da trama
							BCC2 ^= CONTROLDATA;
							if(buf[counter] == trama_num % 255) parametro++;
							else ERROR = TRUE;
							break;
						case 1: // L2
							BCC2 ^= buf[counter - 1];
							l2 = buf[counter];
							parametro++;
							break;
						case 2: // L1 e cálculo do actualSize
							BCC2 ^= buf[counter - 1];
							l1 = buf[counter];
							actualSize = 256 * l2 + l1;
							parametro++;
							break;
						case 3:
							BCC2 ^= buf[counter - 1];
							data = malloc(MINK);
							int j = 0;
							for(int i = 0; i < actualSize; i++) {
								//printf("DATA COUNTER: %i ", j + 1);
								//printf("%i\n", buf[counter]);
								if(buf[counter] == ESCAPE) {
									i++;
									counter++;
									//printf("DATA COUNTER: %i ", j + 1);
									//printf("%i\n", buf[counter]);
									read(fd, buf + counter, 1);
									if(buf[counter] == ESCAPED_FLAG)
										data[j] = FLAG;
									else if(buf[counter] == ESCAPED_ESCAPE)
										data[j] = ESCAPE;
								} else
									data[j] = buf[counter];
								counter++;
								if(i + 1 < actualSize) read(fd, buf + counter, 1);
								j++;
							}
							parametro = 0;
							packetIsRead = TRUE;
							memmove(fileData + ((trama_num - 1) * MINK), data, MINK);
							break;
					}
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
							fileData = malloc(fileSize);
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
		if(send_trama_S(fd, RECEIVER, REJ, RES_SEND, trama_num))
			printf("REJ response sent\n");
	  else
			errorMsg("Failed to send REJ response!");
			return -1;
	} else {
	  printf("DATA received successfully\n");
	  if(send_trama_S(fd, RECEIVER, RR, RES_SEND, trama_num)) {
	    printf("RR response sent\n");
			trama_num++;
	  } else
			errorMsg("Failed to send RR response!");
		return counter;
	}
}
