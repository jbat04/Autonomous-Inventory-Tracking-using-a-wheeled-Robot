/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.thingmagic;

import java.nio.ByteBuffer;
import java.util.EnumMap;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
/**
 *
 *
 */
public class SerialTransport
{

    SerialTransportNative st;
    int transportTimeout = 5000;
    long baseTimeStamp = 0;
    String comPort;
    public boolean connect(String deviceName) throws ReaderException
    {
        comPort = deviceName;
        boolean isConnected = false;
        System.out.println("Trying to connect...");
        baseTimeStamp = 0;
        while(!isConnected)
        {
            try
            {
                if (st != null)
                {
                    st.shutdown();
                }
                st = new SerialTransportNative(deviceName);
                st.open();
                int[] bps =
                {
                    115200, 9600, 921600, 19200, 38400, 57600, 230400, 460800
                };

                for (int count = 0; count < bps.length; count++)
                {
                    st.setBaudRate(bps[count]);
                    st.flush();
                    try
                    {
                        transportTimeout = 2000;
                        receiveResponse();
                        isConnected = true;
                        System.out.println("Connected to the reader successfully");
                        transportTimeout = 5000;
                        break;
                    }
                    catch (Exception ex)
                    {                       

                    }
                }
            } 
            catch (Exception ex)
            {

            }
        }
        return isConnected;
    }

 
    public void receiveStreaming() throws ReaderException
    {
       
        while (true)
        {
            
            Message m = new Message();
            try
            {
                m.readIndex = 0;
                if(baseTimeStamp == 0)
                {
                  baseTimeStamp = System.currentTimeMillis();
                }
                receiveMessage(1000, m);
                if (m.data[2] == 0x2F)
                {
                    //do nothing
                }
                else
                {
                    parseResponse(m);
                }

            }
            catch (Exception re)
            {
                if(re instanceof ReaderCodeException && ((ReaderCodeException)re).getCode() == 0x400)
                {
                    baseTimeStamp = System.currentTimeMillis();
                }
                
                if(re.getMessage() != null && re.getMessage().contains("Timeout"))
                {     
                    System.out.println(re.getMessage());
                    System.out.println("Restoring the serial interface");
                    connect(comPort);
                }            
            }
        }
    }

    public void receiveResponse() throws ReaderException
    {
         Message m = new Message();
         receiveMessage(1000, m);
    }
    
    public void receiveMessage(int timeout, Message m) throws ReaderException
    {
        int sofPosition;
        boolean sofFound = false;
        long enterTime = System.currentTimeMillis();
        int messageLength = 7;
        while ((System.currentTimeMillis() - enterTime) <= timeout)
        {
            // System.out.println("timeout" +timeout + "  transportTimeout = " +transportTimeout+"  Total ="+(timeout + transportTimeout));
            st.receiveBytes(messageLength, m.data, 0, timeout + transportTimeout);
            if (m.data[0] != (byte) 0xff)
            {
                int i = 0;
                for (i = 1; i < (messageLength - 1); i++)
                {
                    if (m.data[i] == (byte) 0xff)
                    {
                        sofPosition = i;
                        sofFound = true;
                        System.arraycopy(m.data, sofPosition, m.data, 0, messageLength - sofPosition);
                        st.receiveBytes(sofPosition, m.data, messageLength - sofPosition, timeout + transportTimeout);
                        break;
                    }
                }
                if (sofFound)
                {
                    break;
                }

            }
            else
            {
                sofFound = true;
                break;
            }
        }

        if (sofFound == false)
        {
            throw new ReaderCommException(String.format("No soh FOund"));
        }

        int len = m.data[1] & 0xff;
        if((len+messageLength) > (m.data.length))
        {
          throw new ReaderException(String.format("Invalid length"));  
        }
        //Now pull in the rest of the data, if exists, + the CRC        

        if (len != 0)
        {
            st.receiveBytes(len, m.data, messageLength, timeout + transportTimeout);
        }

        //Calculate the crc for the data
        int crc = calcCrc(m.data, 1, len + 4);
        //Compare with message's crc
        if ((m.data[len + 5] != (byte) ((crc >> 8) & 0xff))
                || (m.data[len + 6] != (byte) (crc & 0xff)))
        {

            receiveMessage(timeout, m);

        }

        int status = m.getu16at(3);

        if ((status & 0x7f00) == 0x7f00)
        {
            // Module assertion. Decode the assert string from the response.
            int lineNum;

            lineNum = m.getu32at(5);

            throw new ReaderFatalException(
                    String.format("Reader assert 0x%x at %s:%d", status,
                            new String(m.data, 9, m.writeIndex - 9), lineNum));
        }

        if (status != 0x0000)
        {
            throw new ReaderCodeException(status);
        }
        m.writeIndex = 5 + (m.data[1] & 0xff);  //Set the write index to start of CRC
        m.readIndex = 5;

    }

