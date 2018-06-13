using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Management;
using ThingMagic;
using System.IO.Ports;
using System.IO;
using System.Collections;
using System.Threading;
using System.Text.RegularExpressions;

namespace ReceiveAutonomousReading
{
    class ReceiveAutonomousReading
    {
        static void Main(string[] args)
        {
            ReceiveTagData rt = new ReceiveTagData();
            //Connect to Serial port
            rt.Connect();
            // Create and add tag listener
            rt.TagRead += delegate(Object sender, TagReadDataEventArgs e)
            {
                Console.WriteLine("Background read: " + e.TagReadData);
                if (0 < e.TagReadData.Data.Length)
                {
                    Console.WriteLine("  Data:" + ByteFormat.ToHex(e.TagReadData.Data, "", " "));
                } 
            };
            rt.StatsListener += delegate(object sender, StatsReportEventArgs e)
            {
                Console.WriteLine(e.StatsReport.ToString());
            };
            //Start receiving data 
            rt.ReceiveData();
        }
    }
    class ReceiveTagData
    {
        private static SerialPort _serialPort = new SerialPort();
        private DateTime baseTime;
        public static int _backgroundNotifierCallbackCount = 0;
        public static Object _backgroundNotifierLock = new Object();
        public event EventHandler<TagReadDataEventArgs> TagRead;
        /// <summary>
        /// Occurs when reader status parsing in continuous read 
        /// </summary>
        public event EventHandler<StatsReportEventArgs> StatsListener;
      

        #region Connect

        public void Connect()
        {
            try
            {
                bool isConnected = false;
                Console.WriteLine("Trying to connect...");
                baseTime = DateTime.MinValue;
                while (!isConnected)
                {
                    List<string> portNames = GetComPortNames();
                    foreach (string port in portNames)
                    {
                        _serialPort = new SerialPort();
                        _serialPort.PortName = port;
                        _serialPort.WriteTimeout = 1000;
                        _serialPort.ReadTimeout = 1000;
                        _serialPort.DtrEnable = true;
                        int[] bps = { 115200, 9600, 921600, 19200, 38400, 57600, 230400, 460800 };
                        foreach (int baudRate in bps)
                        {
                            try
                            {
                                _serialPort.BaudRate = baudRate;
                                _serialPort.Open();
                                _serialPort.DiscardInBuffer();
                                _serialPort.DiscardOutBuffer();
                                byte[] response = new byte[256];
                                response = receiveMessage(0x22, 1000);
                            }
                            catch (Exception)
                            {
                                _serialPort.Close();
                                continue;
                            }
                            isConnected = true;
                            break;
                        }
                    }
                }
                Console.WriteLine("Connection to the module is successful");
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
            }
        }

        #endregion

        #region ReceiveData
        public void ReceiveData()
        {
            ReceiveResponseStream();
        }
        #endregion

        #region receiveMessage
        /// <summary>
        /// Receive message: the receiving part in "SendTimeoutUnchecked".
        /// </summary>
        /// <param name="opcode">opcode</param>
        /// <param name="timeout">Command timeout -- how long we expect the command itself to take (milliseconds)</param>
        /// <returns>M5e response as byte array, including framing (SOH, length, status, CRC)</returns>
        private byte[] receiveMessage(byte opcode, int timeout)
        {

            byte[] inputBuffer = new byte[256];
            int ibuf = 0;
            int responseLength = 0;
            int sohPosition = 0;
            int messageLength = 0;
            int headerOffset = 0;
            int retryCount = 0;
            try
            {
                _serialPort.WriteTimeout = timeout;
                _serialPort.ReadTimeout = timeout;

                messageLength = 7;

            retryHeader:
                retryCount++;
                //pull at least 7 bytes on first serial receive
                ibuf += ReadAll(_serialPort, inputBuffer, ibuf, messageLength - headerOffset);

                // TODO: Should we keep looking for another 0xFF?
                //Check if 0xFF is on the first byte of response
                if (0xFF != inputBuffer[0])
                {
                    int i = 0;
                    bool sohFound = false;
                    for (i = 1; i < messageLength; i++)
                    {
                        if (inputBuffer[i] == 0xFF)
                        {
                            sohPosition = i;
                            sohFound = true;
                            headerOffset = messageLength - sohPosition;
                            Array.Copy(inputBuffer, sohPosition, inputBuffer, 0, headerOffset);
                            ibuf -= sohPosition;
                            goto retryHeader;
                        }
                    }
                    if (retryCount < 10)
                    {
                        ibuf = 0;
                        /* Retry to get SOH */
                        goto retryHeader;
                    }
                    if (sohFound == false)
                    {
                        throw new Exception(String.Format("Invalid M6e response header, SOH not found in response."));
                    }
                }

                /*if no bytes are read, return null*/
                if (ibuf == 0)
                {
                    return null;
                }

                byte argLength = inputBuffer[1];

                // Total response = SOH, length, opcode, status, CRC
                responseLength = argLength + 1 + 1 + 1 + 2 + 2;

                //Now pull in the rest of the data, if exists, + the CRC
                if (argLength != 0)
                {
                    ibuf += ReadAll(_serialPort, inputBuffer, ibuf, argLength);
                }
            }
            catch (Exception ex)
            {
                throw ex;
            }

            byte[] response = null;
            UInt16 status = 0;
            // Check for return message CRC
            if (responseLength > 0)
            {
                byte[] returnCRC = new byte[2];

                if (true)
                {
                    byte[] inputBufferNoCRC = new byte[(responseLength - 2)];
                    Array.Copy(inputBuffer, 0, inputBufferNoCRC, 0, (responseLength - 2));
                    try
                    {
                        returnCRC = CalcReturnCRC(inputBufferNoCRC);
                    }
                    catch { };
                    if (ByteConv.ToU16(inputBuffer, (responseLength - 2)) != ByteConv.ToU16(returnCRC, 0))
                    {
                        throw new Exception("CRC Error");
                    }
                }
                response = new byte[responseLength];
                Array.Copy(inputBuffer, response, responseLength);

                /* We got a response for a different command than the one we
                 * sent. This usually means we recieved the boot-time message from
                 * a M6e, and thus that the device was rebooted somewhere between
                 * the previous command and this one. Report this as a problem.
                 */
                if ((response[2] != opcode) && (response[2] != 0x2F))
                {
                    throw new Exception(String.Format("Device was reset externally. " +
                        "Response opcode {0:X2} did not match command opcode {1:X2}. ", response[2], 0x22));
                }

                status = ByteConv.ToU16(response, 3);
            }

            try
            {
                //Don't catch TMR_ERROR_NO_TAGS_FOUND for ReadTagMultiple.
                if (status != FAULT_NO_TAGS_FOUND_Exception.StatusCode)
                {
                    GetError(status);
                }
            }
            catch (FAULT_TM_ASSERT_FAILED_Exception)
            {
                throw new FAULT_TM_ASSERT_FAILED_Exception(String.Format("Reader assert 0x{0:X} at {1}:{2}", status,
                    ByteConv.ToAscii(response, 9, response.Length - 9), ByteConv.ToU32(response, 5)));
            }
            catch (FAULT_TAG_ID_BUFFER_AUTH_REQUEST_Exception)
            {
                throw new FAULT_TAG_ID_BUFFER_AUTH_REQUEST_Exception(response);
            }

            return response;
        }
        #endregion

