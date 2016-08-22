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
#include "pebble_sd.h"
void sendSettings();
void sendFftSpec();


/*************************************************************
 * Communications with Phone
 *************************************************************/
void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Message received!");
  // Get the first pair
  Tuple *t = dict_read_first(iterator);

  int settingsChanged = 0;
  
  // Process all pairs present
  while(t != NULL) {
    // Process this pair's key
    APP_LOG(APP_LOG_LEVEL_INFO,"Key=%d",(int) t->key);
    switch (t->key) {
    case KEY_SETTINGS:
      APP_LOG(APP_LOG_LEVEL_INFO, "***********Phone Requesting Settings");
      sendSettings();
      break;
    case KEY_DATA_TYPE:
      APP_LOG(APP_LOG_LEVEL_INFO, "***********Phone Requesting Data");
      sendSdData();
      break;
    case KEY_SET_SETTINGS:
      APP_LOG(APP_LOG_LEVEL_INFO, "***********Phone Setting Settings");
      // We don't actually do anything here - the following sections
      // process the data and update the settings.
      break;
    case KEY_DEBUG:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting DEBUG to %d",
	      debug = (int)t->value->int16);
      break;
    case KEY_DISPLAY_SPECTRUM:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting DISPLAY_SPECTRUM to %d",
	      displaySpectrum = (int)t->value->int16);
    case KEY_SAMPLE_PERIOD:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting SAMPLE_PERIOD to %d",
	      samplePeriod = (int)t->value->int16);
      settingsChanged = 1;
      break;
    case KEY_SAMPLE_FREQ:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting SAMPLE_FREQ to %d",
	      sampleFreq = (int)t->value->int16);
      settingsChanged = 1;
      break;
    case KEY_FREQ_CUTOFF:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting FREQ_CUTOFF to %d",
	      freqCutoff = (int)t->value->int16);
      settingsChanged = 1;
      break;
    case KEY_DATA_UPDATE_PERIOD:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting DATA_UPDATE_PERIOD to %d",
	      dataUpdatePeriod = (int)t->value->int16);
      break;
    case KEY_SD_MODE:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting SD_MODE to %d",
	      sdMode = (int)t->value->int16);
      break;
    case KEY_ALARM_FREQ_MIN:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting ALARM_FREQ_MIN to %d",
	      alarmFreqMin = (int)t->value->int16);
      break;
    case KEY_ALARM_FREQ_MAX:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting ALARM_FREQ_MAX to %d",
	      alarmFreqMax = (int)t->value->int16);
      break;
    case KEY_WARN_TIME:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting WARN_TIME to %d",
	      warnTime = (int)t->value->int16);
      break;
    case KEY_ALARM_TIME:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting ALARM_TIME to %d",
	      alarmTime = (int)t->value->int16);
      break;
    case KEY_ALARM_THRESH:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting ALARM_THRESH to %d",
	      alarmThresh = (int)t->value->int16);
      break;
    case KEY_ALARM_RATIO_THRESH:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting ALARM_RATIO_THRESH to %d",
	      alarmRatioThresh = (int)t->value->int16);
      break;
    case KEY_FALL_ACTIVE:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting FALL_ACTIVE to %d",
	      fallActive = (int)t->value->int16);
      break;
    case KEY_FALL_THRESH_MIN:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting FALL_THRESH_MIN to %d",
	      fallThreshMin = (int)t->value->int16);
      break;
    case KEY_FALL_THRESH_MAX:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting FALL_THRESH_MAX to %d",
	      fallThreshMax = (int)t->value->int16);
      break;
    case KEY_FALL_WINDOW:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting FALL_WINDOW to %d",
	      fallWindow = (int)t->value->int16);
      break;
    case KEY_MUTE_PERIOD:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting MUTE_PERIOD to %d",
	      mutePeriod = (int)t->value->int16);
      break;
    case KEY_MAN_ALARM_PERIOD:
      APP_LOG(APP_LOG_LEVEL_INFO,"Phone Setting MAN_ALARM_PERIOD to %d",
	      manAlarmPeriod = (int)t->value->int16);
      break;
    }
    // Get next pair, if any
    t = dict_read_next(iterator);
  }
  if (settingsChanged) {
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Accelerometer Settings Changed - resetting");
    analysis_init();
  }
}

