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
#include "connection.h"

char *int2bin(int i)
{
  size_t bits = sizeof(int) * CHAR_BIT;

  char *str = malloc(bits + 1);
  if (!str)
    return NULL;
  str[bits] = 0;

  // type punning because signed shift is implementation-defined
  unsigned u = *(unsigned *)&i;
  for (; bits--; u >>= 1)
    str[bits] = u & 1 ? '1' : '0';

  return str;
}

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
    //issuer
    if (agent == 1)
    {
      //open file
      FILE *fo;

      //Open file and handle error
      if ((fo = fopen("pinguim.gif", "r")) == NULL)
      {
        exit(1);
        printf("Error opening file\n");
      }
      else
        printf("File opened\n");

      char *line = NULL;
      size_t len = 0;
      ssize_t read;

      //Read file
      while ((read = getline(&line, &len, fo)) != -1)
      {
        char *binary_string;
        for (int i = 0; i < strlen(line); i++)
        {
          binary_string = int2bin(line[i]);
          printf("%s\n", binary_string);
        }
        //llwrite(PORT, line, read)
      }

      //Close file
      fclose(fo);
    }

    //llwrite();
  }

  return 0;
}
