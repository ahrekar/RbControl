/*
   Program to record polarization.
   RasPi connected to USB 1208LS.
   Target energy: USB1208LS Analog out Ch1 controls HP3617A. See pg 31 my lab book
   PMT Counts: data received from CTR in USB1208
*/

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <asm/types.h>
#include <wiringPi.h>
#include "fileTools.h"
#include "polarizationAnalysisTools.h"
#include "interfacing/interfacing.h"
#ifndef DEFINITIONS_H
#define DEFINITIONS_H
	#include "mathTools.h"
#endif

#define REVOLUTIONS 1
#define STEPSPERREV 1200 
#define DATAPOINTS (DATAPOINTSPERREV * REVOLUTIONS)
#define PI 3.14159265358979
#define HPCAL 28.1/960.0

int getPolarizationData(char* fileName, int aout, int dwell);
void plotData(char* fileName);

int main (int argc, char **argv)
{
	int aout;
	int dwell;
	
	char analysisFileName[80],backgroundFileName[80],rawDataFileName[80],comments[1024]; 
	char dataCollectionFileName[] = "/home/pi/.takingData"; 


	// Variables for getting time information to identify
	// when we recorded the data
	time_t rawtime;
	struct tm * timeinfo;
	float returnFloat;
	char* extension;
	
	// Setup time variables
	time(&rawtime); 
	timeinfo=localtime(&rawtime); 
	struct stat st = {0};

	// Get parameters.
	if (argc==4){
		aout=atoi(argv[1]);
		dwell=atoi(argv[2]);
		strcpy(comments,argv[3]);
		strcpy(backgroundFileName,"NONE");
	} else {
		printf("There is one option for using this program: \n\n");
		printf("usage '~$ sudo ./polarization <aout_for_HeTarget> <dwell> <comments_in_double_quotes>'\n");
		printf("                                (0-1023)           (1-5)s                            '\n");
		return 1;
	}

	// Indicate that data is being collected.
	FILE* dataCollectionFlagFile;
	dataCollectionFlagFile=fopen(dataCollectionFileName,"w");
	if (!dataCollectionFlagFile) {
		printf("unable to open file \n");
		exit(1);
	}

	initializeBoard();
	initializeUSB1208();

	// RUDAMENTARIY ERROR CHECKING
	if (aout<0) aout=0;
	if (aout>1023) aout=1023;

	// Create file name.  Use format "EX"+$DATE+$TIME+".dat"
	strftime(analysisFileName,80,"/home/pi/RbData/%F",timeinfo); 
	if (stat(analysisFileName, &st) == -1){
		mkdir(analysisFileName,S_IRWXU | S_IRWXG | S_IRWXO );
	}
	strftime(rawDataFileName,80,"/home/pi/RbData/%F/POL%F_%H%M%S.dat",timeinfo); 
	strcpy(analysisFileName,rawDataFileName);
	extension = strstr(analysisFileName,".dat");
	strcpy(extension,"analysis.dat");

	printf("\n%s\n",analysisFileName);
	FILE* rawData;
	// Write the header for the raw data file.
	rawData=fopen(rawDataFileName,"w");
	if (!rawData) {
		printf("Unable to open file: %s\n", rawDataFileName);
		exit(1);
	}

	fprintf(rawData,"#File\t%s\n",rawDataFileName);
	fprintf(rawData,"#Comments\t%s\n",comments);

	getIonGauge(&returnFloat);
	printf("IonGauge %2.2E Torr \n",returnFloat);
	fprintf(rawData,"#IonGauge(Torr):\t%2.2E\n",returnFloat);

	getConvectron(GP_N2_CHAN,&returnFloat);
	printf("CVGauge(N2) %2.2E Torr\n", returnFloat);
	fprintf(rawData,"#CVGauge(N2)(Torr):\t%2.2E\n", returnFloat);

	getConvectron(GP_HE_CHAN,&returnFloat);
	printf("CVGauge(He) %2.2E Torr\n", returnFloat);
	fprintf(rawData,"#CVGauge(He)(Torr):\t%2.2E\n", returnFloat);


	getPVCN7500(CN_RESERVE,&returnFloat);
	fprintf(rawData,"#CurrTemp(Res):\t%f\n",returnFloat);
	getSVCN7500(CN_RESERVE,&returnFloat);
	fprintf(rawData,"#SetTemp(Res):\t%f\n",returnFloat);
	getPVCN7500(CN_TARGET,&returnFloat);
	fprintf(rawData,"#CurrTemp(Targ):\t%f\n",returnFloat);
	getSVCN7500(CN_TARGET,&returnFloat);
	fprintf(rawData,"#SetTemp(Targ):\t%f\n",returnFloat);

	fprintf(rawData,"#Aout\t%d\n",aout);
	fprintf(rawData,"#Assumed USB1208->HP3617A conversion\t%2.6f\n",HPCAL);
	fprintf(rawData,"#REVOLUTIONS\t%d\n",REVOLUTIONS);
	fprintf(rawData,"#DataPointsPerRev\t%d\n",DATAPOINTSPERREV);
	fprintf(rawData,"#StepsPerRev\t%d\n",STEPSPERREV);
	fprintf(rawData,"#Datapoints\t%d\n",DATAPOINTS);
	fprintf(rawData,"#PMT dwell time (s)\t%d\n",dwell);

	fclose(rawData);

	// Collect raw data
	getPolarizationData(rawDataFileName, aout, dwell); 

	plotData(rawDataFileName);

	processFileWithBackground(analysisFileName,backgroundFileName,rawDataFileName,DATAPOINTSPERREV,REVOLUTIONS,comments);

	closeUSB1208();

	// Remove the file indicating that we are taking data.
	fclose(dataCollectionFlagFile);
	remove(dataCollectionFileName);

	return 0;
}

