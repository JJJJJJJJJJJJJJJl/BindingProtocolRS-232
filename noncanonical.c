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
#include <sys/poll.h>

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

int count = 0;

int main(int argc, char **argv)
{
  linkLayer UA_FRAME = {"/dev/ttyS11", 0, 1, 3, 3, {FLAG, AEMISS, CUA, BEMISS_UA, FLAG}};
  struct termios oldtio, newtio;
  struct pollfd pfds[1];

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

  pfds[0].fd = UA_FRAME_PORT;
  pfds[0].events = POLLIN;

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
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

  int z = 0, connection = 0, set_frame_received = 0;
  char set_frame_receptor[1], set_frame[5];

  //ESTABLISHING CONNECTION
  while (count < 3)
  {
    //STATE MACHINE - READING SET_FRAME
    int set_frame_bytes, poll_res;
    while (z != 5)
    {

      //poll blocks execution for UA_FRAME.timeout * 1000 seconds unless:
      // - a file descriptor becomes ready
      // - the call is interrupted by a signal handler
      // - the timeout expires
      poll_res = poll(pfds, 1, UA_FRAME.timeout * 1000);
      if (poll_res < 0)
      {
        perror("poll() failed\n");
        break;
      }
      else if (poll_res == 0)
      {
        fprintf(stderr, "Port had no data to be read.\n");
        break;
      }
      else
      {
        if (pfds[0].revents && POLLIN)
        {

          //if SET_FRAME has been resent first it needs to read whats already correct
          //essetially skipping buffer data to correct position
          //eg.:Last time it was processing SET_FRAME C had an error, so FLAG + A are correct,
          //but new SET_FRAME has been sent so we skip FLAG + A on the new buffer
          if (count != 0)
          {
            for (int j = 0; j < z; j++)
            {
              read(UA_FRAME_PORT, set_frame_receptor, 1);
            }
          }

          set_frame_bytes = read(UA_FRAME_PORT, set_frame_receptor, 1);
          if (set_frame_bytes > 0)
          {
            //checking flag values
            if (z == 0 || z == 4)
            {
              //FLAG received,save and move on
              if (set_frame_receptor[0] == FLAG)
              {
                if (z == 4)
                {
                  set_frame_received = 1;
                }
                set_frame[z++] = set_frame_receptor[0];
              }
              //something else received, so it goes back to start
              else
              {
                z = 0;
                memset(set_frame, 0, sizeof(set_frame));
                break;
              }
            }
            //checking A value
            else if (z == 1)
            {
              //A received, save and move on
              if (set_frame_receptor[0] == AEMISS)
              {
                set_frame[z++] = set_frame_receptor[0];
              }
              //FLAG received, so it stays waiting for an A
              else if (set_frame_receptor[0] == FLAG)
              {
                set_frame[0] = FLAG;
                z = 1;
                break;
              }
              //something else received, so it goes back to start
              else
              {
                z = 0;
                memset(set_frame, 0, sizeof(set_frame));
                break;
              }
            }
            //checking C value
            else if (z == 2)
            {
              //C received, save and move on
              if (set_frame_receptor[0] == CSET)
              {
                set_frame[z++] = set_frame_receptor[0];
              }
              //FLAG received, so go back to waiting for an A
              else if (set_frame_receptor[0] == FLAG)
              {
                set_frame[0] = FLAG;
                z = 1;
                break;
              }
              //something else received, so it goes back to start
              else
              {
                z = 0;
                memset(set_frame, 0, sizeof(set_frame));
                break;
              }
            }
            //checking BCC value
            else
            {
              //BCC rceived, save and move on
              if (set_frame_receptor[0] == (BEMISS_SET))
              {
                set_frame[z++] = set_frame_receptor[0];
              }
              //FLAG received, so go back to waiting for an A
              else if (set_frame_receptor[0] == FLAG)
              {
                set_frame[0] = FLAG;
                z = 1;
                break;
              }
              //something else received, so it goes back to start
              else
              {
                z = 0;
                memset(set_frame, 0, sizeof(set_frame));
                break;
              }
            }
          }
          else
          {
            z++;
          }
        }
      }
    }

    //SET FRAME RECEIVED
    if (set_frame_received)
    {
      connection = 1;
      //SEND UA FRAME
      int bytes = write(UA_FRAME_PORT, UA_FRAME.frame, 5);
      //making sure frame is sent
      while (bytes == 0)
      {
        bytes = write(UA_FRAME_PORT, UA_FRAME.frame, 5);
      }
      fprintf(stderr, "UA_FRAME Received - FLAG: %d | A: %d | C: %d | B: %d | FLAG: %d\n", set_frame[0], set_frame[1], set_frame[2], set_frame[3], set_frame[4]);
      fprintf(stderr, "Connection (FROM RECEIVER PERSPECTIVE) has been established..\n");
      break;
    }
    else
    {
      if (poll_res != 0)
      {
        fprintf(stderr, "Frame had errors.\n");
      }
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

  tcsetattr(UA_FRAME_PORT, TCSANOW, &oldtio);
  close(UA_FRAME_PORT);
  return 0;
}
