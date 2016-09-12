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
#include "mathTools.h" //includes stdDeviation

#define PI 3.14159265358979
#define NUMSTEPS 350	
#define STEPSIZE 25
#define STEPSPERREV 350.0
#define WAITTIME 2

#define BUFSIZE 1024

int plotData(char* fileName);
int calculateNumberDensity(char* fileName);
int recordNumberDensity(char* fileName);
int analyzeData(char* fileName);

int main (int argc, char **argv)
{
	float sumI, sumSin, sumCos;
	float f4,f3,df4,df3,angle,stderrangle,count;
	float returnFloat;
	struct tm * timeinfo;
	char fileName[BUFSIZE], comments[BUFSIZE];

	float involts; 	// The amount of light that is entering into the sensor. 
	float stderrinvolts;
	FILE *fp,*dataCollectionFlagFile;


	if (argc==5){
		AoutStart= atoi(argv[1]);
		AoutStop=atoi(argv[2]);
		deltaAout=atoi(argv[3]);
		strcpy(comments,argv[4]);
	} else { 
		printf("usage '~$ sudo ./faradayscan <aoutstart> <aoutstop> <deltaaout> <comments in quotes>'\n");
		return 1;
	}


	plotData(fileName);
	calculateNumberDensity(fileName);
	recordNumberDensity(fileName);

	return 0;
}

// Okay, so gnuplot's fitting function isn't behaving like I want it to
// (not giving me "reasonable" answers), so I'm hacking together
// a quick automated data fitting scheme. It involves using Wolfram,
// so it's going to be kind of messy.
int calculateNumberDensity(char* fileName){
	int i=0;
	char buffer[BUFSIZE];
	//char script[BUFSIZE]="\\home\\pi\\RbControl\\shortGetFit.wl";
	FILE *wolfram;
	wolfram = popen("wolfram >> /dev/null","w"); 

	if (wolfram != NULL){
		sprintf(buffer, "faradayData=Import[\"%s\",\"Data\"]\n", fileName);
		fprintf(wolfram, buffer);

		//sprintf(buffer, "Get[\"%s\"]",script);
		//Removes lines from the file (This one gets rid of the comments)
		//fprintf(wolfram, "faradayData=Delete[faradayData,{{1},{2},{3},{4},{5},{6},{7},{8}}]\n");			
		for(i=0;i<10;i++){
			fprintf(wolfram, "faradayData=Delete[faradayData,{{1}}]\n");			
			
		}
		//Removes unneccesary columns from the file
		for(i=0;i<5;i++){
			fprintf(wolfram, "faradayData=Drop[faradayData,None,{2}]\n");
		}
		fprintf(wolfram, "faradayData=Drop[faradayData,None,{3}]\n");
		//Removes lines from the file (This one gets rid of the data close to resonance)
		//fprintf(wolfram, "faradayData=Delete[faradayData,{{8},{9},{10},{11}}]\n");
		fprintf(wolfram, "l=2.8\n");
		fprintf(wolfram, "c=2.9979*^10\n");
		fprintf(wolfram, "re=2.8179*^-13\n");
		fprintf(wolfram, "fge=0.34231\n");
		fprintf(wolfram, "k=4/3\n");
		fprintf(wolfram, "Mb=9.2740*^-24\n");
		fprintf(wolfram, "B=2.08*^-2\n");
		fprintf(wolfram, "h=6.6261*^-34\n");
		fprintf(wolfram, "vo=3.77107*^14\n");
		fprintf(wolfram, "pi=3.14.159265\n");
		fprintf(wolfram, "aoutConv=-.0266\n");
		fprintf(wolfram, "aoutIntercept=14.961\n");
		fprintf(wolfram, "const=l*c*re*fge*k*Mb*B/(4*pi*h*vo)\n");
		fprintf(wolfram, "model=a+b*const*(vo+(aoutConv*detune+aoutIntercept)*1*^9)/((aoutConv*detune+aoutIntercept)*1*^9)^2+d*(vo+(aoutConv*detune+aoutIntercept))^5/((aoutConv*detune+aoutIntercept))^4\n");
		fprintf(wolfram, "vect=FindFit[faradayData,model,{a,b,d},detune]\n");
		fprintf(wolfram, "Replace[a,vect]>>fitParams.txt\n");
		fprintf(wolfram, "Replace[b,vect]>>>fitParams.txt\n");
		fprintf(wolfram, "Replace[d,vect]>>>fitParams.txt\n");
	}
	return pclose(wolfram);
}

