#!/usr/bin/env python3

import serial
from matplotlib import pyplot as plt
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
        
def sergen(ser):
    ser.flushInput()
    while True:
        _,*cs = ser.readline().decode('utf8').strip().split()
        if len(cs) == nch: # sanity check
            yield [int(c, 16)+n*off for n,c in enumerate(cs)]
            
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

ser = None
for dev in ['/dev/ttyUSB0', '/dev/ttyUSB1']:
    try:
        ser = serial.Serial(dev, 115200)
        break
    except:
        pass

for chunk in chunked(cavg(sergen(ser), 10), 10):
    plotfoo(chunk)
