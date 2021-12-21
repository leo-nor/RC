#include "recetor.h"

struct termios oldtio,newtio;

volatile int STOP = FALSE, ERROR = FALSE;

int fd, trama_num = 1, lastTrama, state = CONTROLSTART;

int allFinished = FALSE;

unsigned char *fileName;
off_t fileSize, lastTramaSize, tmpSize;

FILE *file;

int main(int argc, char** argv) {
  if ( (argc < 2) ||
        ((strcmp("/dev/ttyS0", argv[1])!=0) &&
        (strcmp("/dev/ttyS1", argv[1])!=0) &&
        (strcmp("/dev/ttyS10", argv[1])!=0) &&
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

  if(llopen(fd, RECEIVER) == -1) {
		errorMsg("llopen() falhou!");
		return -1;
	}

	while(!allFinished) {
		switch (state) {
			case CONTROLSTART: {
				unsigned char *buf = malloc(255);
				if(llread(fd, buf) == -1) {
					errorMsg("Failed to receive valid CONTROLSTART!");
					return -1;
				} else {
					state = CONTROLDATA;
					trama_num++;
				}
				free(buf);
				break;
			}
			case CONTROLDATA: {
				unsigned char *buf = malloc(2 * MINK + 6);
				if(llread(fd, buf) == -1) {
					errorMsg("Failed to receive valid CONTROLDATA!");
                } else {
					if(trama_num == lastTrama) {
						state = CONTROLEND;
						closeFile();
					}
					trama_num++;
				}
				free(buf);
				break;
			}
			case CONTROLEND: {
				unsigned char *buf = malloc(255);
				if(llread(fd, buf) == -1) {
					errorMsg("Failed to receive valid CONTROLEND!");
					return -1;
				} else allFinished = TRUE;
				free(buf);
				break;
			}
		}
	}

	allFinished = FALSE;

	while(!allFinished) {
		if(llclose(fd) == -1) {
			errorMsg("llclose() falhou!");
			return -1;
		} else allFinished = TRUE;
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

  newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
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

int test = 0;

int llread(int fd, unsigned char *buf) {
	STOP = FALSE, ERROR = FALSE;
  	int counter = -1, parametro = 0, packetIsData = FALSE, packetIsRead = FALSE;
	int fileNameSize = 0, fileSizeSize = 0, actualSize, l1, l2;
	unsigned char BCC2 = 0x00;
	unsigned char *data, *tmpName;

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
			else
				if(!((trama_num == 1 && buf[4] == CONTROLSTART) || (trama_num >= lastTrama && buf[4] == CONTROLEND))) ERROR = TRUE;
			break;
		}
    	if(counter >= 5) {
			if(packetIsRead) {
				if(packetIsData) {
					if(trama_num < lastTrama)
						for(int i = 0; i < MINK; i++) {
							BCC2 ^= data[i];
						}
					else
						for(int i = 0; i < lastTramaSize; i++) {
							BCC2 ^= data[i];
						}
				} else
					for(int i = 4; i < counter; i++)
						BCC2 ^= buf[i];
				if(buf[counter] != BCC2) {
					ERROR = TRUE;
				}
				else {
					counter++;
			    read(fd, buf + counter, 1);
					if(buf[counter] != FLAG) ERROR = TRUE;
					else STOP = TRUE;
				}
			} else {
				if(packetIsData) {
					switch (parametro) {
						case 0:
							BCC2 ^= CONTROLDATA;
							if(buf[counter] == trama_num % 255) parametro++;
							else ERROR = TRUE;
							break;
						case 1: // L2

							//if(test == 8) ERROR = TRUE;
							//test++;

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
							if(trama_num < lastTrama) data = malloc(MINK);
							else data = malloc(lastTramaSize);
							int j = 0;
							for(int i = 0; i < actualSize; i++) {
                                if(ERROR == TRUE) break;
								if(buf[counter] == ESCAPE) {
									i++;
									counter++;
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
							
							if(trama_num < lastTrama) fwrite(data, sizeof(unsigned char), MINK, file);
							else fwrite(data, sizeof(unsigned char), lastTramaSize, file);
							packetIsRead = TRUE;
							break;
					}
				} else {
					if(buf[4] == CONTROLSTART) {
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
								lastTramaSize = fileSize % MINK;
								lastTrama = fileSize / MINK;
								if(lastTramaSize > 0) lastTrama++;
								else lastTramaSize = MINK;
								lastTrama += 1;
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
								createFile();
								parametro = 0;
								packetIsRead = TRUE;
								break;
						}
					} else if(buf[4] == CONTROLEND) {
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
									tmpSize = tmpSize | (buf[counter] << (8*((fileSizeSize-i)-1)));
									if(!(i == fileSizeSize-1)) {
										counter++;
										read(fd, buf + counter, 1);
									}
								}
								if(tmpSize == fileSize) parametro++;
								else ERROR = TRUE;
								break;
							case 3:
								if(buf[counter] == 0x01) parametro++;
								else ERROR = TRUE;
								break;
							case 4:
								tmpName = (unsigned char *) malloc(buf[counter] + 1);
								fileNameSize = buf[counter];
								parametro++;
								break;
							case 5:
								for(int i = 0; i < fileNameSize; i++) {
									tmpName[i] = buf[counter];
									if(!(i == fileNameSize-1)) {
										counter++;
										read(fd, buf + counter, 1);
									}
								}
								tmpName[fileNameSize] = '\0';
								parametro = 0;
								if(!strcmp(tmpName, fileName)) packetIsRead = TRUE;
								else ERROR = TRUE;
								break;
						}
					}
				}
			}
		}
	}

	if(ERROR == TRUE) {
		char trash[MINK + 255];
		read(fd, trash, MINK + 255);
		if(send_trama_S(fd, RECEIVER, REJ, RES_SEND, trama_num))
			printf("REJ response sent\n");
	  	else
			errorMsg("Failed to send REJ response!");
		return -1;
	} else {
	  printf("DATA received successfully\n");
	  if(send_trama_S(fd, RECEIVER, RR, RES_SEND, trama_num)) {
	  printf("RR response sent\n");
	  } else
			errorMsg("Failed to send RR response!");
		return counter;
	}
}

int llclose(int fd) {
  unsigned char buf[5];
  int counter = 0;
	STOP = FALSE; ERROR = FALSE;

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
        if(buf[2] != DISC) ERROR = TRUE;
        break;
      case 3:
        if(buf[3] != (CMD_REC^DISC)) ERROR = TRUE;
        break;
      case 4:
        if(buf[4] != FLAG) ERROR = TRUE;
        break;
    }
    if(counter == 4) STOP = TRUE;
    else counter++;
  }

	if(ERROR == TRUE) {
		char trash[MINK + 255];
		read(fd, trash, MINK + 255);
		errorMsg("Failed to receive DISC command!");
		return -1;
  } else {
    printf("DISC received successfully\n");
    if(send_trama_S(fd, RECEIVER, DISC, RES_SEND, trama_num))
      printf("DISC response sent\n");
    else
      errorMsg("Failed to send DISC response!");
  }

	STOP = FALSE;
  unsigned char scnd_buf[5];
  counter = 0;

	while (STOP==FALSE && ERROR==FALSE) {
    read(fd, scnd_buf + counter, 1);
    switch (counter) {
      case 0:
        if(scnd_buf[0] != FLAG) ERROR = TRUE;
        break;
      case 1:
        if(scnd_buf[1] != CMD_REC) ERROR = TRUE;
        break;
      case 2:
        if(scnd_buf[2] != UA) ERROR = TRUE;
        break;
      case 3:
        if(scnd_buf[3] != (CMD_REC^UA)) ERROR = TRUE;
        break;
      case 4:
        if(scnd_buf[4] != FLAG) ERROR = TRUE;
        break;
    }
    if(counter == 4) STOP = TRUE;
    else counter++;
  }

	if(ERROR == TRUE) {
		errorMsg("Failed to receive UA command!");
		return -1;
	} else {
		printf("UA received successfully\n");
		return fd;
	}

  if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}
}

void createFile() {
	file = fopen(fileName, "w");

	if(file == NULL) {
		errorMsg("Unable to create file!");
		exit(-1);
	}

	return;
}

void closeFile() {
	fclose(file);
	return;
}
