#define BAUDRATE B38400

#define FLAG 0x7e
#define A 0x03
#define CSET 0x03
#define CUA 0x07
#define CRR 0X05
#define CREJ 0x01
#define BCCSET A ^ CSET
#define BCCUA A ^ CUA
#define BCCRR A ^ CRR
#define BCCREJ A ^ CREJ
#define BCCI 0x02

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