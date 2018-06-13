#ifndef _GPS_H_
#define _GPS_H_

struct location {
    double time;     //hhmmss	
    double latitude; 
    double longitude;
    double speed;    // speed over ground in knots
    double altitude; 
    double course;   // True course or track made good in degrees true
    double satellites;
    //double heading;
    //double bearing;
};
typedef struct location loc_t;

// Initialize device
extern void gps_init(void);
extern void gps_init1(void);
// Activate device
extern void gps_on(void);
// Get the actual location
extern void gps_location(loc_t *, int);


// Turn off device (low-power consumption)
extern void gps_off(void);

// -------------------------------------------------------------------------
// Internal functions
// -------------------------------------------------------------------------

// convert deg to decimal deg latitude, (N/S), longitude, (W/E)
void gps_convert_deg_to_dec(double *, char, double *, char);
double gps_deg_dec(double);

#endif

