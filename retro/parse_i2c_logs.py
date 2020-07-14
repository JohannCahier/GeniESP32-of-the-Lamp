#!/usr/bin/env python

import re
import io

# The @implements decorator checks a class implements an interface
#from accessify import implements


class ICsvBlocParser(object):
    """
    The interface any .CSV I2C log parser must conform
    """
    @classmethod
    def is_start_of_frame(cls, csv_line):
        raise NotImplementedError("ICsvBlocParser")
    @staticmethod
    def is_i2c_fragment(csv_line):
        raise NotImplementedError("ICsvBlocParser")
    @staticmethod
    def is_annotation(csv_line):
        raise NotImplementedError("ICsvBlocParser")

    def __init__(self, csv_lines):
        # TODO : check format
        self.csv = csv_lines
        pass

    def get_timestamp(self):
        raise NotImplementedError("ICsvBlocParser")
    def get_destination(self):
        raise NotImplementedError("ICsvBlocParser")
    def get_source(self):
        raise NotImplementedError("ICsvBlocParser")
    def get_framesize(self):
        raise NotImplementedError("ICsvBlocParser")
    def get_payload(self):
        raise NotImplementedError("ICsvBlocParser")
    def get_message_type(self):
        raise NotImplementedError("ICsvBlocParser")
    #def get_fullframe(self):
        #raise NotImplementedError("ICsvBlocParser")
    def get_checksum(self):
        raise NotImplementedError("ICsvBlocParser")

    @staticmethod
    def get_expected_header() -> str:
        """
        implementation should return the CSV header line
        """
        raise NotImplementedError("ICsvBlocParser")


#@implements(ICsvBlocParser)
class CsvBlocParserJon(ICsvBlocParser):
    # ========= WELCOME TO THE MARVELOUS HELPERWORLD ===============

    CSV_CELL_BYTE_VALUE_REGEX = '(Setup Write to \[)?(0x[0-9A-Za-z]{2})(\])? \+ (NAK|ACK)'

    @classmethod
    def is_start_of_frame(cls, csv_line):
        """
        Check if a given CSV line is a start-of-frame
        """
        splitting = [x.strip() for x in csv_line.split(',')]
        start_regex = re.compile(cls.CSV_CELL_BYTE_VALUE_REGEX)
        try: #avoid out-of-bound error for CSV lines with less than 3 items
            matches = start_regex.match(splitting[2])
            if matches is not None:
                return matches.group(1) is not None
            #else:
                #raise ValueError("%s should match %s"%(splitting[2], cls.CSV_CELL_BYTE_VALUE_REGEX))
        except KeyError:
            return False

    @staticmethod
    def is_i2c_fragment(csv_line):
        """
        Check if a given CSV line is part of an i2c frame
        """
        splitting = [x.strip() for x in csv_line.split(',')]
        try: #avoid out-of-bound error for CSV lines with less than 2 items
            return splitting[1] == "I2C"
        except KeyError:
            return False

    @staticmethod
    def is_annotation(csv_line):
        """
        Check if a given CSV line is part of an i2c frame
        """
        splitting = [x.strip() for x in csv_line.split(',')]
        try: #avoid out-of-bound error for CSV lines with less than 2 items
            return splitting[1] == "Note"
        except KeyError:
            return False

    def split_csv(self, index) :
        """
        Boilerplate with checkings
        """
        if (',' not in self.csv[index]):
            raise ValueError("CsvBlocParserJon::get_timestamp csv[%d] has no comma (%s)"%(index, self.csv[index]))
        splitting = [x.strip() for x in self.csv[index].split(',')]
        #assert(splitting[2].startswith("Setup Write to ["))
        #assert(self.get_framesize() is None or len(self.csv) == self.get_framesize()) # BEWARE OF INFINITE LOOP !
        if self.get_framesize() is not None and len(self.csv) != self.get_framesize():
            print("Warning, Unvalide frame !! %s", self.csv)
        return splitting

    def _get_byte_value_and_ack(self, byte_csv_cell):
        """
        returns a tuple (byte, bool) representing value / acknowledgement
        """
        grp = self._byte_csv_val_re.match(byte_csv_cell)
        if grp is not None:
            byte_value = int(grp.group(2), 0)
            ack = grp.group(3) == "ACK"
            return byte_value, ack
        raise ValueError("_get_byte_value_and_ack : %s doesn't match %s"%(byte_csv_cell, self.CSV_CELL_BYTE_VALUE_REGEX))
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
        # For frames with NAK: eek, ugly...
        #TODO : consider initing _framesize to len(csv_lines), maybe crosscheck afterward ?
        if len(self.csv) < 3:
            self._framesize = len(self.csv)
        else:
            self._framesize = None

    def get_timestamp(self):
        splitting = self.split_csv(0)
        return float(splitting[0])

    def get_destination(self):
        splitting = self.split_csv(0)
        return self._get_byte_value(splitting[2])

    def get_source(self):
        if len(self.csv) < 2:
            return None
        splitting = self.split_csv(1)
        return self._get_byte_value(splitting[2])

    def get_framesize(self):
        if len(self.csv) < 3:
            return None
        elif self._framesize is None:
            # <BEWARE OF="INFINITE LOOP !">
            #splitting = self.split_csv(2) #Don't do that !
            # </BEWARE>
            splitting = [x.strip() for x in self.csv[2].split(',')]
            self._framesize = self._get_byte_value(splitting[2])
        return self._framesize

    def get_message_type(self):
        if len(self.csv) < 4:
            return None
        splitting = self.split_csv(3)
        return self._get_byte_value(splitting[2])

    def get_payload(self):
        if len(self.csv) < 5:
            return []
        payload = []
        for payload_line_nb in range(4, len(self.csv)-1):
            splitting = self.split_csv(payload_line_nb)
            payload.append(self._get_byte_value(splitting[2]))
        return payload

    def get_checksum(self):
        splitting = self.split_csv(-1)
        return self._get_byte_value(splitting[2])

    @staticmethod
    def get_expected_header():
        return "Time [s], Analyzer Name, Decoded Protocol Result"

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

    @property
    def checksum(self):
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
    def checksum(self):
        return None

    @property
    def info(self):
        return self._info

    def __str__(self):
        str_frm = "NOTE: %s"%(self.info)
        return str_frm


