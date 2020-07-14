#!/usr/bin/env python3

import sys
import os
import re

"""
0.231491000000000,I2C,Setup Write to [0x10] + ACK
0.231815200000000,I2C,0x20 + ACK
0.233609400000000,I2C,0x08 + ACK
0.233941200000000,I2C,0x07 + ACK
0.234272800000000,I2C,0x00 + ACK
0.234604400000000,I2C,0x00 + ACK
"""


RE_SIGROK_DATA = "([0-9]+)-([0-9]+) I²C: Address/Data: (.*)"
RE_BUTTON_DATA = "([0-9]+)-[0-9]+ BUTTON UP/DOWN: State: Button (Up|Down)"
RE_SOF = "Start"
RE_VALUE = "(Data write:|Address write:) ([0-9A-Fa-f]{2})"
RE_N_ACK = "(N?ACK)"
RE_EOF = "Stop"

shift=int(os.getenv('SHIFT', 2))

class Byte:
    def __init__(self):
        self.value = None
        self.SOF = False
        pass

    def set_timestamp(self, sample_num, freq):
        self.timestamp = int(sample_num) / freq

    def set_SOF(self):
        self.SOF = True

    def set_value(self, value):
        self.value = int(value, 16)

    def set_ACK(self, ack_str):
        self.ack = ack_str
        if self.ack == "NACK":
            self.ack = "NAK"
    def __str__(self): # will throw exception if some values are still unset
        global shift
        return "%.15f,I2C,%s + %s"%(
            self.timestamp,
            "Setup Write to [0x%02x]"%(self.value*shift) if self.SOF else "0x%02x"%(self.value),
            self.ack)



def usage():
    print("%s <sample_freq:int> <sigrok_i2c-data_dump.input_file> [--button <sigrok_btn_up_down-data.dump>] <output-file.csv>"%sys.argv[0])
    sys.exit(-1)

if len(sys.argv) not in (4,6):
    usage()

freq = int(sys.argv[1])
infile = open(sys.argv[2], "r")
btnfile = None

if len(sys.argv) == 6:
    if sys.argv[3] == "--button":
        btnfile = open(sys.argv[4], "r")
        outfile = open(sys.argv[5], "w")
    else:
        usage()
else:
    outfile = open(sys.argv[3], "w")

outfile.write("Time [s], Analyzer Name, Decoded Protocol Result\n")

sigparser = re.compile(RE_SIGROK_DATA)
btnparser = re.compile(RE_BUTTON_DATA)
re_sof = re.compile(RE_SOF)
re_value = re.compile(RE_VALUE)
re_n_ack = re.compile(RE_N_ACK)
re_eof = re.compile(RE_EOF)

curr_byte = Byte()

parsed_lines = []

for nb, line in enumerate(infile):
    line = line[:-1]
    #print(line)
    parsed = sigparser.match(line)
    if parsed is None:
        print(sys.stderr, "ERROR line #%d: \"%s\""%(nb, line))
    else:
        parsed_lines.append(parsed)


if btnfile is not None:
    for nb, line in enumerate(btnfile):
        line = line[:-1]
        #print(line)
        parsed = btnparser.match(line)
        if parsed is None:
            print(sys.stderr, "BUTTON ERROR line #%d: \"%s\""%(nb, line))
        else:
            parsed_lines.append(parsed)

#sort by sample number
parsed_lines.sort(key=lambda x:int(x.group(1)))

for parsed_line in parsed_lines:
        #print("[%s][%s]"%(parsed_line.group(1), parsed_line.group(3)))
        try:
            sof = re_sof.match(parsed_line.group(3))
            value = re_value.match(parsed_line.group(3))
            ack = re_n_ack.match(parsed_line.group(3))
            eof = re_eof.match(parsed_line.group(3))

            if sof:
                curr_byte.set_SOF()
            elif value:
                curr_byte.set_value(value.group(2))
                curr_byte.set_timestamp(parsed_line.group(1), freq)
            elif ack:
                curr_byte.set_ACK(ack.group(1))
                outfile.write("%s\n"%str(curr_byte))
                print(str(curr_byte))
                curr_byte = Byte()
            elif eof:
                pass
        except:
            l = "%.15f,Note,%s"%(int(parsed_line.group(1)) / freq,"Button %s"%(parsed_line.group(2)))
            outfile.write(l+'\n')
            print(l)
infile.close()
outfile.close()
