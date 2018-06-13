//Main File for Robot
//Robot.cpp
//Author: Team ECE 18.6
//Members: Jesse Batstone, Trung Doan, Jinyanzi Luo, Anna Keziah Pidong
//Date Created: 3/7/2018
//Last Updated: 5/21/2018

#include "roboclaw.h"
#include "gps.h"
#include "compass.h"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <math.h>
#include <fcntl.h>
#include <wiringPi.h>

using namespace std;

char dir;	//control variable
const double ROBOSPEED =2500;	//speed of robot

//waypoints
//double WAYLATS[] = {47.60766,47.60765,47.60781,47.60787,47.60780,0};
//double WAYLONGS[] ={-122.31519,-122.31490,-122.31498,-122.31514,122.31526,0};

double WAYLATS[] = {47.607912,47.607912,47.607813,47.607787,47.607780,0};
double WAYLONGS[] ={-122.315203,-122.315052,-122.315048,122.315108,-122.315165,0};


const int NUMPOINTS = sizeof(WAYLATS);	//number of waypoints+1 in path

//Used for consle window
double globalHeading = 0;	//Communicate heading b/w getGPS() and contRobo()
//GET GPS LOCATION
void getGPS()
{
	ofstream outfile("gpsData.txt"); //file to store GPS data
	double latWay = WAYLATS[0];
	double longWay = WAYLONGS[0];
	int count = 0;		//used for counting waypoints
	double heading = 0;	//local heading var0
	double X, Y;		//Used for heading calculations
	double degToRad = M_PI/180;	//conversion constant
	double radToDeg = 180/M_PI;	//conversion constant
	double aveLat = 0;		//differential GPS
	double aveLong = 0;
	gps_init();			//initializes first gps
	loc_t data;
	gps_init1();			//initialize second gps
	loc_t data1;
	outfile << "RUN START\n\n";
	sleep(1);
   	while(1){
		gps_init(); //too get current time must initialize in loop
        	gps_location(&data,1);	//get gps location
		gps_location(&data1,2);
		aveLat = (data.latitude + data1.latitude) /2;	//averaging gps
		aveLong = (data.longitude + data1.longitude)/2;
		//If all waypoints traversed, stop motors
		if(count >= NUMPOINTS-1){
			printf("\nAll %d traversed, stopping motors\n", NUMPOINTS-1);
			dir = 'q';
		}
		//If robot location is close to waypoint move to next wone
		if((abs(latWay-aveLat)<.000030 && abs(longWay-aveLong)<.000025) && (count < NUMPOINTS-1)){
			printf("Reached Waypoint: %f,%f", WAYLATS[count],WAYLONGS[count]);
			outfile << "Reached Waypoint: " << fixed<<(double)WAYLATS[count]<<" "<<(double)WAYLONGS[count] << "\n";
			count++;
        		latWay = WAYLATS[count];
        		longWay = WAYLONGS[count];

		}
		//Calcuate heading from gps location & waypoint
		if(longWay !=0 && latWay!=0 && aveLat!=0 && aveLong !=0){	//Heading calculation
			X = sin((longWay - aveLong)*degToRad)*cos(latWay*degToRad);
			Y = cos(aveLat*degToRad)*sin(latWay*degToRad)-sin(aveLat*degToRad)*
			cos(latWay*degToRad)*cos((longWay-aveLong)*degToRad);
			heading = atan2(X,Y)*radToDeg;
			if(heading < 0){
				heading =  heading+360;
			}
		}
		//output gps location and time to file for linking later
		outfile << (int)data.time <<" " << (double)data.course << " " << (double)aveLat 
			<< " " << fixed<<(double)aveLong << " "<< (int)heading <<"\n";
		outfile.flush();
		printf("time    latitude   longitude\n");
		printf("%d %f %f\n\n", (int)data.time ,aveLat, aveLong);
		globalHeading = heading;
		sleep(.1);
		if(dir == 'q'){
			outfile << "\n\nRUN END\n\n";
			outfile.flush();
			outfile.close();
			gps_off();
			break;
		}
	}
}

