#!/bin/bash
sudo apt-get update -y
sudo apt-get upgrade -y 
# install prerequisite libraries
sudo apt-get install -y python3 python3-pip python3-pil
sudo pip3 install adafruit-circuitpython-ssd1306
sudo pip3 install python-csv
sudo pip3 install PyYAML
sudo apt-get install -y libusb-1.0-0-dev cmake wiringpi
# generate the C++ executables
rm -rf build
mkdir build
cd build
cmake ../src
make -j4
