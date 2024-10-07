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

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 1024

#define FLAG 0x7E
#define ADDRESS_SET 0x03
#define ADDRESS_UA 0x01
#define CONTROL_SET 0x03
#define CONTROL_UA 0x07

typedef enum setMsgState {
    START_S,
    FLAG_S,
    ADDRESS_S,
    CONTROL_S,
    BCC_S,
    STOP_S
} setMsgState;


volatile int STOP = FALSE;

int alarmEnabled = FALSE;

int alarmCount;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

void stablishConnection() {
    unsigned char set_frame[BUF_SIZE] = {FLAG,ADDRESS_SET,CONTROL_SET,0x00,FLAG};
    set_frame[3] = ADDRESS_SET ^ CONTROL_SET;
    setMsgState state = START_S;


    alarmCount = 0;
    while (alarmCount < 5) {
        if (alarmEnabled == FALSE) {
            alarm(3); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;

            int bytes = writeBytesSerialPort(set_frame, 5);
            printf("%d bytes written\n", bytes);

            sleep(1);

            unsigned char ua_frame[BUF_SIZE] = {0};

            //if (bytes_read < 5) continue;
            while (state != STOP_S) {
                unsigned char byte_read = 0;
                int n_bytes_read = readByteSerialPort(&byte_read);
                if (n_bytes_read != 1) printf("Failed to read byte from serial port.\n");
                switch (state) {
                    case START_S:
                        if(byte_read == FLAG){
                            state = FLAG_S;
                            ua_frame[0] = byte_read;
                            // printf("First flag received\n");
                        }
                        break;
                    
                    case FLAG_S:
                        if(byte_read == ADDRESS_UA){
                            state = ADDRESS_S;
                            ua_frame[1] = byte_read;
                            // printf("Adress received\n");
                        }
                        else if(byte_read != FLAG){
                            state = START_S;
                            // printf("Back to start :(\n");
                        }
                        break;
                    
                    case ADDRESS_S:
                        if(byte_read == CONTROL_UA) {
                            state = CONTROL_S;
                            ua_frame[2] = byte_read;
                            // printf("Control received\n");
                        } else if (byte_read == FLAG) {
                            state = FLAG_S;
                            // printf("Flag received instead of control\n");
                        } else {
                            state = START_S;
                            // printf("Back to start from address\n");
                        }
                        break;
                    
                    case CONTROL_S:
                        if(byte_read == (ADDRESS_UA ^ CONTROL_UA)){
                            state = BCC_S;
                            ua_frame[3] = byte_read;
                            // printf("BCC received\n");
                        }
                        else if(byte_read == FLAG){
                            state = FLAG_S;
                            // printf("Flag received instead of BCC\n");
                        }
                        else {
                            state = START_S;
                            // printf("Back to start from control\n");
                        }
                        break;
                    
                    case BCC_S:
                        if (byte_read == FLAG) {
                            state = STOP_S;
                            ua_frame[4] = byte_read;
                            // printf("Last flag received proceded to stop\n");
                        }
                        else {
                            state = START_S;
                            // printf("All the way to the start from BCC\n");
                        }
                        break;
                    
                    default:
                        break;
                }
            }
            

        }
    }
}

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if(openSerialPort(serialPortName, BAUDRATE) == -1) printf("ERROR: Failed to open Serial Port.\n");

    (void)signal(SIGALRM, alarmHandler);

    stablishConnection();

    if (closeSerialPort() == -1) printf("ERROR: Failed to close Serial Port.\n");

    return 0;
}
