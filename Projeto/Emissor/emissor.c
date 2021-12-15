#include "emissor.h"

struct termios oldtio, newtio;
struct timespec start, finish;

volatile int STOP = FALSE, ERROR = FALSE;

int flag=1, timeout=1, trama_num = 1, lastTrama = -1, state = CONTROLSTART;

int allFinished = FALSE;

int lastDataPacketSize = -1;
unsigned char lastDataPacketBCC2;

unsigned char *fileName, *fileData;
off_t fileSize, lastTramaSize;

void takeAlarm() {
	printf("alarm # %d\n", timeout);
	flag=1;
	timeout++;
  ERROR = TRUE;
}

int main(int argc, char** argv) {
	clock_gettime(CLOCK_REALTIME, &start);

  int fd;

  if ( (argc < 3) ||
        ((strcmp("/dev/ttyS0", argv[1])!=0) &&
        (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    printf("Usage:\tSerialPort filename\n\tex: /dev/ttyS1 pinguim.gif\n");
    exit(1);
  }

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

  fd = open(argv[1], O_RDWR | O_NOCTTY );
  if (fd <0) {perror(argv[1]); exit(-1); }

	registerFileData(argv[2]);

  (void) signal(SIGALRM, takeAlarm);

	if(llopen(fd, TRANSMITTER) == -1) {
		errorMsg("llopen() falhou!");
		return -1;
	}

	while(!allFinished) {
		switch (state) {
			case CONTROLSTART: {
				if(llwrite(fd, create_trama_I(CONTROLSTART, CMD_SEND), 6 + 5 + sizeof(fileSize) + strlen(fileName)) == -1) {
					errorMsg("llwrite() falhou!");
					return -1;
				} else printf("CONTROLSTART command sent\n");

				unsigned char *new_buf = malloc(5);
		    int counter = 0;
				STOP = FALSE, ERROR = FALSE;

		    alarm(TIMEOUT_TIME);

		    while (STOP==FALSE && ERROR==FALSE) {
		      read(fd, new_buf + counter, 1);
		      switch (counter) {
		        case 0:
		          if(new_buf[0] != FLAG) ERROR = TRUE;
		          break;
		        case 1:
		          if(new_buf[1] != RES_REC) ERROR = TRUE;
		          break;
		        case 2:
		          if(get_Nr(trama_num) == 0) {
								if(new_buf[2] != RR && new_buf[2] != REJ)
									ERROR = TRUE;
							} else {
								if(new_buf[2] != (RR ^ 0x80) && new_buf[2] != (REJ ^ 0x80))
									ERROR = TRUE;
							}
		          break;
		        case 3:
		          if(get_Nr(trama_num) == 0) {
								if(new_buf[3] != (RES_REC ^ RR) && new_buf[3] != (RES_REC ^ REJ))
									ERROR = TRUE;
							} else {
								if(new_buf[3] != (RES_REC ^ (RR ^ 0x80)) && new_buf[3] != (RES_REC ^ (REJ ^ 0x80)))
									ERROR = TRUE;
							}
							break;
		        case 4:
		          if(new_buf[4] != FLAG) ERROR = TRUE;
		          break;
		      }
		      if(counter == 4) STOP = TRUE;
		      else counter++;
		    }

		    if(STOP == TRUE) {
					if(new_buf[2] == REJ || new_buf[2] == (REJ ^ 0x80)) {
						printf("REJ received, repeating CONTROLSTART\n");
					} else {
						printf("RR received successfully\n");
						state = CONTROLDATA;
						trama_num++;
					}
					timeout = 1;
				} else {
					if(timeout > RETRY_ATTEMPTS) {
			      errorMsg("Failed to receive valid response to CONTROLSTART!");
						return -1;
					}
		    }
				free(new_buf);
				break;
			}
			case CONTROLDATA: {
				unsigned char *tmp = malloc(lastDataPacketSize + 6);
				tmp = create_trama_I(CONTROLDATA, CMD_SEND);

				if(llwrite(fd, tmp, lastDataPacketSize + 6) == -1) {
					errorMsg("llwrite() falhou!");
					return -1;
				} else printf("CONTROLDATA command sent\n");

				unsigned char *new_buf = malloc(5);
		    int counter = 0;
				STOP = FALSE, ERROR = FALSE;

		    alarm(TIMEOUT_TIME);

		    while (STOP==FALSE && ERROR==FALSE) {
		      read(fd, new_buf + counter, 1);
		      switch (counter) {
		        case 0:
		          if(new_buf[0] != FLAG) ERROR = TRUE;
		          break;
		        case 1:
		          if(new_buf[1] != RES_REC) ERROR = TRUE;
		          break;
		        case 2:
		          if(get_Nr(trama_num) == 0) {
								if(new_buf[2] != RR && new_buf[2] != REJ)
									ERROR = TRUE;
							} else {
								if(new_buf[2] != (RR ^ 0x80) && new_buf[2] != (REJ ^ 0x80))
									ERROR = TRUE;
							}
							break;
		        case 3:
							/*printf("\n\n");
							printf("Num: %i\n", trama_num);
							printf("STOP: %i\n", STOP);
							printf("ERROR: %i\n", ERROR);
							printf("\n\n");*/
		          if(get_Nr(trama_num) == 0) {
								if(new_buf[3] != (RES_REC ^ RR) && new_buf[3] != (RES_REC ^ REJ))
									ERROR = TRUE;
							} else {
								if(new_buf[3] != (RES_REC ^ (RR ^ 0x80)) && new_buf[3] != (RES_REC ^ (REJ ^ 0x80)))
									ERROR = TRUE;
							}
								/*printf("\n\n");
								printf("Num: %i\n", trama_num);
								printf("STOP: %i\n", STOP);
								printf("ERROR: %i\n", ERROR);
								printf("%i != %i\n", new_buf[3], (RES_REC ^ RR));
								printf("\n\n");*/
							break;
		        case 4:
		          if(new_buf[4] != FLAG) ERROR = TRUE;
		          break;
		      }
		      if(counter == 4) STOP = TRUE;
		      else counter++;
		    }
				//for(int i = 0; i < 5; i++) printf("wtf %i\n", new_buf[i]);
				//printf("BREAK\n");

				if(STOP == TRUE) {
					if(new_buf[2] == REJ || new_buf[2] == (REJ ^ 0x80)) {
						printf("REJ received, repeating CONTROLDATA\n");
					} else {
						printf("RR received successfully\n");
						if(trama_num == lastTrama) state = CONTROLEND;
						trama_num++;
					}
					timeout = 1;
				} else {
					if(timeout > RETRY_ATTEMPTS) {
			      errorMsg("Failed to receive valid response to CONTROLDATA!");
						return -1;
					}
		    }
				break;
			}
			case CONTROLEND: {
				if(llwrite(fd, create_trama_I(CONTROLEND, CMD_SEND), 6 + 5 + sizeof(fileSize) + strlen(fileName)) == -1) {
					errorMsg("llwrite() falhou!");
					return -1;
				} else printf("CONTROLEND command sent\n");

				unsigned char *new_buf = malloc(5);
		    int counter = 0;
				STOP = FALSE, ERROR = FALSE;

		    alarm(TIMEOUT_TIME);

		    while (STOP==FALSE && ERROR==FALSE) {
		      read(fd, new_buf + counter, 1);
		      switch (counter) {
		        case 0:
		          if(new_buf[0] != FLAG) ERROR = TRUE;
		          break;
		        case 1:
		          if(new_buf[1] != RES_REC) ERROR = TRUE;
		          break;
		        case 2:
		          if(get_Nr(trama_num) == 0) {
								if(new_buf[2] != RR && new_buf[2] != REJ)
									ERROR = TRUE;
							} else {
								if(new_buf[2] != (RR ^ 0x80) && new_buf[2] != (REJ ^ 0x80))
									ERROR = TRUE;
							}
		          break;
		        case 3:
		          if(get_Nr(trama_num) == 0) {
								if(new_buf[3] != (RES_REC ^ RR) && new_buf[3] != (RES_REC ^ REJ))
									ERROR = TRUE;
							} else {
								if(new_buf[3] != (RES_REC ^ (RR ^ 0x80)) && new_buf[3] != (RES_REC ^ (REJ ^ 0x80)))
									ERROR = TRUE;
							}
							break;
		        case 4:
		          if(new_buf[4] != FLAG) ERROR = TRUE;
		          break;
		      }
		      if(counter == 4) STOP = TRUE;
		      else counter++;
		    }

		    if(STOP == TRUE) {
					if(new_buf[2] == REJ || new_buf[2] == (REJ ^ 0x80)) {
						printf("REJ received, repeating CONTROLEND\n");
					} else {
						printf("RR received successfully\n");
						allFinished = TRUE;
						//trama_num++;
					}
					timeout = 1;
				} else {
					if(timeout > RETRY_ATTEMPTS) {
			      errorMsg("Failed to receive valid response to CONTROLEND!");
						return -1;
					}
		    }
				free(new_buf);
				break;
			}
		}
	}

	if(llclose(fd) == -1) {
		errorMsg("llclose() falhou!");
		return -1;
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

  while(timeout <= RETRY_ATTEMPTS) {
    if(send_trama_S(fd, TRANSMITTER, SET, CMD_SEND, trama_num))
			printf("SET command sent\n");
		else
			errorMsg("Failed to send SET command!");

    alarm(TIMEOUT_TIME);

    unsigned char new_buf[255];
    int counter = 0;

    while (STOP==FALSE && ERROR==FALSE) {
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

  if(timeout > RETRY_ATTEMPTS) {
		errorMsg("Failed to receive UA response!");
		return -1;
	} else {
		printf("UA received successfully\n");
		timeout = 1;
		return fd;
	}
}

int llwrite(int fd, unsigned char *buf, int length) {
	return send_trama_I(fd, buf, length);
}

int llclose(int fd) {
  while(timeout <= RETRY_ATTEMPTS) {
    if(send_trama_S(fd, TRANSMITTER, DISC, CMD_SEND, trama_num))
			printf("DISC command sent\n");
		else
			errorMsg("Failed to send DISC command!");

    alarm(TIMEOUT_TIME);

    unsigned char new_buf[5];
    int counter = 0;

    while (STOP==FALSE && ERROR==FALSE) {
      read(fd, new_buf + counter, 1);
      switch (counter) {
        case 0:
          if(new_buf[0] != FLAG) ERROR = TRUE;
          break;
        case 1:
          if(new_buf[1] != RES_REC) ERROR = TRUE;
          break;
        case 2:
          if(new_buf[2] != DISC) ERROR = TRUE;
          break;
        case 3:
          if(new_buf[3] != (RES_REC^DISC)) ERROR = TRUE;
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

  if(timeout > RETRY_ATTEMPTS) {
		errorMsg("Failed to receive DISC response!");
		return -1;
	} else {
		printf("DISC received successfully\n");
		timeout = 1;
		if(send_trama_S(fd, TRANSMITTER, UA, CMD_SEND, trama_num))
			printf("UA command sent\n");
		else
			errorMsg("Failed to send UA command!");

		clock_gettime(CLOCK_REALTIME, &finish);
		double time_taken = (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec) / 1E9;

		printf("Tempo utilizado pelo programa: %f segundos\n", time_taken);

		if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
			perror("tcsetattr");
			exit(-1);
		}

		return fd;
	}
}

unsigned char * create_trama_I(unsigned char type, unsigned char SEND) {
	unsigned char BCC2 = 0x00;
	unsigned char *packet, *buf;
	int packetSize;
	if(type == CONTROLDATA) {
		packet = createDataPacket();
		packetSize = lastDataPacketSize;
		buf = (unsigned char *) malloc(6 + packetSize);
	} else {
		packet = createControlPacket(type);
		packetSize = 5 + sizeof(fileSize) + strlen(fileName);
		buf = (unsigned char *) malloc(6 + packetSize);
	}
  buf[0] = FLAG;
  buf[1] = SEND;
  buf[2] = get_Ns(trama_num) << 6;
  buf[3] = SEND ^ (get_Ns(trama_num) << 6);
	memmove(buf + 4, packet, packetSize);
	if(type == CONTROLDATA) {
		buf[4 + packetSize] = lastDataPacketBCC2;
	} else {
		for(int i = 0; i < packetSize; i++)
			BCC2 ^= packet[i];
		buf[4 + packetSize] = BCC2;
	}
	buf[4 + packetSize + 1] = FLAG;

  return buf;
}

int send_trama_I(int fd, unsigned char *buf, int length) {

	//for(int i = 8; i < length; i++)
		//printf("OTHER DATA COUNTER: %i %i\n", i-7, buf[i]);

  if(write(fd, buf, length) >= 0) return length;
  else return FALSE;
}

unsigned char *createControlPacket(unsigned char type) {
  unsigned char *packet = (unsigned char *) malloc(5 + sizeof(fileSize) + strlen(fileName));

  packet[0] = type;
  packet[1] = 0x00; //Represents filesize area
  packet[2] = sizeof(fileSize);
  for(int i = 0; i < sizeof(fileSize); i++)
    packet[i + 3] = (fileSize >> (8*(sizeof(fileSize)-1-i)));
  packet[3 + sizeof(fileSize)] = 0x01; //Represents filename area
  packet[3 + sizeof(fileSize) + 1] = strlen(fileName);
	for (int j = 0; j < strlen(fileName); j++)
		packet[5 + sizeof(fileSize) + j] = fileName[j];

		return packet;
}

unsigned char *createDataPacket() {
	unsigned char *packet, *tmp;
	int toTransferSize, actualSize, j = 0;
	if(lastTrama == trama_num) {
		tmp = malloc(lastTramaSize);
		actualSize = lastTramaSize;
		toTransferSize = lastTramaSize;
	} else {
		tmp = malloc(MINK);
		actualSize = MINK;
		toTransferSize = MINK;
	}

	memmove(tmp, fileData + ((trama_num - 2) * MINK), toTransferSize);
  for(int i = 0; i < toTransferSize; i++)
		if(tmp[i] == FLAG || tmp[i] == ESCAPE) actualSize++;

	packet = malloc(4 + actualSize);

  packet[0] = CONTROLDATA;
  packet[1] = trama_num % 255;
	packet[2] = (int) actualSize / 256;
	packet[3] = (int) actualSize % 256;
	lastDataPacketBCC2 = 0x00;
	//printf("%i: %i\n", -1, lastDataPacketBCC2);
	lastDataPacketBCC2 ^= packet[0];
	//printf("%i: %i\n", 0, lastDataPacketBCC2);
	lastDataPacketBCC2 ^= packet[1];
	//printf("%i: %i\n", 1, lastDataPacketBCC2);
	lastDataPacketBCC2 ^= packet[2];
	//printf("%i: %i\n", 2, lastDataPacketBCC2);
	lastDataPacketBCC2 ^= packet[3];
	//printf("%i: %i\n", 3, lastDataPacketBCC2);
  for(int i = 0; i < toTransferSize; i++) {
		lastDataPacketBCC2 ^= tmp[i];
		/*if(trama_num == lastTrama) {
			printf("DATA COUNTER: %i %i\n", i, tmp[i]);
		}*/
		if(tmp[i] == FLAG) {
			packet[4 + j] = ESCAPE;
			j++;
			packet[4 + j] = ESCAPED_FLAG;
		} else if(tmp[i] == ESCAPE) {
			packet[4 + j] = ESCAPE;
			j++;
			packet[4 + j] = ESCAPED_ESCAPE;
		} else {
			packet[4 + j] = tmp[i];
		}
		j++;
	}

	lastDataPacketSize = 4 + actualSize;

	return packet;
}

void registerFileData(unsigned char *fname) {
	fileName = fname;
  FILE *file;
  struct stat st;

  if((file = fopen(fileName, "r")) == NULL) {
    errorMsg("Failed to open file!");
    exit(-1);
  }
  rewind(file);
  if(stat(fileName, &st) == -1) {
    errorMsg("Failed to get file's metadata!");
    exit(-1);
  }
  fileSize = st.st_size;
	lastTramaSize = fileSize % MINK;
	lastTrama = fileSize / MINK;
	if(lastTramaSize > 0) lastTrama++;
	else lastTramaSize = MINK;
	lastTrama += 1;
  fileData = malloc(fileSize);
  if(fread(fileData, sizeof(unsigned char), fileSize, file) < fileSize) {
    errorMsg("Failed to read file's data!");
    exit(-1);
  }
}