void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  if (debug) APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

/***************************************************
 * Send some Seizure Detecto Data to the phone app.
 */
void sendSdData() {
  DictionaryIterator *iter;
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"sendSdData()");
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter,KEY_DATA_TYPE,(uint8_t)DATA_TYPE_RESULTS);
  dict_write_uint8(iter,KEY_ALARMSTATE,(uint8_t)alarmState);
  dict_write_uint32(iter,KEY_MAXVAL,(uint32_t)maxVal);
  dict_write_uint32(iter,KEY_MAXFREQ,(uint32_t)maxFreq);
  dict_write_uint32(iter,KEY_SPECPOWER,(uint32_t)specPower);
  dict_write_uint32(iter,KEY_ROIPOWER,(uint32_t)roiPower);
  // Send simplified spectrum - just 10 integers so it fits in a message.
  dict_write_data(iter,KEY_SPEC_DATA,(uint8_t*)(&simpleSpec[0]),
		  10*sizeof(simpleSpec[0]));
  app_message_outbox_send();
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"sent Results");
}

/*******************************************************
 * Send raw accelerometer data to the phone
 */
void sendRawData(AccelData *data, uint32_t num_samples) {
  DictionaryIterator *iter;
  int32_t accData[25];  // 25 samples.
  for (uint8_t i=0;i<num_samples;i++) {
    accData[i] =
        data[i].x*data[i].x
      + data[i].y*data[i].y
      + data[i].z*data[i].z;
    //accData[3*i] = data[i].x;
    //accData[3*i+1] = data[i].y;
    //accData[3*i+2] = data[i].z;
  }
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"sendRawData() - num_samples=%ld",num_samples);
  app_message_outbox_begin(&iter);
  dict_write_uint8(iter,KEY_DATA_TYPE,(uint8_t)DATA_TYPE_RAW);
  dict_write_uint32(iter,KEY_NUM_RAW_DATA,(uint32_t)num_samples);
  dict_write_data(iter,KEY_RAW_DATA,(uint8_t*)(accData),
		  num_samples*sizeof(accData[0]));
  app_message_outbox_send();
  if (debug) APP_LOG(APP_LOG_LEVEL_DEBUG,"sent Results");
}


/***************************************************
 * Send Seizure Detector Settings to the Phone
 */
