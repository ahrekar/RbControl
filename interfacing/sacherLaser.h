/*
   notes and comments 
   useful information
   to follow

*/

#include <stdio.h>
#include <stdlib.h>
#include "kenBoard.h"

#define HEAD 0xC2
#define TA 0xC6

int initializeSacherLaser(void);
int initializeSacherTA(void);
int setLaserStatus(unsigned short status);
float getLaserTemperature(void);
int setLaserTemperature(float temperature);
int setTACurrent(int current);
float getTACurrent(void);
// chan is the rs485 channel
