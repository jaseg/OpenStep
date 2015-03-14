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

def sergen(s, dev):
    t = time.time()
    while True:
        s.broadcast_acquire()
        try:
#            yield [ e-500+50*i for i,e in enumerate(e for p in s.broadcast_collect_data() for e in p) ]
            yield [ e-500+30*i for i,e in enumerate(p for i in range(len(s.devices)) for p in s.collect_data(i)) ]
            t1 = time.time()
            print(t1-t)
            t = t1
        except:
            pass
            
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

#nch = 3
nch = len(ds)*3

ydata = np.zeros((w,nch))

for _ in range(nch):
    l, = plt.plot(np.arange(len(ydata)))
    l.set_alpha(0.7)
    l.set_xdata(np.arange(0,w))
    ls.append(l)

for chunk in chunked(sergen(s, 4), 1):
    pass
    plotfoo(chunk)
