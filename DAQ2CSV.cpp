//NAME: DAQ2CSV.cpp
//SYNOPSIS: ./DAQ2CSV [DAQFILE].DAQ [VECTORNAVFILE].CSV [CONFIGFILE].txt
//COMPILATION: g++ -g -o DAQ2CSV DAQ2CSV.cpp   // hopefully add to make in the near future
//USAGE: Converts a DAQ file a time stamped CSV file. Reads a config file to determine the
//	sample rate and number of DAQ channels being sampled. Reads the first time stamp
//	from the VectorNav, then time stamps each DAQ sample using a calculated period. The
//	corresponding timestamps are calculated using [initTime]+[period]*[iterator].
//		Eg. 5th sample will have timestamp of [initTime] +[period]*5  
//
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <fcntl.h>
#include <cstring>
#include <sys/stat.h>
// Include this header file to get access to VectorNav sensors.
#include "vn/sensors.h"
// We need this file for our sleep function.
#include "vn/thread.h"

#define PACKETSIZE 110
//#define PERIOD 5000000
#define NAMEMAX 100
using namespace std;
using namespace vn::math;
using namespace vn::sensors;
using namespace vn::protocol::uart;
using namespace vn::xplat;

// Converts [FILENAME].DAQ to [FILENAME].CSV
char * outFileNameConvert(char * inFileName);

// Calculates the 16-bit CRC for the given ASCII or binary message.
unsigned short calculateCRC(unsigned char data[], unsigned int length)
{
	unsigned int i;
	unsigned short crc = 0;
	for (i = 0; i<length; i++) {
		crc = (unsigned char)(crc >> 8) | (crc << 8);
		crc ^= data[i];
		crc ^= (unsigned char)(crc & 0xff) >> 4;
		crc ^= crc << 12;
		crc ^= (crc & 0x00ff) << 5;
	}
	return crc;
}

int main (int argc, char * argv[]){
	ofstream outFile;
	double  dblBuffer;
	uint64_t time;
	int rate, numChan;
	FILE * DAQFile;
	FILE *	configFile;
	DAQFile = fopen(argv[1], "rb");
	configFile = fopen(argv[3], "rb");

	FILE* VNFile = fopen(argv[2], "rb+");		
	if (VNFile == NULL) {
		perror("Can't open file: ");
	}

	char * outFileName = outFileNameConvert(argv[1]);
	outFile.open(outFileName);
	outFile.precision(17);
	int DAQfileDesc = fileno(DAQFile);
	int configFileDesc = fileno(configFile);
	int VNfileDesc = fileno(VNFile);
	int fileSize = lseek(DAQfileDesc, 0, SEEK_END); // Find filesize
	lseek(DAQfileDesc, 0, SEEK_SET); //Reset to the beginning

	//extract config data
	char buf[100];
	const int bytesToRead = 100;
	int numReadConfig = read(configFileDesc, buf, bytesToRead);
	if (numReadConfig< 0){
		perror("file read error. \n");
	       return 1;	
	}
	char * rateStr = strtok(buf, "\n");
	char * numChanStr = strtok(NULL, "\n");

	rate = atoi(rateStr);
	float floatRate = (float)rate;
	numChan = atoi(numChanStr);
	float  period = (((float) 1)/(floatRate))*((float)(1000000000)); // convert rate to period in nanoseconds. VectorNav GPS timestamp is in nanoseconds.
	int intPeriod = (int)period; // convert float period back to int.

	
	//extract first timestamp from VectorNav File
	char buffer[PACKETSIZE];
	int numRead = read(VNfileDesc, buffer, PACKETSIZE);
	if (numRead == -1) {
		perror("Can't read first packet: ");
	}
	//std::cout << "Extraction in progress, please do not quit the program." << endl;
	if(numRead == PACKETSIZE) {
		Packet p(buffer, PACKETSIZE);
		unsigned char check[PACKETSIZE - 1];
		memcpy(check, &buffer[1], PACKETSIZE - 1);
		if (calculateCRC(check, PACKETSIZE - 1) == 0) {
			time = p.extractUint64();
		}
	
	}

	//write to outFile.
	std::cout << "Writing DAQ CSV File." << endl;
	for(uint64_t i = 0; i*sizeof(double) < fileSize/sizeof(double)/numChan; i++){
		outFile << (time + (intPeriod*i));
		for(int j = 0; j < numChan; j++){
			fread(&dblBuffer , sizeof(double), 1, DAQFile);
			outFile << "," <<  dblBuffer;
		}
		outFile << "\n";
	}	
	fclose(DAQFile);
	outFile.close();
	free (outFileName);
	return 0;

		


}

char * outFileNameConvert( char * inFileName){
	size_t length = strlen(inFileName);
	char * outFileName = (char*)malloc((size_t)NAMEMAX);
	strncpy(outFileName, inFileName, NAMEMAX);

	//change DAQ to CSV
	outFileName[length-3] = 'C';
	outFileName[length-2] = 'S';
	outFileName[length-1] = 'V';

	return outFileName;
}