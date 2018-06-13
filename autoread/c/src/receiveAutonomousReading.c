
/**
 * Sample program that demonstrates Autonomous read response capture.
 * @file receiveAutonomousReading.c
 */
#include "receive_autonomous_reading.h"
#include "tmr_tag_data.h"
#ifndef WIN32
#include <unistd.h>
#include <stdio.h>
#endif


FILE *f;
static char hexchars[] = "0123456789ABCDEF";

void errx(int exitval, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);

  exit(exitval);
}

void serialPrinter(bool tx, uint32_t dataLen, const uint8_t data[],
                   uint32_t timeout, void *cookie)
{
  FILE *out = cookie;
  uint32_t i;

  fprintf(out, "%s", tx ? "Sending: " : "Received:");
  for (i = 0; i < dataLen; i++)
  {
    if (i > 0 && (i & 15) == 0)
    {
      fprintf(out, "\n         ");
    }
    fprintf(out, " %02x", data[i]);
  }
  fprintf(out, "\n");
}

const char* protocolName(TMR_TagProtocol protocol)
{
	switch (protocol)
	{
	case TMR_TAG_PROTOCOL_NONE:
		return "NONE";
	case TMR_TAG_PROTOCOL_ISO180006B:
		return "ISO180006B";
	case TMR_TAG_PROTOCOL_GEN2:
		return "GEN2";
	case TMR_TAG_PROTOCOL_ISO180006B_UCODE:
		return "ISO180006B_UCODE";
	case TMR_TAG_PROTOCOL_IPX64:
		return "IPX64";
	case TMR_TAG_PROTOCOL_IPX256:
		return "IPX256";
	default:
		return "unknown";
	}
}

void
TMR_bytesToHex(const uint8_t *bytes, uint32_t size, char *hex)
{

  while (size--)
  {
    *hex++ = hexchars[*bytes >> 4];
    *hex++ = hexchars[*bytes & 15];
    bytes++;
  }
  *hex = '\0';
}


static uint16_t
tm_crc(uint8_t *u8Buf, uint8_t len)
{
  uint16_t crc;
  int i;

  crc = 0xffff;

  for (i = 0; i < len ; i++)
  {
    crc = ((crc << 4) | (u8Buf[i] >> 4))  ^ crctable[crc >> 12];
    crc = ((crc << 4) | (u8Buf[i] & 0xf)) ^ crctable[crc >> 12];
  }

  return crc;
}

/** Minimum number of bytes required to hold a given number of bits.
 *
 * @param bitCount  number of bits to hold
 * @return  Minimum length of bytes that can contain that many bits
 */
int tm_u8s_per_bits(int bitCount) 
{
	return ((0<bitCount) ?((((bitCount)-1)>>3)+1) :0);
}

void 
exceptionCallback(TMR_Status error, void *cookie)
{
  fprintf(stdout, "Error:%s\n", TMR_strerr(error));
}

void callback(const TMR_TagReadData *t, void *cookie)
{
  char epcStr[128];
  char timeStr[128];

  TMR_bytesToHex(t->tag.epc, t->tag.epcByteCount, epcStr);

#ifdef WIN32
	{
		FILETIME ft, utc;
		SYSTEMTIME st;
		char* timeEnd;
		char* end;
		
		utc.dwHighDateTime = t->timestampHigh;
		utc.dwLowDateTime = t->timestampLow;

		FileTimeToLocalFileTime( &utc, &ft );
		FileTimeToSystemTime( &ft, &st );
		timeEnd = timeStr + sizeof(timeStr)/sizeof(timeStr[0]);
		end = timeStr;
		end += sprintf(end, "%d-%d-%d", st.wYear,st.wMonth,st.wDay);
		end += sprintf(end, "T%d:%d:%d %d", st.wHour,st.wMinute,st.wSecond, st.wMilliseconds);
		end += sprintf(end, ".%06d", t->dspMicros);
  }
#else
    {
      uint8_t shift;
      uint64_t timestamp;
      time_t seconds;
      int micros;
      char* timeEnd;
      char* end;

      shift = 32;
      timestamp = ((uint64_t)t->timestampHigh<<shift) | t->timestampLow;
      seconds = timestamp / 1000;
      micros = (timestamp % 1000) * 1000;

      /*
       * Timestamp already includes millisecond part of dspMicros,
       * so subtract this out before adding in dspMicros again
       */
      micros -= t->dspMicros / 1000;
      micros += t->dspMicros;

      timeEnd = timeStr + sizeof(timeStr)/sizeof(timeStr[0]);
      end = timeStr;
      end += strftime(end, timeEnd-end, "%FT%H:%M:%S", localtime(&seconds));
      end += snprintf(end, timeEnd-end, ".%06d", micros);
      end += strftime(end, timeEnd-end, "%z", localtime(&seconds));
    }
#endif
  fprintf(f,"EPC:%s ant:%d count:%d time:%s\n", epcStr, t->antenna, t->readCount, timeStr);
  if (0 < t->data.len)
  {
    char dataStr[128];
    TMR_bytesToHex(t->data.list, t->data.len, dataStr);
    //printf("Data: %s\n", dataStr);
  }
}

