#include <stdio.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
//Libraries for MCC DAQ
#include "uldaq.h"
#include "include/utility.h"
// Library for GPIO pins to enable push to start
#include <wiringPi.h> 
#include <wiringSerial.h>
// Include this header file to get access to VectorNav sensors.
#include "vn/sensors.h"
#include "vn/thread.h"
#include "vn/packet.h"
#include "vn/types.h"

#include "yaml-cpp/yaml.h"

#define MAX_DEV_COUNT  100
#define MAX_STR_LENGTH 64
#define MAX_SCAN_OPTIONS_LENGTH 256
#define ERRSTRLEN 256
#define DAQSIZE 64
#define BUFFERSIZE 16 
// Used for WiringPi numbering scheme. See the full list with `$ gpio readall`
#define GPIO27 2 // GPIO27

//Allows for data recording to be started by a pushbutton connected to GPIO pins
//Set to zero to disable

const char* FIFOFILE = "bc6-printlog";

using namespace std;
using namespace vn::math;
using namespace vn::sensors;
using namespace vn::protocol::uart;
using namespace vn::xplat;

CommonGroup COMMON_MASK = COMMONGROUP_TIMEGPS | COMMONGROUP_YAWPITCHROLL | COMMONGROUP_ANGULARRATE | COMMONGROUP_POSITION | COMMONGROUP_VELOCITY | COMMONGROUP_ACCEL | COMMONGROUP_IMU | COMMONGROUP_INSSTATUS | COMMONGROUP_MAGPRES | COMMONGROUP_SYNCINCNT; // Note use of binary OR to configure flags.
TimeGroup TIME_MASK = TIMEGROUP_TIMEUTC;
ImuGroup IMU_MASK = IMUGROUP_NONE;
AttitudeGroup ATT_MASK = ATTITUDEGROUP_NONE;
InsGroup INS_MASK = INSGROUP_NONE;
GpsGroup GPS_MASK = GPSGROUP_POSLLA; 

struct ScanEventParameters
{
	double* buffer;	// data buffer
	long buffer_size; // size of buffer
	int lowChan;	// first channel in acquisition
	int highChan;	// last channel in acquisition
};
typedef struct ScanEventParameters ScanEventParameters;

struct TransmitArgs { 
	char* xbee_port;
	int xbee_rate;
};

// Method declarations for future use.
void* transmit(void * ptr); //TODO TODO
void* wait_for_sig(void*);
void* wait_for_but(void*);
Range getGain(int vRange);
int  getvRange(int gain);
int getConfigNumber(string pathname);
int handleError(UlError detectError, const string info); 
void connectVs(VnSensor &vs, string vec_port, int baudrate);
void daqEventHandle(DaqDeviceHandle daqDeviceHandle, DaqEventType eventType, unsigned long long eventData, void* userData);
void vecnavBinaryEventHandle(void* userData, Packet& p, size_t index);
void* vec_write(void* vp);

FILE* vecFile;
size_t bor_size = 4; // header (start byte, group mask byte) + crc (2 bytes) 
bool first_sample = false;

unsigned long past_scan = 0; 
FILE* DAQFile;

// char current_vec_bin[bor_size]; // TODO: this wont work because we need bor_size after the fact. NOTE: we can probably pass this in UserData and not need it in the global scope allowing for local alloc
char* current_vec_bin;
stringstream current_daq_bin;
volatile bool stop_sampling = false;

pthread_mutex_t halfone = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t halftwo = PTHREAD_MUTEX_INITIALIZER;
size_t vbuff_ind = 0;

#define VNMAX 400
#define BUFFTIME 2
const uint16_t vec_buff_size=VNMAX*BUFFTIME;
char* vec_cbuff[vec_buff_size];

