# BC5-datalogger
Records data off of MCC DAQ and VectorNav INS

#### AUTHORS: 
Isaac Rowe (isaac.rowe@uky.edu) , Matthew Miller (mtmi233@uky.edu)

#### SUMMARY: 
This software suite allows users to record flight data from the 
VectorNav VN-300 and MCC USB-1608FS-Plus simultaneously.  The software 
allows users to configure sample rates for both devices and voltage 
input ranges for DAQ analog input channels.  The data from both devices
is synchronized to the same time scale and can be output to .CSV files
for convenient post-processing.

Start with the quick guide below or read futher for full documentation.
    
### Quick Start	
### Recording
Simply connect the VectorNav and DAQ to any of the four USB ports on the
Raspberry Pi. The program will automatically begin sampling upon startup.
You can check the DAQ status light to verify the program is running. The
light should be blinking. To end sampling push the button wired to the 
Pi's GPIO pins. The DAQ status light should turn solid. To run the 
program again simply unplug and restart the Pi.

WARNING: Data has the possibility of being corrupted if the program is 
not closed correctly (pressing push button). Do not restart Pi until you
have ended sampling. We are looking into resolving this issue and will
remove this note in the documentation if necessary.

NOTE:
The current setup assumes to have the software pre-installed on a 
Raspberry Pi and have the Pi automatically begin sampling upon
startup. Please read the section below labelled 'Prerequistes' if you are setting up for the first time.

#### Data Retrieval
Data retrieval is best done directly from the Linux terminal. These
instructions assume no knowledge of the command line interface, and exact 
commands to type in are given.

The getData program in the program directory creates three files upon 
execution: VECTORNAVDATAXX.CSV, DATAXX.DAQ, and CONFIGDATAXX.txt. 
XX will be a number from 0-99. Files having the same XX value were made
during the same sampling session. 

To retrieve data from the Pi do the following

1. 	Plug monitor and keyboard into the Pi. Navigate to the current program
	directory.
		
    type: `cd ./BC5-datalogger-final`

2.	Extract VectorNav data using the extract program

    type: `sudo ./extract VECTORNAVDATAXX.CSV`

    Replace XX with the corresponding value. If you are recording data and 
    immediately removing the files upon extraction, this could always be 00.

    This could take some time depending on how long you sampled for. If the 
    program looks like it is frozen, wait 10 minutes or so. 

3.	Create DAQ CSV file

    type: `sudo ./DAQ2CSV [DATAXX.DAQ] [VECTORNAVDATAXX.CSV] [CONFIGFILE.txt]`
	
	Once again, replace the XX with the number from the sampling you are
	using. The configuration file you include as the third argument should 
	have corresponding XX values as well. This tells the program the sample
	rate and number of channels used in the sampling. 

	This outputs a DAQ file with an identical name as the input DAQ file,
	with the .CSV extension. The new DAQ file will have a timestamp column 
	followed by the value recorded for each channel for the corresponding
	sample--for example, a single channel recording will have a timestamp
	column followed by 1 column of data, and an 8 channel recording will have
	a timestamp followed by 8 columns of data.

4. 	Retrieve the created files
     While the terminal is the preferred method for creating our output files, you can retrieve the files any way you please. You can boot the pi up with a monitor, keyboard, and mouse and copy the files to a flash drive with the graphical user interface. It is also possible to copy files via ssh over the network on another machine. Contact one of the creators for detailed instructions on how to do this.

#### Notes: 
Ideally this should be a two step process-- take data and then extract
and convert data into the correct form. We are working on unifying the 
process and will update the manual as necessary when this occurs.

Note that when the program is ran from startup, the created files will be read only to users. The terminal commands above have been typed assuming sudo priveleges are needed.

If you have issues with any of the methods described in this guide 
contact one of the authors. If more manuals are necessary we will create
them.