        #region ReceiveResponseStream
        private void ReceiveResponseStream()
        {
            bool keepReceiving = true;
            byte[] response = default(byte[]);
            while (keepReceiving)
            {
                try
                {
                    if (baseTime == DateTime.MinValue)
                    {
                        //Update the base time with host time at the time start receiving data.
                        baseTime = DateTime.Now;
                    }
                    response = receiveMessage((byte)0x22, 5000);
                    // Detect end of stream
                    if ((response != null) && (response[2] == 0x2f))
                    {
                        baseTime = DateTime.MinValue;
                        //do nothing
                    }
                    else
                    {
                        ProcessStreamingResponse(response);
                    }
                }
                catch (Exception ex)
                {
                    if (ex is TimeoutException || ex is IOException || ex.Message.Equals("The port is closed."))
                    {
                        Console.WriteLine(ex.Message);
                        try
                        {
                            _serialPort.Close();
                        }
                        catch (Exception) { };
                        Connect();
                    }
                    else if (((ex is FAULT_NO_TAGS_FOUND_Exception) || (ex is FAULT_TAG_ID_BUFFER_AUTH_REQUEST_Exception)))
                    {
                        Console.WriteLine(ex.Message);
                    }
                    else if (ex is FAULT_TAG_ID_BUFFER_AUTH_REQUEST_Exception)
                    {
                        Console.WriteLine("Tag Authentication message:" + ex.Message);
                    }
                    else if (ex is FAULT_TAG_ID_BUFFER_FULL_Exception)
                    {
                        Console.WriteLine("Tag ID buffer full:" + ex.Message);
                    }
                    else if ((ex is FAULT_AHAL_HIGH_RETURN_LOSS_Exception)
                        || (ex is FAULT_AHAL_TEMPERATURE_EXCEED_LIMITS_Exception)
                        )
                    {
                        // Continue
                        keepReceiving = true;
                    }
                    else
                    {
                        Console.WriteLine(ex.Message);
                    }
                }
            }
        }
        #endregion

        #region ProcessStreamingResponse

