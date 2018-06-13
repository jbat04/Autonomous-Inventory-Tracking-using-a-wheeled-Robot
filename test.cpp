//Main File for Robot
//Robot.cpp
//Author: Team ECE 18.6
//Members: Jesse Batstone, Trung Doan, Jinyanzi Luo, Anna Keziah Pidong
//Date: 3/7/2018

#include "roboclaw.h"
#include "gps.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include <curses.h>

using namespace std;


//GET GPS LOCATION
void getGPS(loc_t data) {
	
	    printf("time  course    latitude   longitude\n");

        gps_location(&data);

        printf("%d %f %f %f\n", (int)data.time, data.course,
        data.latitude, data.longitude);


}

//ROBOT CONTROL
void contRobo(char dir, RoboClaw robo) {
	switch(dir){
	case 'w':
		robo.ForwardMixed(40);
		robo.TurnRightMixed(0);
		break;
	case 's':
            	robo.BackwardMixed(40);
            	robo.TurnRightMixed(0);
            	break;
        case 'a':
            	robo.ForwardMixed(40);
            	robo.TurnRightMixed(40);
            	break;
        case 'd':
           	robo.ForwardMixed(40);
            	robo.TurnLeftMixed(40);
           	break;
        case 'q':
            	robo.ForwardMixed(0);
            	robo.TurnRightMixed(0);
            	break;
	default:
		robo.ForwardMixed(0);
		robo.TurnRightMixed(0);

		}


}


int main()
{
	RoboClaw robo( "/dev/roboclaw", 5, 0x80 );
	char version[512];
	robo.begin( B1152000 );
    	robo.ReadVersion( version );
   	cout << "ReadVersion: " << version << flush;
	
	gps_init();
    	loc_t data;

	char dir = '-';
	while(dir !='q'){
			cout << "Direction: ";
			cin >> dir;
			contRobo(dir, robo);
			getGPS(data);
	}
}