void sendSettings() {
  DictionaryIterator *iter;
  APP_LOG(APP_LOG_LEVEL_INFO, "sendSettings()");
  app_message_outbox_begin(&iter);
  // Tell the phone this is settings data
  dict_write_uint8(iter,KEY_DATA_TYPE,(uint8_t)DATA_TYPE_SETTINGS);
  dict_write_uint8(iter,KEY_SETTINGS,(uint8_t)1);
  // then the actual settings
  dict_write_uint32(iter,KEY_DEBUG,(uint32_t)debug);
  dict_write_uint32(iter,KEY_DISPLAY_SPECTRUM,(uint32_t)displaySpectrum);
  // first the app version number
  dict_write_uint8(iter,KEY_VERSION_MAJOR,
		   (uint8_t)__pbl_app_info.process_version.major);
  dict_write_uint8(iter,KEY_VERSION_MINOR,
		   (uint8_t)__pbl_app_info.process_version.minor);
  // then the settings
  dict_write_uint32(iter,KEY_SAMPLE_PERIOD,(uint32_t)samplePeriod);
  dict_write_uint32(iter,KEY_SAMPLE_FREQ,(uint32_t)sampleFreq);
  dict_write_uint32(iter,KEY_FREQ_CUTOFF,(uint32_t)freqCutoff);
  dict_write_uint32(iter,KEY_DATA_UPDATE_PERIOD,(uint32_t)dataUpdatePeriod);
  dict_write_uint32(iter,KEY_SD_MODE,(uint32_t)sdMode);
  dict_write_uint32(iter,KEY_ALARM_FREQ_MIN,(uint32_t)alarmFreqMin);
  dict_write_uint32(iter,KEY_ALARM_FREQ_MAX,(uint32_t)alarmFreqMax);
  dict_write_uint32(iter,KEY_NMIN,(uint32_t)nMin);
  dict_write_uint32(iter,KEY_NMAX,(uint32_t)nMax);
  dict_write_uint32(iter,KEY_WARN_TIME,(uint32_t)warnTime);
  dict_write_uint32(iter,KEY_ALARM_TIME,(uint32_t)alarmTime);
  dict_write_uint32(iter,KEY_ALARM_THRESH,(uint32_t)alarmThresh);
  dict_write_uint32(iter,KEY_ALARM_RATIO_THRESH,(uint32_t)alarmRatioThresh);
  BatteryChargeState charge_state = battery_state_service_peek();
  dict_write_uint8(iter,KEY_BATTERY_PC,(uint8_t)charge_state.charge_percent);
  dict_write_uint32(iter,KEY_FALL_ACTIVE,(uint32_t)fallActive);
  dict_write_uint32(iter,KEY_FALL_THRESH_MIN,(uint32_t)fallThreshMin);
  dict_write_uint32(iter,KEY_FALL_THRESH_MAX,(uint32_t)fallThreshMax);
  dict_write_uint32(iter,KEY_FALL_WINDOW,(uint32_t)fallWindow);
  dict_write_uint32(iter,KEY_MUTE_PERIOD,(uint32_t)mutePeriod);
  dict_write_uint32(iter,KEY_MAN_ALARM_PERIOD,(uint32_t)manAlarmPeriod);

  app_message_outbox_send();

}


void comms_init() {
  APP_LOG(APP_LOG_LEVEL_INFO, "comms_init()");
  // Register comms callbacks
  app_message_register_inbox_received(inbox_received_callback);
  APP_LOG(APP_LOG_LEVEL_INFO, "comms_init() - registered inbox_received_callback.");
  app_message_register_inbox_dropped(inbox_dropped_callback);
  APP_LOG(APP_LOG_LEVEL_INFO, "comms_init() - registered inbox_dropped_callback.");
  app_message_register_outbox_failed(outbox_failed_callback);
  APP_LOG(APP_LOG_LEVEL_INFO, "comms_init() - registered outbox_failed_callback.");
  app_message_register_outbox_sent(outbox_sent_callback);
  APP_LOG(APP_LOG_LEVEL_INFO, "comms_init() - registered outbox_failed_callback.");
  // Open AppMessage
  //int retVal = app_message_open(app_message_inbox_size_maximum(), 
  //		   app_message_outbox_size_maximum());
  int retVal = app_message_open(INBOX_SIZE, 
  				OUTBOX_SIZE);

  if (retVal == APP_MSG_OK) 
    APP_LOG(APP_LOG_LEVEL_INFO, "comms_init() - app_message_open() Success - retVal=%d, inbox_size=%d, outbox_size=%d",retVal,
	  INBOX_SIZE,
	  OUTBOX_SIZE);
  else if (retVal == APP_MSG_OUT_OF_MEMORY)
    APP_LOG(APP_LOG_LEVEL_ERROR, "comms_init() - app_message_open() **** OUT_OF_MEMORY **** - retVal=%d, max_inbox_size=%d, max_outbox_size=%d",retVal,
	  INBOX_SIZE,
	  OUTBOX_SIZE);
  else
    APP_LOG(APP_LOG_LEVEL_ERROR, "comms_init() - app_message_open() - retVal=%d, max_inbox_size=%d, max_outbox_size=%d",retVal,
	  INBOX_SIZE,
	  OUTBOX_SIZE);




}
