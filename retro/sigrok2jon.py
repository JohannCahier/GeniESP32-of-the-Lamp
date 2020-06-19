#!/usr/bin/env python3

import sys
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
RE_SOF = "Start"
RE_VALUE = "(Data write:|Address write:) ([0-9A-Fa-f]{2})"
RE_N_ACK = "(N?ACK)"
RE_EOF = "Stop"

class Byte:
    def __init__(self):
        self.SOF = False
        pass

    def set_timestamp(self, sample_num, freq):
        self.timestamp = int(sample_num) / freq

    def set_value(self, value):
        self.value = int(value, 16)

    def set_SOF(self):
        self.SOF = True

    def set_ACK(self, ack_str):
        self.ack = ack_str

    def __str__(self): # will throw exception if some values are still unset
        return "%.15f,I2C,%s + %s"%(
            self.timestamp,
            "Setup Write to [%s]"%hex(self.value) if self.SOF else "0x%s"%hex(self.value),
            self.ack)



def usage():
    print("%s <sample_freq:int> <sigrok_i2c-data_dump.input_file> <output-file.csv>"%sys.argv[0])
    sys.exit(-1)

if len(sys.argv) != 4:
    usage()

freq = int(sys.argv[1])
infile = open(sys.argv[2], "r")
outfile = open(sys.argv[3], "w")

sigparser = re.compile(RE_SIGROK_DATA)
re_sof = re.compile(RE_SOF)
re_value = re.compile(RE_VALUE)
re_n_ack = re.compile(RE_N_ACK)
re_eof = re.compile(RE_EOF)

curr_byte = Byte()

for nb, line in enumerate(infile):
    line = line[:-1]
    #print(line)
    parsed = sigparser.match(line)
    if parsed is None:
        print(sys.stderr, "ERROR line #%d: \"%s\""%(nb, line))
    else:
        #print("[%s][%s]"%(parsed.group(1), parsed.group(3)))
        sof = re_sof.match(parsed.group(3))
        value = re_value.match(parsed.group(3))
        ack = re_n_ack.match(parsed.group(3))
        eof = re_eof.match(parsed.group(3))

        if sof:
            curr_byte.set_SOF()
        elif value:
            curr_byte.set_timestamp(parsed.group(1), freq)
            curr_byte.set_value(value.group(2))
        elif ack:
            curr_byte.set_ACK(ack.group(1))
        elif eof:
            print(str(curr_byte))
            outfile.write("%s\n"%str(curr_byte))
            curr_byte = Byte()

infile.close()
outfile.close()
