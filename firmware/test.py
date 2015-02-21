#!/usr/bin/env python3

import serial
import serial_mux
from matplotlib import pyplot as plt
import time
import struct
import numpy as np
import sys

w = 400
nch = 3
off = 100

def plotfoo(dpoints):
    global ydata, line
    ydata = np.concatenate((ydata[len(dpoints):], dpoints))
    for n,l in enumerate(ls):
        l.set_ydata(ydata[:,n])
    plt.draw()
    plt.pause(0.0000000001)
    
def cavg(gen, n):
    while True:
        l = []
        for i in range(n):
            l.append(next(gen))
        yield [sum(l)/n for l in zip(*l)]

def read_sample(s):
    _pad,a,b,c = struct.unpack('<bHHH', s.ser.read(7))
    return a,b,c
        
def sample(s, n):
    s.ser.write(b'\\#\xFF\xFF')
    time.sleep(0.040) # maximum sample time should be around 30ms
    s.ser.write(b'\\#\xFF\xFE')
    samples = [read_sample(s) for _i in range(n+1)]
    return samples[0]
#    time.sleep(0.040) # maximum sample time should be around 30ms
#    s.ser.write(b'\\#'+bytes([dev])+b'\x01')
#    return read_sample(s)

def sergen(s, dev):
    while True:
        yield sample(s, dev)
            
def chunked(gen, n):
    while True:
        l = []
        for _ in range(n):
            l.append(next(gen))
        yield l

plt.ion()

plt.xlim([0,w-1])
plt.ylim([0,1024])

ydata = np.zeros((w,nch))

ls = []

for _ in range(nch):
    l, = plt.plot(np.arange(len(ydata)))
    l.set_alpha(0.7)
    l.set_xdata(np.arange(0,w))
    ls.append(l)

s = None
for dev in ['/dev/ttyUSB0', '/dev/ttyUSB1', '/dev/ttyUSB2', '/dev/ttyUSB3', '/dev/ttyUSB4', '/dev/ttyUSB5']:
    try:
        s = serial_mux.SerialMux(dev)
        break
    except:
        pass

ds = s.discover()
print(ds)

dev = int(sys.argv[1])

for chunk in chunked(sergen(s, dev), 1):
    plotfoo(chunk)
