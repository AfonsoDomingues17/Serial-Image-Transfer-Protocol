// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PACKET_SIZE 1024

#define PACKET_CTRL_START   1
#define PACKET_CTRL_DATA    2
#define PACKET_CTRL_END     3

#define PACKET_TYPE_SIZE    0
#define PACKET_TYPE_NAME    1

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer linklayer;
    strcpy(linklayer.serialPort, serialPort);
    if (!strcmp(role, "rx")) linklayer.role = LlRx;
    else linklayer.role = LlTx;
    linklayer.baudRate = baudRate;
    linklayer.nRetransmissions = nTries;
    linklayer.timeout = timeout;

    unsigned char packet[PACKET_SIZE * 2];

    if (llopen(linklayer) != 1){
        printf("ERROR: Unable to open serial port\n");
        exit(1);
    }

    switch (linklayer.role) {
        case LlTx : {
            FILE * file = fopen(filename,"r");
            if (file == NULL){
                printf("ERROR: Could not open the file\n");
                exit(1);
            }

            fseek(file,0,SEEK_END); // redirects the pointer to the end of the file
            long int file_size = ftell(file); // returns the current offset from the starting position
            fseek(file,0,SEEK_SET); // redirects the pointer back to the start of the file

            int L1 = 8; // ceil(log2((double)file_size) / 8.0);
            unsigned int L2 = strlen(filename);
            if (L2 > 255) {
                printf("ERROR: File name too big!\n");
                exit(1);
            }
            unsigned int size_ctrl_packet = 1 + 2 + L1 + 2 + L2; // {CTRL, T1, L1, V1[L1], T2, L2, V2[L2]}

            unsigned char ctrl_packet[size_ctrl_packet];
            int i = 0;
            ctrl_packet[i++] = PACKET_CTRL_START;
            ctrl_packet[i++] = PACKET_TYPE_SIZE; // type file size
            ctrl_packet[i++] = L1;
            for(int v = L1 - 1; v >= 0; v--){
                ctrl_packet[i++] = (file_size >> (8 * v) & 0xFF);
            }
            ctrl_packet[i++] = PACKET_TYPE_NAME; // type filename
            ctrl_packet[i++] = L2;
            memcpy(&ctrl_packet[i],filename,L2);
            i += L2;


            if(llwrite(ctrl_packet,size_ctrl_packet) == -1){
                printf("ERROR: Control packet not written properly!\n");
                fclose(file);
                exit(1);
            }
            
            int sequence_number = 0;
            unsigned char data_packet[PACKET_SIZE];
            
            int bytesToSend = file_size;
            unsigned char* file_content = (unsigned char *) calloc(file_size, 1);
            fread(file_content,1,file_size,file);
            unsigned long offset = 0;


            while(bytesToSend > 0){
                printf("INFO: Bytes Still to send:%d\n",bytesToSend);
                i = 0;
                int data_size;
                if(bytesToSend < PACKET_SIZE - 4) data_size = bytesToSend;
                else data_size = PACKET_SIZE - 4;
                
                bytesToSend -= data_size;

                int size_data_packet = 4 + data_size;
                data_packet[i++] = PACKET_CTRL_DATA;
                data_packet[i++] = sequence_number++ % 100;
                data_packet[i++] = (data_size >> 8) & 0xFF;
                data_packet[i++] = data_size & 0xFF;
                memcpy(data_packet + 4, file_content + offset, data_size);
                offset += data_size;

                if(llwrite(data_packet,size_data_packet) == -1){
                    printf("ERROR: Unable to write Data Packet.\n");
                    fclose(file);
                    exit(1);
                }            
            }
            free(file_content);
            ctrl_packet[0] = PACKET_CTRL_END;
            if(llwrite(ctrl_packet,size_ctrl_packet) == -1){
                printf("ERROR: Final Control packet not written properly!\n");
                fclose(file);
                exit(1);
            }

            fclose(file);
            break;
        }
        
        case LlRx: {
            int ctrl_size = llread(packet);
            if (ctrl_size == -1) {
                printf("ERROR: Unable to read Control Packet from link layer.\n");
                exit(1);
            }   
            printf("INFO: Nº de bytes lidos:%d\n",ctrl_size);
            unsigned long fileSize = 0;
            
            for(unsigned i = 0; i < 8; i++) {
                fileSize += packet[3 + i] << 8 * (8 - i - 1);
            }

            FILE * new_file = fopen(filename,"w");
            
            while (TRUE) {
                int read_packet_size = 0;
                read_packet_size = llread(packet);
                if (read_packet_size == -1) {
                    printf("ERROR: Unable to read Data Packet from link layer.\n");
                    fclose(new_file);
                    exit(1);
                }

                if(packet[0] == PACKET_CTRL_END) break;
                else{
                    fwrite(packet + 4, 1, read_packet_size - 4, new_file);
                } 
            }
            fclose(new_file);
            break;
        }
        default: break;
    }

    if (llclose(TRUE) != 1) {
        printf("ERROR: Unable to close serial port\n");
        exit(1);
    }
    printf("INFO: Serial port successfully closed!\n");
    
    return;


}
