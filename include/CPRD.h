#ifndef CPRD_h
#define CPRD_h

#include "Arduino.h"
#include <WiFi.h>
#include <esp_now.h>
#include <EEPROM.h>
#include <config.h>
#include <ESP32Servo.h>
#include <SPIFFS.h>
// #include <Arduino_JSON.h>
#include <ArduinoJson.h>

#define EEPROM_SIZE 64

typedef struct eeprom_struct {
    int boardID;
    bool enableSSID;
    String ssid;
} eeprom_struct;

#define SERVO_CENTER    90

// Must be in sync with sidebars.js:actionArray
#define SETTINGS_SAVE   0
#define PING        1
#define REBOOT      2
#define RESET       3
#define RELAY_ON    4
#define RELAY_OFF   5
#define SERVO_PULSE 6
#define CANCEL      7

#define PULSE_START 10
#define PULSE_STOP  11
#define PULSE_SAVE  12
#define CYCLE_START 13
#define CYCLE_STOP  14
#define CYCLE_SAVE  15
#define LUNG_START  16
#define LUNG_STOP   17
#define LUNG_SAVE   18
#define THERMOMETER_START   19
#define THERMOMETER_STOP    20
#define THERMOMETER_SAVE    21
#define PULSE_ACTIVATE      22

//Must match the receiver structure
typedef struct struct_message {
    int id;
    /* NEW */
    int pulseID;
    int pulseStrength;
    int pulseTimeGap;
    int pulseFrequence;
    int pulseDuration;
    int pulseTrouble = 0;
    int cycleStart;
    int cycleDuration;
    int lungStart;
    int lungDuration;
    int thermometerDuration;
    int32_t rssi;
    float voltage;
    int action; // 0=save, 1=ping, 2=reboot
    int readingId;
    bool mode = false;
    bool pulseRunning = false;
    bool cycleRunning = false;
    bool lungRunning = false;
    bool thermometerRunning = false;
    bool pingRunning = false;
    // Only needed to send data to client
    bool enableSSID;
    String SSID;
} struct_message;

typedef struct pulse_config {
    String name;
    byte pulseID = 123;
    byte pulseStrength;
    byte pulseTimeGap;
    byte pulseFrequence;
    int pulseDuration;
    int pulseTrouble = 0;
} pulse_config;

class CPRD {
    private:
        byte BOARD_ID = 0;
        int R1 = 27000;
        int R2 = 100000;
        bool initialized = false;
        bool clientMode = false;
        void initServer();
        void initClient();
        int x = sizeof(pulse_config);
    public:
        struct_message loadedConfig;
        // JSONVar pulseConfig = undefined;

        CPRD();
        ~CPRD();
        CPRD(String _macAddr){macAddress = _macAddr;};
        void initialize();
        void loop();
        void sendData(uint8_t broadcastAddress[6]);
        void sendDataToWS(byte ID, struct_message data);
        void sendDataToServer();
        void ScanForSlave();

        bool isClient() {return clientMode;}
        void setClient(bool flag) {clientMode = flag;}
        String macAddress;
        char macStr[18];
        int boardID;

        void InitESPNow() {
            WiFi.disconnect();
            if (esp_now_init() == ESP_OK) {
                Serial.println("ESPNow Init Success");
            } else {
                Serial.println("ESPNow Init Failed");
                // Retry InitESPNow, add a counte and then restart?
                // InitESPNow();
                // or Simply Restart
                ESP.restart();
            }
        }
        /*
        int getBoardID() {
            if (BOARD_ID == 0) {
                BOARD_ID = EEPROM.read(0);
            }
            return BOARD_ID;
        }
        void setBoardID(byte ID) {
            BOARD_ID = ID;
            EEPROM.write(0, BOARD_ID);
            EEPROM.commit();
        }
        */


        eeprom_struct getEEPROM() {
            eeprom_struct eepromObj;
            EEPROM.get(1, eepromObj);
            return eepromObj;
        }

        void setEEPROM(eeprom_struct &eepromObj) {
            EEPROM.put(1, eepromObj);
            EEPROM.commit();
        }

        float getVoltage() {
            return (analogRead(PWR)*R2)/(R1+R2);
        }
        float getPercentage() {
            return map(analogRead(PWR), 0.0f, 4095.0f, 0, 100);
        }

        void removeConfig() {
            SPIFFS.remove("/config_new.json");
        }

