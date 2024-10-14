#include "state_machine.h"

int receiveI_frames(int fd, unsigned char frame[], unsigned frame_size, unsigned frame_n){
    if(frame_n != 0 && frame_n != 1) return -1;

    bool duplicate = false;

    frameState_t state = START_STATE;
    unsigned i = 0;
    while (state != STOP_STATE) {
        if (i >= frame_size) return -2;

        unsigned char byte_read = 0;
        int n_bytes_read = read(fd, &byte_read, 1);
        if(n_bytes_read == 0) continue;

        switch (state) {
            case START_STATE:
                if(byte_read == FLAG){
                    state = FLAG_STATE;
                    frame[i] = byte_read;
                    i++;
                    // printf("First flag received\n");
                }
                break;
            
            case FLAG_STATE:
                if(byte_read == ADDRESS_SET){
                    state = ADDRESS_STATE;
                    frame[i] = byte_read;
                    i++;
                    // printf("Adress received\n");
                } else if (byte_read != FLAG) {
                    state = START_STATE;
                    i = 0;
                    // printf("Back to start :(\n");
                }
                break;
            
            case ADDRESS_STATE:
                if ((byte_read == CONTROL_0 && frame_n == 0) || (byte_read == CONTROL_1 && frame_n == 1)) {
                    state = CONTROL_STATE;
                    frame[i] = byte_read;
                    i++;
                    // printf("Control received\n");
                } else if ((byte_read == CONTROL_0 && frame_n == 1) || (byte_read == CONTROL_1 && frame_n == 0)) {
                    duplicate = true;
                    frame[i] = byte_read;
                    i++;
                } else if (byte_read == FLAG) {
                    state = FLAG_STATE;
                    i = 1;
                    // printf("Flag received instead of control\n");
                } else {
                    state = START_STATE;
                    i = 0;
                    // printf("Back to start from address\n");
                }
                break;
            
            case CONTROL_STATE:
                if (byte_read == (frame[1] ^ frame[2])) {
                    state = BCC1_STATE;
                    frame[i] = byte_read;
                    i++;
                    // printf("BCC received\n");
                } else if (byte_read == FLAG) {
                    state = FLAG_STATE;
                    i = 1;
                    // printf("Flag received instead of BCC\n");
                } else {
                    state = START_STATE;
                    i = 0;
                    // printf("Back to start from control\n");
                }
                break;
            
            case BCC1_STATE:
                if (byte_read == ESC) {
                    state = ESC_STATE;
                    // printf("esc read\n");
                } else {
                    state = DATA_STATE;
                    frame[i] = byte_read;
                    i++;
                    // printf("All the way to the start from BCC\n");
                }
                break;

            case ESC_STATE:
                frame[i] = (byte_read ^ 0x20); //destuffing
                i++;
                state = DATA_STATE;
                break;

            case DATA_STATE:
                if (byte_read == FLAG) {
                    state = FLAG2_STATE;
                    frame[i] = byte_read;
                    i++;
                } else if (byte_read == ESC) {
                    state = ESC_STATE;
                } else { // Read normal data
                    frame[i] = byte_read;
                    i++;
                }
                break;

            case FLAG2_STATE:
                unsigned char bcc = frame[4];
                for (unsigned v = 5; v < (i-2); v++) bcc ^= frame[v];
                if (bcc == frame[(i-2)]){
                    state = STOP_STATE;
                } else {
                    i = 0;
                    state = START_STATE;
                }
                break;
            
            default:
                break;
        }
    }

    if (duplicate) return 0;
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
                    printf("First flag received\n");
                }
                break;
            
            case FLAG_STATE:
                if(byte_read == ADDRESS_SET){
                    state = ADDRESS_STATE;
                    printf("Adress received\n");
                }
                else if(byte_read != FLAG){
                    state = START_STATE;
                    printf("Back to start :(\n");
                }
                break;
            
            case ADDRESS_STATE:
                if(byte_read == CONTROL_SET) {
                    state = CONTROL_STATE;
                    printf("Control received\n");
                } else if (byte_read == FLAG) {
                    state = FLAG_STATE;
                    printf("Flag received instead of control\n");
                } else {
                    state = START_STATE;
                    printf("Back to start from address\n");
                }
                break;
            
            case CONTROL_STATE:
                if(byte_read == (ADDRESS_SET ^ CONTROL_SET)){
                    state = BCC1_STATE;
                    printf("BCC received\n");
                }
                else if(byte_read == FLAG){
                    state = FLAG_STATE;
                    printf("Flag received instead of BCC\n");
                }
                else {
                    state = START_STATE;
                    printf("Back to start from control\n");
                }
                break;
            
            case BCC1_STATE:
                if (byte_read == FLAG) {
                    state = STOP_STATE;
                    printf("Last flag received proceded to stop\n");
                }
                else {
                    state = START_STATE;
                    printf("All the way to the start from BCC\n");
                }
                break;
            
            default:
                break;
        }
    }
    return;
}
