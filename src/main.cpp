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
#include "include/utility.h"
// Library for GPIO pins to enable push to start
#include <wiringPi.h> 
// Include this header file to get access to VectorNav sensors.
#include "vn/sensors.h"
#include "vn/thread.h"

#include "yaml-cpp/yaml.h"

#define MAX_DEV_COUNT  100
#define MAX_STR_LENGTH 64
#define MAX_SCAN_OPTIONS_LENGTH 256
#define ERRSTRLEN 256
#define PACKETSIZE 110
// Used for WiringPi numbering scheme. See the full list with `$ gpio readall`
#define GPIO2 8 

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
Range getGain(int vRange);
int  getvRange(int gain);
int handleError(UlError detectError, const string info); 
void connectVs(VnSensor &vs, string vec_port, int baudrate);

char vecFileStr[] = "VNDATA00.RAW";
ofstream vecFile;

int main(int argc, const char *argv[]) {
	wiringPiSetup();
  pinMode(GPIO2, INPUT);

  // Use YAML to set configuration variables.
  int vec_baud;
  string vec_port;
  int vec_rate;
  int volt_range;
  int num_chan; 
  int daq_rate;
  double sample_time;

  if (argc == 2) {
    YAML::Node config = YAML::LoadFile(argv[1]);

    sample_time = config["sample_duration"].as<double>();

    // vectornav config
    YAML::Node vec_config = config["vectornav"];
    vec_baud = vec_config["baud"].as<int>();
    vec_port = vec_config["port"].as<string>();
    vec_rate = vec_config["rate"].as<int>();

    // daq config
    YAML::Node daq_config = config["daq"];
    volt_range = daq_config["v_range"].as<int>();
    num_chan = daq_config["chan_num"].as<int>(); 
    daq_rate = daq_config["rate"].as<int>();

  } else { 
    cerr << "No config file provided. Exiting" << endl;
    exit(1);
  }

	cout << "Rate: " << daq_rate << " Hz" << endl;  
	cout << "Number of Channels: " << num_chan << endl;
	cout << "Range: " << volt_range << " Volts" << endl;
	cout << "Duration: " << sample_time  << " Minutes" << endl;

	// Verify values are acceptable
	if (daq_rate > 100000) {
		cerr << "Individual channel sample rate cannot exceed 100kHz, acquisition will fail\n" << endl;
		return -1;
	}

	if (daq_rate*num_chan > 400000) {
		cerr << "Aggregate sample rate exceeds 400kHz, acquisition will fail\n" << endl;
		return -1;
	}
	if ((volt_range != 1) && (volt_range != 2) && (volt_range != 5) && (volt_range != 10)) {
		cerr << "Invalid range for voltage gain. Must be 1, 2, 5 or 10.\n" << endl;
		return -1;
	}
  if (vec_rate > daq_rate) { 
    cerr << "Vectornav sampling rate cannot be higher than DAQ sampling rate in this configuration." << endl;
    return -1;
  }

	DaqDeviceDescriptor devDescriptors[MAX_DEV_COUNT];
	DaqDeviceInterface interfaceType = ANY_IFC;
	DaqDeviceDescriptor DeviceDescriptor;
	DaqDeviceHandle deviceHandle;
	unsigned int numDevs = MAX_DEV_COUNT;
	UlError detectError = ERR_NO_ERROR;

	// Acquire device(s)
	detectError = ulGetDaqDeviceInventory(interfaceType, devDescriptors, &numDevs);
	if(handleError(detectError, "Cannot acquire device inventory\n")){
		return -1;
	}

	// verify at least one DAQ device is detected
	if (numDevs == 0) {
		cerr << "No DAQ device is detected\n" << endl;
		return -1;
	}

	cout << "Found " <<  numDevs  << " DAQ device(s)" << endl;
	// for (int i = 0; i < (int) numDevs; i++)
	cout << devDescriptors[0].productName << "(" << devDescriptors[0].uniqueId << ")" << endl;
	DeviceDescriptor = devDescriptors[0];

	// get a handle to the DAQ device associated with the first descriptor
	deviceHandle = ulCreateDaqDevice(DeviceDescriptor);
	detectError = ulConnectDaqDevice(deviceHandle);
	// Write config values back to config.txt
  // TODO: there might be a better way to do this
	ofstream configFile;
	char configFileName[] = "CONFIG_RUN00.yml";
	for (int i = 0; i < 100; i++) {
		configFileName[10] = i / 10 + '0';
		configFileName[11] = i % 10 + '0';
		struct stat statbuf;
		if (stat(configFileName, &statbuf) != 0) {//File doesn't already exist
			break;
		}
	}
  ifstream yamlFile(argv[1]);
	configFile.open(configFileName);
  configFile << yamlFile.rdbuf(); // simply clone the config file to this run's config file
	configFile.flush();


  // setup VectorNav
	VnSensor vs;
	connectVs(vs, vec_port, vec_baud);

	// Let's query the sensor's model number.
	string mn = vs.readModelNumber();
	cout << "VectorNav connected. Model Number: " << mn << endl;
	
	vecFileStr[6] = configFileName[10];
	vecFileStr[7] = configFileName[11];
	vecFile.open(vecFileStr, std::ofstream::binary | std::ofstream::app);

	vs.registerAsyncPacketReceivedHandler(NULL, asciiOrBinaryAsyncMessageReceived);
  cout << (int)(daq_rate/vec_rate-1) << endl; // Setting skip factor so that the trigger happens at the desired rate.
	AsciiAsync asciiAsync = (AsciiAsync) 0;
	vs.writeAsyncDataOutputType(asciiAsync); // Turns on Binary Message Type
	SynchronizationControlRegister scr( // synchronizes vecnav off of DAQ clock and triggers at desired rate
		// SYNCINMODE_ASYNC, TODO: test sync_in on another vn300, this one is unresponsive
    SYNCINMODE_COUNT,
		SYNCINEDGE_RISING,
		(int)(daq_rate/vec_rate-1), // Setting skip factor so that the trigger happens at the desired rate.
		SYNCOUTMODE_NONE,
		SYNCOUTPOLARITY_POSITIVE,
		0,
		0);
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


  // setup DAQ
	int durSec = sample_time*60;
	short LowChan = 0;
	short HighChan = num_chan - 1;
	double rated = (double)daq_rate;
	long samplesPerChan = daq_rate; // Acquisition will be for 1 seconds' worth of data, run continuously
	long numBufferPoints = num_chan * daq_rate;
	double* buffer = (double*) malloc(numBufferPoints * sizeof(double));
	if(buffer == 0){
		cout << "Out of memory\n" << endl;
		return -1;
	}

	char DAQfileStr[] = "DAQDATA00.RAW";

	DAQfileStr[7] = configFileName[10];
	DAQfileStr[8] = configFileName[11];
	FILE* DAQFile = fopen(DAQfileStr, "wb+");
	Range gain = getGain(volt_range);
  // tells the DAQ to send its sample rate out the SYNC port, acting as the master in a multidevice system.
	ScanOption options = (ScanOption) ( SO_DEFAULTIO | SO_CONTINUOUS | SO_PACEROUT);
	AInScanFlag flags = AINSCAN_FF_DEFAULT;

	// Wait to start recording if Push to Start is enabled
	if(PUSHTOSTART){
		cout << "Push  button to begin." << endl;
		int hold = 1;
		int btn;
		while(hold ==1){
			btn = digitalRead(GPIO2);
			if (btn == LOW){
				hold = 0;
			}
		}
	}

	cout << "Beginning Sampling for next" << sample_time << " minutes.\n Press enter to quit." << endl;

	time_t currentTime = time(NULL);
	struct tm * timeinfo = localtime (&currentTime);
	configFile << asctime(timeinfo) << endl;
	configFile.close();

	detectError = ulAInScan(deviceHandle, LowChan, HighChan, AI_SINGLE_ENDED, gain, samplesPerChan, &rated, options,  flags, buffer);
  // vs.writeAsyncDataOutputFrequency(rate);

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
	double runningTime = difftime(time(NULL),currentTime);
	while(status == SS_RUNNING && !enter_press() && (runningTime <  durSec) ){
		detectError = ulAInScanStatus(deviceHandle, &status, &tranStat);

		if(handleError(detectError,"Couldn't check scan status\n")){
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
		runningTime = difftime(time(NULL), currentTime);
	}
	fclose(DAQFile);
	vs.unregisterAsyncPacketReceivedHandler();
	vs.disconnect();
	vecFile.close();


	detectError = ulAInScanStop(deviceHandle);
	if(handleError(detectError,"Couldn't stop background process\n")){
		return -1;
	}
	cout << "Sampling completed." << endl;
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
		cerr << info.c_str() << endl;
		char ErrMsg[ERRSTRLEN];
		ulGetErrMsg(detectError, ErrMsg);
		cerr << ErrMsg << endl;
		return 1;
	}
	return 0;
}

