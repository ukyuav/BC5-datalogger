// This Trial of the VectorNav/DAQ Acquisition software timestamps and writes individual DAQ samples using the VectorNav synchronization register as an external clock.

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
//Libraries for MCC DAQ
#include "uldaq.h"
#include "utility.h"
// Library for GPIO pins to enable push to start
#include <wiringPi.h> 
// Include this header file to get access to VectorNav sensors.
#include "vn/sensors.h"
#include "vn/thread.h"

#define MAX_DEV_COUNT  100
#define MAX_STR_LENGTH 64
#define MAX_SCAN_OPTIONS_LENGTH 256
#define ERRSTRLEN 256
#define PACKETSIZE 110
// Used for WiringPi numbering scheme. See the full list with `$ gpio readall`
#define STOP_BUTTON 5

//Allows for data recording to be started by a pushbutton connected to GPIO pins
//Set to zero to disable
#define PUSHTOSTART 0

using namespace std;
using namespace vn::math;
using namespace vn::sensors;
using namespace vn::protocol::uart;
using namespace vn::xplat;

// Method declarations for future use.
void asciiOrBinaryAsyncMessageReceived(void* userData, Packet& p, size_t index);
Range  getGain(int vRange);
int  getvRange(int gain);
int handleError(UlError detectError, const string info); 


char vecFileStr[] = "VECTORNAVDATA00.CSV";
ofstream vecFile;

