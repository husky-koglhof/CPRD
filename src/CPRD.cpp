#include <CPRD.h>
#include <esp_wifi.h>
#include <config.h>

#include "ESPAsyncWebServer.h"
#include <ArduinoJson.h>

#include <SPIFFSEditor.h>
#include <AsyncElegantOTA.h>
#include <LITTLEFS.h>

#include <ESP32Servo.h>

CPRD cprd_local;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

struct_message incomingReadings;
struct_message configData;
struct_message nodeData;
pulse_config all_pulse_configs[20];

CPRD *cprdClients[10];
volatile int cnt = 0;

unsigned int readingId = 0;

// Global copy of slave
esp_now_peer_info_t slave;

String processor(const String& var) {
  Serial.print("VAR: ");Serial.println(var);
  String output = "";
  if (var == "BOARDID") {
    return String(cprd_local.getEEPROM().boardID);
  } else if (var == "VOLTAGE") {
    return String(cprd_local.getVoltage());
  } else if (var == "PERCENTAGE") {
    return String(cprd_local.getPercentage());
  } else if (var == "SSID") {
    eeprom_struct eepromObj = cprd_local.getEEPROM();
    if (eepromObj.enableSSID) {
      return String(eepromObj.ssid);
    } else {
      return String(DEFAULT_SSID);
    }
  } else if (var == "SSID_CHECKED") {
    eeprom_struct eepromObj = cprd_local.getEEPROM();
    return String(eepromObj.enableSSID);
  } else if (var == "SSID_PASSWORD") {
    return String(DEFAULT_SSID_PASS);
  } else if (var == "DEFAULT_USER") {
    return DEFAULT_HTTP_USER;
  } else if (var == "DEFAULT_PASSWORD") {
    return DEFAULT_HTTP_PASS;
  } else if (var == "FIRMWARE_VERSION") {
    return FIRMWARE_VERSION;
  } else if (var == "SPIFFS_VERSION") {
    return SPIFFS_VERSION;
  }

  return String();
}

void CPRD::sendData(uint8_t broadcastAddress[6]) {
  bool exists = esp_now_is_peer_exist(slave.peer_addr);
  if ( !exists) {
    slave.channel = DEFAULT_CHANNEL;
    slave.encrypt = 0;
    for (int ii = 0; ii < 6; ++ii ) {
      slave.peer_addr[ii] = (uint8_t) broadcastAddress[ii];
    }

    // Slave not paired, attempt pair
    esp_err_t addStatus = esp_now_add_peer(&slave);
    if (addStatus == ESP_OK) {
      // Pair success
    } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
      // How did we get so far!!
      Serial.println("ESPNOW Not Init");
    } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
      Serial.println("Invalid Argument");
    } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
      Serial.println("Peer list full");
    } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
      Serial.println("Out of memory");
    } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
      Serial.println("Peer Exists");
    } else {
      Serial.println("Not sure what happened");
    }
  }

  esp_err_t result;
  if (!isClient()) {
    Serial.print("SEND DATA TO ID: ");Serial.println(nodeData.id);
    result = esp_now_send(broadcastAddress, (uint8_t *) &nodeData, sizeof(nodeData));
  } else {
    result = esp_now_send(broadcastAddress, (uint8_t *) &configData, sizeof(configData));
  }

  if (result == ESP_OK) {
    // Serial.println("Success");
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW not Init.");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    Serial.println("Internal Error");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
  }
}

// Wrapper
void CPRD::sendDataToServer() {
  uint8_t *peer_addr = slave.peer_addr;

  configData.id = getEEPROM().boardID;
  // configData.voltage = getVoltage();
  configData.voltage = getPercentage();
  configData.readingId = readingId++;

  if (readingId == 1) { // Initial call after boot
    configData.pulseFrequence = all_pulse_configs[configData.pulseID].pulseFrequence;
    configData.pulseDuration = all_pulse_configs[configData.pulseID].pulseDuration;
    configData.pulseTrouble = all_pulse_configs[configData.pulseID].pulseTrouble;
    configData.pulseStrength = all_pulse_configs[configData.pulseID].pulseStrength;
    configData.pulseTimeGap = all_pulse_configs[configData.pulseID].pulseTimeGap;
  }
  sendData(peer_addr);
}

void OnServerDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) { 
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));

  if (incomingReadings.id != cprd_local.getEEPROM().boardID) {
    return;
  }

  StaticJsonDocument<256> doc;
  JsonObject dataForWS = doc.to<JsonObject>();

  dataForWS["action"] = incomingReadings.action;
  dataForWS["id"] = cprd_local.getEEPROM().boardID;

  JsonArray array = doc.createNestedArray("data");
  if (incomingReadings.action == PULSE_SAVE || incomingReadings.action == PULSE_START || incomingReadings.action == PULSE_STOP) {
    array[0] = incomingReadings.pulseID;
    array[1] = incomingReadings.pulseStrength;
    array[2] = incomingReadings.pulseTimeGap;
    array[3] = incomingReadings.pulseFrequence;
    array[4] = incomingReadings.pulseTrouble;
    array[5] = incomingReadings.pulseDuration;
  } else if (incomingReadings.action == LUNG_SAVE || incomingReadings.action == LUNG_START || incomingReadings.action == LUNG_STOP) {
    array[0] = incomingReadings.lungStart;
    array[1] = incomingReadings.lungDuration;
  } else if (incomingReadings.action == CYCLE_SAVE || incomingReadings.action == CYCLE_START || incomingReadings.action == CYCLE_STOP) {
    array[0] = incomingReadings.cycleStart;
    array[1] = incomingReadings.cycleDuration;
  } else if (incomingReadings.action == THERMOMETER_SAVE || incomingReadings.action == THERMOMETER_START || incomingReadings.action == THERMOMETER_STOP) {
    array[0] = incomingReadings.thermometerDuration;
  } else if (incomingReadings.action == SETTINGS_SAVE) {
    array[0] = "boardID";
    array[1] = incomingReadings.id;
    array[2] = incomingReadings.SSID;
    array[3] = incomingReadings.enableSSID;
  }
  handleMessage(dataForWS);
}

void OnClientDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));

  bool found = false;
  for(int i = 0; i < cnt; i++) {
    if (cprdClients[i]->macAddress == macStr) {
        found = true;
    }
  }

  if (!found && cnt < 10) {
    Serial.println("Add new transmitter");
    cprdClients[cnt] = new CPRD(macStr);
    cprdClients[cnt]->boardID = incomingReadings.id;
    cnt = cnt + 1;
    Serial.println("Transmitter added");
  }

  cprd_local.sendDataToWS(incomingReadings.id, incomingReadings);
}

CPRD::CPRD() {
    Serial.println("init class");
    EEPROM.begin(EEPROM_SIZE);
}

CPRD::~CPRD() {
    Serial.println("init x class");EEPROM.begin(EEPROM_SIZE);
}

void configDeviceAP() {
  String ssid = DEFAULT_SSID;
  eeprom_struct eepromObj = cprd_local.getEEPROM();
  if (eepromObj.enableSSID) {
    ssid = eepromObj.ssid;
  }
  bool result = WiFi.softAP(ssid.c_str(), DEFAULT_SSID_PASS, DEFAULT_CHANNEL, 0);
  if (!result) {
    Serial.println("AP Config failed.");
  } else {
    Serial.println("AP Config Success. Broadcasting with AP: " + String(ssid));
  }
}

