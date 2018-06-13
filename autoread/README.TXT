=====================================

How to run autonomous read code lets and Autonomous Configuration Tool

=====================================
Autonomous Configuration Tool :
Tool requires minimum Java version 1.8.
Windows
 Double click on Windows executables to install the tool.
Linux 
 Use following command to run the tool
 sh AutonomousConfigurationToolLinux32.sh   for 32 bit machine 
 sh AutonomousConfigurationToolLinux64.sh   for 64 bit machine.
Note: On Linux, tool will not prompt any message if Java required version not installed i.e minimum 1.8, where as for windows it will prompt.
 
 

Java :

1. Go to the folder java/AutonomousRead/dist through command prompt.

2. Run the following command 
  
  Java  -jar   AutonomousRead.jar   </comport>
	
  Example: 
  For Windows   Java  -jar  AutonomousRead.jar  /COM1 	
  
  For Linux     Java  -jar  AutonomousRead.jar  /dev/ttyUSB0
  For Mac       Java  -jar  AutonomousRead.jar  /dev/XXX
    
     (OR)		


1. Import the AutonomousRead project in Netbeans.

2. Right click on the project and then click on "Set Configuration" and select customize.

3. Under categories select "Run" and enter the com port(/COM1 or /dev/ttyUSB0) value for the field "Arguments:" . 

4. Click "Ok".

5. Right click on the project then click on "Run".
              
    			   



C:

For Windows:


1.Go to the c\proj\ReceiveAutonomousReading.


2.Click on the ReceiveAutonomousReading visual studio solution file.


3.Right click on the project and select build.


4.Go to the c\proj\ReceiveAutonomousReading/Debug or c\proj\ReceiveAutonomousReading/Release through command prompt.


5.Run the following command 
  
  
	AutonomousRead.exe   </comport>
	

  	Example: 
  For Windows   AutonomousRead.exe  /COM1 	
  


For Linux and Mac:


1. Go to the c\src through command prompt.


2.Type make and hit enter.

3.Run the following command 
  
  
	./autonomousRead   </comport>
	
  
	Example: 
  For Linux/Mac   ./autonomousRead  /dev/ttyUSB0


C# :
1.Go to the cs folder.
2.Click on the ReceiveAutonomousReading visual studio solution file(Visual studio 2010 project).
3.Right click on the project and select build.
4.Go to the cs\ReceiveAutonomousReading\bin\Debug or cs\ReceiveAutonomousReading\bin\Release through command prompt.
5.Run the following command.
	ReceiveAutonomousReading.exe
