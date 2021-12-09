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

#define MAX_FILE_SIZE 1000000

int get_file_bytes(FILE *file)
{
  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  fseek(file, 0, SEEK_SET);
  return size;
}

char *slice_array(char *array, int start, int end)
{
  int numElements = (end - start + 1);
  int numBytes = sizeof(int) * numElements;

  char *slice = malloc(numBytes);
  memcpy(slice, array + start, numBytes);

  return slice;
}

char compareArray(char *a, char *b, int size)
{
  int i;
  for (i = 0; i < size; i++)
  {
    if (a[i] != b[i])
      return 1;
  }
  return 0;
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

      //size of file in bytes
      int file_size_bytes;
      //number of bytes of file
      file_size_bytes = get_file_bytes(file);

      if (file_size_bytes > MAX_FILE_SIZE - 1)
      {
        printf("File is too large\nFile size: %d - Maximum capacity: %d\n", file_size_bytes, MAX_FILE_SIZE);
        fclose(file);
        exit(1);
      }

      //array containing every byte of the file
      char DATA[file_size_bytes];
      printf("File size: %d bytes\n", file_size_bytes);

      char byte[1];

      //process file
      while (fread(byte, sizeof(byte), 1, file) > 0)
      {
        DATA[cur_byte++] = byte[0];
      }

      int bytes_to_pass = 256;
      int l = 0, r = bytes_to_pass;
      //send file
      while (r - l > 0)
      {
        llwrite(PORT, slice_array(DATA, l, r), r - l);
        l = r;
        if (r + bytes_to_pass > cur_byte)
        {
          r = cur_byte;
        }
        else
        {
          r += bytes_to_pass;
        }
      }

      printf("File successfully sent\n");
      fclose(file);

      llclose(PORT, 1) > 0 ? printf("Issuer perspective: Connection successfully closed\n") : printf("Issuer perspective: Connection unsuccessfully closed\n");
    }
    //receptor
    else if (agent == 2)
    {
      FILE *new_file;
      if ((new_file = fopen("received_big.png", "wb")) == NULL)
      {
        printf("Error opening new file\n");
        exit(1);
      }

      char received_bytes[1000];
      int bytes_passed = 0;
      int llread_result;
      int a = 0;
      int k = 0;
      printf("Ready to receive file\n");
      while ((llread_result = llread(PORT, received_bytes)) > -2)
      {
        if (k == 0)
        {
          bytes_passed = llread_result;
        }
        if (llread_result > -1)
        {
          for (int j = 0; j < llread_result - 1; j++)
          {

            if (a > 0)
            {
              fputc(received_bytes[j], new_file);
            }
            a++;
          }
        }
        if (llread_result < bytes_passed)
        {
          break;
        }
        k++;
      }

      printf("File successfully received\n");
      fclose(new_file);

      llclose(PORT, 2) > 0 ? printf("Receptor perspective: Connection successfully closed\n") : printf("Receptor perspective: Connection unsuccessfully closed\n");
    }
  }

  return 0;
}
