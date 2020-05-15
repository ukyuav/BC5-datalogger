# BC5-datalogger
Records data off of MCC DAQ and VectorNav INS

### Table of contents
- [Purpose](https://github.com/irowebbn/BC5-datalogger#purpose)
- [Prerequisites](https://github.com/irowebbn/BC5-datalogger#prerequisites)
- [Building](https://github.com/irowebbn/BC5-datalogger#building)
- [Updating](https://github.com/irowebbn/BC5-datalogger#updating)
- [Usage](https://github.com/irowebbn/BC5-datalogger#usage)
- [Other notes](https://github.com/irowebbn/BC5-datalogger#other-notes)
- [Important resources](https://github.com/irowebbn/BC5-datalogger#important-resources)
- [TODOS](https://github.com/irowebbn/BC5-datalogger#todos)

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
 
- [MCC Universal Library for Linux (uldaq)](https://github.com/mccdaq/uldaq/)
    - This requires [libusb](https://github.com/libusb/libusb)
         - If you're on Ubuntu/Raspian, use `sudo apt-get install libusb-1.0-0-dev`. See the [uldaq README](https://github.com/mccdaq/uldaq/blob/master/README.md) for more installation options.
 
 In order to support the symbolic link for the device, create a file in the `/etc/udev/rules.d/` directory with the following rule:
 ```
 SUBSYSTEM="tty",ATTRS{idProduct}=="6001",SYMLINK+="VN"
 ```
 
 ## Building
 
 1. Clone this repository, then enter the directory that is created.
    ```sh
        git clone  https://github.com/jashley2017/BC5-datalogger.git
        cd BC5-datalogger
    ```
 2. Create a build directory
    ```sh
      mkdir build && cd build
    ```
 3. Run `cmake` and `make`.
    ```sh
      cmake ../src
      make -j4 
    ```
 4. Run `./getData` to begin the program.
 5. Once complete, run `./DAQ2CSV` or `./extract` to obtain human-readable versions of the sensor data.
 
 ## Updating 

 ```
 git pull
 cd build
 make -j4 
 ```
 
 ## Usage
 
 1. The data acquisition uses a Yaml configuration file to determine ports, rates, and sample times in the program. rpi3b.yml has been provided as an example.
 2.    If you would like to start the recording by push button, change the `PUSHTOSTART` option in main.cpp to `1` and wire the GPIO pins as shown below. 
     - <img src="https://github.com/irowebbn/BC5-datalogger/blob/master/doc/GPIO-button.png" width = "400">
 3. Run `./getData [yaml file]` to begin.
 4. Press enter to quit.
 

 - There are either 3 files for each recording session:
    - DAQDATAXX.RAW where XX is some number between 00 and 99
    This contains the raw binary data from the MCC DAQ. This can be interpreted as a series of double precision floats with each channel recorded from lowest channel number to highest on each sample.
    - CONFIGDATAXX.yml, where XX corresponds with a DAQDATAXX.RAW file
    This contains a record of the settings at which a particular DAQDATAXX.RAW session was run. It contains the rate, number of channels, voltage range, duration, and UTC time of when the session began. Each of these fields are separated by newlines.
    - VNDATAXX.RAW, where XX corresponds with a DAQDATAXX.RAW file
    This contains the raw binary data from the VectorNav. It contains a series of 110 byte packets that can be read with the extract program, or the VectorNav Sensor Explorer

- If you build with the default target or run `make extract`, you can decode the binary packets recorded from the VectorNav device. Do this with `./extract VECTORNAVDATAXX.CSV`, which will create a file called VECTORNAVASCIIXX.CSV, which is human-readable. To add timestamps to the DAQ file, run `DAQ2CSV`. The command `./DAQ2CSV [DAQFILE].DAQ [VECTORNAVFILE].CSV [CONFIGFILE].txt` will convert a DAQ file a time stamped CSV file. It reads the config file to determine the sample rate and number of DAQ channels being sampled, reads the first timestamp from the VectorNav, then timestamps each DAQ sample using a calculated period. The corresponding timestamps are calculated using `[initTime]+[period]*[iterator]`, e.g., the 5th sample of a 200 Hz run will have timestamp of `[initTime] + .005 * 5` 

## Other notes
- In order to prevent data loss in the event of power failure, it is necessary to edit the `/etc/fstab` file on your machine and add the `sync` mount option to the main partition. This ensures that writes to the files are immediately saved to the disk.
   
```
/dev/mncblk0p6    /    ext4    defaults,sync,noatime    0    1
```

- You may want to have this program run when the Pi boots, you can achieve this by [editing the rc.local file](https://www.raspberrypi.org/documentation/linux/usage/rc-local.md)

 
 ## Important Resources
 - [VectorNav VN-300 User Manual](https://www.vectornav.com/docs/default-source/documentation/vn-300-documentation/vn-300-user-manual-(um005).pdf)
 - [MCC USB-1608FS-Plus User Manual](https://www.mccdaq.com/PDFs/manuals/USB-1608FS-Plus.pdf)
 - [VectorNav Programming Library (with documentation)](https://www.vectornav.com/docs/default-source/downloads/programming-library/vnproglib-1-1-4.zip?sfvrsn=fe678835_20)
 - [MCC Universal Library for Linux C/C++ Documentation](https://www.mccdaq.com/PDFs/Manuals/UL-Linux/c/index.html)

## TODOs
* Time synchronization currently does not work as intended, the DAQ should be the master and VN the slave, however SYNC_IN is not responsive on the VN300 so far. 
* DAQ sampling should happen asynchronously from the main program.
* The binary packets from both the VN and DAQ need to be periodically sent via serial to an XBEE radio. This should run as a seperate thread.
