#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <cstring>
#include <sys/stat.h>
// Include this header file to get access to VectorNav sensors.
#include "vn/sensors.h"
#include "vn/compositedata.h"
#include "vn/util.h"
// We need this file for our sleep function.
#include "vn/thread.h"

#define NAMEMAX 100
#define HEADERSIZE 20

using namespace std;
using namespace vn::math;
using namespace vn::sensors;
using namespace vn::protocol::uart;
using namespace vn::xplat;

char outFileStr[] = "%s/VECTORNAVASCII%d.CSV";


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

char * outFileNameConvert( char * inFileName){
	size_t length = strlen(inFileName);
	char * outFileName = (char*)malloc((size_t)NAMEMAX);
	strncpy(outFileName, inFileName, NAMEMAX);

	//change RAW to CSV
	outFileName[length-3] = 'C';
	outFileName[length-2] = 'S';
	outFileName[length-1] = 'V';

	return outFileName;
}


int main(int argc, char *argv[])
{
  if (argc < 2) { 
    perror("Please provide binary filepath");
    exit(1);
  }
  char * outFileName = outFileNameConvert(argv[1]);

	ofstream outFile;
	outFile.open(outFileName);
	//outFile << "Time" << " , " << "Yaw Pitch Roll" << ", " << "Yaw Pitch Roll Uncertainties" << " , " << "Yaw Pitch Roll Velocity" << " , " << "Latitude Longitude Altitude" << " , " << "Position Uncertainty" << " , " << "Velocity" << " , " << "Velocity Uncertainty" << " , " << "INS Status" << " , " << "Temperature" << " , " << "Pressure" << endl;

	FILE* inputFile = fopen(argv[1], "rb+");
	if (inputFile == NULL) {
		perror("Can't open file: ");
	}
	int fileDesc = fileno(inputFile);
	int fileSize = lseek(fileDesc, 0, SEEK_END); // Find filesize
	lseek(fileDesc, 0, SEEK_SET); //Reset to the beginning

  // read header to get the packet size
	char head_buffer[HEADERSIZE];
	int numRead = read(fileDesc, head_buffer, HEADERSIZE);
	if (numRead == -1) {
		perror("Can't read first packet: ");
	}
	std::cout << "Extraction in progress, please do not quit the program." << endl;
	int count = 0;
	int countCorrupt = 0;
  size_t pack_size = Packet::computeBinaryPacketLength(head_buffer);
  cout << "packet true size" << pack_size << endl;
  char buffer[pack_size];
  numRead = read(fileDesc, buffer, pack_size - HEADERSIZE); // cleanup after first header read
  numRead = read(fileDesc, buffer, pack_size); // read first packet
  bool header = true;
	while (numRead == pack_size) {
		Packet p(buffer, pack_size);
		unsigned char check[pack_size - 1];
		memcpy(check, &buffer[1], pack_size - 1);
		if (calculateCRC(check, pack_size - 1) == 0) {
      // TODO: make parsing the packet its own function
      // TODO: some kind of execution mapping would make this cleaner
      CompositeData cd = CompositeData::parse(p);
      if (header) { 
        if (cd.hasTimeGps()) {
          outFile << "GPS time" << ",";
        }
        if (cd.hasTimeUtc()) {
          outFile << "UTC time" << "," ;
        }
        if (cd.hasYawPitchRoll()) {
          outFile << "YPR" << ",";
          if (cd.hasAttitudeUncertainty()) {
            outFile << "YPR Uncertainty" << ",";
          }
        }
        if (cd.hasAngularRate()) {
          outFile << "Angular Rate (YPRV)" << ",";
        }
        if (cd.hasAcceleration()) {
          outFile << "Acceleration" << ",";
        }
        if (cd.hasMagnetic()) {
          outFile << "Magnetic" << ",";
        }
        if (cd.hasPositionGpsLla()) {
          outFile << "GPS LLA" << ",";
        }
        if (cd.hasPressure()) { 
          outFile << "Pressure" << ",";
        }
        if (cd.hasSyncInCnt()){
          outFile << "SyncIn Count" << ",";
        }
        if (cd.hasInsStatus()){
          outFile << "InsStatus" << ",";
        }
        if (cd.hasTemperature()) {
          outFile << "Temperature (C)";
        }
        outFile << endl;
        header = false;
      }
      if (cd.hasTimeGps()) {
        uint64_t gps_time = cd.timeGps();
        outFile << gps_time << ",";
      }
      if (cd.hasTimeUtc()) {
        TimeUtc utc_time = cd.timeUtc();
        outFile << 
          (int)utc_time.year << ":" << 
          (int)utc_time.month << ":" << 
          (int)utc_time.day << ":" << 
          (int)utc_time.hour << ":" << 
          (int)utc_time.min << ":" << 
          (int)utc_time.sec << ":" << 
          (int)utc_time.ms << ",";
      }
      if (cd.hasYawPitchRoll()) {
        vec3f ypr = cd.yawPitchRoll();
        outFile << ypr << ",";
        if (cd.hasAttitudeUncertainty()) {
          vec3f yprU = cd.attitudeUncertainty();
          outFile << yprU << ",";
        }
      }
      if (cd.hasAngularRate()) {
        vec3f ar = cd.angularRate();
        outFile << ar << ",";
      }
      if (cd.hasAcceleration()) {
        vec3f ac = cd.acceleration();
        outFile << ac << ",";
      }
      if (cd.hasMagnetic()) {
        vec3f mag = cd.magnetic();
        outFile << mag << ",";
      }
      if (cd.hasPositionGpsLla()) {
        vec3d lla = cd.positionEstimatedLla();
        outFile << lla << ",";
      }
      if (cd.hasPressure()) { 
        float pres = cd.pressure();
        outFile << pres << ",";
      }
      if (cd.hasSyncInCnt()){
        uint32_t cnt = cd.syncInCnt();
        outFile << cnt << ",";
      }
      if (cd.hasInsStatus()){
        InsStatus stat = cd.insStatus();
        outFile << stat << ",";
      }
      if (cd.hasTemperature()) {
        float temp = cd.temperature();
        outFile << temp;
      }
      outFile << endl;
		}
		else {
			std::cout << "Packet Corrupted" << endl;
			unsigned char syncByte;
			long offset = 0;
			read(fileDesc, &syncByte, 1);
			while ((syncByte != 0xFA) && (offset < fileSize)) {
				offset =lseek(fileDesc, 0, SEEK_CUR);
				std::cout << "Location: " << offset << " bytes" << endl;
				if (offset == -1L) {
					perror("Can't realign after corrupted packet: ");
					return -1;
				}
				read(fileDesc, &syncByte, 1);
			}
			lseek(fileDesc, -1, SEEK_CUR);
			countCorrupt++;
		}

		numRead = read(fileDesc, buffer, pack_size);
		if (numRead == -1) {
			perror("Can't load next packet: ");
		}
		else if(numRead == 0) {
			std::cout << "Reached end of file." << endl;
			std::cout << "Packets recieved: " << count << endl;
			std::cout << "Packets corrupted: " << countCorrupt << endl;
			outFile.close();
			return 0;
		}
		count++;
	}
	std::cout << "Packets recieved: " << count << endl;
	std::cout << "Packets corrupted: " << countCorrupt << endl  ;
	outFile.close();
}
