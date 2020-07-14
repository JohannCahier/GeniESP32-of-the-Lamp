import sigrokdecode as srd

class Decoder(srd.Decoder):
    api_version = 3
    id = 'btn_up_down'
    name = 'BUTTON UP/DOWN'
    longname = 'Button Up/Down Decoder'
    desc = 'Generates annotation on button state transition'
    license = 'gplv2+'
    inputs = ['logic']
    outputs = ['btnupdown']
    channels = (
        {'id': 'btn', 'name': 'Button', 'desc': 'Nutton input'},
    )
    optional_channels = ()

    annotations = (
        ('down', 'Up to Down Button Transition'),
        ('up', 'Down to Up Button Transition'),
    )
    annotation_rows = (
        ('state', 'State', (0,1,)),
    )
    options = (
        {'id': 'filter_window', 'desc': 'Bounce filter window size (µs)', 'default': 600},
    )

    edges = (
        [0, ["Button Down", "Down", "Dn"]],
        [1, ["Button Up", "Up", "Up"]],
    )

    def __init__(self, **kwargs):
        self.reset()
        # And various other variable initializations...

    def reset(self):
        pass

    def metadata(self, key, value):
        if key == srd.SRD_CONF_SAMPLERATE:
            self.samplerate = value

    def start(self):
        self.out_ann = self.register(srd.OUTPUT_ANN)

    def handle_edge(self, state):
        if self.last is not None:
            if (self.samplenum - self.last)/self.samplerate > self.filter_window:
                self.put(self.samplenum, self.samplenum, self.out_ann, self.edges[state])
        else:
            self.put(self.samplenum, self.samplenum, self.out_ann, self.edges[state])
        self.last = self.samplenum

    def decode(self):
        self.filter_window = float(self.options['filter_window'])/1000000
        self.last = None
        print("Samplerate: %s Hz"%str(self.samplerate))
        print("Bounce windows: %s µs"%str(self.filter_window*1000000))
        while True:
            res = self.wait({0:'e'}) # wait for an edge
            if res[0] in (0,1):
                self.handle_edge(res[0])
            else:
                raise ValueError("Protocol Decoder: %s, channel 'state' must provide binary values"%(self.longname))
