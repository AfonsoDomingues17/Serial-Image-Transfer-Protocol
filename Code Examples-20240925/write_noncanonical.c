// Write to serial port in non-canonical mode
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
#include <signal.h>

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

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}

typedef enum {
    SEND_0,
    SEND_1
} frame_t;

void stablishConnection(int fd, unsigned char buf[], unsigned size) {
    alarmCount = 0;

    while (alarmCount < 5) {
        if (alarmEnabled == FALSE) {
            alarm(3); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
            
            int bytes = write(fd, buf, size);
            printf("%d bytes written\n", bytes);
            sleep(1); // TODO: sleep less time. Maybe some ticks.
            
            unsigned char ua_frame[BUF_SIZE] = {0};
            int bytes_read = read(fd,ua_frame, BUF_SIZE);
            for (unsigned i = 0; i < bytes_read; i++) printf("Byte[%d]:%x\n",i,ua_frame[i]);

            //if (bytes_read < 5) continue;
            
            if(ua_frame[3] == (ADDRESS_RCVR ^ CONTROL_UA) ){
                if(ua_frame[0] == FLAG &&
                ua_frame[1] == ADDRESS_RCVR &&
                ua_frame[2] == CONTROL_UA &&
                ua_frame[4] == FLAG){
                    alarm(0);
                    alarmEnabled = FALSE;
                    printf("Connection established sucssfuly\n");
                    return;
                } else {
                    printf("Invalid UA frame\n");
                }
            } else {
                printf("Invalid UA frame\n");
            }
        }
    }
    printf("TIMEOUT: Could not establish connection\n");
}
void sendFrame(int fd, unsigned char buf[], unsigned size, unsigned char frame_n) {
    alarmCount = 0;
    
    // BYTE STUFFING:
    unsigned char stuffed_buf[BUF_SIZE * 2] = {0};
    unsigned i = 0, j = 0;
    for (; i < 4; i++) stuffed_buf[j++] = buf[i];
    for (; i < size - 1; i++) {
        if (buf[i] == FLAG || buf[i] == ESC) {
            stuffed_buf[j++] = ESC;
            stuffed_buf[j++] = buf[i] ^ 0x20;
        }
        else stuffed_buf[j++] = buf[i];
    }
    
    printf("Stuff done. Now, sending frame...\n");

    unsigned char rr_0[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_RR0,ADDRESS_RCVR ^ CONTROL_RR0, FLAG};
    unsigned char rr_1[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_RR1,ADDRESS_RCVR ^ CONTROL_RR1, FLAG};
    unsigned char rej_0[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_REJ0,ADDRESS_RCVR ^ CONTROL_REJ0, FLAG};
    unsigned char rej_1[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_REJ1,ADDRESS_RCVR ^ CONTROL_REJ1, FLAG};

    while (alarmCount < 5) {
        if (alarmEnabled == FALSE) {
            alarm(3); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
            
            int bytes = write(fd, stuffed_buf, j);
            printf("%d bytes written\n", bytes);
            
            unsigned char asw_frame[BUF_SIZE] = {0};
            int bytes_read = read(fd, asw_frame, ASW_BUF_SIZE);
            for (unsigned i = 0; i < bytes_read; i++) printf("Byte[%d]:%x\n",i,asw_frame[i]);

            if (frame_n == 0) {
                if (!memcmp(asw_frame, rr_0, 5)) {
                    alarm(0);
                    alarmEnabled = FALSE;
                    return; // TODO: Maybe return 0
                }
                if (!memcmp(asw_frame, rej_0, 5)) {
                    alarm(0);
                    alarmEnabled = FALSE;
                    continue; // The repeated frame will be sent right away.q
                }
            } else {
                if (!memcmp(asw_frame, rr_1, 5)) {
                    alarm(0);
                    alarmEnabled = FALSE;
                    return; // TODO: Maybe return 0
                }
                if (!memcmp(asw_frame, rej_1, 5)) {
                    alarm(0);
                    alarmEnabled = FALSE;
                    continue; // The repeated frame will be sent right away.q
                }
            }
        }
    }
    printf("TIMEOUT: Could not send the frame\n");
}

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

    // Open serial port device for reading and writing, and not as controlling tty
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

    (void)signal(SIGALRM, alarmHandler);

    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    //buf[5] = '\n';

    unsigned char set_frame[BUF_SIZE] = {FLAG,ADDRESS_SNDR,CONTROL_SET,ADDRESS_SNDR ^ CONTROL_SET,FLAG};
    stablishConnection(fd, set_frame,5);
    
    unsigned char frame[BUF_SIZE] = {FLAG,ADDRESS_SNDR,0x00,ADDRESS_SNDR ^ 0x00,0x02,FLAG,0x02,0x02,0x02,0x02,0x02,0x02, 0x7c, FLAG};
    sendFrame(fd, frame,14);

    // Restore the old port setting
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}
