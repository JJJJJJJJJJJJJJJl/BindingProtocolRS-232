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
#include "protocol.h"

int cur_transmission, flag;
void pickup()
{
    flag = 1;
    cur_transmission++;
    printf("fodase\n");
}

char *decimal_to_binary(int n)
{
    int c, d, t;
    char *p;

    t = 0;
    p = (char *)malloc(8 + 1);

    if (p == NULL)
        exit(EXIT_FAILURE);

    for (c = 7; c >= 0; c--)
    {
        d = n >> c;

        if (d & 1)
            *(p + t) = 1 + '0';
        else
            *(p + t) = 0 + '0';

        t++;
    }
    *(p + t) = '\0';
    return p;
}

//establishing connection frames
linkLayer SET_FRAME = {3, 3, {FLAG, A, CSET, BCCSET, FLAG}};
linkLayer UA_FRAME = {3, 3, {FLAG, A, CUA, BCCUA, FLAG}};

//information frames
linkLayer DATA_FRAME = {2, 3, {}};

//verifying information frames
char RR[5] = {FLAG, A, CRR, BCCRR, FLAG};
char REJ[5] = {FLAG, A, CREJ, BCCREJ, FLAG};

//disconnect frame
linkLayer DISC_FRAME = {3, 3, {FLAG, A, CDISC, BCCDISC, FLAG}};

struct termios oldtio, newtio;
struct pollfd pfds[1];
int connection;

