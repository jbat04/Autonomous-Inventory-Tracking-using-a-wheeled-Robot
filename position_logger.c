#include "gps.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <thread>
#include <math.h>
#include <unistd.h>


void gps1()
{
    // Open
    gps_init();
    loc_t data1;

    gps_init1();

    loc_t data2;

   double aveLat = 0;
   double aveLong = 0;

    while (1) {
	gps_init();
	loc_t data1;
	//sleep(1);
	gps_location(&data1,1);
        gps_location(&data2,2);

	int time1 = (int)data1.time;
	double long1 =data1.longitude;
	double lat1 = data1.latitude;
	double sat1 = data1.satellites;

        int time2 = (int)data2.time;

        double lat2 = data2.latitude; 
        double long2=data2.longitude;
        double sat2 = data2.satellites;


	printf("\nGPS 1\n");
	printf("time   latitude	   longitude\n");
	printf("%d %lf %lf\n", time1, lat1, long1);
	printf("Satellites: %f\n\n", sat1);


        printf("\nGPS 2\n");
        printf("time   latitude    longitude\n");
        printf("%d %lf %lf\n", time2, lat2, long2);
        printf("Satellites: %f\n\n", sat2);

	aveLat = (lat2 + lat1)/2;
	aveLong = (long2 + long1)/2;

	printf("\nAverages\n");
        printf("time   latitude    longitude\n");
        printf("%d %lf %lf\n", time2, aveLat, aveLong);


	}
}

int main()
{
	std::thread t1(gps1);
	sleep(2);

	t1.join();
    return EXIT_SUCCESS;
}
