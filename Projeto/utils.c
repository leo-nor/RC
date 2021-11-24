#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1
#define RETRY_ATTEMPTS 3
#define TIMEOUT_TIME 3
#define MINK 1024

#define FLAG 0x7E
#define SET 0x03
#define UA 0x07
#define DISC 0x0B
#define RR 0x05
#define REJ 0x01
#define ESCAPE 0x7D
#define ESCAPED_FLAG 0x5E
#define ESCAPED_ESCAPE 0x5D

#define CONTROLDATA 0x01
#define CONTROLSTART 0x02
#define CONTROLEND 0x03

#define TRANSMITTER 0x01
#define RECEIVER 0x02

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

unsigned char * create_trama_S(unsigned char SENDER, unsigned char C, unsigned char SEND, int trama_num) {
  unsigned char *buf = (unsigned char *) malloc(5);

  buf[0] = FLAG;
  buf[1] = SEND;
  if(SENDER == TRANSMITTER) {
    if((C == RR || C == REJ) && get_Ns(trama_num) == 1) buf[2] = C ^ 0x80;
    else buf[2] = C;
  } else { // RECEIVER
    if((C == RR || C == REJ) && get_Nr(trama_num) == 1) buf[2] = C ^ 0x80;
    else buf[2] = C;
  }
  if(SENDER == TRANSMITTER) {
    if((C == RR || C == REJ) && get_Ns(trama_num) == 1) buf[3] = SEND ^ (C ^ 0x80);
    else buf[3] = SEND ^ C;
  } else { // RECEIVER
    if((C == RR || C == REJ) && get_Nr(trama_num) == 1) buf[3] = SEND ^ (C ^ 0x80);
    else buf[3] = SEND ^ C;
  }
  buf[4] = FLAG;

  return buf;
}

int send_trama_S(int fd, unsigned char SENDER, unsigned char C, unsigned char SEND, int trama_num) {
  unsigned char *buf = create_trama_S(SENDER, C, SEND, trama_num);
  //for(int i = 0; i < 5; i++)
    //printf("wtf %i\n", buf[i]);
  if(write(fd, buf, 5) >= 0) return TRUE;
  else return FALSE;
}