void statsCallback (const TMR_Reader_StatsValues* stats, void *cookie)
{

  /** Each  field should be validated before extracting the value */
  /** Currently supporting only temperature value */
  if (TMR_READER_STATS_FLAG_TEMPERATURE & stats->valid)
  {
    //printf("Temperature %d(C)\n", stats->temperature);
  }
}

/* Get a high/low timestamp pair, being careful not to get one value
 * before a wraparound and the other one after, which would lead to
 * 50-day time jumps forward or backwards.  (high<<32)|low is in milliseconds.
 */
void
tm_gettime_consistent(uint32_t *high, uint32_t *low)
{
  uint32_t tmpHigh;

  *high   = tmr_gettime_high();
  *low    = tmr_gettime_low();
  tmpHigh = tmr_gettime_high();

  if (tmpHigh != *high)
  {
    *high = tmpHigh;
    *low = tmr_gettime_low();
  }
}

TMR_Status receiveAutonomousReading(struct AutonomousReading *autonomous);
TMR_Status serial_receiveAutonomousReading(struct AutonomousReading *autonomous,  TMR_TagReadData *trd, TMR_Reader_StatsValues *stats);
TMR_Status serial_hasMoreTags(struct AutonomousReading *autonomous);
TMR_Status serial_receiveMessage(struct AutonomousReading *autonomous, uint8_t *data, uint8_t opcode, uint32_t timeoutMs);
TMR_Status serial_connect(struct AutonomousReading *autonomous);
void       serial_parseMetadataFromMessage(TMR_TagReadData *read, uint16_t flags, uint8_t *i, uint8_t msg[]);
void       serial_postprocessReaderSpecificMetadata(struct AutonomousReading *autonomous, TMR_TagReadData *read);

int main(int argc, char *argv[])
{
  f = fopen("rfidData.txt","w");
  if (f == NULL){
	printf("Error Opening File");
	exit(1);
  }
  TMR_Status ret;
  AutonomousReading autonomous;
  autonomous.isStatusResponse = false;
/*
  if (argc < 2)
  {
    errx(1, "Please provide reader URL, such as:\n"
           "/com4\n"
           "/dev/ttySo\n");
  }  
*/
  /* Connect to the reader */
  strcpy(autonomous.uri,"/dev/rfid"); //argv[1]);
  ret = serial_connect(&autonomous);
  if (TMR_SUCCESS == ret)
  {
    printf("Connection to the module is successfully\n");
  }

  /* Extract Autonomous read responses */
  ret = receiveAutonomousReading(&autonomous);
  if (TMR_SUCCESS != ret)
  {
    errx(1, "Error %s: %s\n", "receive autonomous reading", TMR_strerr(ret));
  }

  while (1)
  {
#ifdef WIN32
    Sleep(5000);
#else
    sleep(5);
#endif
  }
  fclose(f);
  return 0;
}

