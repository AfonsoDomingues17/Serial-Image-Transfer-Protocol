#include "state_machine.h"

void stablishConnectionSender(int fd){
    stablishConnectionState state = START_STATE;
    while (state != STOP_STATE) {
        unsigned char byte_read = 0;
        int n_bytes_read = read(fd, &byte_read, 1);
        if (n_bytes_read != 1) printf("Failed to read byte from serial port.\n");
        switch (state) {
            case START_STATE:
                if(byte_read == FLAG){
                    state = FLAG_STATE;
                    //printf("First flag received\n");
                }
                break;
            
            case FLAG_STATE:
                if(byte_read == ADDRESS_UA){
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
                if(byte_read == (ADDRESS_UA ^ CONTROL_UA)){
                    state = BCC_STATE;
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
            
            case BCC_STATE:
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
}

void stablishConnectionReceiver(int fd) {
    stablishConnectionState state = START_STATE;
    
    while (state != STOP_STATE) {
        unsigned char byte_read = 0;
        int n_bytes_read = read(fd, &byte_read, 1);
        if (n_bytes_read != 1) printf("Failed to read byte from serial port.\n");
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
                    state = BCC_STATE;
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
            
            case BCC_STATE:
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