int llopen(char *port, int agent)
{
    int PORT;
    if (agent == 1)
    {
        connection = 0;

        PORT = open(port, O_RDWR | O_NOCTTY);
        if (PORT < 0)
        {
            perror(port);
            exit(-1);
        }

        if (tcgetattr(PORT, &oldtio) == -1)
        {
            perror("tcgetattr");
            exit(-1);
        }

        pfds[0].fd = PORT;
        pfds[0].events = POLLIN;

        bzero(&newtio, sizeof(newtio));
        newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR;
        newtio.c_oflag = 0;

        newtio.c_lflag = 0;

        newtio.c_cc[VTIME] = 0;
        newtio.c_cc[VMIN] = 0;

        tcflush(PORT, TCIOFLUSH);

        if (tcsetattr(PORT, TCSANOW, &newtio) == -1)
        {
            perror("tcsetattr");
            exit(-1);
        }

        int machine_state = 0, ua_frame_received = 0;
        char ua_frame_receptor[1], ua_frame[5];

        (void)signal(SIGALRM, pickup);
        cur_transmission = 0;
        //ESTABLISHING CONNECTION
        while (cur_transmission < SET_FRAME.numTransmissions)
        {
            //SEND SET FRAME
            int bytes = write(PORT, SET_FRAME.frame, 5);
            //making sure frame is sent
            while (bytes == 0)
            {
                bytes = write(PORT, SET_FRAME.frame, 5);
            }

            flag = 0;
            alarm(SET_FRAME.timeout);

            //STATE MACHINE - READING UA_FRAME
            int ua_frame_bytes, poll_res;
            while (machine_state != 5)
            {

                if (flag)
                {
                    fprintf(stderr, "Alarm went off\n");
                    break;
                }
                //poll blocks execution for 1000 seconds unless:
                // - a file descriptor becomes ready
                // - the call is interrupted by a signal handler
                // - the timeout expires
                poll_res = poll(pfds, 1, SET_FRAME.timeout * 1000);
                if (poll_res < 0)
                {
                    perror("Either poll() failed or alarm went off\n");
                    break;
                }
                else if (poll_res == 0)
                {
                    fprintf(stderr, "Port had no data to be read.\n");
                    break;
                }
                else
                {
                    if (pfds[0].revents && POLLIN)
                    {
                        ua_frame_bytes = read(PORT, ua_frame_receptor, 1);
                        if (ua_frame_bytes > 0)
                        {
                            //checking flag values
                            if (machine_state == 0 || machine_state == 4)
                            {
                                //FLAG received,save and move on
                                if (ua_frame_receptor[0] == FLAG)
                                {
                                    if (machine_state == 4)
                                    {
                                        ua_frame_received = 1;
                                    }
                                    ua_frame[machine_state++] = ua_frame_receptor[0];
                                }
                                //something else received, so it goes back to start
                                else
                                {
                                    machine_state = 0;
                                    memset(ua_frame, 0, sizeof(ua_frame));
                                    break;
                                }
                            }
                            //checking A value
                            else if (machine_state == 1)
                            {
                                //A received, save and move on
                                if (ua_frame_receptor[0] == A)
                                {
                                    ua_frame[machine_state++] = ua_frame_receptor[0];
                                }
                                //FLAG received, so it stays waiting for an A
                                else if (ua_frame_receptor[0] == FLAG)
                                {
                                    ua_frame[0] = FLAG;
                                    machine_state = 1;
                                    break;
                                }
                                //something else received, so it goes back to start
                                else
                                {
                                    machine_state = 0;
                                    memset(ua_frame, 0, sizeof(ua_frame));
                                    break;
                                }
                            }
                            //checking C value
                            else if (machine_state == 2)
                            {
                                //C received, save and move on
                                if (ua_frame_receptor[0] == CUA)
                                {
                                    ua_frame[machine_state++] = ua_frame_receptor[0];
                                }
                                //FLAG received, so go back to waiting for an A
                                else if (ua_frame_receptor[0] == FLAG)
                                {
                                    ua_frame[0] = FLAG;
                                    machine_state = 1;
                                    break;
                                }
                                //something else received, so it goes back to start
                                else
                                {
                                    machine_state = 0;
                                    memset(ua_frame, 0, sizeof(ua_frame));
                                    break;
                                }
                            }
                            //checking BCC value
                            else
                            {
                                //BCC rceived, save and move on
                                if (ua_frame_receptor[0] == (BCCUA))
                                {
                                    ua_frame[machine_state++] = ua_frame_receptor[0];
                                }
                                //FLAG received, so go back to waiting for an A
                                else if (ua_frame_receptor[0] == FLAG)
                                {
                                    ua_frame[0] = FLAG;
                                    machine_state = 1;
                                    break;
                                }
                                //something else received, so it goes back to start
                                else
                                {
                                    machine_state = 0;
                                    memset(ua_frame, 0, sizeof(ua_frame));
                                    break;
                                }
                            }
                        }
                        else
                        {
                            machine_state++;
                        }
                    }
                }
            }

            if (ua_frame_received)
            {
                connection = 1;
                printf("Issuer perspective: Connection has been established\n");
                alarm(0);
                return PORT;
            }
            else
            {
                if (poll_res != 0)
                {
                    fprintf(stderr, "Frame had errors.\n");
                }
            }
            tcflush(PORT, TCIOFLUSH);
            cur_transmission++;
        }
    }
    else if (agent == 2)
    {
        int PORT = open(port, O_RDWR | O_NOCTTY);
        if (PORT < 0)
        {
            perror(port);
            exit(-1);
        }

        if (tcgetattr(PORT, &oldtio) == -1)
        {
            perror("tcgetattr");
            exit(-1);
        }

        pfds[0].fd = PORT;
        pfds[0].events = POLLIN;

        bzero(&newtio, sizeof(newtio));
        newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR;
        newtio.c_oflag = 0;

        newtio.c_lflag = 0;

        newtio.c_cc[VTIME] = 0;
        newtio.c_cc[VMIN] = 0;

        tcflush(PORT, TCIOFLUSH);

        if (tcsetattr(PORT, TCSANOW, &newtio) == -1)
        {
            perror("tcsetattr");
            exit(-1);
        }

        int machine_state = 0, set_frame_received = 0;
        char set_frame_receptor[1], set_frame[5];

        (void)signal(SIGALRM, pickup);
        //ESTABLISHING CONNECTION
        while (connection != 1)
        {

            flag = 0;
            alarm(UA_FRAME.timeout);

            //STATE MACHINE - READING SET_FRAME
            int set_frame_bytes, poll_res;
            while (machine_state != 5)
            {

                if (flag)
                {
                    fprintf(stderr, "Alarm went off\n");
                    break;
                }
                //poll blocks execution for 1000 seconds unless:
                // - a file descriptor becomes ready
                // - the call is interrupted by a signal handler
                // - the timeout expires
                poll_res = poll(pfds, 1, UA_FRAME.timeout * 1000);
                if (poll_res < 0)
                {
                    perror("poll() failed\n");
                    break;
                }
                else if (poll_res == 0)
                {
                    fprintf(stderr, "Port had no data to be read.\n");
                    break;
                }
                else
                {
                    if (pfds[0].revents && POLLIN)
                    {
                        set_frame_bytes = read(PORT, set_frame_receptor, 1);
                        if (set_frame_bytes > 0)
                        {
                            //checking flag values
                            if (machine_state == 0 || machine_state == 4)
                            {
                                //FLAG received,save and move on
                                if (set_frame_receptor[0] == FLAG)
                                {
                                    if (machine_state == 4)
                                    {
                                        set_frame_received = 1;
                                    }
                                    set_frame[machine_state++] = set_frame_receptor[0];
                                }
                                //something else received, so it goes back to start
                                else
                                {
                                    machine_state = 0;
                                    memset(set_frame, 0, sizeof(set_frame));
                                    break;
                                }
                            }
                            //checking A value
                            else if (machine_state == 1)
                            {
                                //A received, save and move on
                                if (set_frame_receptor[0] == A)
                                {
                                    set_frame[machine_state++] = set_frame_receptor[0];
                                }
                                //FLAG received, so it stays waiting for an A
                                else if (set_frame_receptor[0] == FLAG)
                                {
                                    set_frame[0] = FLAG;
                                    machine_state = 1;
                                    break;
                                }
                                //something else received, so it goes back to start
                                else
                                {
                                    machine_state = 0;
                                    memset(set_frame, 0, sizeof(set_frame));
                                    break;
                                }
                            }
                            //checking C value
                            else if (machine_state == 2)
                            {
                                //C received, save and move on
                                if (set_frame_receptor[0] == CSET)
                                {
                                    set_frame[machine_state++] = set_frame_receptor[0];
                                }
                                //FLAG received, so go back to waiting for an A
                                else if (set_frame_receptor[0] == FLAG)
                                {
                                    set_frame[0] = FLAG;
                                    machine_state = 1;
                                    break;
                                }
                                //something else received, so it goes back to start
                                else
                                {
                                    machine_state = 0;
                                    memset(set_frame, 0, sizeof(set_frame));
                                    break;
                                }
                            }
                            //checking BCC value
                            else
                            {
                                //BCC rceived, save and move on
                                if (set_frame_receptor[0] == (BCCSET))
                                {
                                    set_frame[machine_state++] = set_frame_receptor[0];
                                }
                                //FLAG received, so go back to waiting for an A
                                else if (set_frame_receptor[0] == FLAG)
                                {
                                    set_frame[0] = FLAG;
                                    machine_state = 1;
                                    break;
                                }
                                //something else received, so it goes back to start
                                else
                                {
                                    machine_state = 0;
                                    memset(set_frame, 0, sizeof(set_frame));
                                    break;
                                }
                            }
                        }
                        else
                        {
                            machine_state++;
                        }
                    }
                }
            }

            //SET FRAME RECEIVED
            if (set_frame_received)
            {
                connection = 1;
                //SEND UA FRAME
                int bytes = write(PORT, UA_FRAME.frame, 5);
                //making sure frame is sent
                while (bytes == 0)
                {
                    bytes = write(PORT, UA_FRAME.frame, 5);
                }
                fprintf(stderr, "Receptor perspective: Connection has been established\n");
                alarm(0);
                return PORT;
            }
            else
            {
                if (poll_res != 0)
                {
                    fprintf(stderr, "Frame had errors.\n");
                }
            }
            tcflush(PORT, TCIOFLUSH);
            cur_transmission++;
        }
    }

    perror("Invalid agent.\n");
    return -1;
}

