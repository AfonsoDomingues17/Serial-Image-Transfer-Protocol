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

unsigned char rr_0[ASW_BUF_SIZE] = {FLAG,ADDRESS_SNDR,CONTROL_RR0,ADDRESS_SNDR ^ CONTROL_RR0, FLAG};
unsigned char rr_1[ASW_BUF_SIZE] = {FLAG,ADDRESS_SNDR,CONTROL_RR1,ADDRESS_SNDR ^ CONTROL_RR1, FLAG};
unsigned char rej_0[ASW_BUF_SIZE] = {FLAG,ADDRESS_SNDR,CONTROL_REJ0,ADDRESS_SNDR ^ CONTROL_REJ0, FLAG};
unsigned char rej_1[ASW_BUF_SIZE] = {FLAG,ADDRESS_SNDR,CONTROL_REJ1,ADDRESS_SNDR ^ CONTROL_REJ1, FLAG};
unsigned char disc_frame[BUF_SIZE] = {FLAG,ADDRESS_SNDR,CONTROL_DISC,ADDRESS_SNDR ^ CONTROL_DISC,FLAG};
unsigned char ua_frame[ASW_BUF_SIZE] = {FLAG,ADDRESS_SNDR,CONTROL_UA,ADDRESS_SNDR ^ CONTROL_UA,FLAG};
unsigned char set_frame[BUF_SIZE] = {FLAG,ADDRESS_SNDR,CONTROL_SET,ADDRESS_SNDR ^ CONTROL_SET,FLAG};

volatile int STOP = FALSE;

int alarmEnabled = FALSE;

int alarmCount;
LinkLayerRole role;
int timeout = 0;
int nRetransmitions = 0;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
}

