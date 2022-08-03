import vxi11
import time

class PicoLogic:
    GENERATOR_OFF=0
    GENERATOR_SQUARE_WAVE=1
    GENERATOR_COUNT=2
    GENERATOR_RANDOM=3
    TRIGGER_OFF=0
    TRIGGER_LOW_LEVEL=1
    TRIGGER_HIGH_LEVEL=2
    TRIGGER_POS_EDGE=3
    TRIGGER_NEG_EDGE=4


    def __init__(self, vxi11):
        self.vxi11 = vxi11

    def idn(self):
        return self.vxi11.ask("*IDN?")

    def set_pattern(self, pattern):
        self.vxi11.write(f"l:pat {pattern}")

    def set_rate(self, rate):
        self.vxi11.write(f"rate {rate}")

    def set_trigger(self, trigger_channel, trigger_type):
        self.vxi11.write(f"trig {trigger_channel} {trigger_type}")

    def start_capture(self, num_samples):
        self.vxi11.write(f"l:capture {num_samples}")

    def get_opc(self):
        self.vxi11.write("*opc?")
        return instr.read_raw(num=3)[0]-48

    def get_data(self):
        self.vxi11.write("data?")
        return self.vxi11.read_raw()[8:]

instr = vxi11.Instrument("192.168.1.46")
pico = PicoLogic(instr)

print(pico.idn())

pico.set_rate(500000)
pico.set_pattern(PicoLogic.GENERATOR_OFF)
pico.set_trigger(0,PicoLogic.TRIGGER_OFF)
pico.start_capture(20)

while pico.get_opc() != 1:
        print("waiting")
        time.sleep(0.5)

samples = pico.get_data()
print("     D0 D1 D2 D3 D4 D5 D6 D7")
print("     01 01 01 01 01 01 01 01")
for s in samples:
        print(f"0x{s:02x} " + ''.join([' | ' if s & 1<<bit else '|  ' for bit in range(0, 8)]))

print("done")
