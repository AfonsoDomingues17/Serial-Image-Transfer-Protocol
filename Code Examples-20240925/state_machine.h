#ifndef _STATE_MACHINE_H_
#define _STATE_MACHINE_H_

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 1024

#define FLAG 0x7E
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
#define CONTROL_B0 0X00
#define CONTROL_B1 0x80
#define ESC 0X7D


typedef enum {
    START_STATE,
    FLAG_STATE,
    ADDRESS_STATE,
    CONTROL_STATE,
    BCC1_STATE,
    DATA_STATE,
    ESC_STATE,
    FLAG2_STATE,
    STOP_STATE
} frameState_t;


/**
 * @brief State Machine to stablish connection (to be runned on sender).
 * 
 * @param fd
 */
int receiveI_frame(int fd, unsigned char frame[], unsigned frame_size, unsigned frame_n);

/**
 * @brief State Machine to stablish connection (to be runned on receiver).
 * 
 * @param fd
 */
void stablishConnectionReceiver(int fd);

#endif // _STATE_MACHINE_H_