### Table of contents
- [Purpose](https://github.com/irowebbn/BC5-datalogger#purpose)
- [Prerequisites](https://github.com/irowebbn/BC5-datalogger#prerequisites)
- [Building](https://github.com/irowebbn/BC5-datalogger#building)
- [Usage](https://github.com/irowebbn/BC5-datalogger#usage)
- [Other notes](https://github.com/irowebbn/BC5-datalogger#other-notes)
- [Important resources](https://github.com/irowebbn/BC5-datalogger#important-resources)

## Purpose
Created for use on the BLUECAT V unmanned platform at the University of Kentucky.
This was originally intended to run on a Raspberry Pi, but it should work on most Linux machines.

## Prerequisites

### Hardware
- [VectorNav VN-300](https://www.vectornav.com/products/vn-300)
- [MCC USB-1608FS-Plus](https://www.mccdaq.com/usb-data-acquisition/USB-1608FS-Plus-Series)

This may work with similar models, but has not been tested with anything but the above hardware.

### Software
- [VectorNav Programming Library v1.1](https://www.vectornav.com/support/downloads)
    - v1.1.4 has also been tested
    - Simply download, unzip, and run `make` to build library and example files.
    ```sh
        $ wget https://www.vectornav.com/docs/default-source/downloads/programming-library/vnproglib-1-1-4.zip
        $ unzip vnproglib-1-1-4.zip
        $ cd vnproglib-1-1-4/cpp
        $ make
    ```
 
- [MCC Universal Library for Linux (uldaq)](https://github.com/mccdaq/uldaq/)
    - This requires [libusb](https://github.com/libusb/libusb)
         - If you're on Ubuntu/Raspian, use `sudo apt-get install libusb-1.0-0-dev`. See the [uldaq README](https://github.com/mccdaq/uldaq/blob/master/README.md) for more installation options.
    - Install uldaq using the following commands: 
    ```sh
        $ wget https://github.com/mccdaq/uldaq/releases/download/v1.0.0/libuldaq-1.0.0.tar.bz2
        $ tar -xvjf libuldaq-1.0.0.tar.bz2
        $ cd libuldaq-1.0.0
        $ ./configure && make
        $ sudo make install
    ```
    
 ## Building
 
 1. Clone this repository, then enter the directory that is created.
    ```sh
        git clone  https://github.com/irowebbn/BC5-datalogger.git
    ```
 2. Edit the `VNPATH` and `MCCPATH` macros at the top of the makefile to match the locations where you installed the VectorNav and MCC software on your machine.
 3. Run `make`.
 4. Run `./getData` to begin the program.
 
 If the device already has the repository present, run `git pull` and `make` to update to update to the latest version.
 
 ## Usage
 
 1. Before starting, create a config.txt file in the directory where you will be running the program
    - Fill with the following fields as whole numbers, separated by a tab character: 
         - DAQ Sample Rate (0 - 100000 hz)
         - Number of channels to sample (aggregate sample rate cannot exceed 400 khz)
         - Voltage Range (1, 2, 5, 10)
         - Duration
 2. Wire one side of the pushbutton to ground and the other to GPIO pins 24 (WiringPi Pin 5) and 25 (WiringPi Pin 6).
 3. Add the following lines to `/etc/rc.local`
    ```
    cd /absolute/path/to/executable
    ./getData &
    ```
 4. Run `./getData`  or reboot to begin.
 5. Press the pushbutton.
 
 - There are either 3 files for each recording session:
    - DATAXX.DAQ, where XX is some number between 00 and 99
    This contains the raw binary data from the MCC DAQ. This can be interpreted as a series of double precision floats with each channel recorded from lowest channel number to highest on each sample.
    - CONFIGDATAXX.txt, where XX corresponds with a DATAXX.daq file
    This contains a record of the settings at which a particular DATAXX.daq session was run. It contains the rate, number of channels, voltage range, duration, and UTC time of when the session began. Each of these fields are separated by newlines.
    - VECTORNAVDATAXX.CSV, where XX corresponds with a DATAXX.daq file
    This contains the raw binary data from the VectorNav. It contains a series of 110 byte packets that can be read with the extract program, or the VectorNav Sensor Explorer

- If you build with the default target or run `make extract`, you can decode the binary packets recorded from the VectorNav device. Do this with `./extract VECTORNAVDATAXX.CSV`, which will create a file called VECTORNAVASCIIXX.CSV, which is human-readable.

## Other notes
- In order to prevent data loss in the event of power failure, it is necessary to edit the `/etc/fstab` file on your machine and add the `sync` mount option to the main partition. This ensures that writes to the files are immediately saved to the disk.
   
```/dev/mncblk0p6    /    ext4    defaults,sync,noatime    0    1```

- You may want to have this program run when the Pi boots, you can achieve this by [editing the rc.local file](https://www.raspberrypi.org/documentation/linux/usage/rc-local.md)

 
 ## Important Resources
 - [VectorNav VN-300 User Manual](https://www.vectornav.com/docs/default-source/documentation/vn-300-documentation/vn-300-user-manual-(um005).pdf)
 - [MCC USB-1608FS-Plus User Manual](https://www.mccdaq.com/PDFs/manuals/USB-1608FS-Plus.pdf)
 - [VectorNav Programming Library (with documentation)](https://www.vectornav.com/docs/default-source/downloads/programming-library/vnproglib-1-1-4.zip?sfvrsn=fe678835_20)
 - [MCC Universal Library for Linux C/C++ Documentation](https://www.mccdaq.com/PDFs/Manuals/UL-Linux/c/index.html)
 
