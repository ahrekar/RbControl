/*
   Program to record polarization.
   RasPi connected to USB 1208LS.

   FARADAY SCAN


   use Aout 0 to set laser wavelength. see page 98-100
   usage
   $ sudo ./faradayscan <aoutstart> <aoutstop> <deltaaout> <comments_no_spaces>


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
#include "probeLaserControl.h"

#define PI 3.14159265358979
#define NUMSTEPS 350
#define STEPSIZE 7
#define STEPSPERREV 350.0
#define DATAPTSPERREV STEPSPERREV/STEPSIZE
#define WAITTIME 2

#define BUFSIZE 1024

int plotData(char* fileName);
int recordNumberDensity(char* fileName);
void collectDiscreteFourierData(FILE* fp, int* photoDetector, int numPhotoDetectors,int motor, int revolutions);

int main (int argc, char **argv)
{
    float value, returnFloat, wavelength;
    int revolutions;

    /* Variables for generating a file name with date and time */
	time_t rawtime;
	struct tm * timeinfo;

	char fileName[BUFSIZE], comments[BUFSIZE];
	char dailyFileName[BUFSIZE];
	char dataCollectionFileName[] = "/home/pi/.takingData"; 

	FILE *fp,*dataCollectionFlagFile,*configFile;


	if (argc==2){
		strcpy(comments,argv[1]);
	} else { 
		printf("usage '~$ sudo ./faradayScan <comments in quotes>'\n");
		return 1;
	}

	// Indicate that data is being collected.
	dataCollectionFlagFile=fopen(dataCollectionFileName,"w");
	if (!dataCollectionFlagFile) {
		printf("unable to open file:\t%s\n",dataCollectionFileName);
		exit(1);
	}

    revolutions=1;

	// Set up interfacing devices
	initializeBoard();
	initializeUSB1208();

	// Get file name.  Use format "FDayScan"+$DATE+$TIME+".dat"
	time(&rawtime);
	timeinfo=localtime(&rawtime);
	struct stat st = {0};
	strftime(fileName,BUFSIZE,"/home/pi/RbData/%F",timeinfo); //INCLUDE
	if (stat(fileName, &st) == -1){
		mkdir(fileName,S_IRWXU | S_IRWXG | S_IRWXO );
	}
	strftime(fileName,BUFSIZE,"/home/pi/RbData/%F/FDayScan%F_%H%M%S.dat",timeinfo); //INCLUDE
	strftime(dailyFileName,BUFSIZE,"/home/pi/RbData/%F/FDayScan%F.dat",timeinfo); //INCLUDE

	printf("%s\n",fileName);
	printf("%s\n",comments);

	fp=fopen(fileName,"w");
	if (!fp) {
		printf("Unable to open file: %s\n",fileName);
        fflush(stdout);
		exit(1);
	}

    configFile=fopen("/home/pi/RbControl/system.cfg","r");
    if (!configFile) {
        printf("Unable to open config file\n");
        fflush(stdout);
        exit(1);
    }

	fprintf(fp,"#Filename:\t%s\n#Comments:\t%s\n",fileName,comments);

    /** Record System Stats to File **/
    /** Pressure Gauges **/
	getIonGauge(&returnFloat);
	printf("IonGauge %2.2E Torr \n",returnFloat);
	fprintf(fp,"#IonGauge(Torr):\t%2.2E\n",returnFloat);

	getConvectron(GP_N2_CHAN,&returnFloat);
	printf("CVGauge(N2) %2.2E Torr\n", returnFloat);
	fprintf(fp,"#CVGauge(N2)(Torr):\t%2.2E\n", returnFloat);

	getConvectron(GP_HE_CHAN,&returnFloat);
	printf("CVGauge(He) %2.2E Torr\n", returnFloat);
	fprintf(fp,"#CVGauge(He)(Torr):\t%2.2E\n", returnFloat);

    /** Temperature Controllers **/
	//getPVCN7500(CN_RESERVE,&returnFloat);
	fprintf(fp,"#T_res:\t%f\n",returnFloat);
	//getSVCN7500(CN_RESERVE,&returnFloat);
	fprintf(fp,"#T_res_set:\t%f\n",returnFloat);

	//getPVCN7500(CN_TARGET,&returnFloat);
	fprintf(fp,"#T_trg:\t%f\n",returnFloat);
	//getSVCN7500(CN_TARGET,&returnFloat);
	fprintf(fp,"#T_trg_set:\t%f\n",returnFloat);

    /** End System Stats Recording **/

    char line[1024];
	fgets(line,1024,configFile);
	while(line[0]=='#'){
		fprintf(fp,"%s",line);
		fgets(line,1024,configFile);
	}

	fclose(configFile);

    /*
	int numDet=18,j;
	float scanDet[]={-33,-30,-19,-18,-9,-5,-4.5,-4,-3.5,6,6.5,7,7.5,11,20,21,30,33};
    */

	int numDet=10,j;
	float scanDet[]={-30,-27,-19,-18,-9,11,20,21,30,33};

	fprintf(fp,"#Revolutions:\t%d\n",revolutions);
	fprintf(fp,"#DataPointsPerRev:\t%f\n",DATAPTSPERREV);
	fprintf(fp,"#NumVolts:\t%d\n",numDet);
	fprintf(fp,"#StepSize:\t%d\n",STEPSIZE);

	// Write the header for the data to the file.
	fprintf(fp,"STEP\tPUMP\tPUMPsd\tPRB\tPRBsd\tREF\tREFsd\n");
    fclose(fp);

	printf("Homing motor...\n");
	homeMotor(PROBE_MOTOR);

