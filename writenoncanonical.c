/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7e
#define AEMISS 0x03
#define ARECEPT 0x01
#define CSET 0x03
#define CUA 0x07
#define BEMISS_SET AEMISS ^ CSET
#define BRECEPT_SET ARECEPT ^ CSET
#define BEMISS_UA AEMISS ^ CUA
#define BERECEPT_UA ARECEPT ^ CUA

#define MAX_SIZE 255

struct applicationLayer
{
  int fileDescriptor; //Descritor correspondente à porta série
  int status;         //TRANSMITTER | RECEIVER
};

typedef struct
{
  char port[20];                 //Dispositivo /dev/ttySx, x = 0, 1
  int baudRate;                  //Velocidade de transmissão
  unsigned int sequenceNumber;   //Número de sequência da trama: 0, 1
  unsigned int timeout;          //Valor do temporizador: 1 s
  unsigned int numTransmissions; //Número de tentativas em caso de falha
  char frame[MAX_SIZE];          //Trama
} linkLayer;

volatile int STOP = FALSE;

int hex_to_int(int decimalnum)
{
  int quotient, remainder;
  int i, j = 0;
  char hexadecimalnum[100];

  quotient = decimalnum;

  while (quotient != 0)
  {
    remainder = quotient % 16;
    if (remainder < 10)
      hexadecimalnum[j++] = 48 + remainder;
    else
      hexadecimalnum[j++] = 55 + remainder;
    quotient = quotient / 16;
  }
  return atoi(hexadecimalnum);
}

int main(int argc, char **argv)
{

  linkLayer SET_FRAME = {"/dev/ttyS10", 0, 1, 3, 3, {FLAG, AEMISS, CSET, BEMISS_SET, FLAG}};

  int fd, c, res;
  struct termios oldtio, newtio;
  char buf[255];
  int i, sum = 0, speed = 0;

  /* if ((argc < 2) ||
      ((strcmp("/dev/ttyS10", argv[1]) != 0) &&
       (strcmp("/dev/ttyS11", argv[1]) != 0)))
  {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  } */

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

  fd = open(SET_FRAME.port, O_RDWR | O_NOCTTY);
  if (fd < 0)
  {
    perror(argv[1]);
    exit(-1);
  }

  if (tcgetattr(fd, &oldtio) == -1)
  { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  int SET_FRAME_PORT = open(SET_FRAME.port, O_RDWR | O_NOCTTY);
  if (SET_FRAME_PORT < 0)
  {
    perror(argv[1]);
    exit(-1);
  }

  if (tcgetattr(SET_FRAME_PORT, &oldtio) == -1)
  { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 5;  /* blocking read until 5 chars received */

  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */

  tcflush(fd, TCIOFLUSH);

  if (tcsetattr(fd, TCSANOW, &newtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  char ua_frame[255];
  strcpy(buf, SET_FRAME.frame);
  while (1)
  {
    //SEND SET FRAME
    write(fd, buf, 255);

    //GET UA FRAME
    int ua_frame_received = read(SET_FRAME_PORT, ua_frame, 255);
    if (ua_frame[2] - hex_to_int(CUA) == 0)
    {
      printf("UA_FRAME Received - FLAG: %d | A: %d | C: %d | B: %d\n", ua_frame[0], ua_frame[1], ua_frame[2], ua_frame[3]);
      break;
    }

    sleep(SET_FRAME.timeout);
  }

  if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  close(SET_FRAME_PORT);
  return 0;
}