TMR_Status serial_connect(AutonomousReading *autonomous)
{
  TMR_Status ret = TMR_SUCCESS;
  uint32_t probeBaudRates[] = {115200, 9600, 921600, 19200, 38400, 57600,230400, 460800};

  /*open the serial port */
  TMR_SR_SerialTransportNativeInit(&autonomous->transport, &autonomous->context, autonomous->uri);
  ret = autonomous->transport.open(&autonomous->transport);
  if(TMR_SUCCESS != ret)
  {
    /* Not able to open the serial port, exit early */
    return ret;
  }

  /* Find the baudrate */
  printf("Trying to connect...\n");
  do
  {
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
      ret = autonomous->transport.setBaudRate(&autonomous->transport, probeBaudRates[i]);
      autonomous->transport.flush(&autonomous->transport);
      ret = serial_receiveMessage(autonomous, autonomous->bufResponse ,0x22,5000);      
      if (TMR_SUCCESS == ret)
      {
        break;
      }
    }
  }while(TMR_SUCCESS != ret);

  /* Connected Successfully, do some basic initialization */
  autonomous->isBasetimeUpdated = false;
  autonomous->readTimeHigh = 0x00;
  autonomous->readTimeLow = 0x00;
  autonomous->lastSentTagTimestampHigh = 0x00;
  autonomous->lastSentTagTimestampLow = 0x00;
  return ret;
}

static void*
do_background_receiveAutonomousReading(void * arg)
{
  TMR_Status ret;
  TMR_TagReadData trd;
  AutonomousReading *autonomous;
  TMR_Reader_StatsValues stats;

  autonomous = arg;
  TMR_TRD_init(&trd);

  while (1)
  {
    if (false == autonomous->isBasetimeUpdated)
    {
      /* Update the base time stamp */
      /* update the base time stamp to current host time */
      uint32_t starttimeLow, starttimeHigh;

      starttimeLow = 0;
      starttimeHigh = 0;
      tm_gettime_consistent(&starttimeHigh, &starttimeLow);
      autonomous->readTimeHigh = starttimeHigh;
      autonomous->readTimeLow = starttimeLow;
      autonomous->lastSentTagTimestampHigh = starttimeHigh;
      autonomous->lastSentTagTimestampLow = starttimeLow;
      autonomous->isBasetimeUpdated = true;
    }
    ret = serial_receiveAutonomousReading(autonomous, &trd, &stats);
    if (TMR_SUCCESS != ret)
    {
      if (TMR_ERROR_NO_TAGS_FOUND != ret)
      {
        exceptionCallback(ret, stdout);
        if (TMR_ERROR_TIMEOUT == ret)
        {
          TMR_Status ret1 = TMR_ERROR_TIMEOUT;

          /* something wrong happened.. restore the serial communication */
          printf("Restoring the serial interface\n");
          autonomous->transport.shutdown(&autonomous->transport);
          do
          {
            ret1 = serial_connect(autonomous);
          }while(TMR_SUCCESS != ret1);
        }
      }
    }
    else
    {
      if (false == autonomous->isStatusResponse)
      {
        callback(&trd, stdout);
      }
    }
  }
  return NULL;
}

TMR_Status
receiveAutonomousReading(AutonomousReading *autonomous)
{
  TMR_Status ret;
  
  ret = TMR_SUCCESS;

  /**
  * create the thread
  */
  ret = pthread_create(&autonomous->autonomousBackgroundReader, NULL,
    do_background_receiveAutonomousReading, autonomous);
  if (0 != ret)
  {
    return TMR_ERROR_NO_THREADS;
  }
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  pthread_detach(autonomous->autonomousBackgroundReader);

  return ret;
}

