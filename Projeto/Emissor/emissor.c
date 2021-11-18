#include "emissor.h"

struct termios oldtio, newtio;

volatile int STOP = FALSE, ERROR = FALSE;

int flag=1, timeout=1, trama_num = 1;

unsigned char *fileName, *fileData;
off_t fileSize;

void takeAlarm() {
	printf("alarm # %d\n", timeout);
	flag=1;
	timeout++;
  ERROR = TRUE;
}

int main(int argc, char** argv) {
	struct timespec start, finish;
	clock_gettime(CLOCK_REALTIME, &start);

  int fd;

  if ( (argc < 3) ||
        ((strcmp("/dev/ttyS10", argv[1])!=0) &&
        (strcmp("/dev/ttyS11", argv[1])!=0) )) {
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

	if(llwrite(fd) == -1) {
		errorMsg("llwrite() falhou!");
		return -1;
	}

	clock_gettime(CLOCK_REALTIME, &finish);
	double time_taken = (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec) / 1E9;

	printf("Tempo utilizado pelo programa: %f segundos\n", time_taken);

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

  while(timeout <= RETRY_ATTEMPTS) {
    if(send_trama_S(fd, SET, CMD_SEND, trama_num))
			printf("SET command sent\n");
		else
			errorMsg("Failed to send SET command!");

    alarm(TIMEOUT_TIME);

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

  if(timeout > RETRY_ATTEMPTS) errorMsg("Failed to receive UA response!");
  else printf("UA received successfully\n");

  return 0;
}

int llwrite(int fd) {
	return 0;
}

unsigned char *createControlPacket(unsigned char type) {
  unsigned char *packet = (unsigned char *) malloc (5 + sizeof(fileSize) + strlen(fileName));

  packet[0] = type;
  packet[1] = 0x00; //Represents filesize area
  packet[2] = sizeof(fileSize);
  for(int i = 0; i < sizeof(fileSize); i++)
    packet[i + 3] = (fileSize >> (8*(sizeof(fileSize)-1-i)));
  packet[3 + sizeof(fileSize)] = 0x01; //Represents filename area
  packet[3 + sizeof(fileSize) + 1] = strlen(fileName);
	for (int j = 0; j < strlen(fileName); j++)
		packet[5 + sizeof(fileSize) + j] = fileName[j];
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
  fileData = (unsigned char *) malloc(fileSize);
  if(fread(fileData, sizeof(unsigned char), fileSize, file) < fileSize) {
    errorMsg("Failed to read file's data!");
    exit(-1);
  }
}
