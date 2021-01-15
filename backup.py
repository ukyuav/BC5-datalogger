import sys
import os
import yaml

'''
This script is purposed to locally synchronize the data directories 
such that we can have the logs go to a local folder for the OwnCloud
and the USB drive for the manual extraction.
'''

with open(sys.argv[1]) as f:
    data = yaml.load(f)
    if data.get("output_dir_backup", "") != "":
        os.system(f"rsync -avzh --delete --exclude 'rpi3b.yml' {data['output_dir']}/* {data['output_dir_backup']}")