#@implements(Frame)
class I2CFrame(Frame):
    def __init__(self, timestamp, destination, source, framesize, payload, checksum, acks=None):
        self._timestamp = timestamp
        self._destination = destination
        self._source = source
        self._framesize = framesize
        self._message_type = message_type
        self._payload = payload
        self._checksum = checksum
        #self._acks = acks

    def __init__(self, blocParser):
        assert(isinstance(blocParser, ICsvBlocParser))
        self._timestamp = blocParser.get_timestamp()
        self._destination = blocParser.get_destination()
        self._source = blocParser.get_source()
        self._framesize = blocParser.get_framesize()
        self._message_type = blocParser.get_message_type()
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
    def message_type(self):
        return self._message_type

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
        destination = "0x%02x"%(self.destination) if self.destination is not None else "    "
        source = "0x%02x"%(self.source) if self.source is not None else "    "
        framesize = "0x%02x"%(self.framesize) if self.framesize is not None else "    "
        message_type = "0x%02x"%(self.message_type) if self.message_type is not None else "    "
        str_frm = "%s %s %s %s  -  "%(
                                       destination,
                                       source,
                                       framesize,
                                       message_type)
        for b in self.payload:
            str_frm += "0x%02x "%(b)
        #str_frm += "%s"%(hex(self.checksum))
        #return str_frm
        return str_frm[:-1]



parser_types = {
    "Jon" : CsvBlocParserJon,
}

class CSVFormattingError(Exception):
    """Exception raised when the CSV header line doesn't match the parser's expectation
    Attributes:
        expected -- expected header
        found -- found header
    """
    def __init__(self, expected, found):
        self.expected = expected
        self.found = found


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
        csv_line = csv_line[:-1] #get rid of \n

        if line_nb == 0:
            if csv_line != parser.get_expected_header():
                raise CSVFormattingError(parser.get_expected_header(), csv_line)
            # drop the format line and
            continue

        if parser.is_i2c_fragment(csv_line):
            # is new frame ? If yes, add the current 'bloc' as a new parsed frame
            if parser.is_start_of_frame(csv_line):
                if len(csv_bloc) != 0:
                    bloc = parser(csv_bloc)
                    frames.append(I2CFrame(bloc))
                    csv_bloc = []
            csv_bloc.append(csv_line)

        elif parser.is_annotation(csv_line):
            # NOTE : if annotation is interleaved in frame, it will appear in
            #        frames list before the current bloc. timestamps may mitigate this. Don't bother for now...
            frames.append(Annotation(csv_line))
        # This CSV line looks definitely suspicious...
        else:
            #unparsed_lines.append([line_nb, csv_line])
            ts = 0.0 if len(frames) == 0 else frames[-1].timestamp
            frames.append(Annotation("%.15f, Note, %s"%(ts, csv_line)))

    return frames, unparsed_lines




