#!/bin/bash
sudo python3 $PWD/lcdlog.py & 
sleep 2 # give time for FIFO to init
sudo $PWD/build/getData /media/sda1/rpi3b.yml
sleep 1
echo "TRANSFERING DATA." > bc6-printlog
sleep 1
echo "DO NOT POWER OFF." > bc6-printlog
sudo python3 $PWD/backup.py /media/sda1/rpi3b.yml
# Convert
sleep 1
echo "CONVERTING DATA." > bc6-printlog
sleep 1
echo "DO NOT POWER OFF." > bc6-printlog
RUN_N=0
CONF_PTH=`python3 -c 'import yaml; f = open("rpi3b.yml");data = yaml.load(f); print(data.get("output_dir"))'` 
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
    (( RUN_N-- ))
    echo $RUN_N
    sudo ./extract_all.sh $CONF_PTH $RUN_N
    break
  fi
done
echo "OK to power off" > bc6-printlog
