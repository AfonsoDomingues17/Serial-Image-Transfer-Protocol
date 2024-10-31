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

unsigned char rr_0[ASW_BUF_SIZE] =               {FLAG, ADDRESS_SNDR, CONTROL_RR0 , ADDRESS_SNDR ^ CONTROL_RR0,    FLAG};
unsigned char rr_1[ASW_BUF_SIZE] =               {FLAG, ADDRESS_SNDR, CONTROL_RR1 , ADDRESS_SNDR ^ CONTROL_RR1,    FLAG};
unsigned char rej_0[ASW_BUF_SIZE] =              {FLAG, ADDRESS_SNDR, CONTROL_REJ0, ADDRESS_SNDR ^ CONTROL_REJ0,   FLAG};
unsigned char rej_1[ASW_BUF_SIZE] =              {FLAG, ADDRESS_SNDR, CONTROL_REJ1, ADDRESS_SNDR ^ CONTROL_REJ1,   FLAG};
unsigned char disc_frame_sndr[ASW_BUF_SIZE] =    {FLAG, ADDRESS_SNDR, CONTROL_DISC, ADDRESS_SNDR ^ CONTROL_DISC,   FLAG};
unsigned char disc_frame_rcvr[ASW_BUF_SIZE] =    {FLAG, ADDRESS_RCVR, CONTROL_DISC, ADDRESS_RCVR ^ CONTROL_DISC,   FLAG};
unsigned char ua_frame[ASW_BUF_SIZE] =           {FLAG, ADDRESS_SNDR, CONTROL_UA  , ADDRESS_SNDR ^ CONTROL_UA,     FLAG};
unsigned char ua_frame_disc[ASW_BUF_SIZE] =      {FLAG, ADDRESS_RCVR, CONTROL_UA  , ADDRESS_RCVR ^ CONTROL_UA,     FLAG};
unsigned char set_frame[ASW_BUF_SIZE] =          {FLAG, ADDRESS_SNDR, CONTROL_SET , ADDRESS_SNDR ^ CONTROL_SET,    FLAG};

volatile int STOP = FALSE;

int alarmEnabled = FALSE;

int alarmCount;
LinkLayerRole role;
int timeout = 0;
int nRetransmissions = 0;
int nTries = 0;

int frame_n = 0;

typedef struct {
    unsigned int n_frames;
    unsigned int n_timeouts;
    int n_retransmissions;
    
} Statistics;

