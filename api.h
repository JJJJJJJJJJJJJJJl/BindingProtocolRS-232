#ifndef PDD_LIST_H_INCLUDED
#define PDD_LIST_H_INCLUDED

int llopen(char *port, int agent);
int llwrite(int fd, char bytes);
char llread(int fd, char *buffer);

#endif