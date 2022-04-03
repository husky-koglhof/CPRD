#ifndef Config_h
#define Config_h
#include <Arduino.h>

#define FIRMWARE_VERSION    "1.3.3"
#define SPIFFS_VERSION      "1.4.2"

#define _SCK    SCK     // 18   SPI BUS
#define _SS     SS      // 5    SPI BUS
#define _MOSI   MOSI    // 23   SPI BUS
#define _MISO   MISO    // 19   SPI BUS

#define LED   T2

#define GPIO1 TX   // TXD
#define GPIO2 RX   // RXD

#define RELAY1 A17 // IO27
#define RELAY2 A4  // IO32
#define RELAY3 A10 // IO4
#define RELAY4 17  // IO17

#define SERVO A5   // IO33

#define PWR   A0

#define DEFAULT_SSID            "CPRD"
#define DEFAULT_SSID_PASS       "CPRD_password"
#define DEFAULT_CHANNEL         1

#define DEFAULT_HTTP_USER   "admin"
#define DEFAULT_HTTP_PASS   "admin"

#define ROOT_USER   "admin"
#define ROOT_PASS   "tobedefined"

#endif