# ========= DO FUNCTIONS ================
class CmdLineOptions:
    def extract_option(self, label, convert_fun):
        try:
            index_opt_from = arg.index("--"+label)
            del arg[index_opt_from]
            self._options[label] = convert_fun(arg[index_opt_from])
            del arg[index_opt_from]
        except Exception as e:
            pass

    def __init__(self, args):
        assert(isinstance(args, (list,tuple)))
        self._options = {}
        self.extract_option("from", lambda x: int(x,0))
        self.extract_option("to", lambda x: int(x,0))
        self.extract_option("size", lambda x: int(x,0))
        self.extract_option("type", lambda x: int(x,0))

    def __getitem__(self, key):
        try:
            return self._options[key]
        except KeyError:
            return None

    def match(self, frame : I2CFrame) -> bool:
        for opt, value in self._options.items():
            if   opt == "from" and frame.source       != value: return False
            elif opt == "to"   and frame.destination  != value: return False
            elif opt == "size" and frame.framesize    != value: return False
            elif opt == "type" and frame.message_type != value: return False
            if opt not in ["from", "to", "size", "type"]:
                raise RuntimeError("FUBAR alert : CmdLineOptions _options object corrupted")
        # if we made through here, the frame complies all the filters
        return True

    def dump(self):
        print("Options dump : -------");
        for opt, value in self._options.items():
            print("\"%s\": 0x%02x"%(opt, value))
        print("----------------------")

def do_hello(arg, frames):
    print("hello")

def do_print(arg, frames):
    print("               To  From  Sz   Tp   -   Payload......................(chksum)")
    for frame in frames:
        print("%04.8f    %s................(%s)"%(frame.timestamp, str(frame), frame.checksum))

def do_myshow(arg, frames):
    opts = CmdLineOptions(arg)
    print("                    To  From  Sz   Tp   -   Payload......................(chksum)")
    for frame in frames:
        marker = ">>>> " if isinstance(frame, I2CFrame) and opts.match(frame) else "     "
        print("%s%04.8f    %s................(%s)"%(marker,
                                                    frame.timestamp,
                                                    str(frame),
                                                    frame.checksum))


def do_count(arg, frames):
    #print(arg)
    opts = CmdLineOptions(arg)
    #opts.dump()
    overall_count = 0
    individual_count = {}
    for f in frames:
        if isinstance(f, I2CFrame) and opts.match(f):
            overall_count += 1
            if str(f) in individual_count:
                individual_count[str(f)] += 1
            else:
                individual_count[str(f)] = 1
    print("Total :", overall_count)
    print("                     To  From  Sz   Tp")
    for frame, count in individual_count.items():
        print(" - %5d frames are %s"%(count, frame))


do_functions = {
    'hello': {  'callback': do_hello,
                'description': "print 'hello' on stdout and exit."},
    'default':{ 'callback': do_print,
                'description': "default is alias of print"},
    'print': {  'callback': do_print,
                'description': "print the frames."},
    'count' :{  'callback': do_count,
                'description': "Count frames matching filters options"},
    'show'  : { 'callback': do_myshow,
                'description' : "Like print, but tag frames that match filters"},
}




if __name__ == "__main__":
    import sys

    def do_show_errors(lines):
        for line_nb, line_text in lines:
            print("Error, line %d: `%s`"%(line_nb, line_text))

    def usage():
        print("""
Usage : %s [--do <function>] [--noerrors]
           [--from <val>] [--to <val>] [--size <val>] [--type <val>]
           <parser_type> <i2c_log_file>"""%sys.argv[0])
        sys.exit(-1)

    if len(sys.argv) < 3:
        usage()

    try:
        index_opt_do = sys.argv.index("--noerrors")
        del sys.argv[index_opt_do]
        show_errors = False
    except:
        show_errors = True

    try:
        index_opt_do = sys.argv.index("--do")
        if index_opt_do + 1 == len(sys.argv):
            usage()
            sys.exit(-1)
        del sys.argv[index_opt_do]
        do_function = do_functions[sys.argv[index_opt_do]]['callback']
        del sys.argv[index_opt_do]
        arg = sys.argv[1:-2]
    except:
        do_function = do_functions['default']['callback']
        arg = None

    frames, unparsed_lines = load_csv_logfile(sys.argv[-1], parser_types[sys.argv[-2]])
    if show_errors:
        do_show_errors(unparsed_lines)

    do_function(arg, frames)