Statistics statistics = {0};

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    statistics.n_timeouts++;
    printf("Alarm #%d\n", alarmCount); 
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {return -1;}

    role = connectionParameters.role;
    timeout = connectionParameters.timeout;
    nRetransmissions = connectionParameters.nRetransmissions;

    frameState_t state;
    switch (connectionParameters.role) {
        case LlTx:
            (void)signal(SIGALRM, alarmHandler);
            
            alarmCount = 0;
            
            statistics.n_retransmissions--;
            while (alarmCount < nRetransmissions) {
                if (alarmEnabled == FALSE) {
                    alarm(timeout); // Set alarm to be triggered in 3s
                    alarmEnabled = TRUE;
                    
                    int bytes = writeBytesSerialPort(set_frame, ASW_BUF_SIZE);
                    printf("INFO: %d bytes written\n", bytes);
                    statistics.n_frames++;
                    statistics.n_retransmissions++;
                    
                    state = START_STATE;
                    while (alarmEnabled != FALSE && state != STOP_STATE) {
                        unsigned char byte_read = 0;
                        int n_bytes_read = readByteSerialPort(&byte_read);
                        if(n_bytes_read == 0) continue;
                    
                        switch (state) {
                            case START_STATE:
                                if(byte_read == FLAG){
                                    state = FLAG_STATE;
                                    //printf("First flag received\n");
                                }
                                break;
                            
                            case FLAG_STATE:
                                if(byte_read == ADDRESS_SNDR){
                                    state = ADDRESS_STATE;
                                    //printf("Adress received\n");
                                }
                                else if(byte_read != FLAG){
                                    state = START_STATE;
                                    //printf("Back to start :(\n");
                                }
                                break;
                            
                            case ADDRESS_STATE:
                                if(byte_read == CONTROL_UA) {
                                    state = CONTROL_STATE;
                                    //printf("Control received\n");
                                } else if (byte_read == FLAG) {
                                    state = FLAG_STATE;
                                    //printf("Flag received instead of control\n");
                                } else {
                                    state = START_STATE;
                                    //printf("Back to start from address\n");
                                }
                                break;
                            
                            case CONTROL_STATE:
                                if(byte_read == (ADDRESS_SNDR ^ CONTROL_UA)){
                                    state = BCC1_STATE;
                                    //printf("BCC received\n");
                                }
                                else if(byte_read == FLAG){
                                    state = FLAG_STATE;
                                    //printf("Flag received instead of BCC\n");
                                }
                                else {
                                    state = START_STATE;
                                    //printf("Back to start from control\n");
                                }
                                break;
                            
                            case BCC1_STATE:
                                if (byte_read == FLAG) {
                                    state = STOP_STATE;
                                    alarm(0);
                                    alarmEnabled = FALSE;
                                    printf("INFO: Connection established successfuly\n");
                                    frame_n = 0;
                                    return 1;
                                }
                                else {
                                    state = START_STATE;
                                    //printf("All the way to the start from BCC\n");
                                }
                                break;
                            
                            default:
                                break;
                        }
                    }
                    // printf("Should be here when alarm rings\n"); // DEBUG
                }
            }
            printf("TIMEOUT: Could not establish connection\n");
            break;
        
        case LlRx:
            state = START_STATE;
            while (state != STOP_STATE) {
                unsigned char byte_read = 0;
                int n_bytes_read = readByteSerialPort(&byte_read);
                if(n_bytes_read == 0) continue;
                switch (state) {
                    case START_STATE:
                        if(byte_read == FLAG){
                            state = FLAG_STATE;
                            //printf("First flag received\n");
                        }
                        break;
                    
                    case FLAG_STATE:
                        if(byte_read == ADDRESS_SNDR){
                            state = ADDRESS_STATE;
                            //printf("Adress received\n");
                        }
                        else if(byte_read != FLAG){
                            state = START_STATE;
                            //printf("Back to start :(\n");
                        }
                        break;
                    
                    case ADDRESS_STATE:
                        if(byte_read == CONTROL_SET) {
                            state = CONTROL_STATE;
                            //printf("Control received\n");
                        } else if (byte_read == FLAG) {
                            state = FLAG_STATE;
                            //printf("Flag received instead of control\n");
                        } else {
                            state = START_STATE;
                            //printf("Back to start from address\n");
                        }
                        break;
                    
                    case CONTROL_STATE:
                        if(byte_read == (ADDRESS_SNDR ^ CONTROL_SET)){
                            state = BCC1_STATE;
                            //printf("BCC received\n");
                        }
                        else if(byte_read == FLAG){
                            state = FLAG_STATE;
                            //printf("Flag received instead of BCC\n");
                        }
                        else {
                            state = START_STATE;
                            //printf("Back to start from control\n");
                        }
                        break;
                    
                    case BCC1_STATE:
                        if (byte_read == FLAG) {
                            state = STOP_STATE;
                            //printf("Last flag received proceded to stop\n");
                        }
                        else {
                            state = START_STATE;
                            //printf("All the way to the start from BCC\n");
                        }
                        break;
                    
                    default:
                        break;
                }
            }
            printf("INFO: Connection stablished successfully!\n");
            int bytes = writeBytesSerialPort(ua_frame,ASW_BUF_SIZE);
            if (bytes != 5) printf("ERROR: Failed to send 5 bytes (UA frame).\n");
            frame_n = 0;
            statistics.n_frames++;
            return 1;
            break;

        default:
            break;
    }

    return -1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
    //buf 0 S L1 L2
    //
    alarmCount = 0;
    bool is_rej = false;
    
    // BYTE STUFFING:
    unsigned char stuffed_buf[BUF_SIZE * 2] = {0};
    unsigned j = 0;
    stuffed_buf[j++] = FLAG;
    stuffed_buf[j++] = ADDRESS_SNDR;
    if(frame_n == 0){
        stuffed_buf[j++] = CONTROL_B0;
        stuffed_buf[j++] = CONTROL_B0 ^ ADDRESS_SNDR;
    }
    else{
        stuffed_buf[j++] = CONTROL_B1;
        stuffed_buf[j++] = CONTROL_B1 ^ ADDRESS_SNDR;
    }
    
    unsigned int bcc2 = buf[0];

    for(int i = 1; i < bufSize; i++) bcc2 ^= buf[i]; 

    //printf("BCC2: %X\n", bcc2);
    
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESC) {
            stuffed_buf[j++] = ESC;
            stuffed_buf[j++] = buf[i] ^ 0x20;
        }
        else stuffed_buf[j++] = buf[i];

    }
    stuffed_buf[j++] = bcc2;
    stuffed_buf[j++] = FLAG;

    statistics.n_retransmissions--;
    while (alarmCount < nRetransmissions) {
        if (alarmEnabled == FALSE) {
            alarm(timeout); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
            is_rej = false;
            
            int bytes = writeBytesSerialPort(stuffed_buf,j);
            printf("INFO: %d bytes written\n", bytes);
            statistics.n_frames++;
            statistics.n_retransmissions++;
            //nsigned char asw_frame[BUF_SIZE] = {0};
            //int bytes_read = read(fd, asw_frame, ASW_BUF_SIZE);
            //for (unsigned i = 0; i < ASW_BUF_SIZE; i++) readByteSerialPort(&asw_frame[i]);
                    
            frameState_t state = START_STATE;
            while (alarmEnabled != FALSE && state != STOP_STATE) {
                unsigned char byte_read = 0;
                int n_bytes_read = readByteSerialPort(&byte_read);
                if(n_bytes_read == 0) continue;
            
                switch (state) {
                    case START_STATE:
                        if(byte_read == FLAG){
                            state = FLAG_STATE;
                            //printf("First flag received\n");
                        }
                        break;
                    
                    case FLAG_STATE:
                        if(byte_read == ADDRESS_SNDR){
                            state = ADDRESS_STATE;
                            //printf("Adress received\n");
                        }
                        else if(byte_read != FLAG){
                            state = START_STATE;
                            //printf("Back to start :(\n");
                        }
                        break;
                    
                    case ADDRESS_STATE:
                        if(byte_read == CONTROL_REJ0 || byte_read == CONTROL_REJ1) {
                            state = CONTROL_STATE;
                            //printf("Control REJ received\n");
                        } else if(byte_read == CONTROL_RR0 || byte_read == CONTROL_RR1){
                            state = CONTROL_STATE;
                            //printf("Control RR received\n");
                        
                        } else if (byte_read == CONTROL_UA){
                            state = CONTROL_STATE;
                        }
                        else if (byte_read == FLAG) {
                            state = FLAG_STATE;
                            //printf("Flag received instead of control\n");
                        } else {
                            state = START_STATE;
                            //printf("Back to start from address\n");
                        }
                        break;

                    case CONTROL_STATE:

                        if(byte_read == (ADDRESS_SNDR ^ CONTROL_REJ0) || byte_read == (ADDRESS_SNDR ^ CONTROL_REJ1)){
                            state = BCC1_STATE;
                            is_rej = true;
                            //printf("BCC received\n");
                        }
                        else if (byte_read == (ADDRESS_SNDR ^ CONTROL_RR0) || byte_read == (ADDRESS_SNDR ^ CONTROL_RR1)){
                            state = BCC1_STATE;
                        }
                        else if(byte_read == (ADDRESS_SNDR ^ CONTROL_UA)){
                            state = BCC1_STATE;
                        }
                        else if(byte_read == FLAG){
                            state = FLAG_STATE;
                            //printf("Flag received instead of BCC\n");
                        }
                        else {
                            state = START_STATE;
                            //printf("Back to start from control\n");
                        }
                        break;
                    
                    
                    case BCC1_STATE:
                        if (byte_read == FLAG) {
                            state = STOP_STATE;
                            //printf("Going to stop\n");
                        }
                        else {
                            state = START_STATE;
                            is_rej =false;
                            //printf("All the way to the start from BCC\n");
                        }
                        break;
                    
                    default:
                        break;
                }
            }
            if (is_rej && state == STOP_STATE) {
                alarm(0);
                alarmEnabled = FALSE;
                // nRetransmissions--;
                statistics.n_retransmissions++;
                printf("INFO: Frame rejected - retrasmiting\n"); // TODO: Is this print necessary?
                continue;
            } else if (!is_rej && state == STOP_STATE) {
                alarm(0);
                alarmEnabled = FALSE;
                printf("INFO: Frame Well transmited\n");
                if(frame_n == 0) frame_n = 1;
                else frame_n = 0;
                if (j < 7) return j;
                return j - 7;
            }
            
        }
        
    }
    if (is_rej) printf("ERROR: Number of retransmissions exceeded!\n");
    else printf("TIMEOUT: Could not send the frame\n");

    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    int bytes;
    int size;
    memset(packet, 0, BUF_SIZE*2);
    //printf("Entrei na leitura\n");

    unsigned char address = 0;
    unsigned char control = 0;

    frameState_t state = START_STATE;
    unsigned i = 0;
    while (state != STOP_STATE) {
        if (i >= BUF_SIZE) size = -2;

        unsigned char byte_read = 0;
        int n_bytes_read = readByteSerialPort(&byte_read);
        // printf("%d\n",n_bytes_read);
        if(n_bytes_read == 0 && state != FLAG2_STATE) continue;
        
        unsigned char bcc;
        switch (state) {
            case START_STATE:
                //printf("First flag received\n");
                if(byte_read == FLAG){
                    state = FLAG_STATE;
                    // packet[i] = byte_read;
                    // i++;
                }
                break;
            
            case FLAG_STATE:
                if(byte_read == ADDRESS_SNDR){
                    state = ADDRESS_STATE;
                    // packet[i] = byte_read;
                    address = byte_read;
                    //i++;
                    //printf("Adress received\n");
                } else if (byte_read != FLAG) {
                    state = START_STATE;
                    //i = 0;
                    //printf("Back to start :(\n");
                }
                break;
            
            case ADDRESS_STATE:
                if ((byte_read == CONTROL_B0) || (byte_read == CONTROL_B1) || (byte_read == CONTROL_SET)) {
                    state = CONTROL_STATE;
                    //packet[i] = byte_read;
                    control = byte_read;
                    //i++;
                    //printf("Control received\n");
                
                } else if (byte_read == FLAG) {
                    state = FLAG_STATE;
                    //i = 1;
                    //printf("Flag received instead of control\n");
                } else {
                    state = START_STATE;
                    //i = 0;
                    //printf("Back to start from address\n");
                }
                break;
            
            case CONTROL_STATE:
                if (byte_read == (address ^ control)) {
                    state = BCC1_STATE;
                    //packet[i] = byte_read;
                    //i++;
                    //printf("BCC received\n");
                } else if (byte_read == FLAG) {
                    state = FLAG_STATE;
                    //i = 1;
                    //printf("Flag received instead of BCC\n");
                } else {
                    state = START_STATE;
                    //i = 0;
                    //printf("Back to start from control\n"); //TODO descarta
                    
                }
                break;
            
            case BCC1_STATE:
                if (byte_read == ESC) {
                    state = ESC_STATE;
                    //printf("ESC read\n");
                
                } else if (byte_read == FLAG){
                    state = STOP_STATE;
                    size = 5;
                    //packet[i++] = byte_read;
                    //size = i;
                } 
                else {
                    state = DATA_STATE;
                    packet[i++] = byte_read;
                    //printf("Data Byte BCC\n");
                }
                break;

            case ESC_STATE:
                packet[i++] = (byte_read ^ 0x20); //destuffing
                state = DATA_STATE;
                //printf("Destuffing\n");
                break;

            case DATA_STATE:
                if (byte_read == FLAG) {
                    state = FLAG2_STATE;
                    //packet[i++] = byte_read;
                    //i++;
                    //printf("Read Flag going to end\n");
                } else if (byte_read == ESC) {
                    state = ESC_STATE;
                    //printf("ESC read\n");
                } else { // Read normal data
                    packet[i++] = byte_read;
                    //printf("Read normal data byte\n");
                }
                break;

            case FLAG2_STATE:
                //printf("Estou no estado final\n");
                bcc = packet[0];
                for (unsigned v = 1; v < (i-1); v++) bcc ^= packet[v];
                
                if (bcc == packet[i-1]){
                    state = STOP_STATE;
                    size = i - 1;
                    //printf("Cheguei ao fim\n");
                } else {
                    i = 0;
                    state = STOP_STATE;
                    //printf("BCC2 wrong: stopping to reject\n");
                    size = -1;
                }
                //packet[i-1] = 0;
                break;
            
            default:
                break;
        }
    
        if(state == STOP_STATE){
            if (size == -2) {
                printf("ERROR: Allocated buffer is too small.\n");
                return -1;

            }else if(size == 5){ //Received a SET send a UA_FRAME
                bytes = writeBytesSerialPort(ua_frame,ASW_BUF_SIZE);
                // printf("Sent UA\n");
                size = 0;
            } 
            else if (size == -1) { // BCC2 WRONG!
                if (control == CONTROL_B0) {
                    bytes = writeBytesSerialPort(rej_0,ASW_BUF_SIZE);
                    frame_n = 0;
                    
                    // printf("Sent rej0\n");
                    state = START_STATE;
                } else {
                    bytes = writeBytesSerialPort(rej_1,ASW_BUF_SIZE);
                    frame_n = 1;
                    
                    // printf("Sent rej1\n");
                    state = START_STATE;
                }
            } else if (control == CONTROL_B0 && frame_n == 0) {
                bytes = writeBytesSerialPort(rr_1,ASW_BUF_SIZE);
                frame_n = 1;
                // printf("Sent RR1\n");
            } else if (control == CONTROL_B1 && frame_n == 1) {
                bytes = writeBytesSerialPort(rr_0,ASW_BUF_SIZE);
                frame_n = 0;
                // printf("Sent RR0\n");
                
            } else if (control == CONTROL_B1) { //duplicated I1
                bytes = writeBytesSerialPort(rr_0,ASW_BUF_SIZE);
                frame_n = 0;
                // printf("Sent nothing - duplicate discard\n");
                state = START_STATE;

            } else if (control == CONTROL_B0) { //duplicated I0
                bytes = writeBytesSerialPort(rr_1,ASW_BUF_SIZE);
                frame_n = 1;
                // printf("Sent nothing - duplicate discard\n");
                state = START_STATE;
            }
        }
    }
    printf("INFO: Bytes sent in answer:%d\n",bytes);
    printf("INFO: Read packet size:%d\n",size);
    statistics.n_frames++;
    return size;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
    // TODO: Show Statistics
   
    (void)signal(SIGALRM, alarmHandler);
    
    alarmCount = 0;

    frameState_t state = START_STATE;
    switch (role) {
        case LlRx:
            // Receiving DISC frame:
            state = START_STATE;
            while (state != STOP_STATE) {
                unsigned char byte_read = 0;
                int n_bytes_read = readByteSerialPort(&byte_read);
                if(n_bytes_read == 0) continue;
                switch (state) {
                    case START_STATE:
                        if(byte_read == FLAG){
                            state = FLAG_STATE;
                            //printf("First flag received\n");
                        }
                        break;
                    
                    case FLAG_STATE:
                        if(byte_read == ADDRESS_SNDR){
                            state = ADDRESS_STATE;
                            //printf("Adress received\n");
                        }
                        else if(byte_read != FLAG){
                            state = START_STATE;
                            //printf("Back to start :(\n");
                        }
                        break;
                    
                    case ADDRESS_STATE:
                        if(byte_read == CONTROL_DISC) {
                            state = CONTROL_STATE;
                            //printf("Control received\n");
                        } else if (byte_read == FLAG) {
                            state = FLAG_STATE;
                            //printf("Flag received instead of control\n");
                        } else {
                            state = START_STATE;
                            //printf("Back to start from address\n");
                        }
                        break;
                    
                    case CONTROL_STATE:
                        if(byte_read == (ADDRESS_SNDR ^ CONTROL_DISC)){
                            state = BCC1_STATE;
                            //printf("BCC received\n");
                        }
                        else if(byte_read == FLAG){
                            state = FLAG_STATE;
                            //printf("Flag received instead of BCC\n");
                        }
                        else {
                            state = START_STATE;
                            //printf("Back to start from control\n");
                        }
                        break;
                    
                    case BCC1_STATE:
                        if (byte_read == FLAG) {
                            state = STOP_STATE;
                            statistics.n_frames++;
                            //printf("Last flag received proceded to stop\n");
                        }
                        else {
                            state = START_STATE;
                            //printf("All the way to the start from BCC\n");
                        }
                        break;
                    
                    default:
                        break;
                }
            }
            
            // Sending DISC frame:
            alarmCount = 0;
            while (alarmCount < nRetransmissions) {
                if (alarmEnabled == FALSE) {
                    alarm(timeout);
                    alarmEnabled = TRUE;
                    
                    int bytes = writeBytesSerialPort(disc_frame_rcvr, ASW_BUF_SIZE);
                    printf("INFO: %d bytes written - RECEIVER DISC\n", bytes);
                    
                    state = START_STATE;
                    while (alarmEnabled != FALSE && state != STOP_STATE) {
                        unsigned char byte_read = 0;
                        int n_bytes_read = readByteSerialPort(&byte_read);
                        if(n_bytes_read == 0) continue;
                        switch (state) {
                            case START_STATE:
                                if(byte_read == FLAG){
                                    state = FLAG_STATE;
                                    // printf("First flag received\n");
                                }
                                break;
                            
                            case FLAG_STATE:
                                if(byte_read == ADDRESS_RCVR){
                                    state = ADDRESS_STATE;
                                    // printf("Adress received\n");
                                }
                                else if(byte_read != FLAG){
                                    state = START_STATE;
                                    // printf("Back to start :(\n\n");
                                }
                                break;
                            
                            case ADDRESS_STATE:
                                if(byte_read == CONTROL_UA) {
                                    state = CONTROL_STATE;
                                    // printf("Control received\n");
                                } else if (byte_read == FLAG) {
                                    state = FLAG_STATE;
                                    // printf("Flag received instead of control\n");
                                } else {
                                    state = START_STATE;
                                    // printf("Back to start from address\n");
                                }
                                break;
                            
                            case CONTROL_STATE:
                                if(byte_read == (ADDRESS_RCVR ^ CONTROL_UA)){
                                    state = BCC1_STATE;
                                    // printf("BCC received\n");
                                }
                                else if(byte_read == FLAG){
                                    state = FLAG_STATE;
                                    // printf("Flag received instead of BCC\n");
                                }
                                else {
                                    state = START_STATE;
                                    // printf("Back to start from control\n");
                                }
                                break;
                            
                            case BCC1_STATE:
                                if (byte_read == FLAG) {
                                    state = STOP_STATE;
                                    alarm(0);
                                    alarmEnabled = FALSE;
                                    printf("INFO: Successfully disconnected\n");
                                    statistics.n_frames++;

                                    if (showStatistics) {
                                        printf("\nSTATISTICS:\n");
                                        printf("Number of frames well received: %d\n", statistics.n_frames);
                                    }
                                    return 1;
                                }
                                else {
                                    state = START_STATE;
                                    // printf("All the way to the start from BCC\n");
                                }
                                break;
                            
                            default:
                                break;
                        }
                    }
                }
            }
            printf("TIMEOUT: UA frame not received!\n");
            break;
        
        case LlTx:
            alarmCount = 0;
            statistics.n_retransmissions--;
            while (alarmCount < nRetransmissions) {
                if (alarmEnabled == FALSE) {
                    alarm(timeout); // Set alarm to be triggered in 3s
                    alarmEnabled = TRUE;
                    
                    int bytes = writeBytesSerialPort(disc_frame_sndr, ASW_BUF_SIZE);
                    printf("INFO: %d bytes written - SENDER DISC\n", bytes);
                    statistics.n_frames++;
                    statistics.n_retransmissions++;
                    
                    state = START_STATE;
                    while (alarmEnabled != FALSE && state != STOP_STATE) {
                        unsigned char byte_read = 0;
                        int n_bytes_read = readByteSerialPort(&byte_read);
                        if(n_bytes_read == 0) continue;
                    
                        switch (state) {
                            case START_STATE:
                                if(byte_read == FLAG){
                                    state = FLAG_STATE;
                                    // printf("First flag received\n");
                                }
                                break;
                            
                            case FLAG_STATE:
                                if(byte_read == ADDRESS_RCVR){
                                    state = ADDRESS_STATE;
                                    // printf("Adress received\n");
                                }
                                else if(byte_read != FLAG){
                                    state = START_STATE;
                                    // printf("Back to start :(\n");
                                }
                                break;
                            
                            case ADDRESS_STATE:
                                if(byte_read == CONTROL_DISC) {
                                    state = CONTROL_STATE;
                                    // printf("Control received\n");
                                } else if (byte_read == FLAG) {
                                    state = FLAG_STATE;
                                    // printf("Flag received instead of control\n");
                                } else {
                                    state = START_STATE;
                                    // printf("Back to start from address\n");
                                }
                                break;
                            
                            case CONTROL_STATE:
                                if(byte_read == (ADDRESS_RCVR ^ CONTROL_DISC)){
                                    state = BCC1_STATE;
                                    // printf("BCC received\n");
                                }
                                else if(byte_read == FLAG){
                                    state = FLAG_STATE;
                                    // printf("Flag received instead of BCC\n");
                                }
                                else {
                                    state = START_STATE;
                                    // printf("Back to start from control\n");
                                }
                                break;
                            
                            case BCC1_STATE:
                                if (byte_read == FLAG) {
                                    state = STOP_STATE;
                                    alarm(0);
                                    alarmEnabled = FALSE;
                                    statistics.n_frames++;

                                    int bytes = writeBytesSerialPort(ua_frame_disc, ASW_BUF_SIZE);
                                    printf("INFO: %d bytes written - UA\n", bytes);
                                    printf("INFO: Successfully disconnected\n");

                                    if (showStatistics) {
                                        printf("\nSTATISTICS:\n");
                                        printf("Number of frames sent:                     %d\n",statistics.n_frames);
                                        printf("Number of Timeouts:                        %d\n", statistics.n_timeouts);
                                        printf("Number of frames retransmited (rejection): %d\n", statistics.n_retransmissions);
                                    }
                                    return 1;
                                }
                                else {
                                    state = START_STATE;
                                    // printf("All the way to the start from BCC\n");
                                }
                                break;
                            
                            default:
                                break;
                        }
                    }
                }
            }
            printf("TIMEOUT: DISC frame not received!\n");
            break;
    }

    return -1;
}