TMR_Status
serial_receiveAutonomousReading(AutonomousReading *autonomous, TMR_TagReadData *trd, TMR_Reader_StatsValues *stats)
{
  TMR_Status ret;
  
  ret = TMR_SUCCESS;
  ret = serial_hasMoreTags(autonomous);
  if (TMR_SUCCESS == ret)
  {
    if (false == autonomous->isStatusResponse)
    {
      uint16_t flags;

      /* Ignore the fail cases and pass only valid messages */
      flags = GETU16AT(autonomous->bufResponse, 8);
      serial_parseMetadataFromMessage(trd, flags, &autonomous->bufPointer, autonomous->bufResponse);
      serial_postprocessReaderSpecificMetadata(autonomous, trd);
    }
    else
    {
      /* A status stream response */
      uint8_t offset;
      uint16_t flag = 0;

      stats->temperature = 0;
      offset = autonomous->bufPointer;
      stats->valid = TMR_READER_STATS_FLAG_TEMPERATURE;
      offset += 0x02;

      while (offset < (autonomous->bufResponse[1] + 2))
      {
        if ((0x80) > autonomous->bufResponse[offset])
        {
          flag = autonomous->bufResponse[offset];
        }
        else
        {
          /**
          * the response flag will be in EBV format,
          * convert that to the api enum values
          */
          flag = ((autonomous->bufResponse[offset] << 8) | (autonomous->bufResponse[offset + 1]));
          flag &= 0x7fff;
          flag = ((flag >> 1) | (flag & 0x7f));
        }

        if (flag & TMR_READER_STATS_FLAG_TEMPERATURE)
        {
          offset += 3;
          stats->temperature = GETU8(autonomous->bufResponse, offset);
        }
      }//end of while loop
      statsCallback(stats, stdout); 
    }
  }
  return ret;
}

TMR_Status
serial_hasMoreTags(AutonomousReading *autonomous)
{
  TMR_Status ret;
  uint8_t *msg;
  uint8_t response_type_pos;
  uint8_t response_type;

  ret = TMR_SUCCESS;
  msg = autonomous->bufResponse;

  ret = serial_receiveMessage(autonomous, msg, 0x22, 5000);
  if ((TMR_SUCCESS != ret) && (TMR_ERROR_TAG_ID_BUFFER_FULL != ret)
    && ((TMR_ERROR_NO_TAGS_FOUND != ret)
    || ((TMR_ERROR_NO_TAGS_FOUND == ret) && (0 == msg[1]))))

  {
    autonomous->isBasetimeUpdated = false;
    return ret;
  }

  ret = (0 == GETU16AT(msg, 3)) ? TMR_SUCCESS : TMR_ERROR_CODE(GETU16AT(msg, 3));
  if ((TMR_SUCCESS == ret) && (0 == msg[1]))
  {
    /**
    * In case of streaming and ISO protocol after every search cycle
    * module sends the response for embedded operation status as
    * FF 00 22 00 00.
    * In this case return back with 0x400 error, because success response will deceive
    * the read thread to process it. For GEN2 case we got the response with 0x400 status.
    **/
    return TMR_ERROR_NO_TAGS_FOUND;
  }
  if (((0x2F == msg[2]) && (TMR_ERROR_TAG_ID_BUFFER_FULL == ret))
    ||((0x22 == msg[2]) && (TMR_ERROR_TAG_ID_BUFFER_FULL == ret)))
  {
    return ret;
  }

    if (0x2F == msg[2])
    {
      return TMR_ERROR_NO_TAGS;
    }
    else if (msg[1] < 6)
    { /* Need at least enough bytes to get to Response Type field */
      return TMR_ERROR_PARSE;
    }

    response_type_pos = (0x10 == (msg[5] & 0x10)) ? 10 : 8;

    response_type = msg[response_type_pos];
    switch (response_type)
    {
    case 0x02:
      autonomous->isStatusResponse = true;
      autonomous->bufPointer = 9;
      return TMR_SUCCESS;
    case 0x01:
      autonomous->isStatusResponse = false;
      autonomous->bufPointer = 11;
      return TMR_SUCCESS;
    case 0x00:
      if (TMR_SUCCESS == ret)
      { /* If things look good so far, signal that we are done with tags */
        return TMR_ERROR_NO_TAGS;
      }
      /* otherwise feed the error back (should only be TMR_ERROR_TAG_ID_BUFFER_FULL) */
      return ret;
    default:
      /* Unknown response type */
      return TMR_ERROR_PARSE;
    }
    return ret;
}