int getPolarizationData(char* fileName, int aout, int dwell){
	// Variables for stepper motor control.
	int nsteps,steps,ninc,i;

	// Variables for data collections.
	long returnCounts;
	float current,angle;
	float currentErr;
	int nSamples = 8;
	float* measurement = calloc(nSamples,sizeof(float));

	// Write Aout for He traget here
	setUSB1208AnalogOut(HETARGET,aout);//sets vout such that 0 v at the probe laser
	// NOTE THAT THIS SETS THE FINAL ELECTRON ENERGY. THIS ALSO DEPENDS ON BIAS AND TARGET OFFSET.  AN EXCIATION FN WILL TELL THE
	// USER WHAT OUT TO USE, OR JUST MANUALLY SET THE TARGET OFFSET FOR THE DESIRED ENERGY

	// Begin File setup
	FILE* rawData=fopen(fileName,"a");
	if (!rawData) {
		printf("Unable to open file: %s\n",fileName);
		exit(1);
	}
	// End File setup

	homeMotor(POL_MOTOR); 

	nsteps=STEPSPERREV*REVOLUTIONS;
	ninc=STEPSPERREV/DATAPOINTSPERREV; // The number of steps to take between readings.

	fprintf(rawData,"Steps\tCount\tCurrent\tCurrent Error\tAngle\n");// This line withough a comment is vital for being able to quickly process data. DON'T REMOVE
	printf("Steps\tCounts\tCurrent\n");

	for (steps=0;steps<nsteps;steps+=ninc){
		stepMotor(POL_MOTOR,CCLK,ninc); 

		getUSB1208Counter(dwell*10,&returnCounts);

		current=0.0;
		for (i=0;i<nSamples;i++){
			getUSB1208AnalogIn(K617,&measurement[i]);
			current += measurement[i];
		}

		current = current/(float)nSamples;
		currentErr = stdDeviation(measurement,nSamples);
		angle = (float)steps/STEPSPERREV*2.0*PI;

		printf("%d\t%ld\t%f\n",steps,returnCounts,current);
		fprintf(rawData,"%d\t%ld\t%f\t%f\t%f\n",steps,returnCounts,current,currentErr,angle);
	}
	fclose(rawData);

	// Reset Helium Target Offset back to zero
	setUSB1208AnalogOut(HETARGET,0);

	return 0;
}

void plotData(char* fileName){
	FILE* gnuplot;
	char buffer[1024];
	// Create rough graphs of data.
	gnuplot = popen("gnuplot","w"); 

	if (gnuplot != NULL){
		fprintf(gnuplot, "set terminal dumb size 158,32\n");
		fprintf(gnuplot, "set output\n");			
		
		sprintf(buffer, "set title '%s'\n", fileName);
		fprintf(gnuplot, buffer);

		fprintf(gnuplot, "set key autotitle columnheader\n");
		fprintf(gnuplot, "set xlabel 'Step'\n");			
		fprintf(gnuplot, "set ylabel 'Counts'\n");			
		fprintf(gnuplot, "set yrange [0:*]\n");			
		sprintf(buffer, "plot '%s' using 1:2\n",fileName);
		fprintf(gnuplot, buffer);
		fprintf(gnuplot, "unset output\n"); 
		fprintf(gnuplot, "set terminal png\n");
		sprintf(buffer, "set output '%s.png'\n", fileName);
		fprintf(gnuplot, buffer);
		sprintf(buffer, "plot '%s' using 1:2\n",fileName);
		fprintf(gnuplot, buffer);
	}
	pclose(gnuplot);
}