int llwrite(int fd, char *bytes, int length)
{
    char binary_strings[length][8];
    DATA_FRAME.frame[0] = FLAG;
    DATA_FRAME.frame[1] = A;
    DATA_FRAME.frame[2] = CSET;
    DATA_FRAME.frame[3] = BCCSET;
    for (int j = 4, k = 0; j < (length << 1) + 4 && k < length; j += 2, k++)
    {
        //STUFFING
        if (bytes[k] == FLAG)
        {
            DATA_FRAME.frame[j] = LEAK;
            DATA_FRAME.frame[j + 1] = FLAGXOR;
        }
        else if (bytes[k] == LEAK)
        {
            DATA_FRAME.frame[j] = LEAK;
            DATA_FRAME.frame[j + 1] = LEAKXOR;
        }
        else
        {
            DATA_FRAME.frame[j] = bytes[k];
        }
        strcpy(binary_strings[k], decimal_to_binary(bytes[k]));
    }
    //generating data BCC
    char BCCI[8];
    for (int j = 0; j < 8; j++)
    {
        int ones = 0;
        for (int k = 0; k < length; k++)
        {
            if (binary_strings[j][k] == '1')
            {
                ones++;
            }
        }
        if (ones % 2 == 0)
        {
            BCCI[j] = '0';
        }
        else
        {
            BCCI[j] = '1';
        }
    }
    DATA_FRAME.frame[(length << 1) + 4 + 1] = (char)strtol(BCCI, NULL, 2);
    //DATA BCC STUFFING
    if (DATA_FRAME.frame[(length << 1) + 4 + 1] == FLAG)
    {
        DATA_FRAME.frame[(length << 1) + 4 + 1] = LEAK;
        DATA_FRAME.frame[(length << 1) + 4 + 2] = FLAGXOR;
    }
    else if (DATA_FRAME.frame[(length << 1) + 4 + 1] == LEAK)
    {
        DATA_FRAME.frame[(length << 1) + 4 + 1] = LEAK;
        DATA_FRAME.frame[(length << 1) + 4 + 2] = LEAKXOR;
    }
    else
    {
        DATA_FRAME.frame[(length << 1) + 4 + 2] = 0;
    }

    DATA_FRAME.frame[(length << 1) + 4 + 3] = FLAG;

    int write_bytes, read_bytes;

    cur_transmission = 0;
    (void)signal(SIGALRM, pickup);
    while (cur_transmission < DATA_FRAME.numTransmissions)
    {

        alarm(DATA_FRAME.timeout + 5);
        //send I frame
        write_bytes = write(fd, DATA_FRAME.frame, (length << 1) + 4 + 4);
        char response[5];
        //receive RR or REJ
        int poll_res = poll(pfds, 1, 5000);
        if (poll_res < 1)
        {
            printf("yep?\n");
            return -1;
        }
        else if (pfds[0].revents && POLLIN)
        {
            read_bytes = read(fd, response, 5);
        }
        tcflush(fd, TCIOFLUSH);

        //validation
        if (read_bytes == 5 && response[0] == FLAG && response[1] == A && response[4] == FLAG)
        {
            if (response[2] == CRR && response[3] == (BCCRR))
            {
                break;
            }
            else if (response[2] == CREJ && response[3] == (BCCREJ))
            {
                printf("REJ: Frame was rejected\n");
                cur_transmission++;
            }
            else
            {
                printf("RR/REJ frame had errors\n");
                cur_transmission++;
            }
        }
        else
        {
            printf("RR/REJ frame had errors\n");
            cur_transmission++;
        }
        printf("cur_transmission: %d\n", cur_transmission);
    }
    alarm(0);
    return write_bytes;
}

