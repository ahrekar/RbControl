
/*
   program to set analog output

   RasPi connected to USB 1208LS.

   Sets analog voltage for probe laser. Uses Analog out port 0. Final output to probe laser is through 
   op-amp circuit. see page 98.  Page 99 shows calibration data.

   Usage '$ sudo ./setProbeLaser xxx' where xxx is an integer value between 0 and 1024
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <asm/types.h>
#include "interfacing/topticaLaser.h"
#include "interfacing/USB1208.h"
#include "interfacing/kenBoard.h"



int main (int argc, char *argv[])
{
	float value;
	int laserSock;

	initializeBoard();
	initializeUSB1208();
	laserSock=initializeLaser();

	if (argc==2) {
		value=atof(argv[1]);
	}else{
		printf("Usage '$ sudo ./setPumpLaser xxx' where xxx is the desired scan offset.\n");

		printf(" GETTING OFFSET \n");
		value=getScanOffset(laserSock);
		printf("Pump Offset is: %f\n",value);

		printf(" GETTING CURRENT \n");
		value=getMasterCurrent(laserSock);
		printf("Pump Current is: %f\n",value);

		return 0;
	}

	if (value<0) value=0;

	if (value>1023) value=1023;

	setScanOffset(laserSock, value);

	closeUSB1208();
	close(laserSock);

	printf("Scan Offset %3.1f \n",value);

	return 0;
}