    private static int crcTable[] =
    {
        0x0000, 0x1021, 0x2042, 0x3063,
        0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b,
        0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    };

    // calculates ThingMagic's CRC-16
    private static short calcCrc(byte[] message, int offset, int length)
    {
        int crc = 0xffff;

        for (int i = offset; i < offset + length; i++)
        {
            crc = ((crc << 4) | ((message[i] >> 4) & 0xf)) ^ crcTable[crc >> 12];
            crc &= 0xffff;
            crc = ((crc << 4) | ((message[i] >> 0) & 0xf)) ^ crcTable[crc >> 12];
            crc &= 0xffff;
        }
        return (short) crc;
    }

    static class Message
    {

        byte[] data;
        int writeIndex;
        int readIndex;

        Message()
        {
            this(256);
        }

        Message(int size)
        {
            data = new byte[size];
            writeIndex = 2;
        }

        void setu8(int val)
        {
            data[writeIndex++] = (byte) (val & 0xff);
        }

        void setu16(int val)
        {
            data[writeIndex++] = (byte) ((val >> 8) & 0xff);
            data[writeIndex++] = (byte) ((val >> 0) & 0xff);
        }

        void setu32(int val)
        {
            data[writeIndex++] = (byte) ((val >> 24) & 0xff);
            data[writeIndex++] = (byte) ((val >> 16) & 0xff);
            data[writeIndex++] = (byte) ((val >> 8) & 0xff);
            data[writeIndex++] = (byte) ((val >> 0) & 0xff);
        }

        void setbytes(byte[] array)
        {
            if (array != null)
            {
                setbytes(array, 0, array.length);
            }
        }

        void setbytes(byte[] array, int start, int length)
        {
            System.arraycopy(array, start, data, writeIndex, length);
            writeIndex += length;
        }

        int getu8()
        {
            return getu8at(readIndex++);
        }

        int getu16()
        {
            int val;
            val = getu16at(readIndex);
            readIndex += 2;
            return val;
        }

        short gets16()
        {
            short val;
            val = (short) getu16at(readIndex);
            readIndex += 2;
            return val;
        }

        int getu24()
        {
            int val;
            val = getu24at(readIndex);
            readIndex += 3;
            return val;
        }

        int getu32()
        {
            int val;
            val = getu32at(readIndex);
            readIndex += 4;
            return val;
        }

        void getbytes(byte[] destination, int length)
        {
            System.arraycopy(data, readIndex, destination, 0, length);
            readIndex += length;
        }

        int getu8at(int offset)
        {
            return data[offset] & 0xff;
        }

        int getu16at(int offset)
        {
            return ((data[offset] & 0xff) << 8)
                    | ((data[offset + 1] & 0xff) << 0);
        }

        int getu24at(int offset)
        {
            return ((data[offset] & 0xff) << 16)
                    | ((data[offset + 1] & 0xff) << 8)
                    | ((data[offset + 0] & 0xff) << 0);
        }

        int getu32at(int offset)
        {
            return ((data[offset] & 0xff) << 24)
                    | ((data[offset + 1] & 0xff) << 16)
                    | ((data[offset + 2] & 0xff) << 8)
                    | ((data[offset + 3] & 0xff) << 0);
        }
    }

    void notifyToUser(TagReadData t)
    {
        System.out.println(t.toString());
        if(t.data.length > 0)
        {
            System.out.println("Data: " + convertByteArrayToHexString(t.data));
        }
    }
    