//ROBOT CONTROL
void contRobo(){

	RoboClaw robo( "/dev/roboclaw", 5, 0x80 );	//roboclaw port open
	robo.begin(B1152000);
	robo.SetM1VelocityPID(6765, 1501, 0, 6226);	//PID M1
        robo.SetM2VelocityPID(6490, 1546, 0, 6286);	//PID M2

	double direction = 0;
	double distance;
	comp_t compass;
	sleep(1);

        while(dir!='q'){

		getBearing(&compass);	//get compass data
		direction = globalHeading - compass.bearing;	//what direction robot needs to go, in degrees
		printf("\nBearing: %f\nHeading: %f\nDirection: %f\n\n"
		, compass.bearing, globalHeading, direction);

		if(dir== 'p'){
		//turn on left motor, turn right
                if((direction > 15 && direction < 180) || direction < -180){
			if(direction < -180){direction = 360+direction;}
			distance = direction* 16 - 42;
                        printf("Right turn: %f\n", direction);
                        robo.SpeedAccelDistanceM1(0,0,0);
                        robo.SpeedAccelDistanceM2(ROBOSPEED,1000,distance);
                        sleep(2);
		//turn on right motor, turn left
                }else if((direction < -15 && direction>-180) || direction>180){
                        if(direction<0){direction = abs(direction);}
			else if(direction>180){direction = 360-direction;}
			distance = direction* 16 - 42;
			printf("Left turn: %f\n", direction);
                        robo.SpeedAccelDistanceM2(0,0,0);
                        robo.SpeedAccelDistanceM1(ROBOSPEED,1000,distance);
                        sleep(2);
		//go straight
                }else{
                        //robo.SpeedAccelM1M2(0,0,0);
                        printf("Straight\n\n Direction: %f", direction);
                        robo.SpeedAccelDistanceM1(ROBOSPEED,ROBOSPEED,2000);
			robo.SpeedAccelDistanceM2(ROBOSPEED,ROBOSPEED,2000);
			sleep(2);
                        robo.SpeedAccelDistanceM1(100,100,100);
                        robo.SpeedAccelDistanceM2(100,100,100);
                        sleep(2);
                }}



		sleep(1);




	//MOTOR KEYBOARD OVERIDE
	switch(dir){
	case 'w':
                robo.SpeedAccelM1M2(1000,1000,1000);
		break;
	case 's':
		robo.SpeedAccelM1M2(1000,-1000,-1000);
            	break;
        case 'a':
		robo.SpeedAccelM1M2(1000,1000,-1000);
            	break;
        case 'd':
		robo.SpeedAccelM1M2(1000,-1000,1000);
           	break;
	case 'm':
		robo.SpeedAccelM1M2(1000,-1000,-1000);
		dir = 'p';
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
}

//COnstantly checks if a 'q' is pressed to end the program
void exitProgram()
{
	cout << "Type Q to Stop\n";
        while(dir != 'q'){
                cin >> dir;
		sleep(.05); 
		if(dir == 'q'){	//hard kills rfid program
			system("killall receiveAutonomousReading");
		}
        }

}


void sonarSensor()
{
	int uselessSonarData;
	int usefulSonarData;
	char sonarData[5];
    	int ss = open ( "/dev/ttyAMA0", O_RDWR);
    	while(dir!='q'){
        	read(ss, &sonarData, 5);
		int intSonarData = atoi(sonarData); 	//convert string into interger
		sleep(.5);
		// filter useful and useless sonar data
		if(intSonarData < 300){
			uselessSonarData = intSonarData;
		}

		else{
			usefulSonarData = intSonarData;

			// Object avoidance code
			if(usefulSonarData <= 500 && dir == 'p'){
				dir = 'm';
				printf("\nBACKING OFF, distance ahea: %i \n", usefulSonarData);
				sleep(1);
				//dir = 'p';
			}

			//else{ 
			//	dir = 'p'; //search
			//	printf("\nI AM SEARCHING, distance ahead: %i\n", usefulSonarData);
			//}

			sleep(.5);

		}//end of else






	}//end of while loop

}//end of sonar sensor function

void rfidReader(){
	int status = system("./autoread/c/src/receiveAutonomousReading");

	while(1){
		sleep(.5);
		if(dir=='q'){
			break;
		}
	}


}



int main()
{

	dir = '-';
	thread t1(getGPS);
	thread t2(sonarSensor);
	thread t3(contRobo);
	thread t4(exitProgram);
	thread t5(rfidReader);

	t1.join();
	t2.join();
	t3.join();
	t4.join();
	t5.join();

	int status1 = system("./link");	//call linker program
	sleep(1);
}

