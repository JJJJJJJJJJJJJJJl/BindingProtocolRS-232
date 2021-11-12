/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

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

int count = 0;
void pickup() // atende alarme
{
  printf("alarm #%d\n", count);
  count++;
}

int main(int argc, char **argv)
{
  linkLayer SET_FRAME = {"/dev/ttyS10", 0, 1, 3, 3, {FLAG, AEMISS, CSET, BEMISS_SET, FLAG}};

  struct termios oldtio, newtio;

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

  newtio.c_cc[VTIME] = 1; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 0;  /* blocking read until 5 chars received */

  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */

  tcflush(SET_FRAME_PORT, TCIOFLUSH);

  if (tcsetattr(SET_FRAME_PORT, TCSANOW, &newtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  int z = 0, connection = 0, ua_frame_received = 0;
  char ua_frame_receptor[1], ua_frame[5];

  (void)signal(SIGALRM, pickup);
  //ESTABLISHING CONNECTION
  while (count < 3)
  {
    //SEND SET FRAME
    write(SET_FRAME_PORT, SET_FRAME.frame, 5);

    //STATE MACHINE - READING UA_FRAME
    int ua_frame_bytes;
    while (z != 5)
    {

      alarm(SET_FRAME.timeout);

      //if UA_FRAME has been resent first it needs to read whats already correct
      //essetially skipping buffer data to correct position
      //eg.:Last time it was processing UA_FRAME C had an error, so FLAG + A are correct,
      //but new UA_FRAME has been sent so we skip FLAG + A on the new buffer
      if (count != 0)
      {
        for (int j = 0; j < z; j++)
        {
          read(SET_FRAME_PORT, ua_frame_receptor, 1);
        }
      }

      ua_frame_bytes = read(SET_FRAME_PORT, ua_frame_receptor, 1);
      if (ua_frame_bytes > 0)
      {
        //checking flag values
        if (z == 0 || z == 4)
        {
          //FLAG received,save and move on
          if (ua_frame_receptor[0] == FLAG)
          {
            if (z == 4)
            {
              ua_frame_received = 1;
            }
            ua_frame[z++] = ua_frame_receptor[0];
          }
          //something else received, so it goes back to start
          else
          {
            z = 0;
            memset(ua_frame, 0, sizeof(ua_frame));
            break;
          }
        }
        //checking A value
        else if (z == 1)
        {
          //A received, save and move on
          if (ua_frame_receptor[0] == AEMISS)
          {
            ua_frame[z++] = ua_frame_receptor[0];
          }
          //FLAG received, so it stays waiting for an A
          else if (ua_frame_receptor[0] == FLAG)
          {
            ua_frame[0] = FLAG;
            z = 1;
            break;
          }
          //something else received, so it goes back to start
          else
          {
            z = 0;
            memset(ua_frame, 0, sizeof(ua_frame));
            break;
          }
        }
        //checking C value
        else if (z == 2)
        {
          //C received, save and move on
          if (ua_frame_receptor[0] == CUA)
          {
            ua_frame[z++] = ua_frame_receptor[0];
          }
          //FLAG received, so go back to waiting for an A
          else if (ua_frame_receptor[0] == FLAG)
          {
            ua_frame[0] = FLAG;
            z = 1;
            break;
          }
          //something else received, so it goes back to start
          else
          {
            z = 0;
            memset(ua_frame, 0, sizeof(ua_frame));
            break;
          }
        }
        //checking BCC value
        else
        {
          //BCC rceived, save and move on
          if (ua_frame_receptor[0] == (BEMISS_UA))
          {
            ua_frame[z++] = ua_frame_receptor[0];
          }
          //FLAG received, so go back to waiting for an A
          else if (ua_frame_receptor[0] == FLAG)
          {
            ua_frame[0] = FLAG;
            z = 1;
            break;
          }
          //something else received, so it goes back to start
          else
          {
            z = 0;
            memset(ua_frame, 0, sizeof(ua_frame));
            break;
          }
        }
      }
    }

    if (ua_frame_received)
    {
      connection = 1;
      printf("UA_FRAME Received - FLAG: %d | A: %d | C: %d | B: %d | FLAG: %d\n", ua_frame[0], ua_frame[1], ua_frame[2], ua_frame[3], ua_frame[4]);
      printf("Connection has been established..\n");
      break;
    }
    count++;
  }

  //connection was not made
  if (connection == 0)
  {
    printf("Connection lost..\n");
  }
  //start data transmission
  else
  {
  }

  if (tcsetattr(SET_FRAME_PORT, TCSANOW, &oldtio) == -1)
  {
    perror("tcsetattr");
    exit(-1);
  }

  close(SET_FRAME_PORT);
  return 0;
}
