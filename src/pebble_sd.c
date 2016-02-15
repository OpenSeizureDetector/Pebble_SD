/*
  Pebble_sd - a simple accelerometer based seizure detector that runs on a
  Pebble smart watch (http://getpebble.com).

  See http://openseizuredetector.org.uk for more information.

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

#include "pebble_sd.h"
#include "pebble_process_info.h"
extern const PebbleProcessInfo __pbl_app_info;

/* GLOBAL VARIABLES */
// Settings (obtained from default constants or persistent storage)
int dataUpdatePeriod; // number of seconds between sending data to the phone.
int alarmFreqMin;    // Bin number of lower boundary of region of interest
int alarmFreqMax;    // Bin number of higher boundary of region of interest
int warnTime;        // number of seconds above threshold to raise warning
int alarmTime;       // number of seconds above threshold to raise alarm.
int alarmThresh;     // Alarm threshold (average power of spectrum within
                     //       region of interest.
int alarmRatioThresh;
int nMax = 0;
int nMin = 0;

int fallActive = 0;     // fall detection active (0=inactive)
int fallThreshMin = 0;  // fall detection minimum (lower) threshold (milli-g)
int fallThreshMax = 0;  // fall detection maximum (upper) threshold (milli-g)
int fallWindow = 0;     // fall detection window (milli-seconds).
int fallDetected = 0;   // flag to say if fall is detected (<>0 is fall)


Window *window;
TextLayer *text_layer;
TextLayer *alarm_layer;
TextLayer *clock_layer;
Layer *spec_layer;
AccelData latestAccelData;  // Latest accelerometer readings received.
int maxVal = 0;       // Peak amplitude in spectrum.
int maxLoc = 0;       // Location in output array of peak.
int maxFreq = 0;      // Frequency corresponding to peak location.
long specPower = 0;   // Average power of whole spectrum.
long roiPower = 0;    // Average power of spectrum in region of interest
int roiRatio = 0;     // 10xroiPower/specPower
int freqRes = 0;      // Actually 1000 x frequency resolution

int alarmState = 0;    // 0 = OK, 1 = WARNING, 2 = ALARM
int alarmCount = 0;    // number of seconds that we have been in an alarm state.



/***************************************************************************
 * User Interface
 ***************************************************************************/


/************************************************************************
 * clock_tick_handler() - Analyse data and update display.
 * Updates the text layer clock_layer to show current time.
 * This function is the handler for tick events and is called every 
 * second.
 */
static void clock_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  static char s_time_buffer[16];
  static char s_alarm_buffer[64];
  static char s_buffer[256];
  static int analysisCount=0;
  static int dataUpdateCount = 0;

  /* Only process data every ANALYSIS_PERIOD seconds */
  analysisCount++;
  if (analysisCount>=ANALYSIS_PERIOD) {
    // Do FFT analysis
    if (accDataFull) {
      do_analysis();
      if (fallActive) check_fall();  // sets fallDetected global variable.
      // Check the alarm state, and set the global alarmState variable.
      alarm_check();

      // If no seizure detected, modify alarmState to reflect potential fall
      // detection
      if ((alarmState == 0) && (fallDetected==1)) alarmState = 3;

      //  Display alarm message on screen.
      if (alarmState == 0) {
	text_layer_set_text(alarm_layer, "OK");
      }
      if (alarmState == 1) {
	//vibes_short_pulse();
	text_layer_set_text(alarm_layer, "WARNING");
      }
      if (alarmState == 2) {
	//vibes_long_pulse();
	text_layer_set_text(alarm_layer, "** ALARM **");
      }
      if (alarmState == 3) {
	//vibes_long_pulse();
	text_layer_set_text(alarm_layer, "** FALL **");
      }
      // Send data to phone if we have an alarm condition.
      if (alarmState != 0) {
	sendSdData();
      }
    }
    // Re-set counter.
    analysisCount = 0;
  }

  // See if it is time to send data to the phone.
  dataUpdateCount++;
  if (dataUpdateCount>=dataUpdatePeriod) {
    sendSdData();
    dataUpdateCount = 0;
  }
 
  // Update the display
  text_layer_set_text(text_layer, "OpenSeizureDetector");
  if (clock_is_24h_style()) {
    strftime(s_time_buffer, sizeof(s_time_buffer), "%H:%M:%S", tick_time);
  } else {
    strftime(s_time_buffer, sizeof(s_time_buffer), "%I:%M:%S", tick_time);
  }
  BatteryChargeState charge_state = battery_state_service_peek();
  snprintf(s_time_buffer,sizeof(s_time_buffer),
	   "%s %d%%", 
	   s_time_buffer,
	   charge_state.charge_percent);
  text_layer_set_text(clock_layer, s_time_buffer);
}


