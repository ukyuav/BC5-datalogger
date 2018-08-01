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
    - Simply download, unzip, and run `make` to build library and example files
 
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
 
 1. Edit the `VNPATH` and `MCCPATH` macros at the top of the makefile to match the locations where you installed the VectorNav and MCC software on your machine
 2. Run `make`
 3. Run `./getData` to begin the program
 
 
