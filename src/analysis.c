/*
  Pebble_sd - a simple accelerometer based seizure detector that runs on a
  Pebble smart watch (http://getpebble.com).

  See http://openseizuredetector.org for more information.

  Copyright Graham Jones, 2015, 2016.

  This file is part of pebble_sd.

  Pebble_sd is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  Pebble_sd is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with pebble_sd.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <pebble.h>
/* These undefines prevent SYLT-FFT using assembler code */
#undef __ARMCC_VERSION
#undef __arm__
#include "SYLT-FFT/fft.h"

#include "pebble_sd.h"


/* GLOBAL VARIABLES */
uint32_t num_samples = NSAMP_MAX;
int32_t accData[NSAMP_MAX];   
fft_complex_t *fftData;   // spectrum calculated by FFT
short fftResults[NSAMP_MAX/2];  // FFT results

int simpleSpec[10];   // simplified spectrum - 0-10 Hz

int accDataPos = 0;   // Position in accData of last point in time series.
int accDataFull = 0;  // Flag so we know when we have a complete buffer full
                      // of data.


/*************************************************************
 * Data Analysis
 *************************************************************/

/*********************************************
 * Returns the magnitude of a complex number
 * (well, actually magnitude^2 to save having to do
 * a square root.
 */
int getMagnitude(fft_complex_t c) {
  int mag;
  mag = c.r*c.r + c.i*c.i;
  return mag;
}

int getAmpl(int nBin) {
  return fftData[nBin].r;
}


/***********************************************
 * Analyse spectrum and set alarm condition if
 * appropriate.
 * This routine assumes it is called every second to check the 
 * spectrum for an alarm state.
 */
int alarm_check() {
  bool inAlarm;
  int i;
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"Alarm Check nMin=%d, nMax=%d",nMin,nMax);

  inAlarm = false;
  alarmRoi = 0;
  if (sdMode == SD_MODE_FFT) {
    inAlarm = (roiPower>alarmThresh) && (roiRatio>alarmRatioThresh);
  }
  // Check each of the multiple ROIs - any one being in alarm state is an alarm.
  else if (sdMode == SD_MODE_FFT_MULTI_ROI) {
    if (roiPower>alarmThresh) {
      for (i=0;i<=3;i++) {
	if (roiRatios[i]>alarmRatioThresh) {
	  inAlarm = true;
	  alarmRoi = i;
	  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"doAnalysis() - alarm in ROI %d", alarmRoi);
	}
      }
    }
  }
  
  if (inAlarm) {
    alarmCount+=samplePeriod;
    if (alarmCount>alarmTime) {
      alarmState = 2;
    } else if (alarmCount>warnTime) {
      alarmState = 1;
    }
  } else {
    // If we are in an ALARM state, revert back to WARNING, otherwise
    // revert back to OK.
    if (alarmState == 2) {
      alarmState = 1;
    } else {
      alarmState = 0;
      alarmCount = 0;
    }
  }
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"alarmState = %d, alarmCount=%d",alarmState,alarmCount);

  return(alarmState);
}


/**
 * accel_handler():  Called whenever accelerometer data is available.
 * Add data to circular buffer accData[] and increments accDataPos to show
 * the position of the latest data in the buffer.
 */
void accel_handler(AccelData *data, uint32_t num_samples) {
  int i;

  if (sdMode==SD_MODE_RAW) {
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"num_samples=%ld",num_samples);
    sendRawData(data,num_samples);
  } else {
    // Add the new data to the accData buffer
    for (i=0;i<(int)num_samples;i++) {
      // Wrap around the buffer if necessary
      if (accDataPos>=nSamp) { 
	accDataPos = 0;
	accDataFull = 1;
	break;
      }
      // Ignore any data when the vibrator motor was running.
      // FIXME - this doesn't seem to work - alarm latches on if the 
      //         vibrator operates.
      if (!data[i].did_vibrate) {
	// add good data to the accData array
	accData[accDataPos] = abs(data[i].x) + abs(data[i].y) + abs(data[i].z);
	accDataPos++;
      }
    }
    latestAccelData = data[num_samples-1];
  }
}

/****************************************************************
 * Simple threshold analysis to chech for fall.
 * Called from clock_tick_handler()
 */
void check_fall() {
  int i,j;
  int minAcc, maxAcc;

  int fallWindowSamp = (fallWindow*sampleFreq)/1000; // Convert ms to samples.
  APP_LOG(APP_LOG_LEVEL_DEBUG,"check_fall() - fallWindowSamp=%d",
	  fallWindowSamp);
  // Move window through sample buffer, checking for fall.
  fallDetected = 0;
  for (i=0;i<nSamp-fallWindowSamp;i++) {  // i = window start point
    // Find max and min acceleration within window.
    minAcc = accData[i];
    maxAcc = accData[i];
    for (j=0;j<fallWindowSamp;j++) {  // j = position within window
      if (accData[i+j]<minAcc) minAcc = accData[i+j];
      if (accData[i+j]>maxAcc) maxAcc = accData[i+j];
    }
    if ((minAcc<fallThreshMin) && (maxAcc>fallThreshMax)) {
      if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"check_fall() - minAcc=%d, maxAcc=%d",
	      minAcc,maxAcc);
      if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"check_fall() - ****FALL DETECTED****");
      fallDetected = 1;
      return;
    }
  }
  //if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"check_fall() - minAcc=%d, maxAcc=%d",
  //	  minAcc,maxAcc);

}


/****************************************************************
 * Carry out analysis of acceleration time series to check for seizures
 * Called from clock_tick_handler().
 */
