/*
 * Copyright (c) 2008 ThingMagic, Inc.
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
package com.thingmagic;

/**
 * This class is a namespace for Gen2-specific subclasses of generic
 * Mercury API classes, Gen2 data structures, constants, and Gen2
 * convenience functions.
 */
public class Gen2
{

  // non-public
  Gen2() { }

/**
 * This class extends {@link TagData} to represent the details of a Gen2 RFID tag.
 */
  public static class TagData extends com.thingmagic.TagData 
  {
    final byte[] pc;

    public TagData(byte[] bEPC)
    {
      super(bEPC);

      pc = new byte[2];
      pc[0] = (byte)((epc.length) << 3);
      pc[1] = 0;
    }

    public TagData(byte[] bEPC, byte[] newPC)
    {
      super(bEPC);

      pc = newPC.clone();
    }

    public TagData(byte[] bEPC, byte[] crc, byte[] newPC)
    {
      super(bEPC, crc);

      pc = newPC.clone();
    }

    public TagData(String sEPC)
    {
      super(sEPC);

      pc = new byte[2];
      pc[0] = (byte)((epc.length) << 3);
      pc[1] = 0;
    }

    public TagData(String sEPC, String sCrc)
    {
      super(sEPC,sCrc);

      pc = new byte[2];
      pc[0] = (byte)((epc.length) << 3);
      pc[1] = 0;
    }

    public TagProtocol getProtocol()
    {
      return TagProtocol.GEN2;
    }

    public byte[] pcBytes()
    {
      return pc.clone();
    }

    @Override
    boolean checkLen(int epcbytes)
    {
      if (epcbytes < 0)
      {
        return false;
      }
      if (epcbytes > 62)
      {
        return false;
      }
      if ((epcbytes & 1) != 0)
      {
        return false;
      }
    return true;
    }

    public String toString()
    {
      return String.format("GEN2:%s", epcString());
    }

  }

}