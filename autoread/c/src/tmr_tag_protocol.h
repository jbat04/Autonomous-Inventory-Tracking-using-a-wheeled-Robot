/* ex: set tabstop=2 shiftwidth=2 expandtab cindent: */
#ifndef _TMR_TAG_PROTOCOL_H
#define _TMR_TAG_PROTOCOL_H
/**
 *  @file tmr_tag_protocol.h
 *  @brief Mercury API - Tag Protocols
 *  @author Brian Fiegel
 *  @date 4/18/2009
 */

/*
 * Copyright (c) 2009 ThingMagic, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifdef  __cplusplus
extern "C" {
#endif


/**
 * RFID Tag Protocols
 */
typedef enum TMR_TagProtocol 
{
  TMR_TAG_PROTOCOL_NONE              = 0x00,
  TMR_TAG_PROTOCOL_ISO180006B        = 0x03,
  TMR_TAG_PROTOCOL_GEN2              = 0x05,
  TMR_TAG_PROTOCOL_ISO180006B_UCODE  = 0x06, 
  TMR_TAG_PROTOCOL_IPX64             = 0x07,
  TMR_TAG_PROTOCOL_IPX256            = 0x08,
	TMR_TAG_PROTOCOL_ATA               = 0x1D,
} TMR_TagProtocol;

/** A list of TMR_Protocol values */
typedef struct TMR_TagProtocolList
{
  /** The array of values */
  TMR_TagProtocol *list;
  /** The number of entries there is space for in the array */
  uint8_t max;
  /** The number of entries in the list - may be larger than max, indicating truncated data. */
  uint8_t len;
} TMR_TagProtocolList;

#ifdef  __cplusplus
}
#endif


#endif /* _TMR_TAG_PROTOCL_H_ */