  public static String convertByteArrayToHexString(byte in[])
    {

        byte ch = 0x00;
        int i = 0;

        if (in == null || in.length <= 0)
        {
            return null;
        }

        String pseudo[] = {"0", "1", "2",
            "3", "4", "5", "6", "7", "8",
            "9", "A", "B", "C", "D", "E",
            "F"};

        StringBuilder out = new StringBuilder(in.length * 2);

        while (i < in.length)
        {
            ch = (byte) (in[i] & 0xF0); // Strip off high nibble
            ch = (byte) (ch >>> 4);
            // shift the bits down
            ch = (byte) (ch & 0x0F);
            // must do this is high order bit is on!
            out.append(pseudo[(int) ch]); // convert the nibble to a String Character
            ch = (byte) (in[i] & 0x0F); // Strip off low nibble
            out.append(pseudo[(int) ch]); // convert the nibble to a String Character
            i++;
        }
        return new String(out);

    }
    public static final int TAG_METADATA_NONE = 0x0000,
            TAG_METADATA_READCOUNT = 0x0001,
            TAG_METADATA_RSSI = 0x0002,
            TAG_METADATA_ANTENNAID = 0x0004,
            TAG_METADATA_FREQUENCY = 0x0008,
            TAG_METADATA_TIMESTAMP = 0x0010,
            TAG_METADATA_PHASE = 0x0020,
            TAG_METADATA_PROTOCOL = 0x0040,
            TAG_METADATA_DATA = 0x0080,
            TAG_METADATA_GPIO_STATUS = 0x0100,
            TAG_METADATA_ALL = TAG_METADATA_NONE | TAG_METADATA_READCOUNT | TAG_METADATA_RSSI
            | TAG_METADATA_ANTENNAID | TAG_METADATA_FREQUENCY | TAG_METADATA_TIMESTAMP | TAG_METADATA_PHASE
            | TAG_METADATA_PROTOCOL | TAG_METADATA_GPIO_STATUS;
    static final Map<TagReadData.TagMetadataFlag, Integer> tagMetadataFlagValues;

    static
    {
        tagMetadataFlagValues = new EnumMap<TagReadData.TagMetadataFlag, Integer>(TagReadData.TagMetadataFlag.class);
        tagMetadataFlagValues.put(TagReadData.TagMetadataFlag.READCOUNT, TAG_METADATA_READCOUNT);
        tagMetadataFlagValues.put(TagReadData.TagMetadataFlag.RSSI, TAG_METADATA_RSSI);
        tagMetadataFlagValues.put(TagReadData.TagMetadataFlag.ANTENNAID, TAG_METADATA_ANTENNAID);
        tagMetadataFlagValues.put(TagReadData.TagMetadataFlag.FREQUENCY, TAG_METADATA_FREQUENCY);
        tagMetadataFlagValues.put(TagReadData.TagMetadataFlag.TIMESTAMP, TAG_METADATA_TIMESTAMP);
        tagMetadataFlagValues.put(TagReadData.TagMetadataFlag.PHASE, TAG_METADATA_PHASE);
        tagMetadataFlagValues.put(TagReadData.TagMetadataFlag.PROTOCOL, TAG_METADATA_PROTOCOL);
        tagMetadataFlagValues.put(TagReadData.TagMetadataFlag.DATA, TAG_METADATA_DATA);
        tagMetadataFlagValues.put(TagReadData.TagMetadataFlag.GPIO_STATUS, TAG_METADATA_GPIO_STATUS);
        tagMetadataFlagValues.put(TagReadData.TagMetadataFlag.ALL, TAG_METADATA_ALL);
    }
    private int lastMetadataBits;
    private EnumSet<TagReadData.TagMetadataFlag> lastMetadataFlags;

    Set<TagReadData.TagMetadataFlag> tagMetadataSet(int bits)
    {

        if (bits == lastMetadataBits)
        {
            return lastMetadataFlags.clone();
        }

        EnumSet<TagReadData.TagMetadataFlag> metadataFlags
                = EnumSet.noneOf(TagReadData.TagMetadataFlag.class);

        for (TagReadData.TagMetadataFlag f : TagReadData.TagMetadataFlag.values())
        {
            if (0 != (tagMetadataFlagValues.get(f) & bits))
            {
                metadataFlags.add(f);
            }
        }

        lastMetadataBits = bits;
        lastMetadataFlags = metadataFlags;

        return metadataFlags;
    }