void connectVs(VnSensor &vs, string vec_port, int baudrate) {
  // Default baudrate variable
  int defaultBaudrate;
  // Run through all of the acceptable baud rates until we are connected
  // Looping in case someone has changed the default
  bool baudSet = false;
  // Now let's create a VnSensor object and use it to connect to our sensor.
  while(!baudSet){
    // Make this variable only accessible in the while loop
    static int i = 0;
    defaultBaudrate = vs.supportedBaudrates()[i];
    cout << "Connecting with default at " <<  defaultBaudrate << endl;
    // Default response was too low and retransmit time was too long by default.
    // They would cause errors
    vs.setResponseTimeoutMs(1000); // Wait for up to 1000 ms for response
    vs.setRetransmitDelayMs(50);  // Retransmit every 50 ms

    // Acceptable baud rates 9600, 19200, 38400, 57600, 128000, 115200, 230400, 460800, 921600
    // Data sheet says 128000 is a valid baud rate. It doesn't work with the VN100 so it is excluded.
    // All other values seem to work fine.
    try{
      // Connect to sensor at it's default rate
      if(defaultBaudrate != 128000 && baudrate != 128000)
      {
        vs.connect(vec_port, defaultBaudrate);
        // Issues a change baudrate to the VectorNav sensor and then
        // reconnects the attached serial port at the new baudrate.
        vs.changeBaudRate(baudrate);
        // Only makes it here once we have the default correct
        // cout << "Connected baud rate is " << vs.baudrate();
        baudSet = true;
      }
    }
    // Catch all oddities
    catch(...){
      // Disconnect if we had the wrong default and we were connected
      vs.disconnect();
      sleep(0.2);
    }
    // Increment the default iterator
    i++;
    // There are only 9 available data rates, if no connection
    // made yet possibly a hardware malfunction?
    if(i > 8)
    {
      break;
    }
  }
}