void OnDataSend(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void sendDataToNode(int boardID) {
  Serial.print("sendDataToNode: ");Serial.println(boardID);
  for(int i = 0; i < cnt; i++) {
    if (cprdClients[i]->boardID == boardID) {
      Serial.print("Found node: ");Serial.print(cprdClients[i]->boardID);
      Serial.print(", ");Serial.println(cprdClients[i]->macAddress);
      char macStr[18];
      cprdClients[i]->macAddress.toCharArray(macStr, 19);
      uint8_t broadcastAddress[6];
      for (int ii = 0; ii < 6; ++ii ) {
        broadcastAddress[ii] = (uint8_t) macStr[ii];
      }

      cprd_local.sendData(broadcastAddress);
    }
  }
}

Servo servo;

TaskHandle_t pulsHandler = NULL;
TaskHandle_t cycleHandler = NULL;
TaskHandle_t lungHandler = NULL;
TaskHandle_t thermometerHandler = NULL;

void cancelPulsTask() {
  if (pulsHandler != NULL) {
    configData.pulseRunning = false;
    servo.write(SERVO_CENTER);
    vTaskDelete( pulsHandler );
    pulsHandler = NULL;
  }
}

void cancelCycleTask() {
  if (cycleHandler != NULL) {
    configData.cycleRunning = false;
    digitalWrite(RELAY1, LOW);
    vTaskDelete( cycleHandler );
    cycleHandler = NULL;
  }
}

void cancelLungTask() {
  if (lungHandler != NULL) {
    configData.lungRunning = false;
    digitalWrite(RELAY2, LOW);
    vTaskDelete( lungHandler );
    lungHandler = NULL;
  }
}

void cancelTermometerTask() {
  if (thermometerHandler != NULL) {
    configData.thermometerRunning = false;
    digitalWrite(RELAY3, LOW);
    vTaskDelete( thermometerHandler );
    thermometerHandler = NULL;
  }
}

extern "C" void ServerPulseTroubleTask(void *pvParameters) {
  int first = (60000 / configData.pulseFrequence);
  int second = ((float)first * ((float)configData.pulseTimeGap / 10));
  int begin = millis() + (configData.pulseDuration*1000*60); // mins
  configData.pulseRunning = true;

  int rnd = random(1, configData.pulseTrouble);
  int cnt = 0;
  for (;;) {
    if (cnt == rnd) {
      servo.write(configData.pulseStrength);
      vTaskDelay(first/portTICK_PERIOD_MS);
      servo.write(configData.pulseStrength/2);
      vTaskDelay(second/portTICK_PERIOD_MS);
      vTaskDelay(rnd*50/portTICK_PERIOD_MS);
      rnd = random(0, 5);
      cnt = 0;
    } else {
      servo.write(configData.pulseStrength);
      vTaskDelay(first/portTICK_PERIOD_MS);
      servo.write(configData.pulseStrength/2);
      vTaskDelay(second/portTICK_PERIOD_MS);
      cnt++;
    }
    if (begin <= millis()) {
      cancelPulsTask();
    }
  }
}

extern "C" void ServerPulseTask(void *pvParameters) {
  int first = (60000 / configData.pulseFrequence);
  int second = ((float)first * ((float)configData.pulseTimeGap / 10));
  int begin = millis() + (configData.pulseDuration*1000*60); // mins
  configData.pulseRunning = true;

  for (;;) {
    servo.write(configData.pulseStrength);
    vTaskDelay(first/portTICK_PERIOD_MS);
    servo.write(configData.pulseStrength/2);
    vTaskDelay(second/portTICK_PERIOD_MS);
    if (begin <= millis()) {
      cancelPulsTask();
    }
  }
}

extern "C" void ServerLukeTask(void *pvParameters) {
  int beats[] = {3,3,2,4,3,3,3,3,3,3,4};
  int cnt = 0;
  bool toggle = true;
  configData.pulseRunning = true;

  for (;;) {
    if (toggle) {
      servo.write(20);
      toggle = false;
    } else {
      servo.write(0);
      toggle = true;
    }
    vTaskDelay(beats[cnt]*200/portTICK_PERIOD_MS);
    if (cnt == (sizeof(beats) / sizeof(beats[0]))) {
      cancelPulsTask();
    }
    cnt = cnt+1;
  }
}

extern "C" void ServerXMasTask(void *pvParameters) {
  int beats[] = {2,2,4, 2,2,4, 2,2,2,1,4, 2,2,3, 1,2,2,3, 1,2,2,2,2,4};
  int cnt = 0;
  bool toggle = true;
  configData.pulseRunning = true;

  for (;;) {
    if (toggle) {
      servo.write(20);
      toggle = false;
    } else {
      servo.write(0);
      toggle = true;
    }
    vTaskDelay(beats[cnt]*200/portTICK_PERIOD_MS);
    if (cnt == (sizeof(beats) / sizeof(beats[0]))) {
      cancelPulsTask();
    }
    cnt = cnt+1;
  }
}

extern "C" void ServerCycleTask(void *pvParameters) {
  int start = configData.cycleStart * 1000; // 6
  int begin = millis() + (configData.cycleDuration*1000); // 60

  configData.cycleRunning = true;

  for (;;) {
    digitalWrite(RELAY1, HIGH);
    vTaskDelay(start/portTICK_PERIOD_MS);
    digitalWrite(RELAY1, LOW);
    vTaskDelay(start/portTICK_PERIOD_MS);
    if (begin <= millis()) {
      cancelCycleTask();
    }
  }
}

extern "C" void ServerLungTask(void *pvParameters) {
  int start = configData.lungStart * 1000; // 6
  int begin = millis() + (configData.lungDuration*1000); // 60

  configData.lungRunning = true;

  for (;;) {
    digitalWrite(RELAY2, HIGH);
    vTaskDelay(start/portTICK_PERIOD_MS);
    digitalWrite(RELAY2, LOW);
    vTaskDelay(start/portTICK_PERIOD_MS);
    if (begin <= millis()) {
      cancelLungTask();
    }
  }
}

extern "C" void ServerThermometerTask(void *pvParameters) {
  int begin = millis() + (configData.thermometerDuration*1000); // 60

  digitalWrite(RELAY3, HIGH);
  configData.thermometerRunning = true;

  for (;;) {
    if (begin <= millis()) {
      digitalWrite(RELAY3, LOW);
      cancelTermometerTask();
    }
    vTaskDelay(1/portTICK_PERIOD_MS);
  }
}

extern "C" void TaskBlink(void *pvParameters) {
  int counter = 0;
  configData.pingRunning = true;
  for (;;) {
    digitalWrite(LED, HIGH);
    vTaskDelay(100/portTICK_PERIOD_MS);
    digitalWrite(LED, LOW);
    vTaskDelay(100/portTICK_PERIOD_MS);
    if (counter == 58) {
      configData.pingRunning = false;
      if (cprd_local.isClient()) {
        digitalWrite(LED, LOW);
      } else {
        digitalWrite(LED, HIGH);
      }
      vTaskDelete( NULL );
    }
    counter++;
  }
}

void handleMessage(JsonObject json) {
  int _id = json["id"].as<int>();
  int _action = json["action"].as<int>();
  JsonArray array = json["data"];

  int _len = array.size();
  for (int i = 0; i < _len; i++) {
    int object = array[i];
    Serial.print("Parameter ");Serial.print(i);Serial.print(" = ");Serial.println(object);
  }

  Serial.print("GOT BOARDID = ");Serial.println(_id);
  Serial.print("OWN BOARDID = ");Serial.println(cprd_local.getEEPROM().boardID);
  bool local = false;
  if (_id == cprd_local.getEEPROM().boardID || _id == 0 || _id == -1 || _id == 255) {
    Serial.println("local Board selected");
    local = true;
  } else {
    Serial.println("remote Board selected");
    // _id = array[1];
    nodeData.id = _id;
  }

  Serial.print("action: ");Serial.println(_action);

  nodeData.action = _action;

  if (_action == PULSE_SAVE || _action == PULSE_START || _action == PULSE_STOP || _action == PULSE_ACTIVATE) {
      /*
      0: 4  = pulseID
      1: 30 = pulseStrength
      2: 1  = pulseTimeGap
      3: 60 = pulseFrequence
      4: 0  = pulseTrouble
      5: 10 = pulseDuration
      */
    if (local == false) {
      nodeData.pulseID = array[0];
      nodeData.pulseStrength = array[1];
      nodeData.pulseTimeGap = array[2];
      nodeData.pulseFrequence = array[3];
      nodeData.pulseTrouble = array[4];
      nodeData.pulseDuration = array[5];
    } else { 
      Serial.println("pulse_action");
      configData.pulseID = array[0];
      configData.pulseStrength = array[1];
      configData.pulseTimeGap = array[2];
      configData.pulseFrequence = array[3];
      configData.pulseTrouble = array[4];
      configData.pulseDuration = array[5];

      all_pulse_configs[configData.pulseID].pulseStrength = configData.pulseStrength;
      all_pulse_configs[configData.pulseID].pulseTimeGap = configData.pulseTimeGap;
      all_pulse_configs[configData.pulseID].pulseFrequence = configData.pulseFrequence;
      all_pulse_configs[configData.pulseID].pulseDuration = configData.pulseDuration;
      all_pulse_configs[configData.pulseID].pulseTrouble = configData.pulseTrouble;
      
      if (_action == PULSE_START || _action == PULSE_STOP) {
        cancelPulsTask();
      }
      if (_action == PULSE_START) {
        Serial.println("Create new Task");
        
        if (configData.pulseTrouble > 0) { // Störung
          xTaskCreate(ServerPulseTroubleTask, "PulseTroubleTask", 2048, NULL, 2, &pulsHandler);
        } else if (configData.pulseID == 4712) { // luke
          xTaskCreate(ServerLukeTask, "LukeTask", 2048, NULL, 2, &pulsHandler);
        } else if (configData.pulseID == 4711) { // xmas
          xTaskCreate(ServerXMasTask, "XMasTask", 2048, NULL, 2, &pulsHandler);
        } else {
          xTaskCreate(ServerPulseTask, "PulseTask", 4096, NULL, 4, &pulsHandler);
        }
      }
    }
  } else if (_action == CYCLE_SAVE || _action == CYCLE_START || _action == CYCLE_STOP) {
    if (local == false) {
      nodeData.cycleStart = array[0];
      nodeData.cycleDuration = array[1];
    } else {
      Serial.println("cycle_action");
      configData.cycleStart = array[0];
      configData.cycleDuration = array[1];
      if (_action == CYCLE_START || _action == CYCLE_STOP) {
        cancelCycleTask();
      }
      if (_action == CYCLE_START) {
        Serial.println("Create new Task");
        xTaskCreate(ServerCycleTask, "CycleTask", 1024, NULL, 2, &cycleHandler);
      }
    }
  } else if (_action == LUNG_SAVE || _action == LUNG_START || _action == LUNG_STOP) {
    if (local == false) {
      nodeData.lungStart = array[0];
      nodeData.lungDuration = array[1];
    } else {
      Serial.println("lung_action");
      configData.lungStart = array[0];
      configData.lungDuration = array[1];
      if (_action == LUNG_START || _action == LUNG_STOP) {
        cancelLungTask();
      }
      if (_action == LUNG_START) {
        Serial.println("Create new Task");
        xTaskCreate(ServerLungTask, "LungTask", 2048, NULL, 2, &lungHandler);
      }
    }
  } else if (_action == THERMOMETER_SAVE || _action == THERMOMETER_START || _action == THERMOMETER_STOP) {
    if (local == false) {
      nodeData.thermometerDuration = array[0];
    } else {
      Serial.println("thermometer_action");
      configData.thermometerDuration = array[0];
      if (_action == THERMOMETER_START || _action == THERMOMETER_STOP) {
        cancelTermometerTask();
      }
      if (_action == THERMOMETER_START) {
        Serial.println("Create new Task");
        xTaskCreatePinnedToCore(ServerThermometerTask, "ThermometerTask", 2048, NULL, 2, &thermometerHandler, 1);
      }
    }
  } else if (_action == SETTINGS_SAVE) {
    if (local == false) {
      nodeData.id = array[1];
      nodeData.enableSSID = array[2];
      nodeData.SSID = array[3].as<String>();
    } else {
      int id = array[1];
      eeprom_struct eepromObj;
      eepromObj.boardID = id;
      eepromObj.enableSSID =  array[2];
      eepromObj.ssid = array[3].as<String>();
      cprd_local.setEEPROM(eepromObj);

      ESP.restart();
      return;
    }
  } else if (_action == PING) {
    if (local == true) {
      xTaskCreatePinnedToCore(TaskBlink, "TaskBlink", 1024, NULL, 2, NULL, 0);
      return;
    }
  } else if (_action == REBOOT) {
    if (local == true) {
      ESP.restart();
      return;
    }
  } else if (_action == RESET) {
    if (local == true) {
      int localBoardID = cprd_local.getEEPROM().boardID;
      cprd_local.removeConfig();
      for (int i = 0; i < 512; i++) {
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
      delay(250);
      eeprom_struct eepromObj;
      eepromObj.boardID = localBoardID;
      eepromObj.enableSSID = false;
      eepromObj.ssid = "";
      cprd_local.setEEPROM(eepromObj);
      delay(250);
      ESP.restart();
      return;
    }
  } else if (_action == RELAY_ON) {
    if (local == true) {
      int RELAY_PIN;
      int relay = array[3];
      if (relay == 1) RELAY_PIN = RELAY1;
      if (relay == 2) RELAY_PIN = RELAY2;
      if (relay == 3) RELAY_PIN = RELAY3;
      if (relay == 4) RELAY_PIN = RELAY4;

      digitalWrite(RELAY_PIN, HIGH);
    }
    return;
  } else if (_action == RELAY_OFF) {
    if (local == true) {
      int RELAY_PIN;
      int relay = array[3];
      if (relay == 1) RELAY_PIN = RELAY1;
      if (relay == 2) RELAY_PIN = RELAY2;
      if (relay == 3) RELAY_PIN = RELAY3;
      if (relay == 4) RELAY_PIN = RELAY4;

      digitalWrite(RELAY_PIN, LOW);
    }
    return;
  } else if (_action == SERVO_PULSE) {
    if (local == true) {
      int pulseStrength = array[3];
      int pulse = map(pulseStrength, 0, 100, 0, 180);
      servo.write(pulse);
    }
    return;
  } else if (_action == CANCEL) {
    if (local == true) {
      cancelCycleTask();
      cancelLungTask();
      cancelTermometerTask();
      cancelPulsTask();

      digitalWrite(RELAY1, LOW);
      delay(100);
      digitalWrite(RELAY2, LOW);
      delay(100);
      digitalWrite(RELAY3, LOW);
      delay(100);
      digitalWrite(RELAY4, LOW);
      delay(100);
      servo.write(SERVO_CENTER);
    }
    return;
  }
  
  if (local == false && !cprd_local.isClient()) {
    sendDataToNode(_id);
  }
  if (_action == PULSE_SAVE || _action == CYCLE_SAVE || _action == LUNG_SAVE || _action == THERMOMETER_SAVE) {
    if (local == true) {
      cprd_local.writeDataBack(&configData, all_pulse_configs);
    }
  }

  configData.action = SETTINGS_SAVE;
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    Serial.print("handleWebSocketMessage: ");Serial.println((char*)data);
    StaticJsonDocument<512> doc;
    deserializeJson(doc, (char*)data);
    JsonObject obj = doc.as<JsonObject>();
    handleMessage(obj);
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void CPRD::sendDataToWS(byte ID, struct_message data) {
  DynamicJsonDocument doc(1512); // fixed size
  JsonObject dataForWS = doc.to<JsonObject>();

  int pulseID = data.pulseID;

  dataForWS["pi"] = data.pingRunning;
  dataForWS["pR"] = data.pulseRunning;
  dataForWS["cR"] = data.cycleRunning;
  dataForWS["lR"] = data.lungRunning;
  dataForWS["tR"] = data.thermometerRunning;

  dataForWS["id"] = ID;
  dataForWS["m"] = data.mode; // master
  dataForWS["B"]["s"] = data.cycleStart; // Blut, Start
  dataForWS["B"]["d"] = data.cycleDuration; // Blut, Durchlauf
  dataForWS["L"]["s"] = data.lungStart; // Lunge, Start
  dataForWS["L"]["d"] = data.lungDuration; // Lunge, Durchlauf
  dataForWS["T"]["d"] = data.thermometerDuration; // Temperatur, Durchlauf
  dataForWS["pID"] = data.pulseID; // pulseID

  dataForWS["r"] = data.rssi; // rssi
  dataForWS["v"] = data.voltage; // voltage
  dataForWS["rID"] = data.readingId; // readingID

  JsonArray array = doc.createNestedArray("P"); // Puls
  JsonObject nested;

  for (int i=0;i<20;i++) {
    if (all_pulse_configs[i].pulseID != 123) {
      nested = array.createNestedObject();
      nested["n"] = all_pulse_configs[i].name; // name
      nested["s"] = all_pulse_configs[i].pulseStrength; // pulseStrength
      nested["t"] = all_pulse_configs[i].pulseTimeGap; // pulseTimeGap
      nested["f"] = all_pulse_configs[i].pulseFrequence; // pulseFrequence
      nested["d"] = all_pulse_configs[i].pulseDuration; // pulseDuration
      nested["r"] = all_pulse_configs[i].pulseTrouble; // pulseTrouble
    }
  }

  if (ID != getEEPROM().boardID) {
    dataForWS["P"][pulseID]["s"] = data.pulseStrength; // Puls, pulseStrength
    dataForWS["P"][pulseID]["f"] = data.pulseFrequence; // Puls, pulseFrequence
    dataForWS["P"][pulseID]["t"] = data.pulseTimeGap; // Puls, pulseTimeGap
    dataForWS["P"][pulseID]["d"] = data.pulseDuration; // Puls, pulseDuration
    dataForWS["P"][pulseID]["r"] = data.pulseTrouble; // Puls, pulseTrouble
  }

  char   buffer[1512]; // create temp buffer
  size_t len = serializeJson(dataForWS, buffer);  // serialize to buffer
  ws.textAll(buffer, len); // send buffer to web socket
  yield();
}

fs::LITTLEFSFS _roFS;

void CPRD::initServer() {
  WiFi.mode(WIFI_AP_STA);

  configDeviceAP();

  Serial.print("Station IP Address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  // This is the mac address of the Slave in AP Mode
  Serial.print("AP MAC: "); Serial.println(WiFi.softAPmacAddress());

  /*
  for (int i = 0; i<3; i++) {
    Serial.print(all_pulse_configs[i].pulseID);Serial.print(",");
    Serial.print(all_pulse_configs[i].pulseFrequence);Serial.print(",");
    Serial.print(all_pulse_configs[i].pulseDuration);Serial.print(",");
    Serial.print(all_pulse_configs[i].pulseTrouble);Serial.print(",");
    Serial.print(all_pulse_configs[i].pulseStrength);Serial.print(",");
    Serial.print(all_pulse_configs[i].pulseTimeGap);Serial.println("");
  }
  */
  // Configure itself
  cprdClients[0] = new CPRD(WiFi.softAPmacAddress());

  // Init ESPNow with a fallback logic
  InitESPNow();

  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnClientDataRecv);
  esp_now_register_send_cb(OnDataSend);
  
  _roFS = fs::LITTLEFSFS();
  _roFS.begin(true, "/roFS", 10, "spiffs1");

  server.addHandler(new SPIFFSEditor(_roFS, DEFAULT_HTTP_USER, DEFAULT_HTTP_PASS));

  server.on("/reseteeprom", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("New Board, initialize...");
    eeprom_struct eepromObj;
    eepromObj.boardID = 255;
    eepromObj.enableSSID =  false;
    eepromObj.ssid = "CPRD";
    cprd_local.setEEPROM(eepromObj);

    request->send(_roFS, "/", String(), false);
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(!request->authenticate(DEFAULT_HTTP_USER, DEFAULT_HTTP_PASS))
        return request->requestAuthentication();
    request->send(_roFS, "/settings.htm", String(), false , processor);
  });

  server.on("/sidebars.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(_roFS, "/sidebars.js", String(), false , processor);
  });

  server.on("/node", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(_roFS, "/iframe.htm", String(), false , processor);
  });

  server.on("/modals.htm", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(_roFS, "/modals.htm", String(), false , processor);
  });

  server.on("/home", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(_roFS, "/home.htm", String(), false);
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(_roFS, "/index.htm", String(), false);
  });

  server.serveStatic("/", _roFS, "/").setDefaultFile("index.htm").setCacheControl("max-age=2592000"); // 30d

  server.onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("NOT_FOUND: ");
    if(request->method() == HTTP_GET)
      Serial.printf("GET");
    else if(request->method() == HTTP_POST)
      Serial.printf("POST");
    else if(request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if(request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if(request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if(request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if(request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    request->send(404);
  });

  server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
    Serial.printf("%s", (const char*)data);
    if(final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });

  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      Serial.printf("BodyStart: %u\n", total);
    Serial.printf("%s", (const char*)data);
    if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });
  
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  AsyncElegantOTA.begin(&server, ROOT_USER, ROOT_PASS, "spiffs1");    // Start ElegantOTA
  server.begin();

  initialized = true;
}
void CPRD::initClient() {
  WiFi.mode(WIFI_STA);
  Serial.print("STA MAC: "); Serial.println(WiFi.macAddress());

  byte boardID = getEEPROM().boardID;
  Serial.print("Board ID: "); Serial.println(boardID);
  if (boardID == 0) {
    Serial.println("Please start these node as master and set correct BOARD_ID");
    initialized = false;
    return;
  }

  // Init ESPNow with a fallback logic
  InitESPNow();
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSend);
  // We need these if we get data back
  esp_now_register_recv_cb(OnServerDataRecv);
  initialized = true;
}

