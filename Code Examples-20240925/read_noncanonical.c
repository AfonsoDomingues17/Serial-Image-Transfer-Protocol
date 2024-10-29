// Read from serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "state_machine.h"

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 1024
#define ASW_BUF_SIZE 5

#define FLAG            0x7E
#define ADDRESS_SNDR    0x03
#define ADDRESS_RCVR    0x01

#define CONTROL_SET     0x03
#define CONTROL_UA      0x07
#define CONTROL_RR0     0xAA
#define CONTROL_RR1     0xAB
#define CONTROL_REJ0    0x54
#define CONTROL_REJ1    0x55
#define CONTROL_DISC    0x0B
#define CONTROL_B0      0X00
#define CONTROL_B1      0x80

volatile int STOP = FALSE;
int alarmEnabled = FALSE;

int alarmCount;

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];


    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Loop for input
    unsigned char ua_frame[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_UA,ADDRESS_RCVR ^ CONTROL_UA,FLAG};
    unsigned char rr_0[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_RR0,ADDRESS_RCVR ^ CONTROL_RR0, FLAG};
    unsigned char rr_1[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_RR1,ADDRESS_RCVR ^ CONTROL_RR1, FLAG};
    unsigned char rej_0[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_REJ0,ADDRESS_RCVR ^ CONTROL_REJ0, FLAG};
    unsigned char rej_1[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_REJ1,ADDRESS_RCVR ^ CONTROL_REJ1, FLAG};

    

    stablishConnectionReceiver(fd);
    
    printf("Connection stablished sussfully!\n");
    int bytes = write(fd, ua_frame, 5);
    if (bytes != 5) printf("Failed to send 5 bytes (UA frame).\n");

    unsigned char expected_frame = 0;
    
    unsigned char frame[BUF_SIZE] = {0};

    int size = receiveI_frame(fd, frame,BUF_SIZE);

    if (size == -2) {
        printf("ERROR: Allocated buffer is too small.\n");
        exit(1);

    } else if (size == -1) { // BCC2 WRONG!
        if (frame[2] == CONTROL_B0) {
            bytes = write(fd,rej_0,5);
            expected_frame = 0;
        } else {
            bytes = write(fd,rej_1,5);
            expected_frame = 1;
        }
    }else if (frame[2] == CONTROL_B0 && expected_frame == 0) {
        bytes = write(fd,rr_1,5);
        expected_frame = 1;
    } else if (frame[2] == CONTROL_B1 && expected_frame == 1) {
        bytes = write(fd,rr_0,5);
        expected_frame = 0;
    } else if (frame[2] == CONTROL_B1) { //duplicated I1
        bytes = write(fd,rr_0,5);
        expected_frame = 0;
    } else if (frame[2] == CONTROL_B0) { //duplicated I0
        bytes = write(fd,rr_1,5);
        expected_frame = 1;
    } 

        
       


    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
