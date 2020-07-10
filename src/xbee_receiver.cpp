// C library headers
 #include <stdio.h>
 #include <string.h>

 // Linux headers
 #include <fcntl.h> // Contains file controls like O_RDWR
 #include <errno.h> // Error integer and strerror() function
 #include <termios.h> // Contains POSIX terminal control definitions
 #include <unistd.h> // write(), read(), close()

#include <vn/sensors.h>
#include <vn/thread.h>
#include <string.h>
#define PACKETSIZE 110
#define DAQ_SIZE 64

using namespace std;
using namespace vn::math;
using namespace vn::sensors;
using namespace vn::protocol::uart;
using namespace vn::xplat;

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

int main(int argc, char* argv[]) {
 // Open the serial port. Change device path as needed (currently set to an standard FTDI USB-UART cable type device)
 int serial_port = open("/dev/pts/2", O_RDWR);

 // Create new termios struc, we call it 'tty' for convention
 struct termios tty;
 memset(&tty, 0, sizeof tty);

 // Read in existing settings, and handle any error
 if(tcgetattr(serial_port, &tty) != 0) {
   printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
 }

 tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
 tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
 tty.c_cflag |= CS8; // 8 bits per byte (most common)
 tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
 tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

 tty.c_lflag &= ~ICANON;
 tty.c_lflag &= ~ECHO; // Disable echo
 tty.c_lflag &= ~ECHOE; // Disable erasure
 tty.c_lflag &= ~ECHONL; // Disable new-line echo
 tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
 tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
 tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

 tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
 tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
 // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT PRESENT ON LINUX)
 // tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars (0x004) in output (NOT PRESENT ON LINUX)

 tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
 tty.c_cc[VMIN] = 0;

 // Set in/out baud rate to be 9600
 cfsetispeed(&tty, B9600);
 cfsetospeed(&tty, B9600);

 // Save tty settings, also checking for error
 if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
   printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
 }

 // Allocate memory for read buffer, set size according to your needs
 char read_buf [PACKETSIZE + DAQ_SIZE];
 memset(&read_buf, '\0', sizeof(read_buf));

 char vn_buf[PACKETSIZE];
 char daq_buf[DAQ_SIZE]; 

 // Read bytes. The behaviour of read() (e.g. does it block?,
 // how long does it block for?) depends on the configuration
 // settings above, specifically VMIN and VTIME
 int num_bytes = 0; 
 for(;;) { 
   num_bytes = read(serial_port, &read_buf, sizeof(read_buf));

   if (num_bytes != 174) {
     cout << "bad packet alignment" << endl;
     continue;
   }
   // n is the number of bytes read. n may be 0 if no bytes were received, and can also be -1 to signal an error.
   if (num_bytes < 0) {
     printf("Error reading: %s", strerror(errno));
   }

    memcpy(vn_buf, read_buf, PACKETSIZE);
    memcpy(daq_buf, &read_buf[PACKETSIZE-1], DAQ_SIZE);

    // parse vn_packet 
    Packet p(vn_buf, PACKETSIZE);
    unsigned char check[PACKETSIZE - 1];
    memcpy(check, &vn_buf[1], PACKETSIZE - 1);
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

      // add << fixed before lla or change vector.h
      cout << time << " , " << ypr << ", " << yprU << " , " << yprV << " , "<< fixed << lla << " , " << posU << " , " << vel << ", " << velU << " , " << insStatus << " , " << temp << " , " << pres << endl;
      //outFile.close();
      //cout << "Successfully read packet: " << numRead << " bytes" << endl;
    }

    // parse daq_packet
    double current_double;
    long  lod; 
    char * _dc;
    char double_packet [8]; 
    for(int i = 0; i < 8; i++) { 
      memcpy(double_packet, &daq_buf[i*8], 8); 
      lod = strtol(double_packet, &_dc , 16);
      current_double = (double) lod; 
      cout << " Channel " << i+1 << ": " << current_double;
    }
    cout << endl;
    usleep(1000000);
  }
 // Here we assume we received ASCII data, but you might be sending raw bytes (in that case, don't try and
 // print it to the screen like this!)
 // printf("Read %i bytes. Received message: %s \n", num_bytes, read_buf);

 close(serial_port);
}
