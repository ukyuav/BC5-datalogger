#Purpose: check usb connections every so often and output a message to LCD screen if USB-drive ever disconnects

import os
import time
import serial
import sys
import subprocess 


while True:
#os.system("lsusb")
    sub = subprocess.Popen("lsusb", shell=True, stdout=subprocess.PIPE)
    
    time.sleep(0.05)
    line = sub.stdout.read().decode()
    time.sleep(0.05)
    if "Flash Drive" not in line:
        exit()
    else:
        time.sleep(0.01)

