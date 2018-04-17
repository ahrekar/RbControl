/*
   notes and comments 
   useful information
   to follow

*/

#include <stdio.h>
#include <stdlib.h>
#include "kenBoard.h"

#define HEAD 0xC0
#define TA 196

int initializeLaser(void);
int initializeTA(void);
int setLaserStatus(unsigned short status);
float getLaserTemperature(void);
int setLaserTemperature(float temperature);
int setTACurrent(int current);
// chan is the rs485 channel