#include "roboclaw.h"
#include <iostream>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
int main()
{
    	RoboClaw test( "/dev/roboclaw", 5, 0x80 );
    	test.begin( B1152000 );
    	//test.SpeedAccelDeccelPositionM1(1000,10000,10000,10000,1);
	int statuse = system("./autoread/c/src/receiveAutonomousReading");
}
