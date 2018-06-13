#ifndef _COMPASS_H_
#define _COMPASS_H_

struct compass{
	float bearing;
	int xMag;
	int yMag;
	int zMag;
};

typedef struct compass comp_t;

extern void getBearing(comp_t *);


#endif