TMR_Status
serial_receiveMessage(AutonomousReading *autonomous, uint8_t *data, uint8_t opcode, uint32_t timeoutMs)
{
  TMR_Status ret;

  uint16_t crc, status;
  uint8_t len,headerOffset;
  uint8_t receiveBytesLen;
  uint32_t inlen;
  int i;
  TMR_SR_SerialTransport *transport;
  uint8_t retryCount = 0;

  transport = &autonomous->transport;
 
  /**
  * Initialize the receive bytes length
  **/
  receiveBytesLen = 7;

  headerOffset = 0;
retryHeader:
  inlen = 0;
  retryCount++;
  ret = transport->receiveBytes(transport, receiveBytesLen - headerOffset, &inlen, data + headerOffset, timeoutMs);
  if (TMR_SUCCESS != ret)
  {
    return ret;
  }

  if (data[0] != (uint8_t)0xFF)
  {
    for (i = 1; i < receiveBytesLen; i++) 
    {
      if (data[i] == 0xFF)
      {
        /* An SOH has been found, so we won't enter the "NO SOH" section of code again */
        headerOffset = receiveBytesLen - i;
        memmove(data, data + i, headerOffset);
        goto retryHeader;
      }
    }
    if (retryCount < 10)
    {
      /* Retry to get SOH */
      goto retryHeader;
    }
    return TMR_ERROR_TIMEOUT;
  }

  /* After this point, we have the the bare minimum (5 or 7)  of bytes in the buffer */

  /* Layout of response in data array: 
  * [0] [1] [2] [3]      [4]      [5] [6]  ... [LEN+4] [LEN+5] [LEN+6]
  * FF  LEN OP  STATUSHI STATUSLO xx  xx   ... xx      CRCHI   CRCLO
  */
  len = data[1];

  if (0 == len)
  {
    inlen = 0;
  }
  else if ((0xFF - receiveBytesLen) < len)
  {
    /**
    * packet data size is overflowing the buffer size. This could be a
    * corrupted packet. Discard it and move on. Return back with TMR_ERROR_TOO_BIG
    * error.
    **/
    return TMR_ERROR_TOO_BIG;
  }
  else
  {
    ret = transport->receiveBytes(transport, len, &inlen, data + receiveBytesLen, timeoutMs);
  }

  if (TMR_SUCCESS != ret)
  {
    /* before we can actually process the message, we have to properly receive the message */
    return ret;
  }

  /**
  * Calculate the CRC.
  **/ 
  {
    crc = tm_crc(&data[1], len + 4);
    if ((data[len + 5] != (crc >> 8)) ||
      (data[len + 6] != (crc & 0xff)))
    {
      return TMR_ERROR_CRC_ERROR;
    }
  }

  if ((data[2] != opcode) && ((data[2] != 0x2F)))
  {
    /* We got a response for a different command than the one we
    * sent. This usually means we recieved the boot-time message from
    * a M6e, and thus that the device was rebooted somewhere between
    * the previous command and this one. Report this as a problem.
    */
    return TMR_ERROR_DEVICE_RESET;
  }

  status = GETU16AT(data, 3);
  if (status != 0)
  {
    ret = TMR_ERROR_CODE(status);
    if (ret == TMR_ERROR_TM_ASSERT_FAILED)
    {
      //uint32_t line;
      uint8_t *assert = (uint8_t *) (data + 5);

      memset(autonomous->errMsg, 0 , 255);
      //line = GETU32AT(assert, 0);      
      memcpy(autonomous->errMsg + strlen(autonomous->errMsg), assert + 4, len - 4);
    }
  }  
  return ret;
}