    void metadataFromMessage(TagReadData t, Message m,
            Set<TagReadData.TagMetadataFlag> meta)
    {
        t.metadataFlags = meta;

        if (meta.contains(TagReadData.TagMetadataFlag.READCOUNT))
        {
            t.readCount = m.getu8();
        }
        if (meta.contains(TagReadData.TagMetadataFlag.RSSI))
        {
            t.rssi = (byte) m.getu8(); // keep the sign here
        }
        if (meta.contains(TagReadData.TagMetadataFlag.ANTENNAID))
        {
            t.antenna = m.getu8();
        }
        if (meta.contains(TagReadData.TagMetadataFlag.FREQUENCY))
        {
            t.frequency = m.getu24();
        }
        if (meta.contains(TagReadData.TagMetadataFlag.TIMESTAMP))
        {
            t.readOffset = m.getu32();
        }
        if (meta.contains(TagReadData.TagMetadataFlag.PHASE))
        {
            t.phase = m.getu16();
        }
        if (meta.contains(TagReadData.TagMetadataFlag.PROTOCOL))
        {
            t.readProtocol = codeToProtocolMap.get(m.getu8());
        }
        if (meta.contains(TagReadData.TagMetadataFlag.DATA))
        {
            int dataBits = m.getu16();
            t.data = new byte[(dataBits + 7) / 8];
            m.getbytes(t.data, t.data.length);
//            if (isGen2AllMemoryBankEnabled)
//            {
//                parseTagMemBankdata(t, t.data, 0);
//                isGen2AllMemoryBankEnabled = false;
//        }
        }
        if (meta.contains(TagReadData.TagMetadataFlag.GPIO_STATUS))
        {
            byte gpioByte = (byte) m.getu8();
            int gpioNumber = 4;
//            switch (versionInfo.hardware.part1)
//            {
//                case TMR_SR_MODEL_M6E:
//                    gpioNumber = 4;
//                    break;
//                case TMR_SR_MODEL_M5E:
//                    gpioNumber = 2;
//                    break;
//                default:
//                    gpioNumber = 4;
//                    break;
//            }

            t.gpio = new GpioPin[gpioNumber];
            for (int i = 0; i < gpioNumber; i++)
            {
                (t.gpio)[i] = new GpioPin((i + 1), (((gpioByte >> i) & 1) == 1));
            }
        }
    }

    public static final short PROT_ISO180006B = 0x03,
            PROT_GEN2 = 0x05,
            PROT_UCODE = 0x06,
            PROT_IPX64 = 0x07,
            PROT_IPX256 = 0x08,
            PROT_ATA = 0x1D;

    static final Map<Integer, TagProtocol> codeToProtocolMap;

    static
    {
        codeToProtocolMap = new HashMap<Integer, TagProtocol>();
        codeToProtocolMap.put((int) PROT_ISO180006B, TagProtocol.ISO180006B);
        codeToProtocolMap.put((int) PROT_GEN2, TagProtocol.GEN2);
        codeToProtocolMap.put((int) PROT_UCODE, TagProtocol.ISO180006B_UCODE);
        codeToProtocolMap.put((int) PROT_IPX64, TagProtocol.IPX64);
        codeToProtocolMap.put((int) PROT_IPX256, TagProtocol.IPX256);
        codeToProtocolMap.put((int) PROT_ATA, TagProtocol.ATA);
    }