int llread(int fd, char *buffer)
{
    //((length + 1) << 1) + 5
    int len = 519;
    int read_bytes;
    char i_frame[len + 1];

    int y = 0;
    while (1)
    {
        //read I frame
        int poll_res = poll(pfds, 1, DATA_FRAME.timeout * 1000);
        if (poll_res < 0)
        {
            return -2;
        }
        else if (poll_res == 0)
        {
            printf("Port had no data to be read\n");
            return -2;
        }
        else if (pfds[0].revents && POLLIN)
        {
            read_bytes = read(fd, i_frame, len + 1);
        }

        if (read_bytes < 1)
        {
            if (++y > 2)
            {
                break;
            }
            continue;
        }

        tcflush(fd, TCIOFLUSH);

        len = read_bytes - 1;
        //verify data frame
        if (i_frame[0] != FLAG || i_frame[1] != A || i_frame[2] != CSET || i_frame[3] != (BCCSET) || i_frame[len] != FLAG)
        {
            printf("Frame had errors\n");
            write(fd, REJ, 5);
            return -1;
        }
        char binary_strings[(read_bytes - 7) >> 1][8];
        int j, k;
        for (j = 4, k = 0; j < read_bytes - 3 && k < (read_bytes - 8) >> 1; j += 2, k++)
        {
            //DESTUFFING
            if (i_frame[j] == LEAK && i_frame[j + 1] == FLAGXOR)
            {
                buffer[k] = FLAG;
            }
            else if (i_frame[j] == LEAK && i_frame[j + 1] == LEAKXOR)
            {
                buffer[k] = LEAK;
            }
            else
            {
                buffer[k] = i_frame[j];
            }
            strcpy(binary_strings[k], decimal_to_binary(buffer[k]));
        }

        //DATA BCC DESTUFFING
        if (i_frame[len - 2] == LEAK && i_frame[len - 1] == FLAGXOR)
        {
            buffer[k] = FLAG;
        }
        else if (i_frame[len - 2] == LEAK && i_frame[len - 1] == LEAKXOR)
        {
            buffer[k] = LEAK;
        }
        else
        {
            buffer[k] = i_frame[len - 2];
        }

        char BCCI[8];
        strcpy(BCCI, decimal_to_binary(buffer[k]));
        //validating DATA BCC
        for (int j = 0; j < 8; j++)
        {
            int ones = 0;
            for (int k = 0; k < (read_bytes - 7) >> 1; k++)
            {
                if (binary_strings[j][k] == '1')
                {
                    ones++;
                }
            }
            if ((ones % 2 == 0 && BCCI[j] == '1') || (ones % 2 != 0 && BCCI[j] == '0'))
            {
                printf("Invalid DATA BCC\n");
                write(fd, REJ, 5);
                return -1;
            }
        }
        write(fd, RR, 5);
        return (read_bytes - 6) >> 1;
    }
    return -1;
}

