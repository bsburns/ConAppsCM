#pragma once
/*------------------------------------------------------------------
 * common.h
 *
 * Common header file
 *
 * July 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

enum class StriperModeE : int {
    NOTSET = 0,
    TRANSMITTER = 1,
    RECEIVER = 2
};


// Now define the message using an interprocess vector for the payload
enum class DeqMsgType : int {
    NOTSET = 0,
    MESSAGE = 1,
    PACKET = 2,
    EXIT_PROCESS = 3
};