int main(int argc, const char *argv[]) {
	wiringPiSetup();
	pinMode(STOP_BUTTON, INPUT);

	const char* fileName = "config.txt";

	DaqDeviceDescriptor devDescriptors[MAX_DEV_COUNT];
	DaqDeviceInterface interfaceType = ANY_IFC;
	DaqDeviceDescriptor DeviceDescriptor;
	DaqDeviceHandle deviceHandle;
	unsigned int numDevs = MAX_DEV_COUNT;
	UlError detectError = ERR_NO_ERROR;

	// Read config file
	int fileDesc = open(fileName, O_RDONLY);
	//Error Message if not possible
 	if (fileDesc < 0) {
		fprintf(stderr, "Cannot open file %s\n", fileName);
		return -1;
	}
	// Copy file to string buf
	char buf[100];
	const int bytesToRd = 100;
	int numRead = read(fileDesc, buf, bytesToRd);
	if (numRead < 0) {
		fprintf(stderr, "Cannot read file\n");
		return -1;
	}
	//Close file
	close(fileDesc);
	// Format of config.txt: Rate (Hz), Number of Channels, Voltage Range (1, 2,5, or 10 volts), Duration (Minutes)
	// Each entry separated by tabs
	char* rateStr = strtok(buf, "\t");
	char* numChanStr = strtok(NULL, "\t");
	char* vRangeStr = strtok(NULL, "\t");
	char* durationStr = strtok(NULL, "\t");

	int rate = atoi(rateStr);
	const int numChan = atoi(numChanStr);
	int vRange = atoi(vRangeStr);
	int duration = atoi(durationStr);

	printf("Rate: %i\n", rate);
	printf("Number of Channels: %i\n", numChan);
	printf("Range: %i\n", vRange);
	printf("Duration: %i\n", duration);

	// Verify values are acceptable
	if (rate > 100000) {
		printf("Individual channel sample rate cannot exceed 100kHz, acquisition will fail\n");
		return -1;
	}

	if (rate*numChan > 400000) {
		printf("Aggregate sample rate exceeds 400kHz, acquisition will fail\n");
		return -1;
	}
	if ((vRange != 1) && (vRange != 2) && (vRange != 5) && (vRange != 10)) {
		printf("Invalid range for voltage gain. Must be 1, 2, 5 or 10.\n");
		return -1;
	}
	// Write config values back to data.txt
	ofstream configFile;
	char configFileName[] = "CONFIGDATA00.txt";
	for (int i = 0; i < 100; i++) {
		configFileName[10] = i / 10 + '0';
		configFileName[11] = i % 10 + '0';
		struct stat statbuf;
		if (stat(configFileName, &statbuf) != 0) {//File doesn't already exist
			break;
		}
	}
	configFile.open(configFileName);
	configFile << rate << "\n" << numChan << "\n" << vRange << "\n" << duration << endl;
	configFile.flush();

	// Acquire device(s)
	detectError = ulGetDaqDeviceInventory(interfaceType, devDescriptors, &numDevs);
	if(handleError(detectError, "Cannot acquire device inventory\n")){
		return -1;
	}

	// verify at least one DAQ device is detected
	if (numDevs == 0) {
		printf("No DAQ device is detected\n");
		return -1;
	}

	printf("Found %d DAQ device(s)\n", numDevs);
	// for (int i = 0; i < (int) numDevs; i++)
	printf("  %s: (%s)\n", devDescriptors[0].productName, devDescriptors[0].uniqueId);
	DeviceDescriptor = devDescriptors[0];
	// get a handle to the DAQ device associated with the first descriptor
	deviceHandle = ulCreateDaqDevice(DeviceDescriptor);
	detectError = ulConnectDaqDevice(deviceHandle);
	
	// the symlink at /dev/VN should be created by a udev rule on this system
	// Please ensure it exists or replace this line with /dev/ttyUSB* where * corresponds with the correct port
	const string SensorPort = "/dev/VN";
	const uint32_t SensorBaudrate = 230400;
	// Now let's create a VnSensor object and use it to connect to our sensor.
	VnSensor vs;
	vs.connect(SensorPort, SensorBaudrate);
	// Let's query the sensor's model number.
	string mn = vs.readModelNumber();
	cout << "VectorNav connected. Model Number: " << mn << endl;
	
	int durSec = duration*60;
	short LowChan = 0;
	short HighChan = numChan - 1;
	double rated = (double)rate;
	long samplesPerChan = rate;// Acquisition will be for 1 seconds' worth of data, run continuously
	long numBufferPoints = numChan * rate;
	double * buffer = (double*) malloc(numBufferPoints * sizeof(double));
	if(buffer == 0){
		printf("Out of memory\n");
		return -1;
	}

	char DAQfileStr[] = "DATA00.DAQ";
	DAQfileStr[4] = configFileName[10];
	DAQfileStr[5] = configFileName[11];
	vecFileStr[13] = configFileName[10];
	vecFileStr[14] = configFileName[11];

	FILE* DAQFile = fopen(DAQfileStr, "wb+");
	vecFile.open(vecFileStr,std::ofstream::binary | std::ofstream::app);

	AsciiAsync asciiAsync = (AsciiAsync) 0;
	vs.writeAsyncDataOutputType(asciiAsync); //Turns off ASCII messages
	SynchronizationControlRegister scr(
			SYNCINMODE_COUNT,
			SYNCINEDGE_RISING,
			0,
			SYNCOUTMODE_ITEMSTART,
			SYNCOUTPOLARITY_POSITIVE,
			0,
			2500000);

			vs.writeSynchronizationControl(scr);
	BinaryOutputRegister bor(
		ASYNCMODE_PORT1,
		2, 
		COMMONGROUP_TIMEGPS | COMMONGROUP_YAWPITCHROLL | COMMONGROUP_ANGULARRATE | COMMONGROUP_POSITION | COMMONGROUP_VELOCITY | COMMONGROUP_INSSTATUS, // Note use of binary OR to configure flags.
		TIMEGROUP_NONE,
		IMUGROUP_TEMP | IMUGROUP_PRES,
		GPSGROUP_NONE,
		ATTITUDEGROUP_YPRU,
		INSGROUP_POSU | INSGROUP_VELU);
	vs.writeBinaryOutput1(bor);


	Range gain = getGain(vRange);
	//Remove SO_EXTTRIGGER option if you do not want the DAQ to wait for the VectorNav
	ScanOption options = (ScanOption) (SO_DEFAULTIO | SO_CONTINUOUS |  SO_EXTTRIGGER );
	AInScanFlag flags = AINSCAN_FF_DEFAULT;
	detectError = ulAInSetTrigger(deviceHandle, TRIG_POS_EDGE, 0, 0,0, 0);
	if (handleError(detectError, "Couldn't start scan\n")){
		return -1;
	}


	// Wait to start recording if Push to Start is enabled
	if(PUSHTOSTART){
		printf("Push  button to begin.\n");
		int hold = 1;
		int btn;
		while(hold ==1){
			btn = digitalRead(STOP_BUTTON);
			if (btn == LOW){
				hold = 0;
			}
		}
	}

	printf("Beginning Sampling for next %i minutes.\n Press enter to quit.\n", duration);

	time_t currentTime = time(NULL);
	struct tm * timeinfo = localtime (&currentTime);
	configFile << asctime(timeinfo) << endl;
	configFile.close();
	// initial scan for DAQ data
	detectError = ulAInScan(deviceHandle, LowChan, HighChan, AI_SINGLE_ENDED, gain, samplesPerChan, &rated, options,  flags, buffer);
	vs.registerAsyncPacketReceivedHandler(NULL, asciiOrBinaryAsyncMessageReceived);

	if (handleError(detectError, "Couldn't start scan\n")){
		return -1;
	}

	ScanStatus status;
	TransferStatus tranStat;
	bool readLower = true;
	detectError = ulAInScanStatus(deviceHandle, &status, &tranStat);
	if(handleError(detectError,"Couldn't check scan status\n")){
		return -1;
	}
	
/*	
	// indices for traversing through buffer
	int index = 0;
	int oldIndex = -1;
	double tempData = 0;

	while(status == SS_RUNNING && !enter_press() ){
		detectError = ulAInScanStatus(deviceHandle, &status, &tranStat);
		if(handleError(detectError,"Couldn't check scan status\n")){
			return -1;
		}
			if (tempData != buffer[0]){
			for (int i = 0; i< numChan; i++){
				fwrite(&buffer[i], sizeof(double), 1, DAQFile);
			}
			tempData = buffer[0];
			//printf("%d\n", tempData);
		}
	}
*/
	int btnPress = digitalRead(STOP_BUTTON);	
	double runningTime = difftime(time(NULL),currentTime);
	// Enter press is commented by default because it prevents from sampling at boot.
	while(status == SS_RUNNING /*&& !enter_press()*/ && (runningTime <  durSec) && btnPress != HIGH ){
//	while(status == SS_RUNNING && !enter_press()){
		btnPress = digitalRead(STOP_BUTTON);
		detectError = ulAInScanStatus(deviceHandle, &status, &tranStat);
		if(handleError(detectError,"Couldn't check scan status\n")){
			return -1;
		}
//		index = tranStat.currentIndex;
//		if (index != oldIndex){
//			for (int i = 0; i< numChan; i++){
//				fwrite(&buffer[index+i], sizeof(double), 1, DAQFile);
//			}
//		}
//		oldIndex=index;

		if( (tranStat.currentIndex > (numBufferPoints/2) ) & readLower){
			fwrite(buffer, sizeof(double), numBufferPoints/2 , DAQFile);
			readLower = false;
		}
		else if( (tranStat.currentIndex < (numBufferPoints/2) ) & !readLower){
			fwrite(&(buffer[numBufferPoints/2]), sizeof(double), numBufferPoints/2, DAQFile);
			readLower = true;
		}
		runningTime = difftime(time(NULL), currentTime);
	}
	vs.unregisterAsyncPacketReceivedHandler();
	vs.disconnect();
	vecFile.close();
	fclose(DAQFile);
	

	detectError = ulAInScanStop(deviceHandle);
	if(handleError(detectError,"Couldn't stop background process\n")){
		return -1;
	}
	printf("Sampling completed.\n");
}