int llclose(int fd, int agent)
{
    int read_bytes;

    (void)signal(SIGALRM, pickup);
    if (agent == 1)
    {
        alarm(DISC_FRAME.timeout + 5);
        while (1)
        {
            write(fd, DISC_FRAME.frame, 5);

            char response[5];

            int poll_res = poll(pfds, 1, DISC_FRAME.timeout * 1000);
            if (poll_res < 0)
            {
                printf("Poll() failed or alarm went off\n");
                continue;
            }
            else if (poll_res == 0)
            {
                printf("Port had no data to be read\n");
            }
            else if (pfds[0].revents && POLLIN)
            {
                read_bytes = read(fd, response, 5);
            }

            if (read_bytes == 5 && response[0] == FLAG && response[1] == A && response[2] == CDISC && response[3] == (BCCDISC) && response[4] == FLAG)
            {
                write(fd, UA_FRAME.frame, 5);
                close(fd);
                alarm(0);
                return 1;
            }
        }
        printf("Issuer perspective: Connection unsuccessfully closed\n");
        alarm(0);
        return -1;
    }
    else if (agent == 2)
    {
        int max_wait = 0;

        while (1)
        {
            if (max_wait > 10)
            {
                return -1;
            }

            char response[5];

            int poll_res = poll(pfds, 1, DISC_FRAME.timeout * 1000);
            if (poll_res < 0)
            {
                printf("Poll() failed or alarm went off\n");
            }
            else if (poll_res == 0)
            {
                printf("Port had no data to be read\n");
            }
            else if (pfds[0].revents && POLLIN)
            {
                read_bytes = read(fd, response, 5);
            }
            tcflush(fd, TCIOFLUSH);

            if (read_bytes == 5 && response[0] == FLAG && response[1] == A && response[2] == CDISC && response[3] == (BCCDISC) && response[4] == FLAG)
            {
                write(fd, DISC_FRAME.frame, 5);

                poll_res = poll(pfds, 1, DISC_FRAME.timeout * 1000);
                if (poll_res < 1)
                {
                    return -1;
                }
                else if (pfds[0].revents && POLLIN)
                {
                    read_bytes = read(fd, response, 5);
                }
                tcflush(fd, TCIOFLUSH);

                if (read_bytes == 5 && response[0] == FLAG && response[1] == A && response[2] == CUA && response[3] == (BCCUA) && response[4] == FLAG)
                {
                    close(fd);
                    alarm(0);
                    return 1;
                }
            }
            max_wait++;
        }
        printf("Receptor perspective: Connection unsuccessfully closed\n");
        alarm(0);
        return -1;
    }
    perror("Invalid agent.\n");
    return -1;
}
