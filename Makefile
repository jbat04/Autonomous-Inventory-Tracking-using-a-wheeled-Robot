roboclaw: robot.cpp roboclaw.cpp serial.c nmea.c gps.c compass.c linker.cpp
	g++ -o roboclaw robot.cpp roboclaw.cpp serial.c nmea.c gps.c compass.c -lm -lpthread -lwiringPi -g
	g++ -o link linker.cpp -g
