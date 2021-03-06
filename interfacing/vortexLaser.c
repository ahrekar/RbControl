/*
   notes and comments 
   useful information
   to follow
*/

#include "vortexLaser.h"

int initializeVortex(void){

	char retData[32];
	char outData[32];
	int j;

	strcpy(outData,":OUTP ?");
	writeRS485to232Bridge(outData,retData,VORTEX);
j=-1;
	if (strcmp(retData,"1")==0){
		// laser is on. and talking to us. do nuthing
		j=0;
	} else {
		if (strcmp(retData,"0")==0) {
			// laser is off, but talking 	

//		setLaserOnOff(1);

			j=0;
		} else {
			// somthing else is wrong
			printf(retData);
			printf("\n");
		}
	}
	return j;
}

int setLaserOnOff(int state){
	char retData[32];
	char outData[32];
	int j;
	sprintf(retData,"%d",state);
	strcpy(outData,":OUTP ");
	strcat(outData,retData);

	writeRS485to232Bridge(outData,retData,VORTEX);
	return 0;
}


int setVortexPiezo(float volts){
	char retData[32];
	char outData[32];
	int j;

	j=-1;

	if ((volts >= 0.0) & (volts<=117.5)){
		sprintf(retData,"%2.1f",volts);
		strcpy(outData,":SOUR:VOLT:PIEZ ");
		strcat(outData,retData);
	
		writeRS485to232Bridge(outData,retData,VORTEX);
		j=strlen(retData);  // not expecting much back from Vortex. If we get anything, it will be a parsing, or NO RESPONSE message
		//if (j!=0) printf(retData);
	}

	return j;
}

int getVortexPiezo(float* volts){
	char retData[32];
	char outData[32];

	strcpy(outData,":SOUR:VOLT:PIEZ ?");

	writeRS485to232Bridge(outData,retData,VORTEX);
	*volts=atof(retData);
	return 0;
}
// chan is the rs485 channel