void
serial_parseMetadataFromMessage(TMR_TagReadData *read, uint16_t flags,
                                uint8_t *i, uint8_t msg[])
{
  int msgEpcLen;

  read->metadataFlags = flags;
  read->tag.protocol = TMR_TAG_PROTOCOL_NONE;
  read->readCount = 0;
  read->rssi = 0;
  read->antenna = 0;
  read->phase = 0;
  read->frequency = 0;
  read->dspMicros = 0;
  read->timestampLow = 0;
  read->timestampHigh = 0;
  read->isAsyncRead = false;

  read->gpioCount = 4;    

  /* Fill in tag data from response */
  if (flags & TMR_TRD_METADATA_FLAG_READCOUNT)
  {
    read->readCount = GETU8(msg, *i);
  }
  if (flags & TMR_TRD_METADATA_FLAG_RSSI)
  {
    read->rssi = (int8_t)GETU8(msg, *i);
  }
  if (flags & TMR_TRD_METADATA_FLAG_ANTENNAID)
  {
    read->antenna = GETU8(msg, *i);
  }
  if (flags & TMR_TRD_METADATA_FLAG_FREQUENCY)
  {
    read->frequency = GETU24(msg, *i);
  }
  if (flags & TMR_TRD_METADATA_FLAG_TIMESTAMP)
  {
    read->dspMicros = GETU32(msg, *i);
  }
  if (flags & TMR_TRD_METADATA_FLAG_PHASE)
  {
    read->phase = GETU16(msg, *i);
  }
  if (flags & TMR_TRD_METADATA_FLAG_PROTOCOL)
  {
    read->tag.protocol = (TMR_TagProtocol)GETU8(msg, *i);
  }
  if (flags & TMR_TRD_METADATA_FLAG_DATA)
  {
    int msgDataLen, copyLen;
    
    msgDataLen = tm_u8s_per_bits(GETU16(msg, *i));
    read->data.len = msgDataLen;
    copyLen = msgDataLen;
    if (copyLen > read->data.max)
    {
      copyLen = read->data.max;
    }
	if (NULL != read->data.list)
	{
      memcpy(read->data.list, &msg[*i], copyLen);
	} 

	*i += msgDataLen;
  }
  if (flags & TMR_TRD_METADATA_FLAG_GPIO_STATUS)
  {
    int j;
    uint8_t gpioByte=GETU8(msg, *i);
    for (j=0;j<read->gpioCount;j++) 
    {
      read->gpio[j].id = j+1;
      read->gpio[j].high = (((gpioByte >> j)&0x1)== 1);
    }
  }

  msgEpcLen = tm_u8s_per_bits(GETU16(msg, *i));  
  if (TMR_TAG_PROTOCOL_ATA != read->tag.protocol)
  {
    /* ATA protocol does not have TAG CRC */
    msgEpcLen -= 2; /* Remove 2 bytes CRC*/
  }
  if (TMR_TAG_PROTOCOL_GEN2 == read->tag.protocol)
  {    
    read->tag.u.gen2.pc[0] = GETU8(msg, *i);
    read->tag.u.gen2.pc[1] = GETU8(msg, *i);
    msgEpcLen -= 2;
    read->tag.u.gen2.pcByteCount = 2;

    /* Add support for XPC bits
     * XPC_W1 is present, when the 6th most significant bit of PC word is set
     */
    if ((read->tag.u.gen2.pc[0] & 0x02) == 0x02)
    {
      /* When this bit is set, the XPC_W1 word will follow the PC word
       * Our TMR_Gen2_TagData::pc has enough space, so copying to the same.
       */
      read->tag.u.gen2.pc[2] = GETU8(msg, *i);
      read->tag.u.gen2.pc[3] = GETU8(msg, *i);
      msgEpcLen -= 2;                           /* EPC length will be length - 4(PC + XPC_W1)*/
      read->tag.u.gen2.pcByteCount += 2;        /* PC bytes are now 4*/

      if ((read->tag.u.gen2.pc[2] & 0x80) == 0x80)
      {
        /*
         * If the most siginificant bit of XPC_W1 is set, then there exists
         * XPC_W2. A total of 6  (PC + XPC_W1 + XPC_W2 bytes)
         */
        read->tag.u.gen2.pc[4] = GETU8(msg, *i);
        read->tag.u.gen2.pc[5] = GETU8(msg, *i);
        msgEpcLen -= 2;                       /* EPC length will be length - 6 (PC + XPC_W1 + XPC_W2)*/
        read->tag.u.gen2.pcByteCount += 2;    /* PC bytes are now 6 */
      }
    }    
  }
  read->tag.epcByteCount = msgEpcLen;
  if (read->tag.epcByteCount > TMR_MAX_EPC_BYTE_COUNT)
  {
    read->tag.epcByteCount = TMR_MAX_EPC_BYTE_COUNT;
  }

  memcpy(read->tag.epc, &msg[*i], read->tag.epcByteCount);
  *i += msgEpcLen;
  read->tag.crc = GETU16(msg, *i);
  read->isAsyncRead = true;

}

