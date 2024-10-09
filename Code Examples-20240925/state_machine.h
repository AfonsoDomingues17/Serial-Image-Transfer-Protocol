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

typedef enum {
    START_STATE,
    FLAG_STATE,
    ADDRESS_STATE,
    CONTROL_STATE,
    BCC_STATE,
    STOP_STATE
} stablishConnectionState;

/**
 * @brief State Machine to stablish connection (to be runned on sender).
 * 
 * @param fd
 */
void stablishConnectionSender(int fd);

/**
 * @brief State Machine to stablish connection (to be runned on receiver).
 * 
 * @param fd
 */
void stablishConnectionReceiver(int fd);

#endif // _STATE_MACHINE_H_
