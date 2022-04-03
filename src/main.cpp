#include <CPRD.h>
#include <config.h>
#include <WiFi.h>

/* BROWNOUT DETECTION DISABLED */
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
/* BROWNOUT DETECTION DISABLED */

/* TIMERS */
static const uint8_t TFc_MaxNumOfTimers = 20;

static uint8_t TFv_NoOfRegisteredTimers = 0;
static uint8_t TFv_TimerIDNr[TFc_MaxNumOfTimers + 1];
static uint32_t TFv_TimeStampMillis[TFc_MaxNumOfTimers + 1];

void TF_ClearAllTimers() { // simply set TFv_NoOfRegisteredTimer to zero which means first registered timer is using TimerID_Nr 1 
    TFv_NoOfRegisteredTimers = 0; 
}

int TF_OccupyTimer() {
    int l_Result = -1; // initialise value to return with value meaning "All timers in use"
    // if a free timer is available 
    if (TFv_NoOfRegisteredTimers < TFc_MaxNumOfTimers) {
        TFv_NoOfRegisteredTimers = TFv_NoOfRegisteredTimers + 1;
        l_Result = TFv_NoOfRegisteredTimers;
    } else { // return TimerID-Number
        l_Result = -1;
    } // return -1 for indicating "All timers in use"
    return l_Result;   
}

int TF_StartTimer(int8_t p_TimerID_Nr) {
    int l_result = -1; // initialise result with -1 indicating something went wrong

    // if TimerID_Nr is in the allowed range
    if ((p_TimerID_Nr  >0) && (p_TimerID_Nr  <= TFc_MaxNumOfTimers)) {
        TFv_TimeStampMillis[p_TimerID_Nr] = millis();
        l_result = 0;
    } else {
        l_result = -1;
    }
    return l_result;
}

int TF_UpDateTimer(int8_t p_TimerID_Nr, uint32_t p_IntervalMillis) {
    int l_result = -1;
    // updating the timer means add amount of milliseconds to timestamp given with p_IntervalMillis
    // this means the milliseconds passed while other code was executeddoes NOT cause an extra-delay
    // the difference to TF_StartTimer is that TF_StartTimer stores the current millis()
    // if you call TF_elapsedTime the REAL interval is the value of TF_elapsedTime PLUS the milliseconds passed through exexution of additional code 
    // Timer-ID-Nr is in the allowed range
    if ((p_TimerID_Nr  > 0) && (p_TimerID_Nr  <= TFc_MaxNumOfTimers)) {
        TFv_TimeStampMillis[p_TimerID_Nr] = TFv_TimeStampMillis[p_TimerID_Nr] + p_IntervalMillis;
        l_result = 0;
    }
    return l_result;  
}


uint32_t TF_elapsedTime(int8_t p_TimerID_Nr) {
    uint32_t l_result = -1; // initialise return-value with "something went wrong" 
    // Timer-ID-Nr is in the allowed range
    if ((p_TimerID_Nr > 0 ) && (p_TimerID_Nr  <= TFc_MaxNumOfTimers)) {
        l_result = TFv_TimeStampMillis[p_TimerID_Nr] - millis();
    }
    return l_result; 
}

int SendDataTimer; // variable used to manage nonblocking-Timer for sending ESP-NOW-Data
/* TIMERS */
CPRD* cprd;

bool searchForHotSpot() {
    // Wait a random time (0-5secs), so other nodes can boot
    // delay(random(5)*1000);
    /* Check if there already exists a hotspot */
    if (int32_t n = WiFi.scanNetworks()) {
        Serial.print("Found ");Serial.print(n);Serial.println(" Networks");
        String ssid = DEFAULT_SSID;
        eeprom_struct eepromObj = cprd->getEEPROM();
        if (eepromObj.enableSSID) {
            ssid = eepromObj.ssid;
        }
        // Serial.print("Searching for '");Serial.print(DEFAULT_SSID);Serial.println("'");
        Serial.print("Searching for '");Serial.print(ssid);Serial.println("'");
        for (uint8_t i=0; i<n; i++) {
            Serial.print("--> ");Serial.println(WiFi.SSID(i));
            // if (!strcmp(DEFAULT_SSID, WiFi.SSID(i).c_str())) {
            if (!strcmp(ssid.c_str(), WiFi.SSID(i).c_str())) {
                return true;
            }
        }
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    TF_ClearAllTimers(); 

    // prepare non-blocking timer for use
    SendDataTimer = TF_OccupyTimer();
    if (SendDataTimer > 0) { // Check if assigning a Timer-IDNr was successful
        TF_StartTimer(SendDataTimer);
    } else {
        Serial.println("registering Timer was NOT successful!");
    }

/* BROWNOUT DETECTION DISABLED */
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
/* BROWNOUT DETECTION DISABLED */

    cprd = new CPRD();
    // initialize digital pin LED_BUILTIN as an output.
    pinMode(T2, OUTPUT);
    digitalWrite(T2, LOW);

    if (searchForHotSpot()) {
        // CLIENT MODE
        cprd->setClient(true);
    } else {
        // SERVER MODE
        cprd->setClient(false);
        digitalWrite(T2, HIGH);
    }
    cprd->initialize();
    if (cprd->isClient()) {
        cprd->ScanForSlave();
    }
}
// platformio run --target uploadfs --environment esp32dev
void loop() {
  if (TF_elapsedTime(SendDataTimer) > 2000) { // set to 2 seconds so you have time to read the serial output
     TF_UpDateTimer(SendDataTimer,2000);      // if you make it faster you won't be able to read the whol emessage
     cprd->loop();
    }
}