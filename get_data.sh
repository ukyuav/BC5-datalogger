#/bin/bash
sudo python3 $PWD/lcdlog.py & 
sleep 5 # give time for FIFO to init
sudo python3 iMET_sampling.py /home/pi/BC6B/ &
#if the usb is ever disconnected, usb_disconnect.py will stop itself
sudo python3 usb_disconnect.py &
sudo $PWD/build/getData /media/sda1/rpi3b.yml
sleep 1
echo "TRANSFERING DATA." > bc6-printlog
sleep 1
echo "DO NOT POWER OFF." > bc6-printlog
sleep 1
#stop iMET testing
sudo pkill -9 -f iMET_sampling.py
sleep 1

echo "CONVERTING DATA" > bc6-printlog
sleep 1
echo "DO NOT POWER OFF" > bc6-printlog
sleep 1

#assign usb_count with output of following command
usb_count=`ps -ef | grep -c usb_disconnect.py`
min_count=2
sleep 1
#script should show up twice under grep due to being run as sudo
if (($usb_count >= $min_count))
then
  sudo pkill -9 -f usb_disconnect.py #needs 2 spaces in front of this command if if-else block is uncommented
else
  echo "WARNING: USB WAS" > bc6-printlog
  sleep 0.2
  echo "DISCONNECTED" > bc6-printlog
  sudo pkill -9 -f usb_disconnect.py
fi

#sudo python3 $PWD/backup.py /media/sda1/rpi3b.yml
#Convert
sleep 2
echo "moving files to USB" > bc6-printlog

#commented out to avoid running the automatic csv conversion

RUN_N=0
CONF_PTH=`python3 -c 'import yaml; f = open("rpi3b.yml");data = yaml.load(f); print(data.get("output_dir"))'`
USB_PTH=`python3 -c 'import yaml; f = open("rpi3b.yml");data = yaml.load(f); print(data.get("usb_dir"))'`
NAME="/CONFIG"
EXT=".YML"
NAME="$NAME$RUN_N$EXT"
FULL_CONF="$CONF_PTH$NAME"
while [ $RUN_N -lt 100 ]; do
  echo $RUN_N
  if [ -f $FULL_CONF ]; then
    (( RUN_N++ ))
    NAME="/CONFIG"
    EXT=".YML"
    NAME="$NAME$RUN_N$EXT"
    FULL_CONF="$CONF_PTH$NAME"
  else
   #(( RUN_N-- )) ##possibly not needed
    sleep 1
    (( RUN_N-- )) #this is included on other planes but was causing issues on this one
    sleep 10
    sudo cp /home/pi/BC6B/VNDATA$RUN_N.RAW /media/sda1/VNDATA$RUN_N.RAW
    sleep 1
    sudo cp /home/pi/BC6B/DAQDATA$RUN_N.RAW /media/sda1/DAQDATA$RUN_N.RAW
    sleep 1
    sudo cp /home/pi/BC6B/SAMADATA$RUN_N.CSV /media/sda1/SAMADATA$RUN_N.CSV #TODO: change SAMA to iMET
    sleep 5
    sudo cp /home/pi/BC6B/CONFIG$RUN_N.YML /media/sda1/CONFIG$RUN_N.YML
    sudo ./extract_all.sh $USB_PTH $RUN_N
    sleep 20
    #sudo cp /home/pi/BC6B/VNDATA$RUN_N.CSV /media/sda1/VNDATA$RUN_N.CSV
    #sleep 1 #TODO
    #sudo cp /home/pi/BC6B/DAQDATA$RUN_N.CSV /media/sda1/DAQDATA$RUN_N.CSV
    #sleep 1
    break
  fi
done
sleep 10
echo "OK to power off" > bc6-printlog
