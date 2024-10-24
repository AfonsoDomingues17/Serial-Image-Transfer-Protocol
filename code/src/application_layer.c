// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer linklayer;
    strcpy(linklayer.serialPort, serialPort);
    if (!strcmp(role, &"rx", 2)) linklayer.role = LlRx;
    else linklayer.role = LlTx;
    linklayer.baudRate = baudRate;
    linklayer.nRetransmissions = nTries;
    linklayer.timeout = timeout;

    unsigned char frame[14] = {0x7e,0x03,0x00,0x03 ^ 0x00,0x02,0x7e,0x02,0x02,0x02,0x02,0x02,0x02, 0x7c, 0x7e};
    unsigned char packet[2048];

    if (llopen(linklayer) != 1){
        printf("Error in opening the serial port\n");
        exit(1);
    }

    printf("llopen successfull\n");

    
    switch (linklayer.role) {
        case LlTx:
        int writen_characters = llwrite(frame,14);
        if(writen_characters == -1){
            printf("Numero maximo de retransmissoes atingiddo trama tem de ter problema\n");
            return;
        }
            printf("Numero de characters escritos:%d\n",writen_characters);
            printf("Tudo enviado como manda a spatilha\n");
            break;

        case LlRx:
            llread(&packet);
            for(int i = 0; i < 15; i++) printf("Byte read [%d]:%x\n",i,packet[i]);
            break;

        default: break;
    }




    if(llclose(TRUE) != 1){
        printf("Error in closing the serial port\n");
        exit(1);
    }
    printf("Serial port succssefully closed ^_^\n");
    
    return;


}
