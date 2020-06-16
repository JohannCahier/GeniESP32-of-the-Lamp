#!/usr/bin/env python

import re
import io
#from accessify import implements



class ICsvBlocParser(object):
    """
    The interface any .CSV I2C log parser must conform
    """
    def __init__(self, csv_lines):
        # TODO : check format
        self.csv = csv_lines
        pass

    def get_timestamp(self):
        pass
    def get_destination(self):
        pass
    def get_source(self):
        pass
    def get_framesize(self):
        pass
    def get_payload(self):
        pass
    #def get_fullframe(self):
        #pass
    def get_checksum(self):
        pass


#@implements(ICsvBlocParser)
class CsvBlocParserJon(ICsvBlocParser):
    # ========= WELCOME TO THE MARVELOUS HELPERWORLD ===============

    CSV_CELL_BYTE_VALUE_REGEX = '(Setup Write to \[)?(0x[0-9A-Za-z]{2})(\])? \+ (N)?ACK'

    @classmethod
    def _is_start_frame(cls, csv_line):
        """
        Check if a given CSV line is a start-of-frame
        """
        splitting = [x.strip() for x in csv_line.split(',')]
        start_regex = re.compile(cls.CSV_CELL_BYTE_VALUE_REGEX)
        print("csv_line:%s split:%s"%(csv_line, splitting))
        matches = start_regex.match(splitting[2])
        if matches is not None:
            print(matches)
            return matches.group(1) is not None
        else:
            raise("%s should match %s"%(csv_line, cls.CSV_CELL_BYTE_VALUE_REGEX))

    @classmethod
    def _is_i2c_fragment(cls, csv_line):
        """
        Check if a given CSV line is part of an i2c frame
        """
        splitting = [x.strip() for x in csv_line.split(',')]
        return splitting[1] == "I2C"

    def split_csv(self, index) :
        """
        Boilerplate with checkings
        """
        if (',' not in self.csv[index]):
            raise ValueError("CsvBlocParserJon::get_timestamp csv[%d] has no comma (%s)"%(index, self.csv[index]))
        splitting = [x.strip() for x in self.csv[index].split(',')]
        #assert(splitting[2].startswith("Setup Write to ["))
        assert(len(self.csv) == self.get_framesize()) # BEWARE OF INFINITE LOOP !
        return splitting

    def _get_byte_value_and_ack(self, byte_csv_cell):
        """
        returns a tuple (byte, bool) representing value / acknowledgement
        """
        grp = self._byte_csv_val_re.match(byte_csv_cell)
        if grp is not None:
            byte_value = int(grp.group(2), 0)
            ack = grp.group(3) is None
            return byte_value, ack
        raise ValueError("_get_byte_value_and_ack : %s doesn't match %s"%(byte_csv_cell, ))
    def _get_byte_value(self, byte_csv_cell):
        """
        same but drops ack status
        """
        val, ack = self._get_byte_value_and_ack(byte_csv_cell)
        return val

    # ============= We hope you enjoyed your stay :) ===============

    def __init__(self, csv_lines):
        ICsvBlocParser.__init__(self, csv_lines)
        self._byte_csv_val_re = re.compile(self.CSV_CELL_BYTE_VALUE_REGEX)
        self.csv = csv_lines
        self._framesize = None
    def get_timestamp(self):
        splitting = self.split_csv(0)
        return float(splitting[0])

    def get_destination(self):
        splitting = self.split_csv(0)
        return self._get_byte_value(splitting[2])

    def get_source(self):
        splitting = self.split_csv(1)
        return self._get_byte_value(splitting[2])

    def get_framesize(self):
        if self._framesize is None:
            splitting = self.split_csv(2) # BEWARE OF INFINITE LOOP !
            self._framesize = self._get_byte_value(splitting[2])
        return self._framesize

    def get_payload(self):
        payload = []
        for payload_line in self.csv[3:-1]:
            splitting = self.split_csv(payload_line)
            payload.append(self._get_byte_value(splitting[2]))




class Frame(object):
    def __init__(self, timestamp, destination, source, framesize, payload, checksum, acks=None):
        self._timestamp = timestamp
        self._destination = destination
        self._source = source
        self._framesize = framesize
        self._payload = payload
        self._checksum = checksum
        #self._acks = acks

    def __init__(self, blocParser):
        assert(isinstance(blocParser, ICsvBlocParser))
        self._timestamp = blocParser.get_timestamp()
        self._destination = blocParser.get_destination()
        self._source = blocParser.get_source()
        self._framesize = blocParser.get_framesize()
        self._payload = blocParser.get_payload()
        self._checksum = blocParser.get_checksum()
#        self._ = blocParser.get_()

    @property
    def timestamp(self):
        return self._timestamp

    @property
    def destination(self):
        return self._destination

    @property
    def source(self):
        return self._source

    @property
    def framesize(self):
        return self._framesize

    @property
    def payload(self):
        return self._payload

    @property
    def checksum(self):
        return self._checksum

    #@property
    #def (self):
        #return self._

    def __str__(self):
        str_frm = "%4.6f   %s %s %s "%(self.timestamp,
                                       hex(self.destination),
                                       hex(self.source),
                                       hex(self.framesize))
        for b in self.payload:
            str_frm += "%s "%(hex(b))
        str_frm += "%s"%(hex(self.checksum))
        return str_frm



parser_types = {
    "Jon" : {
        "timestamp_col": 0,
        "data_col": 2,
        "bloc_parser": CsvBlocParserJon,
        "is_frame_start": CsvBlocParserJon._is_start_frame,
        "is_i2c_fragment": CsvBlocParserJon._is_i2c_fragment,
    }
}


def load_csv_log(csv_file, parser_type):
    frames = []

    if isinstance(csv_file, io.IOBase):
        in_file = csv_file
    elif isinstance(csv_file, str):
        in_file = open(csv_file)
    else:
        raise TypeError("load_csv_log() please pass either a file descriptor or a filename (str)")

    csv_bloc = []
    unparsed_lines = []
    for line_nb, csv_line in enumerate(in_file):
        if line_nb == 0:
            continue #ignore title line
        csv_line = csv_line[:-1]
        print("L %5d: %s"%(line_nb, csv_line))
        if len(csv_bloc) != 0:
            if parser_type["is_frame_start"](csv_line) or\
            not parser_type["is_i2c_fragment"](csv_line):
                bloc_parser = parser_type["bloc_parser"](csv_bloc)
                print("Parse this bloc: ", csv_bloc)
                frames.append(Frame(bloc_parser))
                csv_bloc = []

        if parser_type["is_i2c_fragment"](csv_line):
            csv_bloc.append(csv_line)

        #if parser_type["bloc_parser"].is_annotation(csv_line):
            #frames.append(Annotation(csv_line))
        else:
            unparsed_lines.append([line_nb, csv_line])

    return frames


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage : %s <i2c_log_file>"%sys.argv[0])
        sys.exit(-1)
    frames = load_csv_log(sys.argv[1], parser_types["Jon"])

    for frame in frames:
        print(str(frame))
