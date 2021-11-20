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
#include <limits.h>
#include "api.h"

int main(int argc, char **argv)
{

  int agent = atoi(argv[1]);
  /*//receptor
  if (argc == 2)
  {
    agent = 2;
  }
  //issuer
  else if (argc == 3)
  {
    agent = 1;
  }
  */
  char *portt = agent == 1 ? "/dev/ttyS10" : "/dev/ttyS11";
  int PORT = llopen(portt, agent);
  //int PORT = llopen(SET_FRAME.port, agent);

  //connection was not made
  if (PORT < 0)
  {
    printf("Connection lost..\n");
  }
  //data transmission
  else
  {
    int i = 0;
    char DATA[15000];
    FILE *new_file;

    //issuer
    if (agent == 1)
    {
      //open file
      FILE *file;

      //Open file and handle error
      if ((file = fopen("pinguim.gif", "rb")) == NULL)
      {
        exit(1);
        printf("Error opening file\n");
      }
      else
        printf("File opened\n");

      char line[1];
      size_t len = 0;
      ssize_t read;

      //Read file
      while (fread(line, sizeof(line), 1, file) > 0)
      {
        int llwrite_result = -1;
        char *binary_string;
        printf("%d \n", line[0]);
        llwrite(PORT, line[0]);
      }

      //Close file
      fclose(file);
    }
    //receptor
    else if (agent == 2)
    {
      new_file = fopen("received_file.gif", "wb");
      if (new_file == NULL)
      {
        printf("Unable to create file.\n");
        exit(EXIT_FAILURE);
      }

      printf("Ready to start gettin the file..\n");
      char ok[1];
      int a = llread(PORT, ok);
      while (a > 0)
      {
        DATA[i] = ok[0];
        printf("%d\n", DATA[i]);
        fputc(DATA[i++], new_file);
        a = llread(PORT, ok);
      }
      fclose(new_file);
    }
  }

  return 0;
}