    void parseResponse(Message m)
    {
     
        byte flags = (byte) m.getu8();
        int responseTypeIndex = (((byte) flags & (byte) 0x10) == (byte) 0x10) ? 10 : 8;
        byte responseByte = m.data[responseTypeIndex];

        // skipping the search flags
        m.readIndex += 2;
        if (responseByte == 0x02)
        {
            m.readIndex += 2;
            while (m.readIndex < m.writeIndex)
            {
                if (0x82 != m.getu8at(m.readIndex))
                {
                    m.readIndex++;
                    continue;
                }
            
                int statFlag = 0;
                if ((0x80) > m.getu8at(m.readIndex))
                {
                    statFlag = m.getu8at(m.readIndex);
                } 
                else
                {
                    /**
                     * the response flag will be in EBV format, convert that to
                     * the api enum values
                     */
                    statFlag = ((m.getu8at(m.readIndex) << 8) | (m.getu8at(m.readIndex + 1)));
                    statFlag &= 0x7fff;
                    statFlag = ((statFlag >> 1) | (statFlag & 0x7f));
                }

                if ((0 != statFlag) && (statFlag == ReaderStatsFlag.TEMPERATURE.value))
                {
                    TemperatureStatusReport tsr = new TemperatureStatusReport();
                    m.readIndex += 3;
                    tsr.temperature = m.getu8();
                    notifyStatusListeners(tsr);
                }
            }
            
        }
        else
        {
            TagReadData t = new TagReadData();
            Set<TagReadData.TagMetadataFlag> metadataFlags = tagMetadataSet(m.getu16());
            m.readIndex += 1; // skip response type
            if (!metadataFlags.isEmpty())
            {
                metadataFromMessage(t, m, metadataFlags);
                t.antenna =translateSerialAntenna(t.antenna);
                int epcLen = m.getu16() / 8;
                t.tag = parseTag(m, epcLen, t.readProtocol);
                t.readBase = baseTimeStamp;
                if (null != t.tag)
                {
                    notifyToUser(t);
                }
            }
        }
    }
    public int translateSerialAntenna(int txrx)
        {
            int tx = 0;
            if (((txrx >> 4) & 0xF) == ((txrx >> 0) & 0xF))
            {
                tx = (txrx >> 4) & 0xF;
            }
            return tx;
        }
    TagData parseTag(Message m, int epcLen, TagProtocol protocol)
    {
        TagData tag = null;
        byte[] pcbits, epcbits, crcbits;

        switch (protocol)
        {
            case GEN2:
                final int BUFFER_SIZE = 6;
                ByteBuffer pcBuffer = ByteBuffer.allocate(BUFFER_SIZE);

                epcLen -= 2; // removed 2 bytes of CRC

                pcBuffer.put((byte) m.getu8()); // PC Byte 1
                pcBuffer.put((byte) m.getu8()); // PC Byte 2

                epcLen -= 2; // Only PC Bits

                if ((pcBuffer.get(0) & 0x02) == 0x02) // Bitwise AND operation on 4 bytes integer msb
                {
                    pcBuffer.put((byte) m.getu8()); // PC Byte 3
                    pcBuffer.put((byte) m.getu8()); // PC Byte 4

                    epcLen -= 2; // XPC_W1 exists, so epcLen is decremented            

                    if ((pcBuffer.get(2) & 0x80) == 0x80)
                    {
                        /* When MSB bit is set, the XPC_W2 word will follow the XPC_W1 word */
                        pcBuffer.put((byte) m.getu8()); // PC Byte 5
                        pcBuffer.put((byte) m.getu8()); // PC Byte 6

                        epcLen -= 2;  // PC + XPC_W1 + XPC_W2 = 6 bytes                
                    }
                }
                int pos = pcBuffer.position();
                pcBuffer.clear();
                pcbits = new byte[pos];
                pcBuffer.get(pcbits, 0, pos);// copy contents in buffer to pcbits array.
                try
                {
                    if (epcLen > -1)
                    {
                        epcbits = new byte[epcLen];
                        m.getbytes(epcbits, epcLen);
                        crcbits = new byte[2];
                        m.getbytes(crcbits, 2);
                        tag = new Gen2.TagData(epcbits, crcbits, pcbits);
                    }

                    if (epcLen < -2)
                    {
                        m.readIndex = m.readIndex - (pos + epcLen);
                    }
                } catch (NegativeArraySizeException nase)
                {
            // Handling NegativeArraySizeException
                    // Ignoring invalid tag response (epcLen goes to negative), example below

            //  Received: ff 1c 22 00 00 10 00 1b 01 ff 01 01 d9 22 0d c6
                    //  5e 00 00 00 14 00 b1 05 00 00 00 00 20 02 18 17  ab 13 36
                } catch (ArrayIndexOutOfBoundsException aiobe)
                {
                    // Handling ArrayIndexOutOfBoundsException
                }
                break;

            default:
                epcbits = new byte[epcLen - 2];
                m.getbytes(epcbits, epcLen - 2);
                crcbits = new byte[2];
                m.getbytes(crcbits, 2);
                tag = new TagData(epcbits, crcbits);
        }
        return tag;
    }

