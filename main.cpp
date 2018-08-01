#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include "uldaq.h"
#include "utility.h"
#include <time.h>
#include <sys/stat.h>
// Include this header file to get access to VectorNav sensors.
#include "vn/sensors.h"
#include "vn/thread.h"

#define MAX_DEV_COUNT  100
#define MAX_STR_LENGTH 64
#define MAX_SCAN_OPTIONS_LENGTH 256
#define ERRSTRLEN 256
#define PACKETSIZE 110

using namespace std;
using namespace vn::math;
using namespace vn::sensors;
using namespace vn::protocol::uart;
using namespace vn::xplat;

// Method declarations for future use.
void asciiOrBinaryAsyncMessageReceived(void* userData, Packet& p, size_t index);

char vecFileStr[] = "VECTORNAVDATA00.CSV";
ofstream vecFile;

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


int main(int argc, const char *argv[]) {
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


	// Acquire device(s)
	detectError = ulGetDaqDeviceInventory(interfaceType, devDescriptors, &numDevs);

	if (detectError != 0) {
		printf("Cannot acquire device inventory\n");
		char ErrMsg[ERRSTRLEN];
		ulGetErrMsg(detectError, ErrMsg);
		printf(ErrMsg);
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
	const string SensorPort = "/dev/ttyUSB0";
	const uint32_t SensorBaudrate = 230400;
	// Now let's create a VnSensor object and use it to connect to our sensor.
	VnSensor vs;
	vs.connect(SensorPort, SensorBaudrate);
	// Let's query the sensor's model number.
	string mn = vs.readModelNumber();
	cout << "VectorNav connected. Model Number: " << mn << endl;

	short LowChan = 0;
	short HighChan = numChan - 1;
	double rated = (double)rate;
	long samplesPerChan = rate;// Acquisition will be for 1 seconds' worth of data, run continuously
	long numBufferPoints = numChan * rate;
	double* buffer = (double*) malloc(numBufferPoints * sizeof(double));
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
	vecFile.open(vecFileStr,std::ofstream::binary);

	AsciiAsync asciiAsync = (AsciiAsync) 0;
	vs.writeAsyncDataOutputType(asciiAsync); //Turns off ASCII messages
	SynchronizationControlRegister scr(
		SYNCINMODE_COUNT,
		SYNCINEDGE_RISING,
		0,
		SYNCOUTMODE_ITEMSTART,
		SYNCOUTPOLARITY_POSITIVE,
		0,
		100000000);
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
	ScanOption options = (ScanOption) (SO_DEFAULTIO | SO_CONTINUOUS | SO_EXTTRIGGER);
	AInScanFlag flags = AINSCAN_FF_DEFAULT;

	//Configure trigger, only the first two arguments here matter, rest are ignored
	//ulAInSetTrigger(deviceHandle,TRIG_POS_EDGE, 0, 2.4, .6, 0);

	printf("Beginning Sampling for next %i minutes.\n Press enter to quit.\n", duration);

	time_t currentTime = time(NULL);
	struct tm * timeinfo = localtime (&currentTime);
	configFile << asctime(timeinfo) << endl;
	configFile.close();

	detectError = ulAInScan(deviceHandle, LowChan, HighChan, AI_SINGLE_ENDED, gain, samplesPerChan, &rated, options,  flags, buffer);
	vs.registerAsyncPacketReceivedHandler(NULL, asciiOrBinaryAsyncMessageReceived);

	if (detectError != 0) {
		printf("Couldn't start scan\n");
		char ErrMsg[ERRSTRLEN];
		ulGetErrMsg(detectError, ErrMsg);
		printf(ErrMsg);
		return -1;
	}

	ScanStatus status;
	TransferStatus tranStat;
	bool readLower = true;
	detectError = ulAInScanStatus(deviceHandle, &status, &tranStat);
	if(detectError != 0){
		printf("Couldn't check scan status\n");
		char  ErrMsg[ERRSTRLEN];
		ulGetErrMsg(detectError, ErrMsg);
		printf(ErrMsg);
		cout << ErrMsg;
		return -1;
	}

	while(status == SS_RUNNING && !enter_press()){
		detectError = ulAInScanStatus(deviceHandle, &status, &tranStat);
		if(detectError != 0){
			printf("Couldn't check scan status\n");
			char  ErrMsg[ERRSTRLEN];
			ulGetErrMsg(detectError, ErrMsg);
			cout << ErrMsg;
			printf(ErrMsg);
			return -1;
		}
		if( (tranStat.currentIndex > (numBufferPoints/2) ) & readLower){
			fwrite(buffer, sizeof(double), numBufferPoints/2 , DAQFile);
			readLower = false;
		}
		else if( (tranStat.currentIndex < (numBufferPoints/2) ) & !readLower){
			fwrite(&(buffer[numBufferPoints/2]), sizeof(double), numBufferPoints/2, DAQFile);
			readLower = true;
		}
	}
	fclose(DAQFile);
	vs.unregisterAsyncPacketReceivedHandler();
	vs.disconnect();
	vecFile.close();


	detectError = ulAInScanStop(deviceHandle);
	if(detectError !=  0){
		printf("Couldn't stop background process\n");
		char ErrMsg[ERRSTRLEN];
		ulGetErrMsg(detectError, ErrMsg);
		printf(ErrMsg);
		return -1;
	}
	printf("Sampling completed.\n");
}

void asciiOrBinaryAsyncMessageReceived(void* userData, Packet& p, size_t index)
{
	if (p.type() == Packet::TYPE_ASCII && p.determineAsciiAsyncType() == VNYPR)
	{
		vec3f ypr;
		p.parseVNYPR(&ypr);
		cout << "ASCII Async YPR: " << ypr << endl;
		return;
	}
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
	}
}
