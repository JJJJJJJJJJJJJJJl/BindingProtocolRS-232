#ifndef API_H_INCLUDED
#define API_H_INCLUDED

int llopen(char *port, int agent);
int llwrite(int fd, char *bytes, int length);
int llread(int fd, char *buffer);
int llclose(int fd, int agente);

#endif