    public static class ReaderFatalException extends ReaderException
    {

        ReaderFatalException(String message)
        {
            super(message);
        }
    }
    
    public static  class ReaderParseException extends ReaderException
    {
        ReaderParseException(String message)
        {
            super(message);
        }
    }
    
    public static class ReaderCodeException extends ReaderException
    {

        int code;

        ReaderCodeException(int code, String message)
        {
            super(message);
            this.code = code;
        }

        ReaderCodeException(int code)
        {
            this(code, faultCodeToMessage(code));
        }

        public int getCode()
        {
            return code;
        }

        static String faultCodeToMessage(int status)
        {
            String messageString;
            switch (status)
            {
                case 0x100:
                    messageString = "The data length in the message is less than or more "
                            + "than the number of arguments required for the opcode.";
                    break;
                case 0x101:
                    messageString = "The opCode received is invalid or not supported with "
                            + "the current version of code.";
                    break;
                case 0x102:
                    messageString = "Unimplemented Opcode";
                    break;
                case 0x103:
                    messageString = "A message was sent to set the read or write power to "
                            + "a level that is higher than the HW supports.";
                    break;
                case 0x104:
                    messageString = "A message was received by the reader to set the frequency "
                            + "outside the supported range";
                    break;
                case 0x105:
                    messageString = "The reader received a valid command with an unsupported or "
                            + "invalid parameter";
                    break;
                case 0x106:
                    messageString = "A message was received to set the read or write power to a "
                            + "level that is lower than the HW supports.";
                    break;
                case 0x107:
                    messageString = "Wrong number of bits to transmit.";
                    break;
                case 0x108:
                    messageString = "Timeout too long.";
                    break;
                case 0x109:
                    messageString = "Unimplemented feature.";
                    break;
                case 0x10a:
                    messageString = "Invalid baud rate.";
                    break;
                case 0x10b:
                    messageString = "Invalid Region.";
                    break;
                case 0x10c:
                    messageString = "Invalid License Key";
                    break;
                case 0x200:
                    messageString = "CRC validation of firmware image failed";
                    break;
                case 0x201:
                    messageString = "The last word of the firmware image stored in the reader's "
                            + "flash ROM does not have the correct address value.";
                    break;
                case 0x300:
                    messageString = "A command was received to erase some part of the flash but "
                            + "the password supplied with the command was incorrect.";
                    break;
                case 0x301:
                    messageString = "A command was received to write some part of the flash but "
                            + "the password supplied with the command was incorrect.";
                    break;
                case 0x302:
                    messageString = "FAULT_FLASH_UNDEFINED_ERROR - This is an internal error and "
                            + "it is caused by a software problem in the M4e.";
                    break;
                case 0x303:
                    messageString = "An erase or write flash command was received with the sector "
                            + "value and password not matching.";
                    break;
                case 0x304:
                    messageString = "The M4e received a write flash command to an area of flash "
                            + "that was not previously erased.";
                    break;
                case 0x305:
                    messageString = "A flash command can not access multiple sectors.";
                    break;
                case 0x306:
                    messageString = "Verifying flash contents failed.";
                    break;
                case 0x400:
                    messageString = "No tags found.";
                    break;
                case 0x401:
                    messageString = "A command was received to perform a protocol command but no "
                            + "protocol was set.";
                    break;
                case 0x402:
                    messageString = "A Set Protocol command was received for a protocol value that "
                            + "is not supported";
                    break;
                case 0x403:
                    messageString = "Lock failed during a Write Tag Data for ISO18000-6B or UCODE.";
                    break;
                case 0x404:
                    messageString = "A Read Tag ID or Data command was sent but did not succeed.";
                    break;
                case 0x405:
                    messageString = "A command was received while the AFE was in the off state. "
                            + "Please check that a region has been selected and the AFE has "
                            + "not been disabled.";
                    break;
                case 0x406:
                    messageString = "Tag write operation failed.";
                    break;
                case 0x407:
                    messageString = "A command was received which is not supported by the currently "
                            + "selected protocol.";
                    break;
                case 0x408:
                    messageString = "In EPC0+, the first two bits determine the tag ID length. If "
                            + "the first two bits are 0b00, then the tag ID must be 96-bits. "
                            + "Otherwise the tag ID is 64 bits.";
                    break;
                case 0x409:
                    messageString = "A command was received to write to an invalid address in the "
                            + "tag data address space.";
                    break;
                case 0x40a:
                    messageString = "General Tag Error (this error is used by the M5e GEN2 module).";
                    break;
                case 0x40b:
                    messageString = "A command was received to Read Tag Data with a data value that "
                            + "is not the correct size.";
                    break;
                case 0x40c:
                    messageString = "An incorrect kill password was received as part of the Kill command.";
                    break;
                case 0x40d:
                    messageString = "Test failed.";
                    break;
                case 0x40e:
                    messageString = "Kill failed.";
                    break;
                case 0x40f:
                    messageString = "Bit decoding failed.";
                    break;
                case 0x410:
                    messageString = "Invalid EPC on tag.";
                    break;
                case 0x411:
                    messageString = "Invalid quantity of data in tag message.";
                    break;
                case 0x420:
                    messageString = "Other Gen2 error.";
                    break;
                case 0x423:
                    messageString = "Gen2 memory overrun - bad PC.";
                    break;
                case 0x424:
                    messageString = "Gen2 memory locked.";
                    break;
                case 0x42b:
                    messageString = "Gen2 tag has insufficent power.";
                    break;
                case 0x42f:
                    messageString = "Gen2 unspecific error.";
                    break;
                case 0x430:
                    messageString = "Gen2 unknown error.";
                    break;
                case 0x500:
                    messageString = "Invalid frequency.";
                    break;
                case 0x501:
                    messageString = "Channel occupied.";
                    break;
                case 0x502:
                    messageString = "Transmitter already on.";
                    break;
                case 0x503:
                    messageString = "Antenna not connected.";
                    break;
                case 0x504:
                    messageString = "The module has exceeded the maximum or minimum operating "
                            + "temperature anf will not allow an RF operation until it is back "
                            + "in range.";
                    break;
                case 0x505:
                    messageString = "The module has detected high return loss and has ended "
                            + "RF operations to avoid module damage.";
                    break;
                case 0x506:
                    messageString = "PLL not locked.";
                    break;
                case 0x507:
                    messageString = "Invalid antenna configuration.";
                    break;
                case 0x600:
                    messageString = "Not enough tags in tag ID buffer.";
                    break;
                case 0x601:
                    messageString = "Tag ID buffer full.";
                    break;
                case 0x602:
                    messageString = "Tag ID buffer repeated tag ID.";
                    break;
                case 0x603:
                    messageString = "Number of tags too large.";
                    break;
                case 0x7f00:
                    messageString = "System unknown error";
                    break;
                case 0x7f01:
                    messageString = "Software assert.";
                    break;
                default:
                    messageString = "Unknown status code 0x" + Integer.toHexString(status);
            }
            return messageString;
        }
    }
    public enum ReaderStatsFlag
    {
        NONE(0x00),
        
        TEMPERATURE(1 << 8);
        
        int value;

        ReaderStatsFlag(int v)
        {
            value = v;
        }
    }
public static class TemperatureStatusReport extends StatusReport
  {
    /* Temperature */
    int temperature = -1;

    public int getTemperature()
    {
        return temperature;
    }

    @Override
    public String toString()
    {
        StringBuilder tempReport = new StringBuilder();
        tempReport.append("Temperature : ");
        tempReport.append(temperature);
        tempReport.append(" C");
        return tempReport.toString();
    }
  }
/**
 * This object contains the information related to status reports
 * sent by the module during continuous reading
 */
  public static class StatusReport
  {
      
  }
  public static class ReaderStats
  {
      public int temperature = 0;
  }
   void notifyStatusListeners(StatusReport t) 
   {
       System.out.println(""+ t.toString());
   }
}