/**************************************************************************
 * draw_spec() - draw the spectrum to the pebble display
 */
void draw_spec(Layer *sl, GContext *ctx) {
  GRect bounds = layer_get_bounds(sl);
  GPoint p0;
  GPoint p1;
  int i,h;

  /* Draw Tick Marks at ends of region of interest */
  p0 = GPoint(nMin,0);
  p1 = GPoint(nMin,20);
  graphics_draw_line(ctx,p0,p1);

  p0 = GPoint(nMax,0);
  p1 = GPoint(nMax,20);
  graphics_draw_line(ctx,p0,p1);


  /* Now draw the spectrum */
  for (i=0;i<bounds.size.w-1;i++) {
    p0 = GPoint(i,bounds.size.h-1);
    //h = bounds.size.h*accData[i]/2000;
    h = bounds.size.h*getAmpl(i)/1000.;
    p1 = GPoint(i,bounds.size.h - h);
    graphics_draw_line(ctx,p0,p1);
  }

}

/**********************************************************************/

/**
 * window_load(): Initialise main window.
 */
static void window_load(Window *window) {
  static char verStr[16];
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  text_layer = text_layer_create(
				 (GRect) { 
				   .origin = { 0, 0 }, 
				   .size = { bounds.size.w, 
					     bounds.size.h
					     -CLOCK_SIZE
					     -ALARM_SIZE
					     -SPEC_SIZE } 
				 });
  text_layer_set_text(text_layer, "Press a button");
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  text_layer_set_font(text_layer, 
		      fonts_get_system_font(FONT_KEY_GOTHIC_24));
  layer_add_child(window_layer, text_layer_get_layer(text_layer));

  /* Create text layer for alarm display */
  alarm_layer = text_layer_create(
				 (GRect) { 
				   .origin = { 0, bounds.size.h 
					       - CLOCK_SIZE
					       - ALARM_SIZE
					       - SPEC_SIZE
				   }, 
				   .size = { bounds.size.w, ALARM_SIZE } 
				 });
  snprintf(verStr,sizeof(verStr),"V%d.%d",
	   __pbl_app_info.process_version.major,
	   __pbl_app_info.process_version.minor);
  text_layer_set_text(alarm_layer,verStr);
  text_layer_set_text_alignment(alarm_layer, GTextAlignmentCenter);
  text_layer_set_font(alarm_layer, 
		      fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(alarm_layer));


  /* Create layer for spectrum display */
  spec_layer = layer_create(
				 (GRect) { 
				   .origin = { 0, bounds.size.h - CLOCK_SIZE - SPEC_SIZE }, 
				   .size = { bounds.size.w, SPEC_SIZE } 
				 });
  layer_set_update_proc(spec_layer,draw_spec);
  layer_add_child(window_layer, spec_layer);

  /* Create text layer for clock display */
  clock_layer = text_layer_create(
				 (GRect) { 
				   .origin = { 0, bounds.size.h - CLOCK_SIZE }, 
				   .size = { bounds.size.w, CLOCK_SIZE } 
				 });
  text_layer_set_text(clock_layer, "CLOCK");
  text_layer_set_text_alignment(clock_layer, GTextAlignmentCenter);
  text_layer_set_font(clock_layer, 
		      fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(clock_layer));
}

/**
 * window_unload():  destroy window contents
 */
static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
  text_layer_destroy(alarm_layer);
  text_layer_destroy(clock_layer);
  layer_destroy(spec_layer);
}

/**
 * init():  Initialise application - create window for display and register
 * for accelerometer data readings.
 */