void CPRD::initialize() {
  initHardware(all_pulse_configs, &configData);

  ESP32PWM::allocateTimer(0);
  servo.setPeriodHertz(50);
  servo.attach(SERVO, 1000, 2000);

  if (clientMode) {
      initClient();
  } else {
      initServer();
  }
}

#define PRINTSCANRESULTS 0
#define DELETEBEFOREPAIR 0

void CPRD::ScanForSlave() {
  int8_t scanResults = WiFi.scanNetworks(false, false, false, 50, DEFAULT_CHANNEL);
  // reset on each scan
  bool slaveFound = 0;
  memset(&slave, 0, sizeof(slave));

  if (scanResults == 0) {
    Serial.println("No WiFi devices in AP Mode found");
  } else {
    for (int i = 0; i < scanResults; ++i) {
      // Print SSID and RSSI for each device found
      String SSID = WiFi.SSID(i);
      int32_t RSSI = WiFi.RSSI(i);
      String BSSIDstr = WiFi.BSSIDstr(i);

      if (PRINTSCANRESULTS) {
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(SSID);
        Serial.print(" (");
        Serial.print(RSSI);
        Serial.print(")");
        Serial.println("");
      }

      // Check if the current device starts with `Slave`
      String ssid = DEFAULT_SSID;
      eeprom_struct eepromObj = getEEPROM();
      if (eepromObj.enableSSID) {
        ssid = eepromObj.ssid;
      }
      if (SSID.indexOf(ssid) == 0) {
        int mac[6];
        if ( 6 == sscanf(BSSIDstr.c_str(), "%x:%x:%x:%x:%x:%x",  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5] ) ) {
          for (int ii = 0; ii < 6; ++ii ) {
            slave.peer_addr[ii] = (uint8_t) mac[ii];
          }
        }

        slave.channel = DEFAULT_CHANNEL; // pick a channel
        slave.encrypt = 0; // no encryption
        configData.rssi = RSSI;
        slaveFound = 1;
        break;
      }
    }
  }

  if (!slaveFound) {
    Serial.println("Slave Not Found, trying again.");
  }

  // clean up ram
  WiFi.scanDelete();
}

