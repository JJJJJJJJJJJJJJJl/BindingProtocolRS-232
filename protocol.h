#define BAUDRATE B38400

#define FLAG 0x7e
#define A 0x03
#define CSET 0x03
#define CUA 0x07
#define CRR 0X05
#define CREJ 0x01
#define CDISC 0x0b
#define BCCSET A ^ CSET
#define BCCUA A ^ CUA
#define BCCRR A ^ CRR
#define BCCREJ A ^ CREJ
#define BCCDISC A ^ CDISC
#define BCCI 0x02

#define MAX_SIZE 10

typedef struct
{
    unsigned int timeout;
    unsigned int numTransmissions;
    char frame[MAX_SIZE];
} linkLayer;