void
serial_postprocessReaderSpecificMetadata(struct AutonomousReading *autonomous, TMR_TagReadData *read)
{
  uint32_t timestampLow;
  uint64_t currTime64, lastSentTagTime64; /*for comparison*/
  int32_t tempDiff;

  timestampLow = autonomous->readTimeLow;
  read->timestampHigh = autonomous->readTimeHigh;

  timestampLow = timestampLow + read->dspMicros;
  currTime64 = ((uint64_t)read->timestampHigh << 32) | timestampLow;
  lastSentTagTime64 = ((uint64_t)autonomous->lastSentTagTimestampHigh << 32) | autonomous->lastSentTagTimestampLow;
  if (lastSentTagTime64 >= currTime64)
  {
    tempDiff = (int32_t)(currTime64 - lastSentTagTime64);
    timestampLow = timestampLow - tempDiff + 1;
    if (timestampLow < autonomous->lastSentTagTimestampLow) /*account for overflow*/
    {
      read->timestampHigh++;
    }
  }
  if (timestampLow < autonomous->readTimeLow) /* Overflow */
  {
    read->timestampHigh++;
  }
  read->timestampLow = timestampLow;
  autonomous->lastSentTagTimestampHigh = read->timestampHigh;
  autonomous->lastSentTagTimestampLow = read->timestampLow;

  {
    uint8_t tx;
    uint8_t rx;
    tx = (read->antenna >> 4) & 0xF;
    rx = (read->antenna >> 0) & 0xF;

    // Due to limited space, Antenna 16 wraps around to 0
    if (0 == tx) { tx = 16; }
    if (0 == rx) { rx = 16; }
    if (tx == rx)
    {
      read->antenna = tx;
    }   
  }
}

TMR_Status
TMR_TRD_init(TMR_TagReadData *trd)
{
  trd->tag.protocol = TMR_TAG_PROTOCOL_NONE;
  trd->tag.epcByteCount = 0;
  trd->tag.crc = 0;
  trd->metadataFlags = 0;
  trd->phase = 0;
  memset(trd->gpio, 0, sizeof(trd->gpio));
  trd->gpioCount = 0;
  trd->readCount = 0;
  trd->rssi = 0;
  trd->frequency = 0;
  trd->dspMicros = 0;
  trd->timestampLow = 0;
  trd->timestampHigh = 0;

#if TMR_MAX_EMBEDDED_DATA_LENGTH
  trd->data.list = trd->_dataList;
  trd->epcMemData.list = trd->_epcMemDataList;
  trd->tidMemData.list = trd->_tidMemDataList;
  trd->userMemData.list = trd->_userMemDataList;
  trd->reservedMemData.list = trd->_reservedMemDataList;

  trd->data.max = TMR_MAX_EMBEDDED_DATA_LENGTH;
  trd->epcMemData.max = TMR_MAX_EMBEDDED_DATA_LENGTH;
  trd->userMemData.max = TMR_MAX_EMBEDDED_DATA_LENGTH;
  trd->reservedMemData.max = TMR_MAX_EMBEDDED_DATA_LENGTH;
  trd->tidMemData.max = TMR_MAX_EMBEDDED_DATA_LENGTH;
#else
  trd->data.list = NULL;
  trd->epcMemData.list = NULL;
  trd->userMemData.list = NULL;
  trd->tidMemData.list = NULL;
  trd->reservedMemData.list = NULL;

  trd->data.max = 0;
  trd->epcMemData.max = 0;
  trd->userMemData.max = 0;
  trd->reservedMemData.max = 0;
  trd->tidMemData.max = 0;
#endif
  trd->data.len = 0;
  trd->epcMemData.len = 0;
  trd->userMemData.len = 0;
  trd->tidMemData.len = 0;
  trd->reservedMemData.len = 0;

  return TMR_SUCCESS;
}
