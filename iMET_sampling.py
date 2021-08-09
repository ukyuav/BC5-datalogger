# NAME: serial_write.py
# AUTHOR: Matthew Miller
# PURPOSE: Records data from iMet sensor into a file called IMETDATAXX.CSV.
#			If the program detects that an ADC has been attached, the program
#			will send ADC data, immediately followed by iMet data through the
#			serial port. This is intended to be used with an xBee in coordination
#			with a ground station computer. The ground station computer will be 
#			set up to display a live graph of the data.

#NOTE: all xbee communication lines have been commented out for the time being (serial initialization and a single write inside the while loop
#										This was done on 6/7/21 for testing purposes)

import os
import time
import serial
import sys
from gpiozero import LED, Button

#check if Arduino ADC is connected via USB
#if not used, program continues with only iMET recording
try:
    ser_ADC = serial.Serial(
            port='/dev/ARD',
            baudrate = 9600,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS,
            timeout=2
    )
    use_ADC = 1
except: #error if no adc attached.
    print("No ADC attached. Not using.\n")
    use_ADC = 0

      
#ser_xBee = serial.Serial(
#        port='/dev/ttyS0',
#        baudrate = 9600,
#        parity=serial.PARITY_NONE,
#        stopbits=serial.STOPBITS_ONE,
#        bytesize=serial.EIGHTBITS,
#        timeout=1
#)

#check if iMet attached
try:
    ser_iMET = serial.Serial(
            port='/dev/ttyUSB1',
            baudrate = 57600,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            bytesize=serial.EIGHTBITS,
            timeout=1
	)
except:
        print("no iMET attached, cannot continue. \n")
        exit()

try:
    collect_ADC = LED(27)
    stop_button = Button(25, pull_up=False) #TODO: was 25, changed to 13/17

    counter=0#potentially unneccessary

#open iMET file
#first get filename
    config = "/home/pi/BC6B/CONFIG"
    location = sys.argv[1]
    init_iMET_filename = "IMETDATA"
    for i in range(100):
        j = i-1
        to_append = str(j)+".CSV" #TODO was i
        config_to_append = str(i) + ".YML"
        true_config = config + config_to_append
        iMET_appended = location + init_iMET_filename + to_append
        file_exists = os.path.isfile(true_config)
        if file_exists == 0: #if file doesn't exist exit loop and create it
			     #file is created in the following while loop
            break

    print("launching while loop \n")
    
    temp = 1
    while(True):
        if use_ADC == 1:
            iMET_file = open(iMET_appended, "a+") #open new iMET file appending
            data_iMET = ser_iMET.readline().decode()
            collect_ADC.on() #tell arduino to grab data
            collect_ADC.off()
            data_ADC = ser_ADC.readline().decode()
	    #data_all = data_ADC + data_iMET
            #ser_xBee.write(data_all)
            iMET_file.write(data_iMET)
            iMET_file.write("\n")
            iMET_file.close()
		
        else:
        
            iMET_file = open(iMET_appended, "a+") #open new iMET file appending
            data_iMET = ser_iMET.readline().decode()
            iMET_file.write(data_iMET)
            iMET_file.write("\n")
            iMET_file.close()
    
#stop_button.wait_for_press()
#print("aye bro the button was pressed")
    """
    time.sleep(1)
    for x in range (10):
             iMET_file = open(iMET_appended, "a+") #open new iMET file appending
             data_iMET = ser_iMET.readline().decode()
             print(data_iMET)
             iMET_file.write(data_iMET)
             iMET_file.write("\n")
             iMET_file.close()
    

    print("done\n")
    """
except:
    print("exception thrown")
    if not iMET_file.closed:
        iMET_file.close()
		
		
