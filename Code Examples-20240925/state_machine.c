#include "state_machine.h"

int receiveI_frame(int fd, unsigned char frame[], unsigned frame_size){
    // if(frame_n != 0 && frame_n != 1) return -1; // return value -1 is used when bcc2 is wrong
    printf("Entrei na leitura\n");

    frameState_t state = START_STATE;
    unsigned i = 0;
    while (state != STOP_STATE) {
        if (i >= frame_size) return -2;

        unsigned char byte_read = 0;
        int n_bytes_read = read(fd, &byte_read, 1);
        // printf("%d\n",n_bytes_read);
        if(n_bytes_read == 0) continue;

        switch (state) {
            case START_STATE:
                printf("First flag received\n");
                if(byte_read == FLAG){
                    state = FLAG_STATE;
                    frame[i] = byte_read;
                    i++;
                }
                break;
            
            case FLAG_STATE:
                if(byte_read == ADDRESS_SNDR){
                    state = ADDRESS_STATE;
                    frame[i] = byte_read;
                    i++;
                    printf("Adress received\n");
                } else if (byte_read != FLAG) {
                    state = START_STATE;
                    i = 0;
                    printf("Back to start :(\n");
                }
                break;
            
            case ADDRESS_STATE:
                if ((byte_read == CONTROL_B0) || (byte_read == CONTROL_B1)) {
                    state = CONTROL_STATE;
                    frame[i] = byte_read;
                    i++;
                    printf("Control received\n");
                
                } else if (byte_read == FLAG) {
                    state = FLAG_STATE;
                    i = 1;
                    printf("Flag received instead of control\n");
                } else {
                    state = START_STATE;
                    i = 0;
                    printf("Back to start from address\n");
                }
                break;
            
            case CONTROL_STATE:
                if (byte_read == (frame[1] ^ frame[2])) {
                    state = BCC1_STATE;
                    frame[i] = byte_read;
                    i++;
                    printf("BCC received\n");
                } else if (byte_read == FLAG) {
                    state = FLAG_STATE;
                    i = 1;
                    printf("Flag received instead of BCC\n");
                } else {
                    state = START_STATE;
                    i = 0;
                    printf("Back to start from control\n"); 
                }
                break;
            
            case BCC1_STATE:
                if (byte_read == ESC) {
                    state = ESC_STATE;
                    printf("ESC read\n");
                } else {
                    state = DATA_STATE;
                    frame[i] = byte_read;
                    i++;
                    printf("Data bytes\n");
                }
                break;

            case ESC_STATE:
                frame[i] = (byte_read ^ 0x20); //destuffing
                i++;
                state = DATA_STATE;
                printf("Destuffing\n");
                break;

            case DATA_STATE:
                if (byte_read == FLAG) {
                    state = FLAG2_STATE;
                    frame[i] = byte_read;
                    i++;
                    printf("Read Flag going to end\n");
                } else if (byte_read == ESC) {
                    state = ESC_STATE;
                    printf("ESC read\n");
                } else { // Read normal data
                    frame[i] = byte_read;
                    i++;
                    printf("Read normal data byte\n");
                }
                break;

            case FLAG2_STATE:
                unsigned char bcc = frame[4];
                for (unsigned v = 5; v < (i-2); v++) bcc ^= frame[v];
                if (bcc == frame[(i-2)]){
                    state = STOP_STATE;
                    printf("BCC2 okay -- procedded to stop\n");
                } else {
                    i = 0;
                    state = START_STATE;
                    printf("BCC2 wrong: going back to start state"); 
                    return -1;
                }
                break;
            
            default:
                break;
        }
    }

    return i;
}



void stablishConnectionReceiver(int fd) {
    frameState_t state = START_STATE;
    while (state != STOP_STATE) {
        unsigned char byte_read = 0;
        int n_bytes_read = read(fd, &byte_read, 1);
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
    return;
}
