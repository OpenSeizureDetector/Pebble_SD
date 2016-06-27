#!/usr/bin/python
#
#############################################################################
#
# Copyright Graham Jones, 2013-2016
#
#############################################################################
#
#   This file is part of OpenSeizureDetector.
#
#    OpenSeizureDetector is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    OpenSeizureDetector is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with OpenSeizureDetector.  If not, see <http://www.gnu.org/licenses/>.
##############################################################################
"""Prepare sample acceleration data files to test analysis algorithms"""
 
__appname__ = "prepare_test_data.py"
__author__  = "Graham Jones"
__version__ = "0.1"
__license__ = "GNU GPL 3.0 or later"

import json
import math
import matplotlib
#matplotlib.use('Agg')

import matplotlib.pyplot as plt

import numpy


DEBUG = 0

def prepare_test_data(samplePeriod,sampleFreq,components,fname):
    nSamp = samplePeriod*sampleFreq

    i=0;
    outArr = []
    tArr = []
    while (i<=nSamp):
        t = 1.0*i / sampleFreq
        if (DEBUG): print "t=%f sec" % t
        outVal = 0
        f = 0
        while (f<len(components)):
            if (f!=0):
                val = components[f] * math.sin(2.0*math.pi*t*f)
            else:
                val = components[f]
            if (DEBUG): print "f=%d, val=%f" % (f,val)
            outVal += val
            f+=1

        outArr.append(outVal)
        tArr.append(t)
        i+=1

    return (tArr,outArr)


def doPlot(tArr,outArr,sampleFreq,components,freqArr=None,fftArr=None):
    fig = plt.figure()
    ax1 = fig.add_subplot(211)
    timeChart, = ax1.plot(tArr,outArr)
    plt.xlabel("time(s)")
    plt.ylabel("value")
    #plt.ylim([0,255])
    plt.title("Simulated data sampled at %2.0f Hz\nComponents=%s" % \
              (sampleFreq,components))
    plt.draw()

    ax2 = fig.add_subplot(212)
    freqChart, = ax2.plot(freqArr,fftArr)
    plt.draw()

    plt.show()


if __name__=="__main__":
    # Boilerplate code from https://gist.github.com/ssokolow/151572
    from optparse import OptionParser
    parser = OptionParser(version="%%prog v%s" % __version__,
            usage="%prog [options] <argument> ...",
            description=__doc__.replace('\r\n', '\n').split('\n--snip--\n')[0])
    parser.add_option('-c', '--components', dest="components",
                      default="[1,1,0,0,0,0,1,0,0,0,0,0,0]", help="[x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x] - an array of amplitudes at integer frequencies (starting at zero)")
    parser.add_option('-s', '--sampleFreq', dest="sampleFreq",
                      default=100, help="Sample frequency in Hz")
    parser.add_option('-p', '--samplePeriod', dest="samplePeriod",
                      default=5, help="Sample period in seconds")
    parser.add_option('-f', '--file', dest="fname",
                      default="test_data.txt", help="output filename.")
 
    opts, args  = parser.parse_args()

    
    print opts
    print args
    
    comps = json.loads(opts.components)
    print comps

    (tArr,outArr) = prepare_test_data(float(opts.samplePeriod),float(opts.sampleFreq),comps,opts.fname)

    #print tArr,outArr

    fftArr = numpy.fft.fft(outArr)
    freqArr = [0]
    #freqArr.append[0]
    n = 1
    while (n<len(fftArr)):
        freqArr.append(
            freqArr[n-1] + float(opts.sampleFreq)/len(tArr))
        n+=1

    
    #print "FFT:"
    #print freqArr,fftArr

    doPlot(tArr,outArr,float(opts.sampleFreq),opts.components,freqArr,numpy.absolute(fftArr))
