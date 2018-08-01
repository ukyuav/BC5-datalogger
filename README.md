# BC5-datalogger
Records data off of MCC DAQ and VectorNav INS

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
 
 1. Edit the `VNPATH` and `MCCPATH` macros at the top of the makefile to match the locations where you installed the VectorNav and MCC software on your machine.
 2. Run `make`.
 3. Run `./getData` to begin the program.
 
 ## Usage
 
 1. Before starting, create a config.txt file in the directory where you will be running the program
    - Fill with the following fields as whole numbers, separated by a tab character: 
         - DAQ Sample Rate (0 - 100000 hz)
         - Number of channels to sample (aggregate sample rate cannot exceed 400 khz)
         - Voltage Range (1, 2, 5, 10)
         - Duration (in minutes, this is currently ignored)
 2. Run `./getData` to begin.
 3. Press enter to quit.
 - There are either 3 files for each recording session:
    - DATAXX.DAQ, where XX is some number between 00 and 99
    This contains the raw binary data from the MCC DAQ. This can be interpreted as a series of double precision floats with each channel recorded from lowest channel number to highest on each sample.
    - CONFIGDATAXX.txt, where XX corresponds with a DATAXX.daq file
    This contains a record of the settings at which a particular DATAXX.daq session was run. It contains the rate, number of channels, voltage range, duration, and UTC time of when the session began. Each of these fields are separated by newlines.
    - VECTORNAVDATAXX.CSV, where XX corresponds with a DATAXX.daq file
    This contains the raw binary data from the VectorNav. It contains a series of 110 byte packets that can be read with the extract program, or the VectorNav Sensor Explorer
    
- If you build with the default target or run `make extract`, you can decode the binary packets recorded from the VectorNav device. Do this with `./extract VECTORNAVDATAXX.CSV`, which will create a file called VECTORNAVASCIIXX.CSV, which is human-readable.


 
 
