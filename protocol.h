#define BAUDRATE B38400

#define FLAG 0x7e
#define LEAK 0x7d
#define FLAGXOR 0x5e //FLAG ^ 0x20
#define LEAKXOR 0x5d //LEAK ^ 0x20
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

#define MAX_SIZE 1500

typedef struct
{
    unsigned int timeout;
    unsigned int numTransmissions;
    char frame[MAX_SIZE];
} linkLayer;