        private void ProcessStreamingResponse(byte[] response)
        {
            try
            {
                if (null != response)
                {
                    // To avoid index was outside the bounds of the array error.
                    if (response.Length >= 8)
                    {
                        byte responseType = response[((response[5] & (byte)0x10) == 0x10) ? 10 : 8];

                        BoolResponse br = new BoolResponse();
                        ParseResponseByte(responseType, br);
                        if (br.parseResponse)
                        {
                            if (br.statusResponse)
                            {   
                                    // Requested for reader stats
                                    int offSet = 5 + 4;
                                    var statusFlags = response[offSet++] << 8 | response[offSet++];

                                    ReaderStatsReport statusreport = new ReaderStatsReport();
                                    
                                    byte[] tempResponse = new byte[response.Length - 2];
                                    Array.Copy(response, 0, tempResponse, 0, response.Length - 2);
                                    statusreport.STATS = ParseReaderStatValues(tempResponse, offSet, Convert.ToUInt16(statusFlags));
                                   
                                    OnStatsRead(statusreport);
                                br.statusResponse = false;
                            }
                            else
                            {
                                TagReadData t = new TagReadData();
                                t._baseTime = baseTime;
                                int readOffset = 5 + 6; //start of metadata response[11]
                                TagProtocol protocol = TagProtocol.NONE;
                                ParseTagMetadata(ref t, response, ref readOffset, TagMetadataFlag.ALL, ref protocol);
                                t._tagData = ParseTagData(response, ref readOffset, protocol, 0);
                                // Handling NegativeArraySizeException
                                // Ignoring invalid tag response (epcLen goes to negative), which disturbs further parsing of tagresponse
                                if (t._tagData._crc != null)
                                {
                                    QueueTagReads(new TagReadData[] { t });
                                }
                            }
                        }
                    }
                    else
                    {
                        // Update base time with host time on each read cycle
                        baseTime = DateTime.Now;
                        //Don't catch TMR_ERROR_NO_TAGS_FOUND for ReadTagMultiple.
                        if (ByteConv.ToU16(response, 3) != FAULT_NO_TAGS_FOUND_Exception.StatusCode)
                        {
                            // In case of streaming and ISO protocol after every
                            // search cycle module sends the response for 
                            // embedded operation status as FF 00 22 00 00. In
                            // this case return back with 0x400 error, because 
                            // success response will deceive the read thread 
                            // to process it. For GEN2 case we got the response
                            // with 0x400 status.

                            if (ByteConv.ToU16(response, 3) == 0 && response[1] == 0)
                            {
                                //do nothing
                            }
                            else
                            {
                                Console.WriteLine("Unable to parse data");
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
            }
        }

        #endregion

        #region BoolResponse

        /// <summary>
        /// Class for wheather to parse the status or response
        /// </summary>
        private class BoolResponse
        {
            internal bool parseResponse = true;
            internal bool statusResponse = false;
        }

        #endregion

        #region ParseResponseByte
        /// <summary>
        /// Internal method to parse response byte in the 22h command response
        /// </summary>
        /// <param name="responseByte">responseByte</param>
        /// <param name="br"></param>
        /// <returns>true/false</returns>
        private bool ParseResponseByte(byte responseByte, BoolResponse br)
        {
            bool response = true;
            switch (responseByte)
            {
                case 0x02:
                    //mid stream status response
                    br.statusResponse = true;
                    break;
                case 0x01:
                    // mid stream tag buffer response
                    break;
                case 0x00:
                    // final stream response
                    br.parseResponse = false;
                    break;
            }
            return response;
        }

        #endregion

        #region ParseReaderStatValues

        /// <summary>
        /// Fill reader stat values from response
        /// </summary>
        /// <param name="response"></param>
        /// <param name="offset"></param>
        /// <param name="flag"></param>
        /// <returns></returns>
        private Stat.Values ParseReaderStatValues(byte[] response, int offset, UInt16 flag)
        {
            Stat.Values values = new Stat.Values();
            while (offset < (response.Length))
            {
                if (0x82 != response[offset])
                {
                    offset++;
                    continue;                    
                }
                if ((0x80) > response[offset])
                {
                    flag = response[offset];
                }
                else
                {
                    // the response flag will be in EBV format,
                    // convert that to the api enum values
                    flag = (ushort)((response[offset] << 8) | (response[offset + 1]));
                    flag &= 0x7fff;
                    flag = (ushort)((flag >> 1) | (flag & 0x7f));
                   
                }
                Stat.StatsFlag tempStatFlag = (Stat.StatsFlag)flag;
                if ((tempStatFlag & Stat.StatsFlag.TEMPERATURE) != 0)
                {
                    // Response flag is in EBV format, skip two bytes to get length
                    offset += 2;
                    int length = response[offset++];
                    values.TEMPERATURE = (uint)ByteConv.GetU8(response, offset);
                    offset += length;
                }
            }// End of while loop
            values.VALID = Stat.StatsFlag.TEMPERATURE;
            return values;
        }

        #endregion

        #region OnStatsRead
        /// <summary>
        /// Reader Stats message event
        /// </summary>
        /// <param name="sReport">array of status reports</param>
        protected void OnStatsRead(ReaderStatsReport sReport)
        {
            if (null != StatsListener)
            {
                StatsListener(this, new StatsReportEventArgs(sReport));
            }
        }

        #endregion

        #region ParseTagData

        /// <summary>
        /// Parse one tag data record out of GetTagBuffer response.
        /// Input Format: lenhi lenlo data... [pad...]
        /// </summary>
        /// <param name="response">GetTagBuffer response</param>
        /// <param name="readOffset">Index of start of record.  Will be updated to point after record.</param>
        /// <param name="protocol"></param>
        /// <param name="reclen">Length of record, in bytes, not including length field.
        /// e.g., 16 for 16-bit PC + 96-bit EPC + 16-bit CRC
        /// e.g., 66 for 16-bit PC + 496-bit EPC + 16-bit CRC
        /// Specify 0 for variable-length records.</param>
        /// <returns></returns>

        private static TagData ParseTagData(byte[] response, ref int readOffset, TagProtocol protocol, int reclen)
        {
            int idlenbits = ByteConv.ToU16(response, ref readOffset);
            int datastartOffset = readOffset;
            List<byte> pcList = new List<byte>();

            if (0 != (idlenbits % 8)) throw new Exception("EPC length not a multiple of 8 bits");
            int idlen = idlenbits / 8;
            TagData td = null;
            int crclen = 2;
            int epclen = idlen;
            if (!(TagProtocol.ATA == protocol))
            {
                epclen = idlen - crclen;
            }

            if (TagProtocol.GEN2 == protocol)
            {
                int pclen = 2;
                pcList.AddRange(SubArray(response, ref readOffset, pclen));
                epclen -= pclen;
                /* Add support for XPC bits
                 * XPC_W1 is present, when the 6th most significant bit of PC word is set
                 */
                if ((pcList[0] & 0x02) == 0x02)
                {
                    /* When this bit is set, the XPC_W1 word will follow the PC word
                     * Our TMR_Gen2_TagData::pc has enough space, so copying to the same.
                     */
                    if (readOffset < response.Length)
                    {
                        byte[] xpcW1 = SubArray(response, ref readOffset, 2);
                        pcList.AddRange(xpcW1);
                        pclen += 2;  /* PC bytes are now 4*/
                        epclen -= 2; /* EPC length will be length - 4(PC + XPC_W1)*/
                    }
                    /*
                     * If the most siginificant bit of XPC_W1 is set, then there exists
                     * XPC_W2. A total of 6  (PC + XPC_W1 + XPC_W2 bytes)
                     */
                    if ((pcList[2] & 0x80) == 0x80)
                    {
                        if (readOffset < response.Length)
                        {
                            byte[] xpcW2 = SubArray(response, ref readOffset, 2);
                            pcList.AddRange(xpcW2);
                            pclen += 2;  /* PC bytes are now 6 */
                            epclen -= 2; /* EPC length will be length - 6 (PC + XPC_W1 + XPC_W2)*/
                        }
                    }
                }
            }
            byte[] pc = pcList.ToArray();
            byte[] epc, crc;
            try
            {
                // Handling NegativeArraySizeException
                // Ignoring invalid tag response (epcLen goes to negative), which disturbs further parsing of tagresponse
                // example below
                // 0xFFDC29000001FF000714E3110E0CAE0000001200890500000C00300800DEADAC930
                // 6DA110E0CAE00000016007E0500000C0080300052DF9DFE14F5B6F0010100707F4F
                // *** Corrupt data in tag response(Looks like PC word in some of these tags got corrupted with XPC bit)
                // 01 A6 11 0E0CAE 00000019 005F 05 0000 0C 0020 06206C34
                // ***
                // 05D4110E0CAE0000001A00890500000C00803400DEADCAFEDEADCAFEDEADCAFE80691
                // 2CF110E0CAE00000022008C0500000C0080300043215678DEADCAFEDEADCAFEFDB11D
                // DF110E0CAE0000002300A00500000C00803000300833B2DDD901400000000039BB0BE
                // 3110E0CAE0000002C00970500000C00803000DEADBEEFDEADBEEFDEADBEEF9C1E
                // Even if EPC length is 4 bytes and PC word is corrupted with XPC bit
                if (epclen > -1)
                {
                    epc = SubArray(response, ref readOffset, epclen);
                }
                else
                {
                    epc = null;
                }
                if ((crclen > -1) && (epclen > -1))
                {
                    crc = SubArray(response, ref readOffset, crclen);
                }
                else
                {
                    crc = null;
                    if (TagProtocol.GEN2 == protocol)
                    {
                        // if PC word got corrupted with XPC bit i.e XPC_W1 and XPC_W2
                        // if pc.Length = 4, PC word is followed by XPC_W1.
                        // if pc.Length = 6 , PC word is followed by XPC_W1 and XPC_W2
                        if (epclen < -2)
                        {
                            readOffset = readOffset - (pc.Length + (epclen));
                        }
                    }
                }

                switch (protocol)
                {
                    case TagProtocol.GEN2:
                        td = new Gen2.TagData(epc, crc, pc);
                        break;
                }
            }
            catch (Exception ex)
            {
                throw ex;
            }
            if (0 < reclen) { readOffset = datastartOffset + reclen; }
            return td;
        }

        #endregion

        #region QueueTagReads
        /// <summary>
        /// Submit tag reads for read listener background processing
        /// </summary>
        /// <param name="reads">List of tag reads</param>
        protected void QueueTagReads(ICollection<TagReadData> reads)
        {
            lock (_backgroundNotifierLock)
                _backgroundNotifierCallbackCount++;

            TagReadCallback callback = new TagReadCallback(reads);

            ThreadPool.QueueUserWorkItem(new WaitCallback(ThreadPoolCallBack), callback);
        }
        #endregion

        #region TagReadCallback

        /// <summary>
        /// ThreadPool-compatible wrapper for servicing asynchronous reads
        /// </summary>
        private sealed class TagReadCallback
        {
            #region Fields

            public ICollection<TagReadData> _reads;

            #endregion

            #region Construction

            /// <summary>
            /// Create ThreadPool-compatible wrapper
            /// </summary>
            /// <param name="rdr">Reader object that will service TagReadData event</param>
            /// <param name="reads">TagReadData event to servic e</param>

            public TagReadCallback(ICollection<TagReadData> reads)
            {
                _reads = reads;
            }

            #endregion
        }
        #endregion

        #region ThreadPoolCallBack

        /// <summary>
        /// ThreadPool-compatible callback to be passed to ThreadPool.QueueUserWorkItem
        /// </summary>
        /// <param name="threadContext">Identifier of thread that is servicing this callback</param>
        public void ThreadPoolCallBack(Object threadContext)
        {
            TagReadCallback trc = (TagReadCallback)threadContext;
            try
            {
                foreach (TagReadData read in trc._reads)
                {

                    OnTagRead(read);
                }
            }
            catch (Exception)
            {
                //do nothing
            }
            finally
            {
                lock (_backgroundNotifierLock)
                    _backgroundNotifierCallbackCount--;

            }
        }

        #endregion

        #region OnTagRead

        /// <summary>
        /// Internal accessor to TagRead event.
        /// Called by members of the Reader class to fire a TagRead event.
        /// </summary>
        /// <param name="tagReadData">Data from a single tag read</param>
        protected void OnTagRead(TagReadData tagReadData)
        {
            if (null != TagRead)
                TagRead(this, new TagReadDataEventArgs(tagReadData));
        }

        #endregion

        #region GetError

        private static void GetError(UInt16 error)
        {
            switch (error)
            {
                case 0:
                    break;

                case FAULT_MSG_WRONG_NUMBER_OF_DATA_Exception.StatusCode:
                    throw new FAULT_MSG_WRONG_NUMBER_OF_DATA_Exception();
                case FAULT_INVALID_OPCODE_Exception.StatusCode:
                    throw new FAULT_INVALID_OPCODE_Exception();
                case FAULT_UNIMPLEMENTED_OPCODE_Exception.StatusCode:
                    throw new FAULT_UNIMPLEMENTED_OPCODE_Exception();
                case FAULT_MSG_POWER_TOO_HIGH_Exception.StatusCode:
                    throw new FAULT_MSG_POWER_TOO_HIGH_Exception();
                case FAULT_MSG_INVALID_FREQ_RECEIVED_Exception.StatusCode:
                    throw new FAULT_MSG_INVALID_FREQ_RECEIVED_Exception();
                case FAULT_MSG_INVALID_PARAMETER_VALUE_Exception.StatusCode:
                    throw new FAULT_MSG_INVALID_PARAMETER_VALUE_Exception();

                case FAULT_MSG_POWER_TOO_LOW_Exception.StatusCode:
                    throw new FAULT_MSG_POWER_TOO_LOW_Exception();

                case FAULT_UNIMPLEMENTED_FEATURE_Exception.StatusCode:
                    throw new FAULT_UNIMPLEMENTED_FEATURE_Exception();
                case FAULT_INVALID_BAUD_RATE_Exception.StatusCode:
                    throw new FAULT_INVALID_BAUD_RATE_Exception();

                case FAULT_INVALID_REGION_Exception.StatusCode:
                    throw new FAULT_INVALID_REGION_Exception();

                case FAULT_INVALID_LICENSE_KEY_Exception.StatusCode:
                    throw new FAULT_INVALID_LICENSE_KEY_Exception();

                case FAULT_BL_INVALID_IMAGE_CRC_Exception.StatusCode:
                    throw new FAULT_BL_INVALID_IMAGE_CRC_Exception();

                case FAULT_NO_TAGS_FOUND_Exception.StatusCode:
                    throw new FAULT_NO_TAGS_FOUND_Exception();
                case FAULT_NO_PROTOCOL_DEFINED_Exception.StatusCode:
                    throw new FAULT_NO_PROTOCOL_DEFINED_Exception();
                case FAULT_INVALID_PROTOCOL_SPECIFIED_Exception.StatusCode:
                    throw new FAULT_INVALID_PROTOCOL_SPECIFIED_Exception();
                case FAULT_WRITE_PASSED_LOCK_FAILED_Exception.StatusCode:
                    throw new FAULT_WRITE_PASSED_LOCK_FAILED_Exception();
                case FAULT_PROTOCOL_NO_DATA_READ_Exception.StatusCode:
                    throw new FAULT_PROTOCOL_NO_DATA_READ_Exception();
                case FAULT_AFE_NOT_ON_Exception.StatusCode:
                    throw new FAULT_AFE_NOT_ON_Exception();
                case FAULT_PROTOCOL_WRITE_FAILED_Exception.StatusCode:
                    throw new FAULT_PROTOCOL_WRITE_FAILED_Exception();
                case FAULT_NOT_IMPLEMENTED_FOR_THIS_PROTOCOL_Exception.StatusCode:
                    throw new FAULT_NOT_IMPLEMENTED_FOR_THIS_PROTOCOL_Exception();
                case FAULT_PROTOCOL_INVALID_WRITE_DATA_Exception.StatusCode:
                    throw new FAULT_PROTOCOL_INVALID_WRITE_DATA_Exception();
                case FAULT_PROTOCOL_INVALID_ADDRESS_Exception.StatusCode:
                    throw new FAULT_PROTOCOL_INVALID_ADDRESS_Exception();
                case FAULT_GENERAL_TAG_ERROR_Exception.StatusCode:
                    throw new FAULT_GENERAL_TAG_ERROR_Exception();
                case FAULT_DATA_TOO_LARGE_Exception.StatusCode:
                    throw new FAULT_DATA_TOO_LARGE_Exception();
                case FAULT_PROTOCOL_INVALID_KILL_PASSWORD_Exception.StatusCode:
                    throw new FAULT_PROTOCOL_INVALID_KILL_PASSWORD_Exception();
                case FAULT_PROTOCOL_KILL_FAILED_Exception.StatusCode:
                    throw new FAULT_PROTOCOL_KILL_FAILED_Exception();

                case FAULT_AHAL_ANTENNA_NOT_CONNECTED_Exception.StatusCode:
                    throw new FAULT_AHAL_ANTENNA_NOT_CONNECTED_Exception();
                case FAULT_AHAL_TEMPERATURE_EXCEED_LIMITS_Exception.StatusCode:
                    throw new FAULT_AHAL_TEMPERATURE_EXCEED_LIMITS_Exception();
                case FAULT_AHAL_HIGH_RETURN_LOSS_Exception.StatusCode:
                    throw new FAULT_AHAL_HIGH_RETURN_LOSS_Exception();

                case FAULT_TAG_ID_BUFFER_NOT_ENOUGH_TAGS_AVAILABLE_Exception.StatusCode:
                    throw new FAULT_TAG_ID_BUFFER_NOT_ENOUGH_TAGS_AVAILABLE_Exception();
                case FAULT_TAG_ID_BUFFER_FULL_Exception.StatusCode:
                    throw new FAULT_TAG_ID_BUFFER_FULL_Exception();
                case FAULT_TAG_ID_BUFFER_REPEATED_TAG_ID_Exception.StatusCode:
                    throw new FAULT_TAG_ID_BUFFER_REPEATED_TAG_ID_Exception();
                case FAULT_TAG_ID_BUFFER_NUM_TAG_TOO_LARGE_Exception.StatusCode:
                    throw new FAULT_TAG_ID_BUFFER_NUM_TAG_TOO_LARGE_Exception();
                case FAULT_TAG_ID_BUFFER_AUTH_REQUEST_Exception.StatusCode:
                    throw new FAULT_TAG_ID_BUFFER_AUTH_REQUEST_Exception();

                case FAULT_SYSTEM_UNKNOWN_ERROR_Exception.StatusCode:
                    throw new FAULT_SYSTEM_UNKNOWN_ERROR_Exception();
                case FAULT_FLASH_BAD_ERASE_PASSWORD_Exception.StatusCode:
                    throw new FAULT_FLASH_BAD_ERASE_PASSWORD_Exception();
                case FAULT_FLASH_BAD_WRITE_PASSWORD_Exception.StatusCode:
                    throw new FAULT_FLASH_BAD_WRITE_PASSWORD_Exception();
                case FAULT_FLASH_UNDEFINED_ERROR_Exception.StatusCode:
                    throw new FAULT_FLASH_UNDEFINED_ERROR_Exception();
                case FAULT_FLASH_ILLEGAL_SECTOR_Exception.StatusCode:
                    throw new FAULT_FLASH_ILLEGAL_SECTOR_Exception();
                case FAULT_FLASH_WRITE_TO_NON_ERASED_AREA_Exception.StatusCode:
                    throw new FAULT_FLASH_WRITE_TO_NON_ERASED_AREA_Exception();
                case FAULT_FLASH_WRITE_TO_ILLEGAL_SECTOR_Exception.StatusCode:
                    throw new FAULT_FLASH_WRITE_TO_ILLEGAL_SECTOR_Exception();
                case FAULT_FLASH_VERIFY_FAILED_Exception.StatusCode:
                    throw new FAULT_FLASH_VERIFY_FAILED_Exception();
                case FAULT_GEN2_PROTOCOL_OTHER_ERROR_Exception.StatusCode:
                    throw new FAULT_GEN2_PROTOCOL_OTHER_ERROR_Exception();
                case FAULT_GEN2_PROTOCOL_MEMORY_OVERRUN_BAD_PC_Exception.StatusCode:
                    throw new FAULT_GEN2_PROTOCOL_MEMORY_OVERRUN_BAD_PC_Exception();
                case FAULT_GEN2_PROTOCOL_MEMORY_LOCKED_Exception.StatusCode:
                    throw new FAULT_GEN2_PROTOCOL_MEMORY_LOCKED_Exception();
                case FAULT_GEN2_PROTOCOL_INSUFFICIENT_POWER_Exception.StatusCode:
                    throw new FAULT_GEN2_PROTOCOL_INSUFFICIENT_POWER_Exception();
                case FAULT_GEN2_PROTOCOL_NON_SPECIFIC_ERROR_Exception.StatusCode:
                    throw new FAULT_GEN2_PROTOCOL_NON_SPECIFIC_ERROR_Exception();
                case FAULT_GEN2_PROTOCOL_UNKNOWN_ERROR_Exception.StatusCode:
                    throw new FAULT_GEN2_PROTOCOL_UNKNOWN_ERROR_Exception();
                case FAULT_AHAL_INVALID_FREQ_Exception.StatusCode:
                    throw new FAULT_AHAL_INVALID_FREQ_Exception();
                case FAULT_AHAL_CHANNEL_OCCUPIED_Exception.StatusCode:
                    throw new FAULT_AHAL_CHANNEL_OCCUPIED_Exception();
                case FAULT_AHAL_TRANSMITTER_ON_Exception.StatusCode:
                    throw new FAULT_AHAL_TRANSMITTER_ON_Exception();
                case FAULT_TM_ASSERT_FAILED_Exception.StatusCode:
                    throw new FAULT_TM_ASSERT_FAILED_Exception("");
                case FAULT_PROTOCOL_BIT_DECODING_FAILED_Exception.StatusCode:
                    throw new FAULT_PROTOCOL_BIT_DECODING_FAILED_Exception();
                case FAULT_PROTOCOL_INVALID_NUM_DATA_Exception.StatusCode:
                    throw new FAULT_PROTOCOL_INVALID_NUM_DATA_Exception();
                case FAULT_PROTOCOL_INVALID_EPC_Exception.StatusCode:
                    throw new FAULT_PROTOCOL_INVALID_EPC_Exception();
                default:
                    throw new M5eStatusException(error);
            }
        }

        #endregion

        #region ReadAll

        private static int ReadAll(SerialPort ser, byte[] buffer, int offset, int count)
        {
            int bytesReceived = 0;
            int exitLoop = 0;
            int iterations = 0;

            try
            {
                while (bytesReceived < count && exitLoop == 0)
                {
                    iterations += 1;
                    bytesReceived += ser.Read(buffer, offset + bytesReceived, count - bytesReceived);
                }
            }
            catch (Exception ex)
            {
                throw ex;
            }
            return bytesReceived;
        }
        #endregion

        #region ParseTagMetadata

        private void ParseTagMetadata(ref TagReadData t, byte[] response, ref int readOffset, TagMetadataFlag metadataFlags, ref TagProtocol protocol)
        {
            const TagMetadataFlag understoodFlags = 0
              | TagMetadataFlag.READCOUNT
              | TagMetadataFlag.RSSI
              | TagMetadataFlag.ANTENNAID
              | TagMetadataFlag.FREQUENCY
              | TagMetadataFlag.TIMESTAMP
              | TagMetadataFlag.PHASE
              | TagMetadataFlag.PROTOCOL
              | TagMetadataFlag.DATA
              | TagMetadataFlag.GPIO
              ;
            if (metadataFlags != (metadataFlags & understoodFlags))
            {
                TagMetadataFlag misunderstoodFlags = metadataFlags & (~understoodFlags);
                throw new Exception("Unknown metadata flag bits: 0x" +
                                                   ((int)misunderstoodFlags).ToString("X4"));
            }

            if (0 != (metadataFlags & TagMetadataFlag.READCOUNT))
            {
                t.ReadCount = response[readOffset++];
            }
            if (0 != (metadataFlags & TagMetadataFlag.RSSI))
            {
                t._lqi = (sbyte)(response[readOffset++]);
            }
            if (0 != (metadataFlags & TagMetadataFlag.ANTENNAID))
            {
                t._antenna = TranslateSerialAntenna(response[readOffset++]);
            }
            if (0 != (metadataFlags & TagMetadataFlag.FREQUENCY))
            {
                t._frequency = response[readOffset++] << 16;
                t._frequency |= response[readOffset++] << 8;
                t._frequency |= response[readOffset++] << 0;
            }
            if (0 != (metadataFlags & TagMetadataFlag.TIMESTAMP))
            {
                t._readOffset = response[readOffset++] << 24;
                t._readOffset |= response[readOffset++] << 16;
                t._readOffset |= response[readOffset++] << 8;
                t._readOffset |= response[readOffset++] << 0;
            }
            if (0 != (metadataFlags & TagMetadataFlag.PHASE))
            {
                t._phase = response[readOffset++] << 8;
                t._phase |= response[readOffset++] << 0;
            }
            if (0 != (metadataFlags & TagMetadataFlag.PROTOCOL))
            {
                // TagData.Protocol is not directly modifiable.
                // It is changed by subclassing TagData.
                // Just remember it here in order to to create the right class later.
                protocol = TranslateProtocol((SerialTagProtocol)response[readOffset++]);
            }

            if (0 != (metadataFlags & TagMetadataFlag.DATA))
            {
                int bitlength = ByteConv.ToU16(response, readOffset);
                int length = bitlength / 8;
                t._data = new byte[length];
                Array.Copy(response, (readOffset + 2), t._data, 0, length);
                readOffset += (2 + length);
            }

            if (0 != (metadataFlags & TagMetadataFlag.GPIO))
            {
                byte gpioByte = response[readOffset++];
                int gpioNumber = 4;

                t._GPIO = new GpioPin[gpioNumber];
                for (int i = 0; i < gpioNumber; i++)
                {
                    (t._GPIO)[i] = new GpioPin((i + 1), (((gpioByte >> i) & 1) == 1));
                }
            }
        }

        #endregion

        #region TranslateSerialAntenna

        /// <summary>
        /// Translate antenna ID from M5e Get Tag Buffer command to logical antenna number
        /// </summary>
        /// <param name="serant">M5e serial protocol antenna ID (TX: 4 msbs, RX: 4 lsbs)</param>
        /// <returns>Logical antenna number</returns>
        public int TranslateSerialAntenna(byte txrx)
        {
            int tx = 0;
            if (((txrx >> 4) & 0xF) == ((txrx >> 0) & 0xF))
            {
                tx = (txrx >> 4) & 0xF;
            }
            return tx;
        }

        #endregion

        #region TranslateProtocol

        /// <summary>
        /// Translate tag protocol codes from M5e internal to ThingMagic external representation.
        /// </summary>
        /// <param name="intProt">Internal M5e tag protocol code</param>
        /// <returns>External ThingMagic protocol code</returns>
        private static ThingMagic.TagProtocol TranslateProtocol(SerialTagProtocol intProt)
        {
            switch (intProt)
            {
                case SerialTagProtocol.NONE:
                    return ThingMagic.TagProtocol.NONE;
                case SerialTagProtocol.GEN2:
                    return ThingMagic.TagProtocol.GEN2;
                case SerialTagProtocol.ISO18000_6B:
                    return ThingMagic.TagProtocol.ISO180006B;
                case SerialTagProtocol.UCODE:
                    return ThingMagic.TagProtocol.ISO180006B_UCODE;
                case SerialTagProtocol.IPX64:
                    return ThingMagic.TagProtocol.IPX64;
                case SerialTagProtocol.IPX256:
                    return ThingMagic.TagProtocol.IPX256;
                default:
                    return ThingMagic.TagProtocol.NONE;
                //throw new ReaderParseException("No translation for M5e tag protocol code: " + intProt.ToString());
            }
        }
        #endregion

        #region TranslateProtocol
        /// <summary>
        /// Translate tag protocol codes from M5e internal to ThingMagic external representation.
        /// </summary>
        /// <param name="intProt">Internal M5e tag protocol code</param>
        /// <returns>External ThingMagic protocol code</returns>
        private static SerialTagProtocol TranslateProtocol(ThingMagic.TagProtocol intProt)
        {
            switch (intProt)
            {
                case ThingMagic.TagProtocol.NONE:
                    return SerialTagProtocol.NONE;
                case ThingMagic.TagProtocol.GEN2:
                    return SerialTagProtocol.GEN2;
                case ThingMagic.TagProtocol.ISO180006B:
                    return SerialTagProtocol.ISO18000_6B;
                case ThingMagic.TagProtocol.ISO180006B_UCODE:
                    return SerialTagProtocol.UCODE;
                case ThingMagic.TagProtocol.IPX64:
                    return SerialTagProtocol.IPX64;
                case ThingMagic.TagProtocol.IPX256:
                    return SerialTagProtocol.IPX256;
                default:
                    return SerialTagProtocol.NONE;
                //throw new ReaderParseException("No translation for ThingMagic tag protocol code: " + intProt.ToString());
            }
        }

        #endregion

        #region SerialTagProtocol

        /// <summary>
        /// Tag Protocol identifiers, as used in Get and Set Current Tag Protocol
        /// </summary>
        public enum SerialTagProtocol
        {
            /// <summary>
            /// No protocol selected
            /// </summary>
            NONE = 0,
            /// <summary>
            /// EPC0 and Matrics-style EPC0+
            /// </summary>
            EPC0_MATRICS = 1,
            /// <summary>
            /// EPC1
            /// </summary>
            EPC1 = 2,
            /// <summary>
            /// ISO 18000-6B
            /// </summary>
            ISO18000_6B = 3,
            /// <summary>
            /// Impinj-style EPC0+
            /// </summary>
            EPC0_IMPINJ = 4,
            /// <summary>
            /// Gen2
            /// </summary>
            GEN2 = 5,
            /// <summary>
            /// UCODE
            /// </summary>
            UCODE = 6,
            /// <summary>
            /// IPX64
            /// </summary>
            IPX64 = 7,
            /// <summary>
            /// IPX256
            /// </summary>
            IPX256 = 8,
        }

        #endregion

        #region SubArray
        /// <summary>
        /// Extract subarray
        /// </summary>
        /// <param name="src">Source array</param>
        /// <param name="offset">Start index in source array</param>
        /// <param name="length">Number of source elements to extract</param>
        /// <returns>New array containing specified slice of source array</returns>
        private static byte[] SubArray(byte[] src, int offset, int length)
        {
            return SubArray(src, ref offset, length);
        }

        /// <summary>
        /// Extract subarray, automatically incrementing source offset
        /// </summary>
        /// <param name="src">Source array</param>
        /// <param name="offset">Start index in source array.  Automatically increments value by copied length.</param>
        /// <param name="length">Number of source elements to extract</param>
        /// <returns>New array containing specified slice of source array</returns>
        private static byte[] SubArray(byte[] src, ref int offset, int length)
        {
            byte[] dst = new byte[length];
            Array.Copy(src, offset, dst, 0, length);
            offset += length;
            return dst;
        }

        #endregion

        #region TagMetadataFlag

        /// <summary>
        /// Enum to define the Tag Metadata.
        /// </summary>
        [Flags]
        public enum TagMetadataFlag
        {
            /// <summary>
            /// Get read count in Metadata
            /// </summary>
            READCOUNT = 0x0001,
            /// <summary>
            /// Get RSSI count in Metadata
            /// </summary>
            RSSI = 0x0002,
            /// <summary>
            /// Get Antenna ID count in Metadata
            /// </summary>
            ANTENNAID = 0x0004,
            /// <summary>
            /// Get frequency in Metadata
            /// </summary>
            FREQUENCY = 0x0008,
            /// <summary>
            /// Get timestamp in Metadata
            /// </summary>
            TIMESTAMP = 0x0010,
            /// <summary>
            /// Get pahse in Metadata
            /// </summary>
            PHASE = 0x0020,
            /// <summary>
            /// Get protocol in Metadata
            /// </summary>
            PROTOCOL = 0x0040,
            /// <summary>
            /// Get read data in Metadata
            /// </summary>
            DATA = 0x0080,
            ///<summary>
            /// Get GPIO value in Metadata
            /// </summary>
            GPIO = 0x0100,
            ///<summary>
            /// Shortcut to get all metadata attributes available during normal search.
            /// Excludes data, which requires special operation above and beyond ReadTagMultiple.
            /// </summary>
            ALL = READCOUNT | RSSI | ANTENNAID | FREQUENCY | TIMESTAMP | PROTOCOL | GPIO | PHASE | DATA,
        }
        #endregion

        #region CalcReturnCRC

        /// <summary>
        /// Calculates CRC of the data returned from the M5e,
        /// </summary>
        /// <param name="command">Byte Array that needs CRC calculation</param>
        /// <returns>CRC Byte Array</returns>
        private static byte[] CalcReturnCRC(byte[] command)
        {
            UInt16 tempcalcCRC1 = CalcCRC8(65535, command[1]);
            tempcalcCRC1 = CalcCRC8(tempcalcCRC1, command[2]);
            byte[] CRC = new byte[2];

            //if (command[1] != 0)
            {
                for (int i = 0; i < (command[1] + 2); i++)
                    tempcalcCRC1 = CalcCRC8(tempcalcCRC1, command[3 + i]);
            }

            CRC = BitConverter.GetBytes(tempcalcCRC1);

            Array.Reverse(CRC);

            return CRC;
        }

        #endregion

        #region CalcCRC8

        private static UInt16 CalcCRC8(UInt16 beginner, byte ch)
        {
            byte[] tempByteArray;
            byte xorFlag;
            byte element80 = new byte();
            element80 = 0x80;
            byte chAndelement80 = new byte();
            bool[] forxorFlag = new bool[16];

            for (int i = 0; i < 8; i++)
            {
                tempByteArray = BitConverter.GetBytes(beginner);
                Array.Reverse(tempByteArray);
                BitArray tempBitArray = new BitArray(tempByteArray);

                for (int j = 0; j < tempBitArray.Count; j++)
                    forxorFlag[j] = tempBitArray[j];

                Array.Reverse(forxorFlag, 0, 8);
                Array.Reverse(forxorFlag, 8, 8);

                for (int k = 0; k < tempBitArray.Count; k++)
                    tempBitArray[k] = forxorFlag[k];

                xorFlag = BitConverter.GetBytes(tempBitArray.Get(0))[0];
                beginner = (UInt16)(beginner << 1);
                chAndelement80 = (byte)(ch & element80);

                if (chAndelement80 != 0)
                    ++beginner;

                if (xorFlag != 0)
                    beginner = (UInt16)(beginner ^ 0x1021);

                element80 = (byte)(element80 >> 1);
            }

            return beginner;
        }

        #endregion

        #region GetComPortNames
        /// <summary>
        /// Returns the COM port names as list
        /// </summary>
        public List<string> GetComPortNames()
        {
            List<string> portNames = new List<string>();
            List<string> comPortValue = new List<string>();
            using (var searcher = new ManagementObjectSearcher("root\\CIMV2", "SELECT * FROM Win32_PnPEntity WHERE ConfigManagerErrorCode = 0"))
            {
                foreach (ManagementObject queryObj in searcher.Get())
                {
                    if ((queryObj != null) && (queryObj["Name"] != null))
                    {
                        if (queryObj["Name"].ToString().Contains("(COM"))
                            portNames.Add(queryObj["Name"].ToString());
                    }
                }
            }
            foreach (string portName in portNames)
            {
                MatchCollection mc = Regex.Matches(portName, @"(?<=\().+?(?=\))");
                foreach (Match m in mc)
                {
                    comPortValue.Add(m.ToString());
                }
            }
            return comPortValue;
        }
        #endregion
    }

    #region Reader Stats

    /// <summary>
    /// Reader stats object.
    /// </summary>
    public class Stat
    {

        #region ReaderStatsValues

        /// <summary>
        /// Reader stats values
        /// </summary>
        public class Values : Stat
        {
            /// <summary>
            /// Cache stats flag
            /// </summary>
            private StatsFlag valid = StatsFlag.NONE;
            /// <summary>
            /// Current temperature (degrees C)
            /// </summary>
            private UInt32? temperature = null;
           

            /// <summary>
            /// Cache stats flag
            /// </summary>
            public StatsFlag VALID
            {
                get { return valid; }
                set { valid = value; }
            }
            /// <summary>
            /// Current temperature (degrees C)
            /// </summary>
            public UInt32? TEMPERATURE
            {
                get { return temperature; }
                set { temperature = value; }
            }
           

            #region ToString
            /// <summary>
            /// Human-readable representation
            /// </summary>
            /// <returns>Human-readable representation</returns>
            public override string ToString()
            {
                StringBuilder stringbuild = new StringBuilder();
               
                if (0 != (valid & Stat.StatsFlag.TEMPERATURE))
                {
                    stringbuild.Append("Temperature: " + temperature.ToString() + " C" + "\n");
                }
                return stringbuild.ToString();
            }
            #endregion ToString
        }

        #endregion ReaderStatsValues

        #region ReaderStatsFlag

        /// <summary>
        /// Reader Stats Flag Enum - 
        /// </summary>
        [Flags]
        public enum StatsFlag
        {
            /// <summary>
            /// None
            /// </summary>
            NONE = 0,
            /// <summary>
            /// Current temperature of the device in units of Celcius
            /// </summary>
            TEMPERATURE = 1 << 8,
        }

        #endregion ReaderStatsFlag
    }

    #endregion Reader Stats
}
