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

int count = 0, flag;
void pickup() // atende alarme
{
    flag = 1;
    count++;
}

//establishing connection frames
linkLayer SET_FRAME = {"/dev/ttyS10", 0, 1, 3, 3, {FLAG, A, CSET, BCCSET, FLAG}};
linkLayer UA_FRAME = {"/dev/ttyS11", 0, 1, 3, 3, {FLAG, A, CUA, BCCUA, FLAG}};

//information frames
char DATA_FRAME[7] = {FLAG, A, CSET, BCCSET, -1, BCCI, FLAG};

//verifying information frames
char RR[5] = {FLAG, A, CRR, BCCRR, FLAG};
char REJ[5] = {FLAG, A, CREJ, BCCREJ, FLAG};

struct termios oldtio, newtio;
struct pollfd pfds[1];
int connection;

int llopen(char *port, int agent)
{
    int PORT;
    if (agent == 1)
    {
        connection = 0;

        PORT = open(SET_FRAME.port, O_RDWR | O_NOCTTY);
        if (PORT < 0)
        {
            perror(SET_FRAME.port);
            exit(-1);
        }

        if (tcgetattr(PORT, &oldtio) == -1)
        { //save current port settings
            perror("tcgetattr");
            exit(-1);
        }

        pfds[0].fd = PORT;
        pfds[0].events = POLLIN;

        bzero(&newtio, sizeof(newtio));
        newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR;
        newtio.c_oflag = 0;

        //set input mode (non-canonical, no echo,...)
        newtio.c_lflag = 0;

        newtio.c_cc[VTIME] = 0; //inter-character timer unused
        newtio.c_cc[VMIN] = 0;  //blocking read until 5 chars received

        tcflush(PORT, TCIOFLUSH);

        if (tcsetattr(PORT, TCSANOW, &newtio) == -1)
        {
            perror("tcsetattr");
            exit(-1);
        }

        int machine_state = 0, ua_frame_received = 0;
        char ua_frame_receptor[1], ua_frame[5];

        (void)signal(SIGALRM, pickup);
        //ESTABLISHING CONNECTION
        while (count < 3)
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
            count++;
        }
    }
    else if (agent == 2)
    {
        int PORT = open(UA_FRAME.port, O_RDWR | O_NOCTTY);
        if (PORT < 0)
        {
            perror("Port could not be opened.");
            exit(-1);
        }

        if (tcgetattr(PORT, &oldtio) == -1)
        { //save current port settings
            perror("tcgetattr");
            exit(-1);
        }

        pfds[0].fd = PORT;
        pfds[0].events = POLLIN;

        bzero(&newtio, sizeof(newtio));
        newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR;
        newtio.c_oflag = 0;

        //set input mode (non-canonical, no echo,...)
        newtio.c_lflag = 0;

        newtio.c_cc[VTIME] = 0; //inter-character timer unused
        newtio.c_cc[VMIN] = 0;  //blocking read until 5 chars received

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
            count++;
        }
    }

    perror("Invalid agent.\n");
    return -1;
}

int llwrite(int fd, char bytes)
{
    DATA_FRAME[4] = bytes;
    int write_bytes;

    //send I frame
    write_bytes = write(fd, DATA_FRAME, 7);

    char response[5];
    //receive RR or REJ
    read(fd, response, 5);

    //frame was rejected
    if (response[2] == CREJ)
    {
        return -1;
    }

    return write_bytes;
}

char llread(int fd, char *buffer)
{
    int read_bytes;
    char i_frame[7];
    //read I frame
    int poll_res = poll(pfds, 1, 1000);
    if (poll_res < 1)
    {
        return 0;
    }
    else if (pfds[0].revents && POLLIN)
    {
        read_bytes = read(fd, i_frame, 7);
    }
    tcflush(fd, TCIOFLUSH);
    buffer[0] = i_frame[4];

    //verify data frame

    int valid = 1;
    //check wtv parity stuff..

    //send response back
    //i frame had error or duplicate wtv
    if (valid != 1)
    {
        write(fd, REJ, 5);
        return -1;
    }

    //valid i frame
    write(fd, RR, 5);
    return read_bytes;
}