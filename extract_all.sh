#!/bin/bash
display_usage() {
  echo "Usage: ./extract_all.sh {output_dir} {run_number}"
}

if [ $# -le 1 ]; then
  display_usage
  exit 1
fi
if [[ ($# == "--help") || ($# == "-h") ]] ; then 
  display_usage
  exit 0 
fi

if [[ $USER != "root" ]]; then 
 echo "Extraction program must be run as root!"
 exit 1
fi 

sudo ./build/extract $1/VNDATA$2.RAW
sudo ./build/DAQ2CSV $1/DAQDATA$2.RAW $1/VNDATA$2.RAW $1/CONFIG$2.YML
