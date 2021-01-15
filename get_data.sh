sudo python3 $PWD/lcdlog.py & 
sleep 2 # give time for FIFO to init
sudo $PWD/build/getData /media/sda1/rpi3b.yml
sudo python3 $PWD/backup.py /media/sda1/rpi3b.yml