int recordNumberDensity(char* fileName){
	char analysisFileName[1024];
	strcpy(analysisFileName,fileName);
	char* extensionStart=strstr(analysisFileName,".dat");
	strcpy(extensionStart,"analysis.dat");
	float a,b,c;
	int bExp,cExp;

	FILE* data = fopen("fitParams.txt","r");
	if (!data) {
		printf("Unable to open file %s\n",fileName);
		exit(1);
	}
	fscanf(data,"%f\n%f*^%d\n%f*^%d\n",	&a,&b,&bExp,&c,&cExp);
	fclose(data);

	FILE* dataSummary;
	// Record the results along with the raw data in a file.
	dataSummary=fopen(analysisFileName,"w");
	if (!dataSummary) {
		printf("Unable to open file: %s\n", analysisFileName);
		exit(1);
	}
	fprintf(dataSummary,"#File\t%s\n",analysisFileName);
	fprintf(dataSummary,"theta_o\tN\tdetuneFourthTerm\n");
	fprintf(dataSummary,"%2.2E\t%2.2E\t%2.2E\n",a,b*pow(10,bExp),c*pow(10,cExp));
	printf("theta_o\tdetuneSquare\tdetuneFourth\n");
	printf("%2.2E\t%2.2E\t%2.2E\n",a,b*pow(10,bExp),c*pow(10,cExp));
	fclose(dataSummary);

	return 0;
}

int plotData(char* fileName){
	char buffer[BUFSIZE];
	FILE *gnuplot;
	gnuplot = popen("gnuplot","w"); 

	if (gnuplot != NULL){
		fprintf(gnuplot, "set terminal dumb size 100,28\n");
		fprintf(gnuplot, "set output\n");			

		sprintf(buffer, "set title '%s'\n", fileName);
		fprintf(gnuplot, buffer);

		fprintf(gnuplot, "set key autotitle columnheader\n");
		fprintf(gnuplot, "set xlabel 'Aout (Detuning)'\n");			
		fprintf(gnuplot, "set ylabel 'Theta'\n");			
		fprintf(gnuplot, "set xrange [0:1024] reverse\n");			
		sprintf(buffer, "plot '%s' using 1:7:8 with errorbars\n",fileName);
		fprintf(gnuplot, buffer);
		fprintf(gnuplot, "unset output\n"); 
		fprintf(gnuplot, "set terminal png\n");
		sprintf(buffer, "set output '%s.png'\n", fileName);
		fprintf(gnuplot, buffer);
		sprintf(buffer, "plot '%s' using 1:7:8 with errorbars\n",fileName);
		fprintf(gnuplot, buffer);
	}
	return pclose(gnuplot);
}

int analyzeData(char* fileName, int dataPointsPerRevolution, int revolutions){
	int totalDatapoints=dataPointsPerRevolution*revolutions;
	extensionStart=strstr(fileName,".dat");
	strcpy(extensionStart,"Analysis.dat");
	FILE* data = fopen(fileName,"r");
	// Write the header for the data to the file.
	fprintf(fp,"Aout\tc0\ts4\td-s4\tc4\td-c4\tangle\tangleError\n");

	stderrangle=0;
	for(Aout=AoutStart;Aout<AoutStop;Aout+=deltaAout){
		dataPoints+=1;
		printf("Aout %d\n",Aout);
		sumSin=0.0;
		sumCos=0.0;
		df4=0.0;
		df3=0.0;
		angle=0.0;
		count=0.0;
		sumI=0.0;

		setUSB1208AnalogOut(PROBEOFFSET,Aout);

		for (steps=0;steps < NUMSTEPS;steps+=STEPSIZE){ // We want to go through a full revolution of the linear polarizer
			// (NUMSTEPS) in increments of STEPSIZE

			delay(150); // watching the o-scope, it looks like it takes ~100ms for the ammeter to settle after a change in LP
			//get samples and average
			involts=0.0;	
			for (i=0;i<nsamples;i++){ // Take several samples of the voltage and average them.
				getUSB1208AnalogIn(PROBE_LASER,&measurement[i]);
				involts=involts+measurement[i];
				delay(WAITTIME);
			}
			involts=involts/(float)nsamples; 

			stderrinvolts = stdDeviation(measurement,nsamples);

			angle=2.0*PI*(float)(steps)/STEPSPERREV; // Calculate the angle in radians of the axis of the LP
			count=count+1.0;
			sumSin+=involts*sin(2*angle);
			sumCos+=involts*cos(2*angle);
			sumI+=involts;
			df3+=pow(stderrinvolts,2)*pow(sin(2*angle),2);
			df4+=pow(stderrinvolts,2)*pow(cos(2*angle),2);

			stepMotor(PROBE_MOTOR,CLK,STEPSIZE);
		}
		sumI=sumI/count;
		f3=sumSin/count;
		f4=sumCos/count;
		df3=sqrt(df3)/count;
		df4=sqrt(df4)/count;

		angle = 0.5*atan2(f4,f3);
		angle = angle*180.0/PI;

		stderrangle=(1/(1+pow(f4/f3,2)))*sqrt(pow(f3,-2))*(sqrt(pow(df4,2) + stderrangle*pow(df3,2))/2.0);

		stderrangle = stderrangle*180.0/PI;

		printf("c0 = %f\t",sumI);
		printf("s4 = %f\t",f3);
		printf("c4 = %f\t",f4);
		printf("angle = %f (%f)\n",angle,stderrangle);
		// As a reminder, these are the headers: fprintf(fp,"Aout\tf0\tf3\td-f3\tf4\td-f4\tangle\tangleError\n");
		fprintf(fp,"%d\t%f\t%f\t%f\t%f\t%f\t%f\t%f\n",Aout,sumI,f3,df3,f4,df4,angle,stderrangle);
	}//end for Aout

	fclose(fp);

}
