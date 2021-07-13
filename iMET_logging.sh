#!/bin/bash
echo "launching iMET python script\n"
sleep 1
sudo python3 iMET_sampling.py /home/pi/BC6B &
sleep 1
echo "end of logger.sh"
