/*
   Program to record polarization.
   RasPi connected to USB 1208LS.

   FARADAY ROTATION


   use Aout 0 to set laser wavelength. see page 98-100
   usage
   $ sudo ./faradayRotation <aoutstart> <aoutstop> <deltaaout> <comments_no_spaces>


   2015-12-31
   added error calculations. see page 5 and 6 of "FALL15" lab book
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
#include "mathTools.h" //includes stdDeviation
#include "interfacing/interfacing.h"
#include "faradayScanAnalysisTools.h"
#include "interfacing/waveMeter.h"

#define PI 3.14159265358979
#define STEPSIZE 7.0
#define STEPSPERREV 350.0
#define WAITTIME 2

#define BUFSIZE 1024

int recordNumberDensity(char* fileName);
float collectDiscreteFourierData(FILE* fp, int* photoDetector, int numPhotoDetectors,int motor, int revolutions);

int main (int argc, char **argv)
{
    int i;
	int upOrDown;
    int revolutions,dataPointsPerRevolution;
    time_t rawtime;
    float returnFloat;
    //float probeOffset,mag1Voltage,mag2Voltage;
    struct tm * timeinfo;
    char fileName[BUFSIZE], comments[BUFSIZE];
    char dailyFileName[BUFSIZE];
    char dataCollectionFileName[] = "/home/pi/.takingData"; 
    FILE *fp,*dataCollectionFlagFile,*configFile;
	float angle=-99;
	float desiredAngle,tempChange;


    if (argc==4){
        upOrDown=atof(argv[1]);
        desiredAngle=atof(argv[2]);
        strcpy(comments,argv[3]);
    } else { 
        printf("usage '~$ sudo ./stepTemperatureWaitForRotationAngle <up or down (0 down, 1 up)> <desired Angle> <comments in quotes>'\n");
        printf("    Don't forget to edit the config file!             \n");
        return 1;
    }

    // Indicate that data is being collected.
    dataCollectionFlagFile=fopen(dataCollectionFileName,"w");
    if (!dataCollectionFlagFile) {
        printf("unable to open file:\t%s\n",dataCollectionFileName);
        exit(1);
    }

    revolutions=1;
    dataPointsPerRevolution=(int)STEPSPERREV/STEPSIZE;

    // Set up interfacing devices
    initializeBoard();
    initializeUSB1208();


	if(upOrDown==0){
		tempChange=-.1;
	}else{
		tempChange=.1;
	}
	do{
		getPVCN7500(CN_RESERVE,&returnFloat);
		setSVCN7500(CN_RESERVE,returnFloat+tempChange);
		delay(1800000);
		//delay(180);
		configFile=fopen("/home/pi/RbControl/system.cfg","r");
		if (!configFile) {
			printf("Unable to open config file\n");
			exit(1);
		}

		// Get file name.  Use format "FDayScan"+$DATE+$TIME+".dat"
		time(&rawtime);
		timeinfo=localtime(&rawtime);
		struct stat st = {0};
		strftime(fileName,BUFSIZE,"/home/pi/RbData/%F",timeinfo); //INCLUDE
		if (stat(fileName, &st) == -1){
			mkdir(fileName,S_IRWXU | S_IRWXG | S_IRWXO );
		}
		strftime(fileName,BUFSIZE,"/home/pi/RbData/%F/FDayRotation%F_%H%M%S.dat",timeinfo); //INCLUDE
		strftime(dailyFileName,BUFSIZE,"/home/pi/RbData/%F/FDayRotation%F.dat",timeinfo); //INCLUDE

		printf("%s\n",fileName);
		printf("%s\n",comments);

		fp=fopen(fileName,"w");
		if (!fp) {
			printf("Unable to open file: %s\n",fileName);
			exit(1);
		}

		fprintf(fp,"#File:\t%s\n#Comments:\t%s\n",fileName,comments);

		getIonGauge(&returnFloat);
		//printf("IonGauge %2.2E Torr \n",returnFloat);
		fprintf(fp,"#IonGauge(Torr):\t%2.2E\n",returnFloat);

		getConvectron(GP_N2_CHAN,&returnFloat);
		//printf("CVGauge(N2) %2.2E Torr\n", returnFloat);
		fprintf(fp,"#CVGauge(N2)(Torr):\t%2.2E\n", returnFloat);

		getConvectron(GP_HE_CHAN,&returnFloat);
		//printf("CVGauge(He) %2.2E Torr\n", returnFloat);
		fprintf(fp,"#CVGauge(He)(Torr):\t%2.2E\n", returnFloat);

		getPVCN7500(CN_RESERVE,&returnFloat);
		fprintf(fp,"#CurrTemp(Res):\t%f\n",returnFloat);
		getSVCN7500(CN_RESERVE,&returnFloat);
		fprintf(fp,"#SetTemp(Res):\t%f\n",returnFloat);

		getPVCN7500(CN_TARGET,&returnFloat);
		fprintf(fp,"#CurrTemp(Targ):\t%f\n",returnFloat);
		getSVCN7500(CN_TARGET,&returnFloat);
		fprintf(fp,"#SetTemp(Targ):\t%f\n",returnFloat);

		char line[1024];
		fgets(line,1024,configFile);
		while(line[0]=='#'){
			fprintf(fp,"%s",line);
			fgets(line,1024,configFile);
		}

		fclose(configFile);

		fprintf(fp,"#Revolutions:\t%d\n",revolutions);
		fprintf(fp,"#DataPointsPerRev:\t%d\n",dataPointsPerRevolution);
		fprintf(fp,"#NumVoltages:\t%d\n",1);
		fprintf(fp,"#PumpWavelength:\t%f\n",getWaveMeter());
		//fprintf(fp,"#ProbeWavelength:\t%f\n",getProbeFreq());

		// UNCOMMENT THE FOLLOWING LINES WHEN COLLECTING STANDARD DATA
		int photoDetectors[] = {PUMP_LASER,PROBE_LASER,REF_LASER};
		char* names[]={"PMP","PRB","REF"};
		// UNCOMMENT THE FOLLOWING LINES WHEN USING THE FLOATING PD
		//int photoDetectors[] = {PROBE_LASER,PUMP_LASER,REF_LASER};
		//char* names[]={"PRB","PMP","REF"};
		int numPhotoDetectors = 3;
		int motor = PROBE_MOTOR;

		// Write the header for the data to the file.
		fprintf(fp,"STEP");
		for(i=0;i<numPhotoDetectors;i++){
			fprintf(fp,"\t%s\t%ssd",names[i],names[i]);
		}
		fprintf(fp,"\n");
		fclose(fp);

		fp=fopen(fileName,"a");

		homeMotor(motor);
		angle=collectDiscreteFourierData(fp, photoDetectors, numPhotoDetectors, motor, revolutions);

		printf("The measured angle was: %3.1f\n",angle);

		fclose(fp);

		printf("Processing Data...\n");
		analyzeData(fileName, 1, revolutions, dataPointsPerRevolution);
	}while(angle > desiredAngle + .4 || angle < desiredAngle -.4);

    closeUSB1208();

    // Remove the file indicating that we are taking data.
    fclose(dataCollectionFlagFile);
    remove(dataCollectionFileName);

    return 0;
}

float collectDiscreteFourierData(FILE* fp, int* photoDetector, int numPhotoDetectors,int motor, int revolutions)
{
    float sumSin=0;
    float sumCos=0;
    int count=0;
    int steps,i,j,k;
	float angle;
    float laserAngle=-100;


    int nSamples=16;
	float* measurement = malloc(nSamples*sizeof(float));

    
    float* involts = calloc(numPhotoDetectors,sizeof(float));
    float* stdDev = calloc(numPhotoDetectors,sizeof(float));

    for (k=0;k<revolutions;k++){ //revolutions
        for (steps=0;steps < STEPSPERREV;steps+=(int)STEPSIZE){ // steps
            // (STEPSPERREV) in increments of STEPSIZE
            delay(300); // watching the o-scope, it looks like it takes ~100ms for the ammeter to settle after a change in LP. UPDATE: with the Lock-in at a time scale of 100 ms, it takes 500 ms to settle. 
            // With time scale of 30 ms, takes 300 ms to settle.

            //get samples and average
            for(j=0;j<numPhotoDetectors;j++){ // numPhotoDet1
                involts[j]=0.0;	
                for (i=0;i<nSamples;i++){ // nSamples
                        getUSB1208AnalogIn(photoDetector[j],&measurement[i]);
                        involts[j]=involts[j]+fabs(measurement[i]);
                        delay(WAITTIME);
                } // nSamples
                involts[j]=involts[j]/(float)nSamples; 
                stdDev[j]=stdDeviation(measurement,nSamples);
            } // numPhotoDet1

            fprintf(fp,"%d\t",steps+(int)STEPSPERREV*k);
            for(j=0;j<numPhotoDetectors;j++){
                if(j!=numPhotoDetectors-1)
                    fprintf(fp,"%f\t%f\t",involts[j],stdDev[j]);
                else
                    fprintf(fp,"%f\t%f\n",involts[j],stdDev[j]);
            }
            angle=2.0*PI*(steps)/STEPSPERREV;
            sumSin+=involts[1]*sin(2*angle);
            sumCos+=involts[1]*cos(2*angle);
			

            count++;
            stepMotor(motor,CLK,(int)STEPSIZE);
        } // steps
		laserAngle=180/PI*0.5*atan2(sumCos,sumSin);
    } // revolutions
    free(measurement);
    free(stdDev);
    free(involts);
	
	return laserAngle;
}