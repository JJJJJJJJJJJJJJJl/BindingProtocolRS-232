#ifndef PDD_LIST_H_INCLUDED
#define PDD_LIST_H_INCLUDED

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

int llopen(char *port, int agent);

#endif