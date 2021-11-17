#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1
#define RETRY_ATTEMPTS 3
#define TIMEOUT_TIME 3

#define FLAG 0x7E
#define SET 0x03
#define UA 0x07
#define DISC 0x0B
#define RR 0x05
#define REJ 0x01
#define ESCAPE 0x7D
#define ESCAPED_FLAG 0x5E
#define ESCAPED_ESCAPE 0x5D

void errorMsg(char* err) {
  printf("O programa falhou: %s\n", err);
}

int get_Ns(int t_num) {
  if(t_num % 2 == 0) return 1;
  else return 0;
}

int get_Nr(int t_num) {
  if(t_num % 2 == 0) return 0;
  else return 1;
}

unsigned char * create_trama_S(char C, char SEND, int trama_num) {
  unsigned char *buf = (unsigned char *) malloc(5);

  buf[0] = FLAG;
  buf[1] = SEND;
  if((C == RR || C == REJ) && get_Nr(trama_num) == 1) buf[2] = C ^ 0x80;
  else buf[2] = C;
  buf[3] = SEND ^ C;
  buf[4] = FLAG;

  return buf;
}

int send_trama_S(int fd, char C, char SEND, int trama_num) {
  if(write(fd, create_trama_S(C, SEND, trama_num), 5) >= 0) return TRUE;
  else return FALSE;

  switch (C) {
  case SET:
    printf("SET command sent\n");
    break;
  case UA:
    printf("UA response sent\n");
    break;
  case DISC:
    printf("DISC command sent\n");
    break;
  }
}