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
    def is_start_of_frame(cls, csv_line):
        """
        Check if a given CSV line is a start-of-frame
        """
        splitting = [x.strip() for x in csv_line.split(',')]
        start_regex = re.compile(cls.CSV_CELL_BYTE_VALUE_REGEX)
        matches = start_regex.match(splitting[2])
        if matches is not None:
            return matches.group(1) is not None
        #else:
            #raise ValueError("%s should match %s"%(splitting[2], cls.CSV_CELL_BYTE_VALUE_REGEX))

    @staticmethod
    def is_i2c_fragment(csv_line):
        """
        Check if a given CSV line is part of an i2c frame
        """
        splitting = [x.strip() for x in csv_line.split(',')]
        return splitting[1] == "I2C"

    @staticmethod
    def is_annotation(csv_line):
        """
        Check if a given CSV line is part of an i2c frame
        """
        splitting = [x.strip() for x in csv_line.split(',')]
        return splitting[1] == "Note"

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
            # <BEWARE OF="INFINITE LOOP !">
            #splitting = self.split_csv(2) #Don't do that !
            # </BEWARE>
            splitting = [x.strip() for x in self.csv[2].split(',')]
            self._framesize = self._get_byte_value(splitting[2])
        return self._framesize

    def get_payload(self):
        payload = []
        for payload_line_nb in range(3, len(self.csv)-1):
            splitting = self.split_csv(payload_line_nb)
            payload.append(self._get_byte_value(splitting[2]))
        return payload

    def get_checksum(self):
        splitting = self.split_csv(-1)
        return self._get_byte_value(splitting[2])



class Frame(object):
    """
    Base object reprensenting a frame
    """
    def get_type(self):
        raise NotImplementedError("Frame")

    def __str__(self):
        raise NotImplementedError("Frame")

    @property
    def timestamp(self):
        raise NotImplementedError("Frame")

#@implements(Frame)
class Annotation(Frame):
    """
    Hand inserted annotation with timestamp
    """
    def __init__(self, timestamp, info):
        self._timestamp = timestamp
        self._info = info

    # TODO: remove this destructor. Jon's format specific, this breaks the abstraction
    def __init__(self, csv_line: str):
        splitting = [x.strip() for x in csv_line.split(',')]
        self._timestamp = float(splitting[0])
        self._info = splitting[2]

    @property
    def timestamp(self):
        return self._timestamp

    @property
    def info(self):
        return self._info

    def __str__(self):
        str_frm = "%4.6f   %s"%(self.timestamp, self.info)
        return str_frm


#@implements(Frame)
class I2CFrame(object):
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
    "Jon" : CsvBlocParserJon,
}


def load_csv_logfile(csv_file, parser):
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
        if len(csv_bloc) != 0:
            if not parser.is_i2c_fragment(csv_line) or parser.is_start_of_frame(csv_line):
                bloc = parser(csv_bloc)
                frames.append(I2CFrame(bloc))
                csv_bloc = []

        if parser.is_i2c_fragment(csv_line):
            csv_bloc.append(csv_line)

        elif parser.is_annotation(csv_line):
            frames.append(Annotation(csv_line))

        else:
            unparsed_lines.append([line_nb, csv_line])

    return frames, unparsed_lines




# ========= DO FUNCTIONS ================
def do_hello(arg):
    print("hello")

def do_default(arg):
    frames, unparsed_lines = load_csv_logfile(sys.argv[-1], parser_types[sys.argv[-2]])

    for frame in frames:
        print(str(frame))

    if len(unparsed_lines) > 0:
        print("We got some garbage:")
        for line in unparsed_lines:
            print("%d: %s"%(line[0], line[1]))



do_functions = {
    "hello": do_hello,
    "default" : do_default,
}




if __name__ == "__main__":
    import sys

    def usage():
        print("Usage : %s [--do <function>] <type> <i2c_log_file>"%sys.argv[0])
        sys.exit(-1)

    if len(sys.argv) < 3:
        usage()

    try:
        index_opt_do = sys.argv.index("--do")
        if index_opt_do + 1 == len(sys.argv):
            usage()
            sys.exit(-1)
        del sys.argv[index_opt_do]
        do_function = do_functions[sys.argv[index_opt_do]]
        del sys.argv[index_opt_do]
        arg = sys.argv[1:-2]
    except:
        do_function = do_default
        arg = None

    do_function(arg)
