# BC6-datalogger
Records data off of MCC DAQ and VectorNav INS

## Purpose
Created for use on the BLUECAT VI (BC6) unmanned aerial platform at the University of Kentucky.
This is intended for use of the Raspberry Pi 3b+. With some modification to the functionality of 
the GPIO it could be used on other linux machines. It requires an I2C bus, 3.3V power, and GPIO 
on the device to support all features.

## Prerequisites

### Hardware

- [VectorNav VN-300](https://www.vectornav.com/products/vn-300)
- [MCC USB-1608FS-Plus](https://www.mccdaq.com/usb-data-acquisition/USB-1608FS-Plus-Series)
- [SSD1306 LCD](https://www.amazon.com/MakerFocus-Display-SSD1306-3-3V-5V-Arduino/dp/B0761LV1SD/ref=sr_1_4?dchild=1&keywords=SSD1306&qid=1608144476&sr=8-4)

This may work with similar models, but has not been tested with anything but the above hardware.

### Software

The following software is automatically included and built alongside the datalogger.
- [VectorNav Programming Library v1.1](https://www.vectornav.com/support/downloads)
- [MCC Universal Library for Linux (uldaq)](https://github.com/mccdaq/uldaq/)
- [Yaml CPP Parser/Emitter](https://github.com/jbeder/yaml-cpp)

All other required software is documented in the install.sh file. When launched this file will automatically 
download all of the necessary software prerequisites for the features of the BC6. See the section below: 
[Initial Setup](https://github.com/irowebbn/BC5-datalogger#important-resources) for installation information.

## Scripts

Below is a list of scripts and their functions

* ./get_data.sh
    - Runs the full datalogging software including logging from the vectornav and uldaq as well as printing to the LCD screen.
* ./install.sh
    - Script to run upon first installation. Should be ran while connected to the internet so that the script can install the required packages.
* ./extract_all.sh
    - After running "./get_data.sh", raw binary files will be produced in the output directory. "./extract_all.sh" takes a path and run number and converts those binaries toa csv.
 
## Initial Setup

### Software install and build
For simplicity, all of the required software libraries are installed and built automatically by running the script: 
```
./install.sh
```
This will take some time and should only be done once.

### Setting up to launch on bootup

_TODO_

The reboot script can be found by doing 'crontab -e' on the pre-existing BC6 implementations and copying that command to the other planes. 

### Setting up USB drive

A USB drive is advisable for high data-rate storage. This is a good place to write the raw data while logging.
Additionally, the current launch script "./get_data.sh" looks for the configuration file in a USB drive mounted 
to "/media/sda1".

In order to accomplish this and be able to launch the script on bootup, we need to be able to auto-mount the USB drive. 
This can be done by following a guide to editing the "/etc/fstab" file [here](https://www.linuxbabe.com/desktop-linux/how-to-automount-file-systems-on-linux).

There are some custom changes that will need to be made that are slightly different from this guide. Your fstab line should look like this:

```
UUID=eb67c479-962f-4bcc-b3fe-cefaf908f01e  /media/sda1  vfat  defaults,nofail  0  2
```

## Full Launch Process

1. Connect the BC6 system to power. The Raspberry Pi should then begin booting up.
2. Wait 20 seconds
3. If connected properly, the LCD should start displaying log messages from the datalogger. This will tell you the status of the logger.
4. Potential LCD messages.
    a. "Press button to begin logging.": The system is fully connected and is ready to begin logging upon button press.
    b. "No DAQ device is detected": The DAQ device is not plugged in or is unresponsive.
    c. "Awaiting GPS Fix": The Vectornav is still waiting on GPS signal to be acquired to gain the proper GPS fix. Await the fix or move to a more open area.
    d. "Output directory will not work for sampling.": There are more than 99 log files in current directory and needs to be cleared or archived.
    e. "Sampling completed.": The sampling has successfully completed and the program has terminated. Please await this message before shutting down the Pi to avoid corruption.
    f. Other: There are various possible errors with the DAQ that often means invalid configuration. Consult the DAQ manual for permitted configurations.
5. Once the LCD reads "Press button to begin logging." then you can press the button on the side of the plane labelled "Pi" and that will launch the datalogger program.
6. Upon landing the plane, press the same button once more and await the message "Sampling completed." before shutting down.

 ## Important Resources
 - [VectorNav VN-300 User Manual](https://www.vectornav.com/docs/default-source/documentation/vn-300-documentation/vn-300-user-manual-(um005).pdf)
 - [MCC USB-1608FS-Plus User Manual](https://www.mccdaq.com/PDFs/manuals/USB-1608FS-Plus.pdf)
 - [VectorNav Programming Library (with documentation)](https://www.vectornav.com/docs/default-source/downloads/programming-library/vnproglib-1-1-4.zip?sfvrsn=fe678835_20)
 - [MCC Universal Library for Linux C/C++ Documentation](https://www.mccdaq.com/PDFs/Manuals/UL-Linux/c/index.html)
