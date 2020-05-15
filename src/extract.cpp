#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <cstring>
#include <sys/stat.h>
// Include this header file to get access to VectorNav sensors.
#include "vn/sensors.h"
// We need this file for our sleep function.
#include "vn/thread.h"

#define PACKETSIZE 110

using namespace std;
using namespace vn::math;
using namespace vn::sensors;
using namespace vn::protocol::uart;
using namespace vn::xplat;

char outFileStr[] = "VECTORNAVASCII00.CSV";


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


int main(int argc, char *argv[])
{
	ofstream outFile;
	for (int i = 0; i < 100; i++) {
		outFileStr[14] = i / 10 + '0';
		outFileStr[15] = i % 10 + '0';
    struct stat statbuf;
		if (stat(outFileStr, &statbuf) != 0) {//File doesn't already exist
			break;
		}
	}
	outFile.open(outFileStr);
	outFile << "Time" << " , " << "Yaw Pitch Roll" << ", " << "Yaw Pitch Roll Uncertainties" << " , " << "Yaw Pitch Roll Velocity" << " , " << "Latitude Longitude Altitude" << " , " << "Position Uncertainty" << " , " << "Velocity" << " , " << "Velocity Uncertainty" << " , " << "INS Status" << " , " << "Temperature" << " , " << "Pressure" << endl;
	//outFile.close();

	FILE* inputFile = fopen(argv[1], "rb+");
	if (inputFile == NULL) {
		perror("Can't open file: ");
	}
	int fileDesc = fileno(inputFile);
	int fileSize = lseek(fileDesc, 0, SEEK_END); // Find filesize
	lseek(fileDesc, 0, SEEK_SET); //Reset to the beginning
	char buffer[PACKETSIZE];
	int numRead = read(fileDesc, buffer, PACKETSIZE);
	if (numRead == -1) {
		perror("Can't read first packet: ");
	}
	std::cout << "Extraction in progress, please do not quit the program." << endl;
	int count = 0;
	int countCorrupt = 0;
	while (numRead == PACKETSIZE) {
		Packet p(buffer, PACKETSIZE);
		unsigned char check[PACKETSIZE - 1];
		memcpy(check, &buffer[1], PACKETSIZE - 1);
		if (calculateCRC(check, PACKETSIZE - 1) == 0) {
		//if((byte)buffer[0] == 0xFA){
			uint64_t time = p.extractUint64();
			vec3f ypr = p.extractVec3f();
			vec3f yprV = p.extractVec3f();
			vec3d lla = p.extractVec3d();
			vec3f vel = p.extractVec3f();
			uint16_t insStatus = p.extractUint16();
			float temp = p.extractFloat();
			float pres = p.extractFloat();
			vec3f yprU = p.extractVec3f();
			float posU = p.extractFloat();
			float velU = p.extractFloat();

			insStatus = insStatus & (0x0003); // We really only care about the first 2 bits here (INS Filter Mode)

			outFile.precision(17);
			// add << fixed before lla or change vector.h
			outFile << time << " , " << ypr << ", " << yprU << " , " << yprV << " , "<< fixed << lla << " , " << posU << " , " << vel << ", " << velU << " , " << insStatus << " , " << temp << " , " << pres << endl;
			//outFile.close();
			//cout << "Successfully read packet: " << numRead << " bytes" << endl;
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

		numRead = read(fileDesc, buffer, PACKETSIZE);
		if (numRead == -1) {
			perror("Can't load next packet: ");
		}
		else if(numRead == 0) {
			std::cout << "Reached end of file." << endl;
			std::cout << "Packets recieved: " << count << endl;
			std::cout << "Packets corrupted: " << countCorrupt;
			outFile.close();
			return 0;
		}
		count++;
	}
	std::cout << "Packets recieved: " << count << endl;
	std::cout << "Packets corrupted: " << countCorrupt << endl  ;
	outFile.close();
}