// TODO: STORE STATISTICS
//      Number of frames sent
//      Time in the connection

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {return -1;}

    role = connectionParameters.role;
    timeout = connectionParameters.timeout;
    nRetransmitions = connectionParameters.nRetransmissions;   

    switch (connectionParameters.role) {
        case LlTx:
            (void)signal(SIGALRM, alarmHandler);
            
            alarmCount = 0;

            while (alarmCount < 5) {
                if (alarmEnabled == FALSE) {
                    alarm(timeout); // Set alarm to be triggered in 3s
                    alarmEnabled = TRUE;
                    
                    int bytes = writeBytesSerialPort(set_frame, 5);
                    printf("%d bytes written\n", bytes);
                    
                    

                    
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
                            printf("Connection established sucssfuly\n");
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
            printf("Should be here when alarm rings\n");
            }
                
            }
            printf("TIMEOUT: Could not establish connection\n");
            break;
        
        case LlRx:
            frameState_t state = START_STATE;
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
            printf("Connection stablished sussfully!\n");
            int bytes = writeBytesSerialPort(ua_frame,ASW_BUF_SIZE);
            if (bytes != 5) printf("Failed to send 5 bytes (UA frame).\n");
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
    // TODO: TIMEOUTS AND RETRANSMITIONS

    alarmCount = 0;
    bool is_rej = false;
    
    // BYTE STUFFING:
    unsigned char stuffed_buf[BUF_SIZE * 2] = {0}; // TODO: modify this size
    unsigned i = 0, j = 0;
    for (; i < 4; i++) stuffed_buf[j++] = buf[i];
    for (; i < bufSize - 1; i++) {
        if (buf[i] == FLAG || buf[i] == ESC) {
            stuffed_buf[j++] = ESC;
            stuffed_buf[j++] = buf[i] ^ 0x20;
        }
        else stuffed_buf[j++] = buf[i];
    }
    stuffed_buf[j++] = buf[i];

    while (nRetransmitions > 0 && alarmCount < 5) {
        if (alarmEnabled == FALSE) {
            alarm(timeout); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
            is_rej = false;
            
            int bytes = writeBytesSerialPort(stuffed_buf,j);
            printf("%d bytes written\n", bytes);
            
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
            if(is_rej){
                alarm(0);
                alarmEnabled = FALSE;
                nRetransmitions--;
                printf("Packet rejected - retrasmiting\n");
                continue;
            }
            else{
                alarm(0);
                alarmEnabled = FALSE;
                printf("Packet Well transmited\n");
                if (j < 7) return j;
                return j - 7;
            }
            
        }
        
    }
    if (is_rej) printf("NUMBER OF RETRANSMISSIONS EXCEDDED!\n");
    else printf("TIMEOUT: Could not send the frame\n");

    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    int bytes;
    int size;
    unsigned char expected_frame = 0;
    memset(packet, 0, BUF_SIZE*2);
    printf("Entrei na leitura\n");


    frameState_t state = START_STATE;
    unsigned i = 0;
    while (state != STOP_STATE) {
        if (i >= BUF_SIZE) size = -2;

        unsigned char byte_read = 0;
        int n_bytes_read = readByteSerialPort(&byte_read);
        // printf("%d\n",n_bytes_read);
        if(n_bytes_read == 0 && state != FLAG2_STATE) continue;
        

        switch (state) {
            case START_STATE:
                //printf("First flag received\n");
                if(byte_read == FLAG){
                    state = FLAG_STATE;
                    packet[i] = byte_read;
                    i++;
                }
                break;
            
            case FLAG_STATE:
                if(byte_read == ADDRESS_SNDR){
                    state = ADDRESS_STATE;
                    packet[i] = byte_read;
                    i++;
                    //printf("Adress received\n");
                } else if (byte_read != FLAG) {
                    state = START_STATE;
                    i = 0;
                    //printf("Back to start :(\n");
                }
                break;
            
            case ADDRESS_STATE:
                if ((byte_read == CONTROL_B0) || (byte_read == CONTROL_B1) || (byte_read == CONTROL_SET)) {
                    state = CONTROL_STATE;
                    packet[i] = byte_read;
                    i++;
                    //printf("Control received\n");
                
                } else if (byte_read == FLAG) {
                    state = FLAG_STATE;
                    i = 1;
                    //printf("Flag received instead of control\n");
                } else {
                    state = START_STATE;
                    i = 0;
                    //printf("Back to start from address\n");
                }
                break;
            
            case CONTROL_STATE:
                if (byte_read == (packet[1] ^ packet[2])) {
                    state = BCC1_STATE;
                    packet[i] = byte_read;
                    i++;
                    //printf("BCC received\n");
                } else if (byte_read == FLAG) {
                    state = FLAG_STATE;
                    i = 1;
                    //printf("Flag received instead of BCC\n");
                } else {
                    state = START_STATE;
                    i = 0;
                    //printf("Back to start from control\n"); //TODO descarta
                    
                }
                break;
            
            case BCC1_STATE:
                if (byte_read == ESC) {
                    state = ESC_STATE;
                    //printf("ESC read\n");
                
                } else if (byte_read == FLAG){
                    state = STOP_STATE;
                    packet[i++] = byte_read;
                    size = i;
                } 
                else {
                    state = DATA_STATE;
                    packet[i] = byte_read;
                    i++;
                    //printf("Data Byte BCC\n");
                }
                break;

            case ESC_STATE:
                packet[i] = (byte_read ^ 0x20); //destuffing
                i++;
                state = DATA_STATE;
                //printf("Destuffing\n");
                break;

            case DATA_STATE:
                if (byte_read == FLAG) {
                    state = FLAG2_STATE;
                    packet[i] = byte_read;
                    i++;
                    //printf("Read Flag going to end\n");
                } else if (byte_read == ESC) {
                    state = ESC_STATE;
                    //printf("ESC read\n");
                } else { // Read normal data
                    packet[i] = byte_read;
                    i++;
                    //printf("Read normal data byte\n");
                }
                break;

            case FLAG2_STATE:
            //printf("Estou no estado final\n");
                unsigned char bcc = packet[4];
                for (unsigned v = 5; v < (i-2); v++) bcc ^= packet[v];
                
                if (bcc == packet[(i-2)]){
                    state = STOP_STATE;
                    size = i;
                    //printf("Cheguei ao fim\n");
                } else {
                    i = 0;
                    state = STOP_STATE;
                    //printf("BCC2 wrong: stopping to reject\n"); 
                    size = -1;
                    
                }
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
        printf("Sent UA\n");
    } 
    else if (size == -1) { // BCC2 WRONG!
        if (packet[2] == CONTROL_B0) {
            bytes = writeBytesSerialPort(rej_0,ASW_BUF_SIZE);
            expected_frame = 0;
            printf("Sent rej0\n");
            state = START_STATE;
        } else {
            bytes = writeBytesSerialPort(rej_1,ASW_BUF_SIZE);
            expected_frame = 1;
            printf("Sent rej1\n");
            state = START_STATE;
        }
    } else if (packet[2] == CONTROL_B0 && expected_frame == 0) {
        bytes = writeBytesSerialPort(rr_1,ASW_BUF_SIZE);
        expected_frame = 1;
        printf("Sent RR1\n");
    } else if (packet[2] == CONTROL_B1 && expected_frame == 1) {
        bytes = writeBytesSerialPort(rr_0,ASW_BUF_SIZE);
        expected_frame = 0;
        printf("Sent RR0\n");
        
    } else if (packet[2] == CONTROL_B1) { //duplicated I1
        bytes = writeBytesSerialPort(rr_0,ASW_BUF_SIZE);
        expected_frame = 0;
        printf("Sent nothing - duplicate discard\n");
        state = START_STATE;

    } else if (packet[2] == CONTROL_B0) { //duplicated I0
        bytes = writeBytesSerialPort(rr_1,ASW_BUF_SIZE);
        expected_frame = 1;
        printf("Sent nothing - duplicate discard\n");
        state = START_STATE;
    } 
    }
    }
    printf("Bytes sent in answer:%d",bytes);
    printf("tamanho do packet lido:%d\n",size);
 
    return size;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
   
    (void)signal(SIGALRM, alarmHandler);
    
    alarmCount = 0;

    switch (role) {
        case LlTx:
            
           
            while (alarmCount < 5) {
                if (alarmEnabled == FALSE) {
                    alarm(timeout); // Set alarm to be triggered in 3s
                    alarmEnabled = TRUE;
                    
                    int bytes = writeBytesSerialPort(disc_frame, ASW_BUF_SIZE);
                    printf("%d bytes written\n", bytes);
                    
            
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
                            alarm(0);
                            alarmEnabled = FALSE;
                            int bytes = writeBytesSerialPort(ua_frame, 5);
                            printf("ua_frame sent now up to the receiver\n");
                            return 1;
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
                    
                    
                }
                
            }
            printf("TIMEOUT: Could not des-establish connection\n");
            break;
        
        case LlRx:
            bool endConn = false;
            frameState_t state = START_STATE;
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
                        
                        } else if(byte_read == CONTROL_UA){
                            state = CONTROL_STATE;
                            endConn = true;
                        }else if (byte_read == FLAG) {
                            state = FLAG_STATE;
                            //printf("Flag received instead of control\n");
                        } else {
                            state = START_STATE;
                            //printf("Back to start from address\n");
                        }
                        break;
                    
                    case CONTROL_STATE:
                        if(byte_read == (ADDRESS_SNDR ^ CONTROL_UA) || byte_read == (ADDRESS_SNDR ^ CONTROL_DISC)){
                            state = BCC1_STATE;
                            //printf("BCC received\n");
                        }
                        else if(byte_read == FLAG){
                            state = FLAG_STATE;
                            endConn = false;
                            //printf("Flag received instead of BCC\n");
                        }
                        else {
                            state = START_STATE;
                            endConn = false;
                            //printf("Back to start from control\n");
                        }
                        break;
                    
                    case BCC1_STATE:
                        if (byte_read == FLAG) {
                            if(endConn){
                            //printf("Closing\n");
                            state = STOP_STATE;
                            int clstat = closeSerialPort();
                            return 1;
                            }
                            else{
                                int bytes = writeBytesSerialPort(disc_frame, ASW_BUF_SIZE);
                                printf("dis_frame sent back to the transmitter\n");
                                state = START_STATE;
                            }
                            //printf("Last flag received proceded to stop\n");
                        }
                        else {
                            state = START_STATE;
                            endConn = false;
                            
                            //printf("All the way to the start from BCC\n");
                        }
                        break;
                    
                    default:
                        break;
                }
            }
            
        
            break;

        default:
            break;
    }
    
        
    
    return -1;
}
