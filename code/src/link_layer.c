// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"
#include "state_machine.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source


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

unsigned char rr_0[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_RR0,ADDRESS_RCVR ^ CONTROL_RR0, FLAG};
unsigned char rr_1[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_RR1,ADDRESS_RCVR ^ CONTROL_RR1, FLAG};
unsigned char rej_0[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_REJ0,ADDRESS_RCVR ^ CONTROL_REJ0, FLAG};
unsigned char rej_1[ASW_BUF_SIZE] = {FLAG,ADDRESS_RCVR,CONTROL_REJ1,ADDRESS_RCVR ^ CONTROL_REJ1, FLAG};

volatile int STOP = FALSE;

int alarmEnabled = FALSE;

int alarmCount;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }

    // TODO

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    alarmCount = 0;
    
    // BYTE STUFFING:
    unsigned char stuffed_buf[BUF_SIZE * 2] = {0};
    unsigned i = 0, j = 0;
    for (; i < 4; i++) stuffed_buf[j++] = buf[i];
    for (; i < bufSize - 1; i++) {
        if (buf[i] == FLAG || buf[i] == ESC) {
            stuffed_buf[j++] = ESC;
            stuffed_buf[j++] = buf[i] ^ 0x20;
        }
        else stuffed_buf[j++] = buf[i];
    }
    
    printf("Stuff done. Now, sending frame...\n");



    while (alarmCount < 5) {
        if (alarmEnabled == FALSE) {
            alarm(3); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
            
            int bytes = writeBytesSerialPort(stuffed_buf,j);
            printf("%d bytes written\n", bytes);
            
            unsigned char asw_frame[BUF_SIZE] = {0};
            //int bytes_read = read(fd, asw_frame, ASW_BUF_SIZE);
            int bytes_read;
            for (unsigned i = 0; i < ASW_BUF_SIZE; i++) bytes_read = readByteSerialPort(asw_frame[i]);

            if (buf[2] == CONTROL_B0) {
                if(asw_frame[3] == (ADDRESS_RCVR ^ CONTROL_RR1) ){
                    if(asw_frame[0] == FLAG &&
                    asw_frame[1] == ADDRESS_RCVR &&
                    asw_frame[2] == CONTROL_RR1 &&
                    asw_frame[4] == FLAG){
                        alarm(0);
                        alarmEnabled = FALSE;
                        printf("Packet Well transmited\n");
                        return;
                    }
                }
                if(asw_frame[3] == (ADDRESS_RCVR ^ CONTROL_REJ0) ){
                    if(asw_frame[0] == FLAG &&
                    asw_frame[1] == ADDRESS_RCVR &&
                    asw_frame[2] == CONTROL_REJ0 &&
                    asw_frame[4] == FLAG){
                        alarm(0);
                        alarmEnabled = FALSE;
                        printf("Packet rejected - retrasmiting\n");
                        continue;
                    }
                }
                
            } else {
                if(asw_frame[3] == (ADDRESS_RCVR ^ CONTROL_RR0) ){
                    if(asw_frame[0] == FLAG &&
                    asw_frame[1] == ADDRESS_RCVR &&
                    asw_frame[2] == CONTROL_RR0 &&
                    asw_frame[4] == FLAG){
                        alarm(0);
                        alarmEnabled = FALSE;
                        printf("Packet Well transmited\n");
                        return;
                    }
                }
                if(asw_frame[3] == (ADDRESS_RCVR ^ CONTROL_REJ1) ){
                    if(asw_frame[0] == FLAG &&
                    asw_frame[1] == ADDRESS_RCVR &&
                    asw_frame[2] == CONTROL_REJ1 &&
                    asw_frame[4] == FLAG){
                        alarm(0);
                        alarmEnabled = FALSE;
                        printf("Packet rejected - retrasmiting\n");
                        continue;
                    }
                }
            }
        }
    }
    printf("TIMEOUT: Could not send the frame\n");

    

    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
     

    
    int bytes;

    //bool finnished = false;
    unsigned char expected_frame = 0;
    
    //while (!finnished) { // Read a frame each itteration. Ler a imagem toda.
    unsigned char frame[BUF_SIZE] = {0};

    int size = receiveI_frame(frame,BUF_SIZE);

    if (size == -2) {
        printf("ERROR: Allocated buffer is too small.\n");
        exit(1);
    } else if (frame[2] == CONTROL_B0 && expected_frame == 0) {
        bytes = writeBytesSerialPort(rr_1,5);
        expected_frame = 1;
    } else if (frame[2] == CONTROL_B1 && expected_frame == 1) {
        bytes = writeBytesSerialPort(rr_0,5);
        expected_frame = 0;
    } else if (frame[2] == CONTROL_B1) { //duplicated I1
        bytes = writeBytesSerialPort(rr_0,5);
        expected_frame = 0;
    } else if (frame[2] == CONTROL_B0) { //duplicated I0
        bytes = writeBytesSerialPort(rr_1,5);
        expected_frame = 1;
    } else if (size == -1) { // BCC2 WRONG!
        if (frame[2] == CONTROL_B0) {
            bytes = writeBytesSerialPort(rej_0,5);
            expected_frame = 0;
        } else {
            bytes = writeBytesSerialPort(rej_1,5);
            expected_frame = 1;
        }
    }


    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}
