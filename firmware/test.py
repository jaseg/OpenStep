#!/usr/bin/env python3

import serial
import serial_mux
from matplotlib import pyplot as plt
import time
import struct
import numpy as np
import sys

w = 400
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

def read_sample(s, i):
    a,b,c = struct.unpack('<HHH', s.ser.rx_unescape(6))
    print('sampled:', a, b, c)
    return a-300+150*i,b-300+150*i,c-300+150*i
        
def sample(s, n):
    s.ser.write(b'\\?\xFF\xFF')
    time.sleep(0.040) # maximum sample time should be around 30ms
    s.ser.write(b'\\?\xFF\xFE')
    samples = [read_sample(s, i) for i in range(n)]
    print('samples', samples)
    return [ b for a in samples for b in a ]
#    time.sleep(0.040) # maximum sample time should be around 30ms
#    s.ser.write(b'\\?'+bytes([dev])+b'\x01')
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

ls = []

s = None
for dev in ['/dev/ttyUSB0', '/dev/ttyUSB1', '/dev/ttyUSB2', '/dev/ttyUSB3', '/dev/ttyUSB4', '/dev/ttyUSB5']:
    try:
        s = serial_mux.SerialMux(dev)
        break
    except:
        pass

ds = s.discover()
print(ds)

nch = 12

ydata = np.zeros((w,nch))

for _ in range(nch):
    l, = plt.plot(np.arange(len(ydata)))
    l.set_alpha(0.7)
    l.set_xdata(np.arange(0,w))
    ls.append(l)

for chunk in chunked(sergen(s, 4), 1):
    plotfoo(chunk)