int main(int argc, const char *argv[]) {



	wiringPiSetup();
	pinMode(GPIO27, INPUT);
	pullUpDnControl(GPIO27, PUD_DOWN);

	// Use YAML to set configuration variables.
	int vec_baud;
	string vec_port;
	int vec_rate; 
	int volt_range;
	int num_chan; 
	int daq_rate;
	double sample_time;
	int xbee_rate;
	string xbee_port; 
	string output_dir; 
	bool button_start;
	int min_gps_fix = 0;

	if (argc == 2) {
		YAML::Node config = YAML::LoadFile(argv[1]);

		sample_time = config["sample_duration"].as<double>();
		output_dir = config["output_dir"].as<string>();
		button_start = config["button_start"].as<bool>(); 

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

		// xbee config
		YAML::Node xbee_config = config["xbee"];
		xbee_rate = xbee_config["rate"].as<int>();
		xbee_port = xbee_config["port"].as<string>();

		// gps config
		min_gps_fix = config["gps_fix"].as<int>();
	} else { 
		cerr << "No config file provided. Exiting" << endl;
		exit(1);
	}

	std::ofstream fifo;
	fifo.open(FIFOFILE, ios::out);
	if(! fifo.is_open() ){
		std::cout << " error : cannot open fifo file " << std :: endl; 
		return 1;
	}
	fifo.close();                  
	
	// acquire filenames 
	int config_num = getConfigNumber(output_dir);
	if (config_num < 0) { 
		cerr << "Output directory will not work for sampling." << endl;
		fifo.open(FIFOFILE, ios::out);
		fifo << "Output directory will not work for sampling." << endl;
		fifo.close();
		exit(1);
	}
	cout << "This is config run: " << config_num << endl; 
	cout << "Outputting files to: " << output_dir << endl; 
	char* vec_file_str = new char[output_dir.length()+13];
	char* daq_file_str = new char[output_dir.length()+14];
	char* conf_file_str = new char[output_dir.length()+13];
	sprintf(vec_file_str, "%s/VNDATA%d.RAW", output_dir.c_str(), config_num);
	sprintf(daq_file_str, "%s/DAQDATA%d.RAW", output_dir.c_str(), config_num);
	sprintf(conf_file_str, "%s/CONFIG%d.YML", output_dir.c_str(), config_num);

	cout << "Rate: " << daq_rate << " Hz" << endl;  
	cout << "Number of Channels: " << num_chan << endl;
	cout << "Range: " << volt_range << " Volts" << endl;
	cout << "Duration: " << sample_time  << " Minutes" << endl;

	// Verify values are acceptable
	if (daq_rate > 100000) {
		cerr << "Individual channel sample rate cannot exceed 100kHz, acquisition will fail\n" << endl;
		fifo.open(FIFOFILE, ios::out);
		fifo << "Individual channel sample rate cannot exceed 100kHz, acquisition will fail\n" << endl;
		fifo.close();
		return -1;
	}

	if (daq_rate*num_chan > 400000) {
		cerr << "Aggregate sample rate exceeds 400kHz, acquisition will fail\n" << endl;
		fifo.open(FIFOFILE, ios::out);
		fifo << "Aggregate sample rate exceeds 400kHz, acquisition will fail\n" << endl;
		fifo.close();
		return -1;
	}
	if ((volt_range != 1) && (volt_range != 2) && (volt_range != 5) && (volt_range != 10)) {
		cerr << "Invalid range for voltage gain. Must be 1, 2, 5 or 10.\n" << endl;
		fifo.open(FIFOFILE, ios::out);
		fifo << "Invalid range for voltage gain. Must be 1, 2, 5 or 10.\n" << endl;
		fifo.close();
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
		fifo.close();
		return -1;
	}

	// verify at least one DAQ device is detected
	if (numDevs == 0) {
		cerr << "No DAQ device is detected\n" << endl;
		fifo.open(FIFOFILE, ios::out);
		fifo << "No DAQ device is detected\n" << endl;
		fifo.close();
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
	ifstream yamlFile(argv[1]);
	configFile.open(conf_file_str);
	configFile << yamlFile.rdbuf(); // simply clone the config file to this run's config file
	configFile.flush();
	

	// setup VectorNav
	VnSensor vs;
	connectVs(vs, vec_port, vec_baud);

	// Let's query the sensor's model number.
	string mn = vs.readModelNumber();
	cout << "VectorNav connected. Model Number: " << mn << endl;

	vecFile = fopen(vec_file_str, "wb+");
	
	if(COMMON_MASK != COMMONGROUP_NONE){
		bor_size += 2 + Packet::computeNumOfBytesForBinaryGroupPayload(BINARYGROUP_COMMON, COMMON_MASK);
	}
	if(TIME_MASK != TIMEGROUP_NONE){
		bor_size += 2 + Packet::computeNumOfBytesForBinaryGroupPayload(BINARYGROUP_TIME, TIME_MASK);
	}
	if(IMU_MASK != IMUGROUP_NONE){
		bor_size += 2 + Packet::computeNumOfBytesForBinaryGroupPayload(BINARYGROUP_IMU, IMU_MASK);
	}
	if(GPS_MASK != GPSGROUP_NONE){
		bor_size += 2 + Packet::computeNumOfBytesForBinaryGroupPayload(BINARYGROUP_GPS, GPS_MASK);
	}
	if(ATT_MASK != ATTITUDEGROUP_NONE){
		bor_size += 2 + Packet::computeNumOfBytesForBinaryGroupPayload(BINARYGROUP_ATTITUDE, ATT_MASK);
	}
	if(INS_MASK != INSGROUP_NONE){
		bor_size += 2 + Packet::computeNumOfBytesForBinaryGroupPayload(BINARYGROUP_INS, INS_MASK);
	}
	cout << "VectorNav Binary Output Register Size: " << bor_size;

	for (int i=0; i < vec_buff_size; i++) {
		vec_cbuff[i] = new char[bor_size];
	}
	
	GpsConfigurationRegister gcr = vs.readGpsConfiguration();
	cout << "GCR: " << gcr.mode << endl;
	cout << "Awaiting GPS fix" << endl;
	fifo.open(FIFOFILE, ios::out);
	fifo << "Awaiting GPS fix: " << min_gps_fix << endl; 
	fifo.close();
	int gps_fix = 0;
	
	while(min_gps_fix > gps_fix) { 
		GpsSolutionLlaRegister gslr = vs.readGpsSolutionLla();
		gps_fix = gslr.gpsFix;
		cout << gps_fix << endl;
		sleep(1);
	}
	
	sleep(1);
	fifo.open(FIFOFILE, ios::out);
	fifo << "GPS Acquired." << endl;	
	fifo.close();


	pthread_t vn_write_thread;
	pthread_create(&vn_write_thread, NULL, vec_write, NULL);
	vs.registerAsyncPacketReceivedHandler(NULL, vecnavBinaryEventHandle);
	AsciiAsync asciiAsync = (AsciiAsync) 0;
	vs.writeAsyncDataOutputType(asciiAsync); // Turns on Binary Message Type
	SynchronizationControlRegister scr( // synchronizes vecnav off of DAQ clock and triggers at desired rate
			SYNCINMODE_ASYNC,
			SYNCINEDGE_RISING,
			(int)(daq_rate/vec_rate-1), // Setting skip factor so that the trigger happens at the desired rate.
			SYNCOUTMODE_IMUREADY,
			SYNCOUTPOLARITY_POSITIVE,
			0,
			1250000);
	vs.writeSynchronizationControl(scr);
	// TODO: the messages should be customizable from the configuration. Should not be constants here and in vecnavHandle
	BinaryOutputRegister bor(
			ASYNCMODE_PORT1,
			0,
			COMMON_MASK,
			TIME_MASK, 
			IMU_MASK,
			GPS_MASK,
			ATT_MASK,
			INS_MASK
			);

	// overwrites test output
	vs.writeBinaryOutput1(bor);
	

	// setup DAQ
	short LowChan = 0;
	short HighChan = num_chan - 1;
	double rated = (double)daq_rate;
	long samplesPerChan = daq_rate*10; // holds 10s of data
	long numBufferPoints = num_chan * samplesPerChan;
	double* buffer = (double*) malloc(numBufferPoints * sizeof(double));
	if(buffer == 0){
		cout << "Out of memory\n" << endl;
		return -1;
	}

	DAQFile = fopen(daq_file_str, "wb+");
	Range gain = getGain(volt_range);
	// DAQ is master to Vectornav, attached VN_SYNCIN to CLKOUT on DAQ.
	ScanOption options = (ScanOption) (SO_DEFAULTIO | SO_CONTINUOUS | SO_PACEROUT);
	AInScanFlag flags = AINSCAN_FF_DEFAULT;

	sleep(1);
	// Wait to start recording if Push to Start is enabled
	if(button_start){
		cout << "Push  button to begin." << endl;
		fifo.open(FIFOFILE, ios::out);
		fifo << "Push  button to begin." << endl;
		fifo.close();
		int hold = 1;
		int btn;
		while(hold ==1){
			btn = digitalRead(GPIO27);
			if (btn == HIGH){ //ensure button is being held for at least 4 seconds
				sleep(4);
				btn = digitalRead(GPIO27);
				if (btn == HIGH){
					hold = 0;
				}
			}
		}
	}
	fifo.open(FIFOFILE, ios::out);
	fifo << "Started sampling" << endl;
	fifo.close();

	// setup scan event for the DAQ
	long event_on_samples = samplesPerChan/100; // trigger event every 0.1 seconds.
	DaqEventType scan_event = (DE_ON_DATA_AVAILABLE);
	ScanEventParameters user_data;
	user_data.buffer = buffer;
	user_data.buffer_size = numBufferPoints; 
	user_data.lowChan = LowChan;
	user_data.highChan = HighChan;
	detectError = ulEnableEvent(deviceHandle, scan_event, event_on_samples, daqEventHandle, &user_data);
	if (handleError(detectError, "Could not enable event\n")){
		return -1;
	}

	time_t currentTime = time(NULL);
	struct tm * timeinfo = localtime (&currentTime);
	configFile << asctime(timeinfo) << endl;
	configFile.close();

	usleep(20000); // increase stability of the deviceHandle when on the external clock

	detectError = ulAInScan(deviceHandle, LowChan, HighChan, AI_SINGLE_ENDED, gain, samplesPerChan, &rated, options,  flags, buffer);
	if (handleError(detectError, "Couldn't start scan\n")){
		return -1;
	}

	cout << "Actual sample rate: " << rated << endl;

	// setup transmitter and timer threads
	struct TransmitArgs xbee_args;
	xbee_args.xbee_port = strdup(xbee_port.c_str());
	xbee_args.xbee_rate = xbee_rate; 
	pthread_t transmit_thread;
	pthread_t timer_thread;
	pthread_create(&transmit_thread, NULL, transmit, &xbee_args);
	if (button_start) { 
		pthread_create(&timer_thread, NULL, wait_for_but, NULL); 
	}
	else {
		cout << "Beginning Sampling for next " << sample_time << " minutes.\n Press enter to quit." << endl;
		pthread_create(&timer_thread, NULL, wait_for_sig, NULL); 
	}
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += (sample_time*60);
	pthread_timedjoin_np(timer_thread, NULL, &ts); // more efficient than checking the time in a loop
	pthread_cancel(timer_thread);
	cout << "enter pressed, should be wrapping up" << endl;
	stop_sampling = true; // this kills transmission and file writing threads.
	
	pthread_join(transmit_thread, NULL);
	pthread_join(vn_write_thread, NULL);
	

	// wrap up daq
	ulAInScanStop(deviceHandle);
	ulDisableEvent(deviceHandle, scan_event);
	ulDisconnectDaqDevice(deviceHandle);
	sleep(1);
	fflush(DAQFile);
	fclose(DAQFile);

	
	// wrap up vecnav
	vs.unregisterAsyncPacketReceivedHandler();
	vs.disconnect();
	fflush(vecFile);
	fclose(vecFile);
	
	cout << "Sampling completed." << endl;
	sleep(1);
	fifo.open(FIFOFILE, ios::out);
	fifo << "Sampling completed." << endl;
	fifo.close();
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
void vecnavBinaryEventHandle(void *userData, Packet& p, size_t index)
{
	// 	The following line and the vecFile.close() command at the bottom of this function are not recommended when the sync mount option is set for the filesystem
	if (p.type() == Packet::TYPE_BINARY)
	{
		string p_str = p.datastr();
		// First make sure we have a binary packet type we expect since there
		// are many types of binary output types that can be configured.
		if (!p.isCompatible(
					COMMON_MASK, 
					TIME_MASK, 
					IMU_MASK, 
					GPS_MASK,
					ATT_MASK, 
					INS_MASK
				   )
		   )
			// Not the type of binary packet we are expecting.
			return;
		const char * p_cstr = p_str.c_str();
		size_t pack_size = p_str.length();
		if (pack_size != bor_size){ 
			cout << "packet discrepency: " << bor_size << " ; " << pack_size << endl; 
		}
		//
		// get current pointer to the circular buffer
		// increment current_pointer, if the pointer is overflowed, set pointer to 0
		// insert p_cstr into the buffer
		//
		if(vbuff_ind  == (vec_buff_size/2)) {  // in the second half of the buffer
			pthread_mutex_lock(&halftwo);
			pthread_mutex_unlock(&halfone);
		} else if (vbuff_ind == 0) { // in the first half of the buffer
			pthread_mutex_lock(&halfone);
			pthread_mutex_unlock(&halftwo);
		}
		memcpy(vec_cbuff[vbuff_ind], p_cstr, bor_size);
		if(++vbuff_ind >= vec_buff_size) { 
			vbuff_ind = 0;
		}
		first_sample = true;
	}
}

/* writes out available portions of the vectornav data buffer */

void* vec_write(void* vp){ 
	while(!first_sample) {
		sleep(0.1);
	}
	while(!stop_sampling){ 
		pthread_mutex_lock(&halfone); 
		for(int i=0; i<(vec_buff_size)/2;i++){
			// write one of the binary ouputs of the vec_cbuff
			fwrite(vec_cbuff[i], sizeof(char)*bor_size, 1, vecFile);
		}
		fflush(vecFile);
		pthread_mutex_unlock(&halfone);
		pthread_mutex_lock(&halftwo); 
		for(int i=0; i<(vec_buff_size)/2;i++){
			fwrite(vec_cbuff[vec_buff_size/2+i], sizeof(char)*bor_size, 1, vecFile);
		}
		fflush(vecFile);
		pthread_mutex_unlock(&halftwo);
	}
	return NULL;
}


void daqEventHandle(DaqDeviceHandle daqDeviceHandle, DaqEventType eventType, unsigned long long eventData, void* userData) {

	/*
	 * eventData is the total number of sample events that have occured
	 * past_scan is the event index that was stopped at last scan
	 * total_samples eventData*chanCount is effectively the index of the buffer
	 * the buffer will wrap around once total_samples > bufferSize 
	 * this makes total_samples%bufferSize the true index accounting for the buffer wrap around
	 */

	DaqDeviceDescriptor activeDevDescriptor;
	ulGetDaqDeviceDescriptor(daqDeviceHandle, &activeDevDescriptor);
	UlError err = ERR_NO_ERROR;

	ScanEventParameters* scanEventParameters = (ScanEventParameters*) userData;
	int chan_count = scanEventParameters->highChan - scanEventParameters->lowChan + 1; 
	unsigned long long total_samples = eventData*chan_count; 
	long number_of_samples; 

	if (eventType == DE_ON_DATA_AVAILABLE) {
		unsigned long sample_index = total_samples % scanEventParameters->buffer_size;
		if (sample_index < past_scan) { // buffer wrap around
			number_of_samples = scanEventParameters->buffer_size - past_scan; // go to the end of the buffer
			fwrite(&(scanEventParameters->buffer[past_scan]), sizeof(double), number_of_samples, DAQFile);
			number_of_samples = sample_index;
			fwrite(&(scanEventParameters->buffer[0]), sizeof(double), number_of_samples, DAQFile); // restart on the buffer
		} else { // normal operation
			number_of_samples = sample_index - past_scan;
			fwrite(&(scanEventParameters->buffer[past_scan]), sizeof(double), number_of_samples, DAQFile);
		}
		//current_daq_bin.str("");
		//for(int i=chan_count-1; i>=0; i--){
		// current_daq_bin << std::hexfloat << scanEventParameters->buffer[sample_index-i]; // converts the double values to hex
		//}
		past_scan = sample_index;
	} else if (eventType == DE_ON_INPUT_SCAN_ERROR) {
		err = (UlError) eventData;
		char errMsg[ERR_MSG_LEN];
		ulGetErrMsg(err, errMsg);
		printf("Error Code: %d \n", err);
		printf("Error Message: %s \n", errMsg);
	} else if (eventType == DE_ON_END_OF_INPUT_SCAN) {
		printf("\nThe scan using device %s (%s) is complete \n", activeDevDescriptor.productName, activeDevDescriptor.uniqueId);
	}
}

void * transmit(void * ptr){
	
	struct TransmitArgs *args = (struct TransmitArgs *)ptr; 
	int rate = args->xbee_rate;
	char *port = args->xbee_port;
	int fd;
	if((fd=serialOpen(port, 9600)) < 0) {
		cerr << "Could not connect to Xbee" << endl; 
		return NULL;
	}
	while(!stop_sampling) { 
		for(size_t i=0; i < bor_size; i++){
			serialPutchar(fd, current_vec_bin[i]);
		}
		serialPuts(fd, current_daq_bin.str().c_str());
		usleep((int)(1000000/rate));
	}
	
	return NULL;
}

void* wait_for_but(void*){
	// Wait to start recording if Push to Start is enabled
	cout << "Push  button to end sampling." << endl;
	sleep(5); // you want at least 5 seconds of data right? Stops bounce issues with start
	int hold = 1;
	int btn;
	while(hold ==1){
		btn = digitalRead(GPIO27);
		if (btn == HIGH){
			sleep(4); //hold button for 5 seconds to end sampling
			btn = digitalRead(GPIO27);
			if (btn == HIGH){
			hold = 0;
			}
		}
	}
	return NULL;
}

void* wait_for_sig(void*){
	string _dummy;
	getline(cin, _dummy);
	return NULL;
}


int getConfigNumber(string pathname) { 
	int config_num = 0; 
	const char* conf_str = "%s/CONFIG%d.YML"; 
	char * conf_path = new char[pathname.length()+13]();
	sprintf(conf_path, conf_str, pathname.c_str(), config_num);
	struct stat statbuf;
	while(config_num < 100) { 
		if(stat(conf_path, &statbuf) != 0){  // file doesnt already exist
			return config_num; 
		} else { 
			config_num++; 
			sprintf(conf_path, conf_str, pathname.c_str(), config_num);
		}
	}
	return -1; // somehow there are no integers left
}
