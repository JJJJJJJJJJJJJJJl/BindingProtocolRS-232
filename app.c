//open virtual ports (port names might differ) - sudo socat -d  -d  PTY,link=/dev/ttyS10,mode=777   PTY,link=/dev/ttyS11,mode=777
//run as issuer - ./app /dev/ttyS10 pinguim.gif
//run as receptor - ./app /dev/ttyS11

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

#define MAX_FILE_SIZE 50000

int get_file_bytes(FILE *file)
{
  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  fseek(file, 0, SEEK_SET);
  return size;
}

int main(int argc, char **argv)
{

  //issuer(1) or receptor(2)
  int agent;
  //port
  char *port = argv[1];
  //name of file
  char *file_name;

  //receptor
  if (argc == 2)
  {
    agent = 2;
  }
  //issuer
  else if (argc == 3)
  {
    agent = 1;
    file_name = argv[2];
  }

  //make connection
  int PORT = llopen(port, agent);

  //connection was not made
  if (PORT < 0)
  {
    printf("Connection lost..\n");
    exit(1);
  }

  //successfully connected
  //proceed to data transmission
  else
  {
    //size of file in bytes
    int file_size_bytes;
    //array containing every byte of the file
    char DATA[MAX_FILE_SIZE];
    int cur_byte = 1;

    //issuer
    if (agent == 1)
    {
      //open file
      FILE *file;

      //Open file and handle error
      if ((file = fopen(file_name, "rb")) == NULL)
      {
        printf("Error opening file\n");
        exit(1);
      }
      else
      {
        printf("File '%s' opened\n", file_name);
      }

      //number of bytes of file
      file_size_bytes = get_file_bytes(file);

      if (file_size_bytes > MAX_FILE_SIZE - 1)
      {
        printf("File is too large\nFile size: %d - Maximum capacity: %d\n", file_size_bytes, MAX_FILE_SIZE);
        fclose(file);
        exit(1);
      }

      printf("File size: %d bytes\n", file_size_bytes);

      char byte[1];

      //process file
      while (fread(byte, sizeof(byte), 1, file) > 0)
      {
        DATA[cur_byte++] = byte[0];
      }

      int cur_byte_send = 1;
      //send file
      while (cur_byte_send != cur_byte)
      {
        llwrite(PORT, DATA[cur_byte_send++]);
      }

      printf("File successfully sent\n");
      fclose(file);
    }
    //receptor
    else if (agent == 2)
    {
      FILE *new_file;
      if ((new_file = fopen("received_file.gif", "wb")) == NULL)
      {
        printf("Error opening new file\n");
        exit(1);
      }

      char received_byte[1];
      int llread_result;

      printf("Ready to receive file\n");
      while ((llread_result = llread(PORT, received_byte)) > 0)
      {
        DATA[cur_byte++] = received_byte[0];
      }

      //writting bytes to file
      for (int i = 1; i < cur_byte; i++)
      {
        fputc(DATA[i], new_file);
      }

      printf("File successfully received\n");
      fclose(new_file);
    }
  }

  return 0;
}