void asciiOrBinaryAsyncMessageReceived(void* userData, Packet& p, size_t index)
{
// 	The following line and the vecFile.close() command at the bottom of this function are not recommended when the sync mount option is set for the filesystem
//	vecFile.open(vecFileStr,std::ofstream::binary | std::ofstream::app);
	
	if (p.type() == Packet::TYPE_BINARY)
	{
		// First make sure we have a binary packet type we expect since there
		// are many types of binary output types that can be configured.
		if (!p.isCompatible(
			COMMONGROUP_TIMEGPS | COMMONGROUP_YAWPITCHROLL | COMMONGROUP_ANGULARRATE | COMMONGROUP_POSITION | COMMONGROUP_VELOCITY | COMMONGROUP_INSSTATUS, // Note use of binary OR to configure flags.
			TIMEGROUP_NONE,
			IMUGROUP_TEMP | IMUGROUP_PRES,
			GPSGROUP_NONE,
			ATTITUDEGROUP_YPRU,
			INSGROUP_POSU | INSGROUP_VELU))
			// Not the type of binary packet we are expecting.
			return;
		vecFile.write(p.datastr().c_str(), PACKETSIZE );
	// 	vecFile.close();
	}
}

Range getGain(int vRange) {
	Range gain;
	switch (vRange) {
		case(1): {
			gain = BIP1VOLTS;
			return gain;
			break;
		}
		case(2): {
			gain = BIP2VOLTS;
			return gain;
			break;
		}
		case(5): {
			gain = BIP5VOLTS;
			return gain;
			break;
		}
		case(10): {
			gain = BIP10VOLTS;
			return gain;
			break;
		}
		default: {
			gain = BIP5VOLTS;
			return gain;
			break;
		}
	}
}

int getvRange(int gain) {
	int vRange;
	switch (gain) {
		case(BIP1VOLTS): {
			vRange = 1;
			return vRange;
			break;
		}
		case(BIP2VOLTS): {
			vRange = 2;
			return vRange;
			break;
		}
		case(BIP5VOLTS): {
			vRange = 5;
			return vRange;
			break;
		}
		case(BIP10VOLTS): {
			vRange = 10;
			return vRange;
			break;
		}
		default: {
			vRange = 5;
			return vRange;
			break;
		}
	}
}

int handleError(UlError detectError, const string info){
	if(detectError != 0){
		fprintf(stderr, info.c_str());
		char ErrMsg[ERRSTRLEN];
		ulGetErrMsg(detectError, ErrMsg);
		fprintf(stderr, ErrMsg);
		return 1;
	}
	return 0;
}
