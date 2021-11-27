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
			printf("LOOOOOOOK: %i\n\n\n", trama_num);
				unsigned char *buf = malloc(255);
				if(llread(fd, buf) == -1) {
					errorMsg("Failed to receive valid CONTROLSTART!");
					return -1;
				} else state = CONTROLDATA;
				trama_num++;
				free(buf);
				break;
			}
			case CONTROLDATA: {
				unsigned char *buf = malloc(255);
				if(llread(fd, buf) == -1) {
					errorMsg("Failed to receive valid CONTROLDATA!");
					return -1;
				} else {
					if(fileSize % MINK > 0) {
						if(trama_num == ((fileSize / MINK) + 1)) allFinished = TRUE;
					} else {
						if(trama_num == (fileSize / MINK)) allFinished = TRUE;
					}
					/*if(trama_num == 2) {
						printf("\n\n\n");
						printf("PACKET DE DADOS: \n");
						for(int i = 0; i < MINK; i++)
							printf("%i: %i\n", i, fileData[i]);
						printf("\n\n\n");
					}*/
					printf("LOOK: %i == %li\n", trama_num, (fileSize / MINK) + 1);
					printf("Finished: %i\n", allFinished);
					trama_num++;
				}
				free(buf);
				break;
			}
		}
	}

	createFile();

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

  unsigned char buf[5];
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
				if(packetIsData) {
					for(int i = 0; i < MINK; i++) {
						BCC2 ^= data[i];
						//printf("%i: %i\n", i+4, BCC2);
					}
				} else
					for(int i = 4; i < counter; i++)
						BCC2 ^= buf[i];
				if(buf[counter] != BCC2) {ERROR = TRUE;printf("BCC2: %i == %i\n", buf[counter], BCC2);}
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
							//printf("%i: %i\n", -1, BCC2);
							BCC2 ^= CONTROLDATA;
							//printf("%i: %i\n", 0, BCC2);
							if(buf[counter] == trama_num % 255) parametro++;
							else ERROR = TRUE;
							break;
						case 1: // L2
							BCC2 ^= buf[counter - 1];
							//printf("%i: %i\n", 1, BCC2);
							l2 = buf[counter];
							parametro++;
							break;
						case 2: // L1 e cálculo do actualSize
							BCC2 ^= buf[counter - 1];
							//printf("%i: %i\n", 2, BCC2);
							l1 = buf[counter];
							actualSize = 256 * l2 + l1;
							parametro++;
							break;
						case 3:
							BCC2 ^= buf[counter - 1];
							//printf("%i: %i\n", 3, BCC2);
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
							printf("TRAMA %i: %i\n", trama_num, ((trama_num - 2) * MINK));
							memcpy(fileData + ((trama_num - 2) * MINK), data, MINK);
							if(trama_num == 2) {
								for(int i = 0; i < MINK; i++)
									printf("FIRST DATA %i: %i\n", i, fileData[i]);
							}
							/*if(trama_num == 9) {
								for(int i = 0; i < MINK; i++)
									printf("DATA COUNTER: %i %i\n", i, (fileData + ((trama_num - 2) * MINK))[i]);
							}*/
							packetIsRead = TRUE;
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
							fileName = (unsigned char *) malloc(buf[counter] + 1);
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
							fileName[fileNameSize] = '\0';
							parametro = 0;
							packetIsRead = TRUE;
							break;
					}
				}
			}
		}
	}
// VERIFICAR PORQUE É QUE ESTÁ A REESCREVER O BYTE 0
	if(ERROR == TRUE) {
		if(send_trama_S(fd, RECEIVER, REJ, RES_SEND, trama_num))
			printf("REJ response sent\n");
	  else
			errorMsg("Failed to send REJ response!");
			return -1;
	} else {
	  printf("DATA received successfully\n");
	  if(send_trama_S(fd, RECEIVER, RR, RES_SEND, trama_num)) {
		printf("Trama: %i\n", trama_num);
		printf("FIRST DATA %i: %i\n", 0, fileData[0]);
	  printf("RR response sent\n");
	  } else
			errorMsg("Failed to send RR response!");
		return counter;
	}
}

void createFile() {
	FILE *file;
	file = fopen(fileName, "w");

	for(int i = 0; i < fileSize - 2000; i++)
		//printf("FILEDATA %i: %i\n", i, fileData[i]);

	if(file == NULL) {
		errorMsg("Unable to create file!");
		exit(-1);
	}

	fwrite(fileData, sizeof(unsigned char), fileSize, file);

	fclose(file);

	return;
}