static void init(void) {
  APP_LOG(APP_LOG_LEVEL_DEBUG,"init() - Loading persistent storage variables...");
  // Load data from persistent storage into global variables.
  dataUpdatePeriod = DATA_UPDATE_PERIOD_DEFAULT;
  if (persist_exists(KEY_DATA_UPDATE_PERIOD))
    dataUpdatePeriod = persist_read_int(KEY_DATA_UPDATE_PERIOD);
  alarmFreqMin = ALARM_FREQ_MIN_DEFAULT;
  if (persist_exists(KEY_ALARM_FREQ_MIN))
    alarmFreqMin = persist_read_int(KEY_ALARM_FREQ_MIN);
  alarmFreqMax = ALARM_FREQ_MAX_DEFAULT;
  if (persist_exists(KEY_ALARM_FREQ_MAX))
    alarmFreqMax = persist_read_int(KEY_ALARM_FREQ_MAX);
  warnTime = WARN_TIME_DEFAULT;
  if (persist_exists(KEY_WARN_TIME))
    warnTime = persist_read_int(KEY_WARN_TIME);
  alarmTime = ALARM_TIME_DEFAULT;
  if (persist_exists(KEY_ALARM_TIME))
    alarmTime = persist_read_int(KEY_ALARM_TIME);
  alarmThresh = ALARM_THRESH_DEFAULT;
  if (persist_exists(KEY_ALARM_THRESH))
    alarmThresh = persist_read_int(KEY_ALARM_THRESH);
  alarmRatioThresh = ALARM_RATIO_THRESH_DEFAULT;
  if (persist_exists(KEY_ALARM_RATIO_THRESH))
    alarmRatioThresh = persist_read_int(KEY_ALARM_RATIO_THRESH);

  // Fall detection settings
  fallActive = FALL_ACTIVE_DEFAULT;
  if (persist_exists(KEY_FALL_ACTIVE))
    fallActive = persist_read_int(KEY_FALL_ACTIVE);
  fallThreshMin = FALL_THRESH_MIN_DEFAULT;
  if (persist_exists(KEY_FALL_THRESH_MIN))
    fallThreshMin = persist_read_int(KEY_FALL_THRESH_MIN);
  fallThreshMax = FALL_THRESH_MAX_DEFAULT;
  if (persist_exists(KEY_FALL_THRESH_MAX))
    fallThreshMax = persist_read_int(KEY_FALL_THRESH_MAX);
  fallWindow = FALL_WINDOW_DEFAULT;
  if (persist_exists(KEY_FALL_WINDOW))
    fallWindow = persist_read_int(KEY_FALL_WINDOW);
  
  // Create Window for display.
  APP_LOG(APP_LOG_LEVEL_DEBUG,"Creating Window....");
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);

  // Initialise analysis of accelerometer data.
  APP_LOG(APP_LOG_LEVEL_DEBUG,"Initialising Analysis System....");
  analysis_init();

  // Register comms callbacks
  APP_LOG(APP_LOG_LEVEL_DEBUG,"Initialising Communications System....");
  comms_init();

  /* Subscribe to TickTimerService for analysis */
  APP_LOG(APP_LOG_LEVEL_DEBUG,"Intialising Clock Timer....");
  tick_timer_service_subscribe(SECOND_UNIT, clock_tick_handler);
}

/**
 * deinit(): destroy window before app exits.
 */
static void deinit(void) {
  // Save settings to persistent storage
  persist_write_int(KEY_DATA_UPDATE_PERIOD,dataUpdatePeriod);
  persist_write_int(KEY_ALARM_FREQ_MIN,alarmFreqMin);
  persist_write_int(KEY_ALARM_FREQ_MAX,alarmFreqMax);
  persist_write_int(KEY_WARN_TIME,warnTime);
  persist_write_int(KEY_ALARM_TIME,alarmTime);
  persist_write_int(KEY_ALARM_THRESH,alarmThresh);
  persist_write_int(KEY_ALARM_RATIO_THRESH,alarmRatioThresh);

  persist_write_int(KEY_FALL_ACTIVE,fallActive);
  persist_write_int(KEY_FALL_THRESH_MIN,fallThreshMin);
  persist_write_int(KEY_FALL_THRESH_MAX,fallThreshMax);
  persist_write_int(KEY_FALL_WINDOW,fallWindow);
  
  // destroy the window
  window_destroy(window);
}

/**
 * main():  Main programme entry point.
 */
int main(void) {
  init();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
  app_event_loop();
  deinit();
}
