/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

package com.thingmagic;

/**
 *
 * @author QSSLP170
 */
public class GpioPin
{
     final public int id;
    final public boolean high;
    final public boolean output;

    public GpioPin(int id, boolean high)
    {
      this.id = id;
      this.high = high;
      this.output = false;
    }

    public GpioPin(int id, boolean high, boolean output)
    {
      this.id = id;
      this.high = high;
      this.output = output;
    }

    @Override public boolean equals(Object o)
    {
      if (!(o instanceof GpioPin))
      {
        return false;
      }
      GpioPin gp = (GpioPin)o;
      return (this.id == gp.id) && (this.high == gp.high) && (this.output == gp.output);
    }

    @Override
    public int hashCode()
    {
      int x;
      x = id * 2;
      if (high)
      {
        x++;
      }
      return x;
    }

    @Override
    public String toString()
    {
      return String.format("%2d%s%s", this.id, this.high?"H":"L", this.output?"O":"I");
    }
    
}