void do_analysis() {
  int i,n;
  APP_LOG(APP_LOG_LEVEL_DEBUG,"do_analysis");
  // Calculate the frequency resolution of the output spectrum.
  // Stored as an integer which is 1000 x the frequency resolution in Hz.

  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"do_analysis");
  freqRes = (int)(1000*sampleFreq/nSamp);
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"T=%d ms, freqRes=%d Hz/(1000 bins)",
		     1000*nSamp/sampleFreq,freqRes);
  // Set the frequency bounds for the analysis in fft output bin numbers.
  nMin = (int)(1000*alarmFreqMin/freqRes);
  nMax = (int)(1000*alarmFreqMax/freqRes);

  // Set frequency bounds for multi-ROI mode
  // ROI 0 is the whole ROI
  nMins[0] = nMin;
  nMaxs[0] = nMax;

  // ROI 1 is the lower half
  nMins[1] = nMin;
  nMaxs[1] = (nMin+nMax)/2;

  // ROI 2 is the upper half
  nMins[2] = (nMin+nMax)/2;
  nMaxs[2] = nMax;

  // ROI 3 is the middle half
  nMins[3] = nMin + (nMax-nMin)/4;
  nMaxs[3] = nMax - (nMax-nMin)/4;
  
  // Calculate the bin number of the cutoff frequency
  nFreqCutoff = (int)(1000*freqCutoff/freqRes);  

  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"do_analysis():  nMin=%d, nMax=%d, nFreqCutoff=%d, fftBits=%d, nSamp=%d",
		     nMin,nMax,nFreqCutoff,fftBits,nSamp);

  if (debug) for (i=0;i<=3;i++) {
      APP_LOG(APP_LOG_LEVEL_DEBUG,"do_analysis(): nMins[%d]=%d, nMaxs[%d]=%d",
	      i,nMins[i],i,nMaxs[i]);
    }

  // Do the FFT conversion from time to frequency domain.
  // The output is stored in accData.  fftData is a pointer to accData.
  fft_fftr((fft_complex_t *)accData,fftBits);


  // Ignore position zero though (DC component)
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"Calculating specPower - nSamp=%d",nSamp);
  specPower = 0;
  for (i=1;i<nSamp/2;i++) {
    // Find absolute value of the imaginary fft output.
    if (i<=nFreqCutoff) {
      specPower = specPower + getMagnitude(fftData[i]);
    } else {
      //if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"i = %d, zeroing data above cutoff frequency",i);
      fftData[i].r = 0;
    }
    // fftResults is used by UI to display spectrum.
    fftResults[i] = getMagnitude(fftData[i]);
  }
  // specPower is average power per bin for whole spectrum.
  specPower = specPower/(nSamp/2);
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"specPower=%ld",specPower);

  // calculate spectrum power in the region of interest
  roiPower = 0;
  for (int i=nMin;i<nMax;i++) {
    roiPower = roiPower + getMagnitude(fftData[i]);
  }
  // roiPower is average power per bin within ROI.
  roiPower = roiPower/(nMax-nMin);
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"roiPower=%ld",roiPower);
  roiRatio = 10 * roiPower/specPower;

  // calculate spectrum power in each of the regions of interest
  // for multi-ROI mode.
  roiPowers[0] = roiPower;  
  for (n=1;n<=3;n++) {
    roiPowers[n]=0;
    for (int i=nMins[n];i<nMaxs[n];i++) {
      roiPowers[n] = roiPowers[n] + getMagnitude(fftData[i]);
    }
    // roiPower is average power per bin within ROI.
    roiPowers[n] = roiPowers[n]/(nMaxs[n]-nMins[n]);
    roiRatios[n] = 10 * roiPowers[n]/specPower;
    if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"roiPower[%d]=%ld",n,roiPowers[n]);
  }
  

  // Calculate the simplified spectrum - power in 1Hz bins.
  for (int ifreq=0;ifreq<10;ifreq++) {
    int binMin = 1 + 1000*ifreq/freqRes;    // add 1 to loose dc component
    int binMax = 1 + 1000*(ifreq+1)/freqRes;
    simpleSpec[ifreq]=0;
    for (int ibin=binMin;ibin<binMax;ibin++) {
      simpleSpec[ifreq] = simpleSpec[ifreq] + getMagnitude(fftData[ibin]);
    }
    simpleSpec[ifreq] = simpleSpec[ifreq] / (binMax-binMin);
    
  }


  /* Start collecting new buffer of data */
  /* FIXME = it would be best to make this a rolling buffer and analyse it
  * more frequently.
  */
  accDataPos = 0;
  accDataFull = 0;
}

void analysis_init() {
  int nsInit;  // initial number of samples per period, before rounding
  int i,ns;
  // Zero all data arrays:
  for (i = 0; i<NSAMP_MAX; i++) {
    accData[i] = 0;
  }

  // Initialise analysis of accelerometer data.
  // get number of samples per period, and round up to a power of 2
  nsInit = samplePeriod * sampleFreq;
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "samplePeriod=%d, sampleFreq=%d - nsInit=%d",
	  samplePeriod,sampleFreq,nsInit);

  for (i=0;i<1000;i++) {
    ns = 2<<i;
      if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "i=%d  ns=%d nsInit = %d",
	    i,ns,nsInit);
    if (ns >= nsInit) {
      nSamp = ns;
      fftBits = i;
      break;
    }
  }
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG, "nSamp rounded up to %d",
		     nSamp);

  
  /* Subscribe to acceleration data service */
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"Analysis Init:  Subcribing to acceleration data at frequency %d Hz",sampleFreq);
  accel_data_service_subscribe(25,accel_handler);
  // Choose update rate
  accel_service_set_sampling_rate(sampleFreq);

  fftData = (fft_complex_t*)accData;
}