        void writeDataBack(struct_message *_ownData, pulse_config all_pulse_configs[]) {
            Serial.println("writeDataBack");

            DynamicJsonDocument doc(1512); // fixed size
            JsonObject dataForWS = doc.to<JsonObject>();

            dataForWS["id"] = _ownData->id;
            dataForWS["Blut"]["Start"] = _ownData->cycleStart;
            dataForWS["Blut"]["Durchlauf"] = _ownData->cycleDuration;
            dataForWS["Lunge"]["Start"] = _ownData->lungStart;
            dataForWS["Lunge"]["Durchlauf"] = _ownData->lungDuration;
            dataForWS["Temperatur"]["Durchlauf"] = _ownData->thermometerDuration;
            dataForWS["pulseID"] = _ownData->pulseID;

            JsonArray array = doc.createNestedArray("Puls");
            JsonObject nested;

            for (int i=0;i<20;i++) {
                if (all_pulse_configs[i].pulseID != 123) {
                nested = array.createNestedObject();
                nested["name"] = all_pulse_configs[i].name;
                nested["pulseStrength"] = all_pulse_configs[i].pulseStrength;
                nested["pulseTimeGap"] = all_pulse_configs[i].pulseTimeGap;
                nested["pulseFrequence"] = all_pulse_configs[i].pulseFrequence;
                nested["pulseDuration"] = all_pulse_configs[i].pulseDuration;
                nested["pulseTrouble"] = all_pulse_configs[i].pulseTrouble;
                }
            }

            File configFile = SPIFFS.open("/config_new.json", "w");
            if (serializeJsonPretty(doc, configFile) == 0) {
                Serial.println(F("Failed to write to file"));
            }

            configFile.close();
        }

        void initHardware(pulse_config pulse_config_array[], struct_message *message) {
            Serial.println("initHardware");
            pinMode(RELAY1, OUTPUT);
            pinMode(RELAY2, OUTPUT);
            pinMode(RELAY3, OUTPUT);
            pinMode(RELAY4, OUTPUT);

            // pinMode(GPIO1, OUTPUT);
            // pinMode(GPIO2, OUTPUT);

            digitalWrite(RELAY1, LOW);
            digitalWrite(RELAY2, LOW);
            digitalWrite(RELAY3, LOW);
            digitalWrite(RELAY4, LOW);

/*
            delay(1000);
            digitalWrite(RELAY1, HIGH);
            delay(1000);
            digitalWrite(RELAY2, HIGH);
            delay(1000);
            digitalWrite(RELAY3, HIGH);
            delay(1000);
            digitalWrite(RELAY4, HIGH);
            
            delay(2000);
            digitalWrite(RELAY1, LOW);
            delay(1000);
            digitalWrite(RELAY2, LOW);
            delay(1000);
            digitalWrite(RELAY3, LOW);
            delay(1000);
            digitalWrite(RELAY4, LOW);
*/
            // digitalWrite(GPIO1, LOW);
            // digitalWrite(GPIO2, LOW);

            SPIFFS.begin();
            File configFile;
            if (SPIFFS.exists("/config_new.json")) {
                configFile = SPIFFS.open("/config_new.json", "r");
            } else {
                Serial.println("no additional config found, load default");
                configFile = SPIFFS.open("/config.json", "r");
            }
            size_t filesize = configFile.size(); //the size of the file in bytes
            DynamicJsonDocument doc(filesize);
            DeserializationError error = deserializeJson(doc, configFile);
            if (error)
                Serial.println(F("Failed to read file, using default configuration"));

            message->cycleDuration = doc["Blut"]["Durchlauf"];
            message->cycleStart = doc["Blut"]["Start"];
            message->lungDuration = doc["Lunge"]["Durchlauf"];
            message->lungStart = doc["Lunge"]["Start"];
            message->thermometerDuration = doc["Temperatur"]["Durchlauf"];
            int pulseID = doc["pulseID"];

            JsonArray array = doc["Puls"].as<JsonArray>();

            for (int i=0; i<array.size(); i++) {
                JsonObject object = array[i].as<JsonObject>();
                pulse_config_array[i].pulseID = i;
                pulse_config_array[i].name = object["name"].as<String>();
                pulse_config_array[i].pulseFrequence = object["pulseFrequence"];
                pulse_config_array[i].pulseDuration = object["pulseDuration"];
                pulse_config_array[i].pulseTrouble = object["pulseTrouble"];
                pulse_config_array[i].pulseStrength = object["pulseStrength"];
                pulse_config_array[i].pulseTimeGap = object["pulseTimeGap"];
                Serial.println(pulse_config_array[i].name);
                if (pulseID == i) {
                    Serial.println("found");
                    message->pulseID = i;
                }
            }
            configFile.close();
        }
};

// void handleMessage(JSONVar json);
void handleMessage(JsonObject json);
#endif