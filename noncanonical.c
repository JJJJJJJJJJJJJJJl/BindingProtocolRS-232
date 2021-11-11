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
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7e
#define AEMISS 0x03
#define ARECEPT 0x01
#define CSET 0x03
#define CUA 0x07
#define ERROR 0x00
#define BEMISS_SET AEMISS ^ CSET
#define BRECEPT_SET ARECEPT ^ CSET
#define BEMISS_UA AEMISS ^ CUA
#define BERECEPT_UA ARECEPT ^ CUA

#define MAX_SIZE 255

typedef struct
{
  int fileDescriptor; //Descritor correspondente à porta série
  int status;         //TRANSMITTER | RECEIVER
} applicationLayer;

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
  int j = 0;
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
  linkLayer UA_FRAME = {"/dev/ttyS11", 0, 1, 3, 3, {FLAG, AEMISS, CUA, BEMISS_UA, FLAG}};
  struct termios oldtio, newtio;
  char buf[255];

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

  int UA_FRAME_PORT = open(UA_FRAME.port, O_RDWR | O_NOCTTY);
  if (UA_FRAME_PORT < 0)
  {
    perror(argv[1]);
    exit(-1);
  }

  if (tcgetattr(UA_FRAME_PORT, &oldtio) == -1)
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

  newtio.c_cc[VTIME] = 1; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 0;  /* blocking read until 5 chars received */

  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */

  tcflush(UA_FRAME_PORT, TCIOFLUSH);

  if (tcsetattr(UA_FRAME_PORT, TCSANOW, &newtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  int set_frame_received;
  //ESTABLISHING CONNECTION
  while (1)
  {
    //GET SET_FRAME
    set_frame_received = read(UA_FRAME_PORT, buf, 5);
    printf("SET_FRAME Received - FLAG: %d | A: %d | C: %d | B: %d | FLAG: %d\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
    buf[set_frame_received] = '\0'; // not sure if completely necessary

    //SEND UA FRAME - only if SET_FRAME WAS RECEIVED
    if (set_frame_received > 0 && buf[2] == CSET)
    {
      write(UA_FRAME_PORT, UA_FRAME.frame, 5);
      printf("Connection has been established..\n");
      break;
    }
  }

  tcsetattr(UA_FRAME_PORT, TCSANOW, &oldtio);
  close(UA_FRAME_PORT);
  return 0;
}
