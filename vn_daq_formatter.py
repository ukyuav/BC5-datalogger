#! /usr/bin/python3 
import csv
import sys

daq_string = sys.argv[1]
vn_string = sys.argv[2]

def duplicate_row(outwriter, vn_row, daq_reader, row_disc): # duplicates a vn_row across the discrepency
    for i in range(row_disc, -1, -1):
        outwriter.writerow([l for l in vn_row.values()] + next(daq_reader))

if __name__ == "__main__":
    with open(daq_string) as daq_file:
        daq_reader = csv.reader(daq_file, delimiter=",")
        with open(vn_string) as vn_file:
            vn_reader = csv.DictReader(vn_file, delimiter=",")
            first_row = next(vn_reader)
            first_sync = int(first_row.get("SyncInCnt", False))
            if not first_sync:
                print("ERROR: cannot combine files without SYNCINCNT being recorded on VNDATA")
                quit()
            else:
                with open("vndaq_combined.csv", "w") as outfile:
                    outwriter = csv.writer(outfile, delimiter=",")
                    outwriter.writerow(vn_reader.fieldnames + ["DAQ1", "DAQ2", "DAQ3", "DAQ4", "DAQ5", "DAQ6", "DAQ7", "DAQ8"])
                    last_sync = 0
                    current_sync = int(first_sync)
                    outwriter.writerow([l for l in first_row.values()] + next(daq_reader)) # need to write the first row of things to get us started
                    try:
                        for row in vn_reader:
                            last_sync = int(current_sync) # the most recent recorded sync
                            current_sync = int(row["SyncInCnt"]) # the sync we are trying to catch up to
                            row_diff =  current_sync - last_sync # the difference between where we are vs. where we were in the daq samples
                            if row_diff < 1:
                                continue # skip row
                            elif row_diff > 1:
                                duplicate_row(outwriter, row, daq_reader, row_diff - 1)
                            else:
                                outwriter.writerow([l for l in row.values()] + next(daq_reader))
                    except StopIteration as e:
                        pass
                print("file combining complete!")