//    int numPd=3;
//    int pd[] = {PUMP_LASER,PROBE_LASER,REF_LASER};
    int numPd=2;
    int pd[] = {BOTLOCKIN,TOPLOCKIN};

	fp=fopen(fileName,"a");

	setProbeDetuning(scanDet[0]);
	delay(10000);


	for(j=0;j<numDet;j++){
		setProbeDetuning(scanDet[j]);
		getVortexPiezo(&value);
		delay(1000); 

	    getProbeFrequency(&wavelength);
	   // wavelength=-1;

        fprintf(fp,"\n\n#VOLT:%f(%f)\n",value,wavelength);
        printf("VOLT:%f(%f)\t",value,wavelength);
        fflush(stdout);

        quickHomeMotor(PROBE_MOTOR);
        collectDiscreteFourierData(fp,pd,numPd /*numPhotoDet*/,PROBE_MOTOR,revolutions);
	}//end for Volt

	fclose(fp);


	//printf("Processing Data...\n");
	//analyzeData(fileName, numDet, revolutions, dataPointsPerRevolution, FOI);

	char* extensionStart;
	extensionStart=strstr(fileName,".dat");
	strcpy(extensionStart,"RotationAnalysis.dat");

	//printf("Plotting Data...\n");
	//plotData(fileName);
	//printf("Calculating number density...\n");
	//calculateNumberDensity(fileName, 0, 0);
	//printf("Recording number density to file...\n");
	//recordNumberDensity(fileName);

	closeUSB1208();

	// Remove the file indicating that we are taking data.
	fclose(dataCollectionFlagFile);
	remove(dataCollectionFileName);

	return 0;
}

void collectDiscreteFourierData(FILE* fp, int* photoDetector, int numPhotoDetectors,int motor, int revolutions)
{
    int count=0;
    int steps,i,j,k;

    int nSamples=16;
	float* measurement = malloc(nSamples*sizeof(float));

    
    float* involts = calloc(numPhotoDetectors,sizeof(float));
    float* stdDev = calloc(numPhotoDetectors,sizeof(float));

    for (k=0;k<revolutions;k++){ //revolutions
        for (steps=0;steps < NUMSTEPS;steps+=STEPSIZE){ // steps
            // (NUMSTEPS) in increments of STEPSIZE
            delay(150); // watching the o-scope, it looks like it takes ~100ms for the ammeter to settle after a change in LP
            //get samples and average
            for(j=0;j<numPhotoDetectors;j++){ // numPhotoDet1
                involts[j]=0.0;	
                for (i=0;i<nSamples;i++){ // nSamples
                        getMCPAnalogIn(photoDetector[j],&measurement[i]);
                        involts[j]=involts[j]+measurement[i];
                        delay(WAITTIME);
                } // nSamples
                involts[j]=involts[j]/(float)nSamples; 
                stdDev[j]=stdDeviation(measurement,nSamples);
            } // numPhotoDet1

            fprintf(fp,"%d\t",steps+NUMSTEPS*k);
            for(j=0;j<numPhotoDetectors;j++){
                if(j!=numPhotoDetectors-1)
                    fprintf(fp,"%f\t%f\t",involts[j],stdDev[j]);
                else
                    fprintf(fp,"%f\t%f\n",involts[j],stdDev[j]);
            }

            count++;
            stepMotor(motor,CLK,STEPSIZE);
        } // steps
    } // revolutions
    free(measurement);
    free(stdDev);
    free(involts);
}