void deletePeer() {
  esp_err_t delStatus = esp_now_del_peer(slave.peer_addr);
  Serial.print("Slave Delete Status: ");
  if (delStatus == ESP_OK) {
    // Delete success
    Serial.println("Success");
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW Not Init");
  } else if (delStatus == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
  }
}

bool manageSlave() {
  if (slave.channel == DEFAULT_CHANNEL) {
    if (DELETEBEFOREPAIR) {
      deletePeer();
    }

    // check if the peer exists
    bool exists = esp_now_is_peer_exist(slave.peer_addr);
    if ( exists ) {
      // Slave already paired.
      return true;
    } else {
      // Slave not paired, attempt pair
      esp_err_t addStatus = esp_now_add_peer(&slave);
      if (addStatus == ESP_OK) {
        // Pair success
        return true;
      } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
        // How did we get so far!!
        Serial.println("ESPNOW Not Init");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
        Serial.println("Invalid Argument");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
        Serial.println("Peer list full");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
        Serial.println("Out of memory");
        return false;
      } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
        Serial.println("Peer Exists");
        return true;
      } else {
        Serial.println("Not sure what happened");
        return false;
      }
    }
  } else {
    // No slave found to process
    Serial.println("No Slave found to process");
    return false;
  }
}

void CPRD::loop() {
  if (clientMode) {
    if (!initialized) return;

    // In the loop we scan for slave
    ScanForSlave();

    // If Slave is found, it would be populate in `slave` variable
    // We will check if `slave` is defined and then we proceed further
    if (slave.channel == DEFAULT_CHANNEL) { // check if slave channel is defined
      // `slave` is defined
      // Add slave as peer if it has not been added already
      bool isPaired = manageSlave();
      if (isPaired) {
        // pair success or already paired
        // Send data to device
        sendDataToServer();
      } else {
        // slave pair failed
        Serial.println("Slave pair failed!");
      }
    } else {
        // No slave found to process
    }
  } else {
    configData.rssi = WiFi.RSSI();
    configData.voltage = getPercentage();
    // configData.voltage = getVoltage();
    configData.mode = true;
    sendDataToWS(getEEPROM().boardID, configData);
  }
}
