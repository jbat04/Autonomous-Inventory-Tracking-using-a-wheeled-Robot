#ifndef MY_HEADER_FILE_
#define MY_HEADER_FILE_

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <tmr_gpio.h>
#include <tmr_utils.h>
#include <tmr_tag_protocol.h>
#include <tmr_status.h>
#include <tmr_types.h>
#include <tmr_serial_transport.h>
#include <osdep.h>

#define TMR_MAX_EMBEDDED_DATA_LENGTH 128
typedef struct AutonomousReading
{
  /* Buffer tag records fetched from module but not yet passed to caller. */
  uint8_t bufResponse[255];
  /* bufResopnse read index */
  uint8_t bufPointer;
  /* Buffer to store additional error message */
  char errMsg[255];
  /* Reader URI */
  char uri[255];
  /* Reader Stats response */
  bool isStatusResponse;
  /* Temporary storage during a read and subsequent fetch of tags */
  uint32_t readTimeLow, readTimeHigh;
  uint32_t lastSentTagTimestampHigh, lastSentTagTimestampLow;
  /* The flag to be used in case of tag reading */
  bool isBasetimeUpdated;
  TMR_SR_SerialTransport transport;
  TMR_SR_SerialPortNativeContext context;
  pthread_t autonomousBackgroundReader;
}AutonomousReading;

/*
 * ThingMagic-mutated CRC used for messages.
 * Notably, not a CCITT CRC-16, though it looks close.
 */
uint16_t crctable[] = 
{
  0x0000, 0x1021, 0x2042, 0x3063,
  0x4084, 0x50a5, 0x60c6, 0x70e7,
  0x8108, 0x9129, 0xa14a, 0xb16b,
  0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
};

/**
 *  Reader Stats Flag Enum
 */
typedef enum TMR_Reader_StatsFlag
{
  /** Current temperature of the device in units of Celcius */
  TMR_READER_STATS_FLAG_TEMPERATURE = (1 << 8),
}TMR_Reader_StatsFlag;

typedef struct TMR_Reader_StatsValues
{
  TMR_Reader_StatsFlag valid;
  /** Current temperature (degrees C) */
  int8_t temperature;
}TMR_Reader_StatsValues;
#endif
