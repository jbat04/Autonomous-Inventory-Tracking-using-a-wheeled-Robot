/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.thingmagic;



/**
 * Sample programs that pull the autonomous streaming and prints the tags
 * As there is no auto port detection in Java, this sample program expects autonomous
 * streaming port as argument.
 */
public class ReceiveAutonomousReading
{

  static void usage()
  {
    System.out.printf("Usage: As there in auto port detection in Java, this sample program expects "
            + "autonomous streaming port as argument. \n Please provide reader URL such as /COM12 or /dev/ttyS0");
    System.exit(1);
  }
   
    public static void main(String[] argv)
    {
        
        try
        {
            if(argv.length < 1)
            {
                usage();
            }
            if(!argv[0].startsWith("/"))
            {
               usage(); 
            }
            
            boolean isConnected = false;
            SerialTransport serialTransport = new SerialTransport();
            isConnected = serialTransport.connect(argv[0]);          
            if (isConnected)
            {
                serialTransport.receiveStreaming();
            }           
        }
        catch (ReaderException re)
        {
            re.printStackTrace();
        } 
        catch (Exception ex)
        {
            ex.printStackTrace();
        }
    }
}
