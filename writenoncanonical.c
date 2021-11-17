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
#include "connection.h"

int main(int argc, char **argv)
{

  /* int agent;
  //receptor
  if (argc == 2)
  {
    agent = 2;
  }
  //issuer
  else if (argc == 3)
  {
    agent = 1;
  } */
  char *portt = atoi(argv[1]) == 1 ? "/dev/ttyS10" : "/dev/ttyS11";
  int PORT = llopen(portt, atoi(argv[1]));
  //int PORT = llopen(SET_FRAME.port, agent);

  //connection was not made
  if (PORT < 0)
  {
    printf("Connection lost..\n");
  }
  //start data transmission
  else
  {
    //open file

    //llwrite();
  }

  return 0;
}
