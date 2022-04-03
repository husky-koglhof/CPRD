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

void TF_ClearAllTimers() {
    TFv_NoOfRegisteredTimers = 0; 
}

int TF_OccupyTimer() {
    int l_Result = -1;
    if (TFv_NoOfRegisteredTimers < TFc_MaxNumOfTimers) {
        TFv_NoOfRegisteredTimers = TFv_NoOfRegisteredTimers + 1;
        l_Result = TFv_NoOfRegisteredTimers;
    } else {
        l_Result = -1;
    }
    return l_Result;   
}

int TF_StartTimer(int8_t p_TimerID_Nr) {
    int l_result = -1;

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
    if ((p_TimerID_Nr  > 0) && (p_TimerID_Nr  <= TFc_MaxNumOfTimers)) {
        TFv_TimeStampMillis[p_TimerID_Nr] = TFv_TimeStampMillis[p_TimerID_Nr] + p_IntervalMillis;
        l_result = 0;
    }
    return l_result;  
}


uint32_t TF_elapsedTime(int8_t p_TimerID_Nr) {
    uint32_t l_result = -1;
    if ((p_TimerID_Nr > 0 ) && (p_TimerID_Nr  <= TFc_MaxNumOfTimers)) {
        l_result = TFv_TimeStampMillis[p_TimerID_Nr] - millis();
    }
    return l_result; 
}

int SendDataTimer;
/* TIMERS */
CPRD* cprd;

bool searchForHotSpot() {
    /* Check if there already exists a hotspot */
    if (int32_t n = WiFi.scanNetworks()) {
        Serial.print("Found ");Serial.print(n);Serial.println(" Networks");
        String ssid = DEFAULT_SSID;
        eeprom_struct eepromObj = cprd->getEEPROM();
        if (eepromObj.enableSSID) {
            ssid = eepromObj.ssid;
        }
        Serial.print("Searching for '");Serial.print(ssid);Serial.println("'");
        for (uint8_t i=0; i<n; i++) {
            Serial.print("--> ");Serial.println(WiFi.SSID(i));
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
    if (SendDataTimer > 0) {
        TF_StartTimer(SendDataTimer);
    } else {
        Serial.println("registering Timer was NOT successful!");
    }

    /* BROWNOUT DETECTION DISABLED */
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    /* BROWNOUT DETECTION DISABLED */

    cprd = new CPRD();
    // initialize digital pin LED_BUILTIN as an output.
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);

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

void loop() {
  if (TF_elapsedTime(SendDataTimer) > 2000) { // set to 2 seconds so you have time to read the serial output
     TF_UpDateTimer(SendDataTimer,2000);      // if you make it faster you won't be able to read the whol emessage
     cprd->loop();
    }
}