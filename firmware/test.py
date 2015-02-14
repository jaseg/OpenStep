#!/usr/bin/env python3

import serial
import serial_mux
from matplotlib import pyplot as plt
import time
import struct
import numpy as np

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
        
def sample(s):
    s.ser.write(b'\\#\xFF\xFF')
    time.sleep(0.040) # maximum sample time should be around 30ms
    s.ser.write(b'\\#\xFF\xFE')
    return struct.unpack('HHH', s.ser.read(6))

def sergen(s):
    while True:
        yield sample(s)
            
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
for dev in ['/dev/ttyUSB0', '/dev/ttyUSB1']:
    try:
        s = serial_mux.SerialMux(dev)
        break
    except:
        pass

s.discover()

for chunk in chunked(sergen(s), 20):
    plotfoo(chunk)
