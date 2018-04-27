/*
  sonoff.ino - Sonoff-Tasmota firmware for iTead Sonoff, Wemos and NodeMCU hardware

  Copyright (C) 2017  Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*====================================================
  Prerequisites:
    - Change libraries/PubSubClient/src/PubSubClient.h
        #define MQTT_MAX_PACKET_SIZE 512

    - Select IDE Tools - Flash Mode: "DOUT"
    - Select IDE Tools - Flash Size: "1M (no SPIFFS)"
  ====================================================*/

#define VERSION                0x05080000  // 5.8.0

enum log_t   {LOG_LEVEL_NONE, LOG_LEVEL_ERROR, LOG_LEVEL_INFO, LOG_LEVEL_DEBUG, LOG_LEVEL_DEBUG_MORE, LOG_LEVEL_ALL};
enum week_t  {Last, First, Second, Third, Fourth};
enum dow_t   {Sun=1, Mon, Tue, Wed, Thu, Fri, Sat};
enum month_t {Jan=1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec};
enum wifi_t  {WIFI_RESTART, WIFI_SMARTCONFIG, WIFI_MANAGER, WIFI_WPSCONFIG, WIFI_RETRY, WIFI_WAIT, MAX_WIFI_OPTION};
enum swtch_t {TOGGLE, FOLLOW, FOLLOW_INV, PUSHBUTTON, PUSHBUTTON_INV, PUSHBUTTONHOLD, PUSHBUTTONHOLD_INV, MAX_SWITCH_OPTION};
enum led_t   {LED_OFF, LED_POWER, LED_MQTTSUB, LED_POWER_MQTTSUB, LED_MQTTPUB, LED_POWER_MQTTPUB, LED_MQTT, LED_POWER_MQTT, MAX_LED_OPTION};
enum emul_t  {EMUL_NONE, EMUL_WEMO, EMUL_HUE, EMUL_MAX};

#include "user_config.h"
#include "user_config_override.h"
#include "i18n.h"
#include "sonoff_template.h"

/*********************************************************************************************\
 * No user configurable items below
\*********************************************************************************************/

#define MODULE                 SONOFF_BASIC // [Module] Select default model

#define USE_DHT                             // Default DHT11 sensor needs no external library
#ifndef USE_DS18x20
#define USE_DS18B20                         // Default DS18B20 sensor needs no external library
#endif
//#define DEBUG_THEO                          // Add debug code

#ifdef BE_MINIMAL
#ifdef USE_MQTT_TLS
#undef USE_MQTT_TLS                         // Disable TLS support won't work as the MQTTHost is not set
#endif
#ifdef USE_DISCOVERY
#undef USE_DISCOVERY                        // Disable Discovery services for both MQTT and web server
#endif
#ifdef USE_DOMOTICZ
#undef USE_DOMOTICZ                         // Disable Domoticz
#endif
//#ifdef USE_WEBSERVER
//#undef USE_WEBSERVER                        // Disable Webserver
//#endif
#ifdef USE_EMULATION
#undef USE_EMULATION                        // Disable Wemo or Hue emulation
#endif
#ifdef USE_DS18x20
#undef USE_DS18x20                          // Disable DS18x20 sensor
#endif
#ifdef USE_I2C
#undef USE_I2C                              // Disable all I2C sensors
#endif
#ifdef USE_WS2812
#undef USE_WS2812                           // Disable WS2812 Led string
#endif
#ifdef USE_DS18B20
#undef USE_DS18B20                          // Disable internal DS18B20 sensor
#endif
#ifdef USE_DHT
#undef USE_DHT                              // Disable internal DHT sensor
#endif
#ifdef USE_IR_REMOTE
#undef USE_IR_REMOTE                        // Disable IR driver
#endif
#ifdef DEBUG_THEO
#undef DEBUG_THEO                           // Disable debug code
#endif
#endif  // BE_MINIMAL

#ifndef SWITCH_MODE
#define SWITCH_MODE            TOGGLE       // TOGGLE, FOLLOW or FOLLOW_INV (the wall switch state)
#endif

#ifndef MQTT_FINGERPRINT
#define MQTT_FINGERPRINT       "A5 02 FF 13 99 9F 8B 39 8E F1 83 4F 11 23 65 0B 32 36 FC 07"
#endif

#ifndef WS2812_LEDS
#define WS2812_LEDS            30           // [Pixels] Number of LEDs
#endif

#define MQTT_TOKEN_PREFIX      "%prefix%"   // To be substituted by mqtt_prefix[x]
#define MQTT_TOKEN_TOPIC       "%topic%"    // To be substituted by mqtt_topic, mqtt_grptopic, mqtt_buttontopic, mqtt_switchtopic

#define WIFI_HOSTNAME          "%s-%04d"    // Expands to <MQTT_TOPIC>-<last 4 decimal chars of MAC address>
#define CONFIG_FILE_SIGN       0xA5         // Configuration file signature
#define CONFIG_FILE_XOR        0x5A         // Configuration file xor (0 = No Xor)

#define HLW_PREF_PULSE         12530        // was 4975us = 201Hz = 1000W
#define HLW_UREF_PULSE         1950         // was 1666us = 600Hz = 220V
#define HLW_IREF_PULSE         3500         // was 1666us = 600Hz = 4.545A

#define MQTT_RETRY_SECS        10           // Minimum seconds to retry MQTT connection
#define APP_POWER              0            // Default saved power state Off
#define MAX_COUNTERS           4            // Max number of counter sensors
#define MAX_PULSETIMERS        4            // Max number of supported pulse timers
#define WS2812_MAX_LEDS        512          // Max number of LEDs

#define PWM_RANGE              1023         // 255..1023 needs to be devisible by 256
//#define PWM_FREQ               1000         // 100..1000 Hz led refresh
//#define PWM_FREQ               910          // 100..1000 Hz led refresh (iTead value)
#define PWM_FREQ               880          // 100..1000 Hz led refresh (BN-SZ01 value)

#define MAX_POWER_HOLD         10           // Time in SECONDS to allow max agreed power (Pow)
#define MAX_POWER_WINDOW       30           // Time in SECONDS to disable allow max agreed power (Pow)
#define SAFE_POWER_HOLD        10           // Time in SECONDS to allow max unit safe power (Pow)
#define SAFE_POWER_WINDOW      30           // Time in MINUTES to disable allow max unit safe power (Pow)
#define MAX_POWER_RETRY        5            // Retry count allowing agreed power limit overflow (Pow)

#define STATES                 20           // State loops per second
#define SYSLOG_TIMER           600          // Seconds to restore syslog_level
#define SERIALLOG_TIMER        600          // Seconds to disable SerialLog
#define OTA_ATTEMPTS           10           // Number of times to try fetching the new firmware

#define INPUT_BUFFER_SIZE      250          // Max number of characters in (serial) command buffer
#define CMDSZ                  20           // Max number of characters in command
#define TOPSZ                  100          // Max number of characters in topic string
#ifdef USE_MQTT_TLS
  #define MAX_LOG_LINES        10           // Max number of lines in weblog
#else
  #define MAX_LOG_LINES        20           // Max number of lines in weblog
#endif
#define MAX_BACKLOG            16           // Max number of commands in backlog (chk blogidx and blogptr code)
#define MIN_BACKLOG_DELAY      2            // Minimal backlog delay in 0.1 seconds

#define APP_BAUDRATE           115200       // Default serial baudrate
#define MAX_STATUS             11           // Max number of status lines

enum butt_t  {PRESSED, NOT_PRESSED};
enum opt_t   {P_HOLD_TIME, P_MAX_POWER_RETRY, P_MAX_PARAM8};   // Index in sysCfg.param

#include "support.h"                        // Global support

#include <PubSubClient.h>                   // MQTT
#ifndef MESSZ
  #define MESSZ                405          // Max number of characters in JSON message string (4 x DS18x20 sensors)
#endif

// Max message size calculated by PubSubClient is (MQTT_MAX_PACKET_SIZE < 5 + 2 + strlen(topic) + plength)
#if (MQTT_MAX_PACKET_SIZE -TOPSZ -7) < MESSZ  // If the max message size is too small, throw an error at compile time
                                            // See pubsubclient.c line 359
  #error "MQTT_MAX_PACKET_SIZE is too small in libraries/PubSubClient/src/PubSubClient.h, increase it to at least 512"
#endif

#include <Ticker.h>                         // RTC, HLW8012, OSWatch
#include <ESP8266WiFi.h>                    // MQTT, Ota, WifiManager
#include <ESP8266HTTPClient.h>              // MQTT, Ota
#include <ESP8266httpUpdate.h>              // Ota
#include <StreamString.h>                   // Webserver, Updater
#include <ArduinoJson.h>                    // WemoHue, IRremote, Domoticz
#ifdef USE_WEBSERVER
  #include <ESP8266WebServer.h>             // WifiManager, Webserver
  #include <DNSServer.h>                    // WifiManager
#endif  // USE_WEBSERVER
#ifdef USE_DISCOVERY
  #include <ESP8266mDNS.h>                  // MQTT, Webserver
#endif  // USE_DISCOVERY
#ifdef USE_I2C
  #include <Wire.h>                         // I2C support library
#endif  // USE_I2C
#ifdef USE_SPI
  #include <SPI.h>                          // SPI support, TFT
#endif  // USE_SPI
#include "settings.h"


struct TIME_T {
  uint8_t       Second;
  uint8_t       Minute;
  uint8_t       Hour;
  uint8_t       Wday;      // day of week, sunday is day 1
  uint8_t       Day;
  uint8_t       Month;
  char          MonthName[4];
  uint16_t      DayOfYear;
  uint16_t      Year;
  unsigned long Valid;
} rtcTime;

struct TimeChangeRule
{
  uint8_t       week;      // 1=First, 2=Second, 3=Third, 4=Fourth, or 0=Last week of the month
  uint8_t       dow;       // day of week, 1=Sun, 2=Mon, ... 7=Sat
  uint8_t       month;     // 1=Jan, 2=Feb, ... 12=Dec
  uint8_t       hour;      // 0-23
  int           offset;    // offset from UTC in minutes
};

TimeChangeRule myDST = { TIME_DST };  // Daylight Saving Time
TimeChangeRule mySTD = { TIME_STD };  // Standard Time

int Baudrate = APP_BAUDRATE;          // Serial interface baud rate
byte SerialInByte;                    // Received byte
int SerialInByteCounter = 0;          // Index in receive buffer
byte Hexcode = 0;                     // Sonoff dual input flag
uint16_t ButtonCode = 0;              // Sonoff dual received code
int16_t savedatacounter;              // Counter and flag for config save to Flash
uint8_t mqttcounter = 0;              // MQTT connection retry counter
uint8_t fallbacktopic = 0;            // Use Topic or FallbackTopic
unsigned long timerxs = 0;            // State loop timer
int state = 0;                        // State per second flag
int mqttflag = 2;                     // MQTT connection messages flag
int otaflag = 0;                      // OTA state flag
int otaok = 0;                        // OTA result
byte otaretry = OTA_ATTEMPTS;         // OTA retry counter
int restartflag = 0;                  // Sonoff restart flag
int wificheckflag = WIFI_RESTART;     // Wifi state flag
int uptime = 0;                       // Current uptime in hours
boolean uptime_flg = true;            // Signal latest uptime
int tele_period = 0;                  // Tele period timer
byte logidx = 0;                      // Index in Web log buffer
byte logajaxflg = 0;                  // Reset web console log
byte Maxdevice = 0;                   // Max number of devices supported
int status_update_timer = 0;          // Refresh initial status
uint16_t pulse_timer[MAX_PULSETIMERS] = { 0 }; // Power off timer
uint16_t blink_timer = 0;             // Power cycle timer
uint16_t blink_counter = 0;           // Number of blink cycles
uint8_t blink_power;                  // Blink power state
uint8_t blink_mask = 0;               // Blink relay active mask
uint8_t blink_powersave;              // Blink start power save state
uint16_t mqtt_cmnd_publish = 0;       // ignore flag for publish command
uint8_t latching_power = 0;           // Power state at latching start
uint8_t latching_relay_pulse = 0;     // Latching relay pulse timer
uint8_t blogidx = 0;                  // Command backlog index
uint8_t blogptr = 0;                  // Command backlog pointer
uint8_t blogmutex = 0;                // Command backlog pending
uint16_t blogdelay = 0;               // Command backlog delay
uint8_t interlockmutex = 0;           // Interlock power command pending

#ifdef USE_MQTT_TLS
  WiFiClientSecure espClient;         // Wifi Secure Client
#else
  WiFiClient espClient;               // Wifi Client
#endif
PubSubClient mqttClient(espClient);   // MQTT Client
WiFiUDP portUDP;                      // UDP Syslog and Alexa

uint8_t power;                        // Current copy of sysCfg.power
byte syslog_level;                    // Current copy of sysCfg.syslog_level
uint16_t syslog_timer = 0;            // Timer to re-enable syslog_level
byte seriallog_level;                 // Current copy of sysCfg.seriallog_level
uint16_t seriallog_timer = 0;         // Timer to disable Seriallog
uint8_t sleep;                        // Current copy of sysCfg.sleep
uint8_t stop_flash_rotate = 0;        // Allow flash configuration rotation

int blinks = 201;                     // Number of LED blinks
uint8_t blinkstate = 0;               // LED state

uint8_t lastbutton[4] = { NOT_PRESSED, NOT_PRESSED, NOT_PRESSED, NOT_PRESSED };     // Last button states
uint8_t holdbutton[4] = { 0 };        // Timer for button hold
uint8_t multiwindow[4] = { 0 };       // Max time between button presses to record press count
uint8_t multipress[4] = { 0 };        // Number of button presses within multiwindow
uint8_t lastwallswitch[4];            // Last wall switch states
uint8_t holdwallswitch[4] = { 0 };    // Timer for wallswitch push button hold
uint8_t blockgpio0 = 4;               // Block GPIO0 for 4 seconds after poweron to workaround Wemos D1 RTS circuit

mytmplt my_module;                    // Active copy of GPIOs
uint8_t pin[GPIO_MAX];                // Possible pin configurations
uint8_t rel_inverted[4] = { 0 };      // Relay inverted flag (1 = (0 = On, 1 = Off))
uint8_t led_inverted[4] = { 0 };      // LED inverted flag (1 = (0 = On, 1 = Off))
uint8_t dht_flg = 0;                  // DHT configured
uint8_t hlw_flg = 0;                  // Power monitor configured
uint8_t i2c_flg = 0;                  // I2C configured
uint8_t spi_flg = 0;                  // SPI configured
uint8_t pwm_flg = 0;                  // PWM configured
uint8_t sfl_flg = 0;                  // Sonoff Led flag (0 = No led, 1 = BN-SZ01, 2 = Sonoff Led, 5 = Sonoff B1)
uint8_t pwm_idxoffset = 0;            // Allowed PWM command offset (change for Sonoff Led)

boolean mDNSbegun = false;

char Version[16];                     // Version string from VERSION define
char Hostname[33];                    // Composed Wifi hostname
char MQTTClient[33];                  // Composed MQTT Clientname
char serialInBuf[INPUT_BUFFER_SIZE + 2];  // Receive buffer
char mqtt_data[MESSZ];                // MQTT publish buffer
char log_data[TOPSZ + MESSZ];         // Logging
String Log[MAX_LOG_LINES];            // Web log buffer
String Backlog[MAX_BACKLOG];          // Command backlog

/********************************************************************************************/

void getClient(char* output, const char* input, byte size)
{
  char *token;
  uint8_t digits = 0;

  if (strstr(input, "%")) {
    strlcpy(output, input, size);
    token = strtok(output, "%");
    if (strstr(input, "%") == input) {
      output[0] = '\0';
    } else {
      token = strtok(NULL, "");
    }
    if (token != NULL) {
      digits = atoi(token);
      if (digits) {
        snprintf_P(output, size, PSTR("%s%c0%dX"), output, '%', digits);
        snprintf_P(output, size, output, ESP.getChipId());
      }
    }
  }
  if (!digits) {
    strlcpy(output, input, size);
  }
}

void getTopic_P(char *stopic, byte prefix, char *topic, const char* subtopic)
{
  char romram[CMDSZ];
  String fulltopic;

  snprintf_P(romram, sizeof(romram), subtopic);
  if (fallbacktopic) {
    fulltopic = FPSTR(PREFIXES[prefix]);
    fulltopic += F("/");
    fulltopic += MQTTClient;
  } else {
    fulltopic = sysCfg.mqtt_fulltopic;
    if ((0 == prefix) && (-1 == fulltopic.indexOf(F(MQTT_TOKEN_PREFIX)))) {
      fulltopic += F("/" MQTT_TOKEN_PREFIX);  // Need prefix for commands to handle mqtt topic loops
    }
    for (byte i = 0; i < 3; i++) {
      if ('\0' == sysCfg.mqtt_prefix[i][0]) {
        snprintf_P(sysCfg.mqtt_prefix[i], sizeof(sysCfg.mqtt_prefix[i]), PREFIXES[i]);
      }
    }
    fulltopic.replace(F(MQTT_TOKEN_PREFIX), sysCfg.mqtt_prefix[prefix]);
    fulltopic.replace(F(MQTT_TOKEN_TOPIC), topic);
  }
  fulltopic.replace(F("#"), "");
  fulltopic.replace(F("//"), "/");
  if (!fulltopic.endsWith("/")) {
    fulltopic += "/";
  }
  snprintf_P(stopic, TOPSZ, PSTR("%s%s"), fulltopic.c_str(), romram);
}

char* getStateText(byte state)
{
  if (state > 3) {
    state = 1;
  }
  return sysCfg.state_text[state];
}

/********************************************************************************************/

/********************************************************************************************/

void setLatchingRelay(uint8_t power, uint8_t state)
{
  power &= 1;
  if (2 == state) {           // Reset relay
    state = 0;
    latching_power = power;
    latching_relay_pulse = 0;
  }
  else if (state && !latching_relay_pulse) {  // Set port power to On
    latching_power = power;
    latching_relay_pulse = 2; // max 200mS (initiated by stateloop())
  }
  if (pin[GPIO_REL1 +latching_power] < 99) {
    digitalWrite(pin[GPIO_REL1 +latching_power], rel_inverted[latching_power] ? !state : state);
  }
}

void setRelay(uint8_t rpower)
{
  uint8_t state;

  if (4 == sysCfg.poweronstate) {  // All on and stay on
    power = (1 << Maxdevice) -1;
    rpower = power;
  }
  if (sysCfg.flag.interlock) {     // Allow only one or no relay set
    uint8_t mask = 0x01;
    uint8_t count = 0;
    for (byte i = 0; i < Maxdevice; i++) {
      if (rpower & mask) {
        count++;
      }
      mask <<= 1;
    }
    if (count > 1) {
      power = 0;
      rpower = 0;
    }
  }
  if ((SONOFF_DUAL == sysCfg.module) || (CH4 == sysCfg.module)) {
    Serial.write(0xA0);
    Serial.write(0x04);
    Serial.write(rpower);
    Serial.write(0xA1);
    Serial.write('\n');
    Serial.flush();
  }
  else if (sfl_flg) {
    sl_setPower(rpower);
  }
  else if (EXS_RELAY == sysCfg.module) {
    setLatchingRelay(rpower, 1);
  }
  else {
    for (byte i = 0; i < Maxdevice; i++) {
      state = rpower &1;
      if (pin[GPIO_REL1 +i] < 99) {
        digitalWrite(pin[GPIO_REL1 +i], rel_inverted[i] ? !state : state);
      }
      rpower >>= 1;
    }
  }
  hlw_setPowerSteadyCounter(2);
}

void setLed(uint8_t state)
{
  if (state) {
    state = 1;
  }
  digitalWrite(pin[GPIO_LED1], (led_inverted[0]) ? !state : state);
}

/********************************************************************************************/

void mqtt_publish_sec(const char* topic, boolean retained)
{
  if (sysCfg.flag.mqtt_enabled) {
    if (mqttClient.publish(topic, mqtt_data, retained)) {
      snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_MQTT "%s = %s%s"), topic, mqtt_data, (retained) ? " (" D_RETAINED ")" : "");
//      mqttClient.loop();  // Do not use here! Will block previous publishes
    } else  {
      snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_RESULT "%s = %s"), topic, mqtt_data);
    }
  } else {
    snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_RESULT "%s = %s"), strrchr(topic,'/')+1, mqtt_data);
  }

  addLog(LOG_LEVEL_INFO);
  if (sysCfg.ledstate &0x04) {
    blinks++;
  }
}

void mqtt_publish(const char* topic, boolean retained)
{
  char *me;

  if (!strcmp(sysCfg.mqtt_prefix[0],sysCfg.mqtt_prefix[1])) {
    me = strstr(topic,sysCfg.mqtt_prefix[0]);
    if (me == topic) {
      mqtt_cmnd_publish += 8;
    }
  }
  mqtt_publish_sec(topic, retained);
}

void mqtt_publish(const char* topic)
{
  mqtt_publish(topic, false);
}

void mqtt_publish_topic_P(uint8_t prefix, const char* subtopic, boolean retained)
{
/* prefix 0 = cmnd using subtopic
 * prefix 1 = stat using subtopic
 * prefix 2 = tele using subtopic
 * prefix 4 = cmnd using subtopic or RESULT
 * prefix 5 = stat using subtopic or RESULT
 * prefix 6 = tele using subtopic or RESULT
 */
  char romram[16];
  char stopic[TOPSZ];

  snprintf_P(romram, sizeof(romram), ((prefix > 3) && !sysCfg.flag.mqtt_response) ? S_RSLT_RESULT : subtopic);
  for (byte i = 0; i < strlen(romram); i++) {
    romram[i] = toupper(romram[i]);
  }
  prefix &= 3;
  getTopic_P(stopic, prefix, sysCfg.mqtt_topic, romram);
  mqtt_publish(stopic, retained);
}

void mqtt_publish_topic_P(uint8_t prefix, const char* subtopic)
{
  mqtt_publish_topic_P(prefix, subtopic, false);
}

void mqtt_publishPowerState(byte device)
{
  char stopic[TOPSZ];
  char scommand[16];

  if ((device < 1) || (device > Maxdevice)) {
    device = 1;
  }
  getPowerDevice(scommand, device, sizeof(scommand));
  getTopic_P(stopic, 1, sysCfg.mqtt_topic, (sysCfg.flag.mqtt_response) ? scommand : S_RSLT_RESULT);
  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"%s\":\"%s\"}"), scommand, getStateText(bitRead(power, device -1)));
  mqtt_publish(stopic);

  getTopic_P(stopic, 1, sysCfg.mqtt_topic, scommand);
  snprintf_P(mqtt_data, sizeof(mqtt_data), getStateText(bitRead(power, device -1)));
  mqtt_publish(stopic, sysCfg.flag.mqtt_power_retain);
}

void mqtt_publishPowerBlinkState(byte device)
{
  char scommand[16];

  if ((device < 1) || (device > Maxdevice)) {
    device = 1;
  }
  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"%s\":\"" D_BLINK " %s\"}"),
    getPowerDevice(scommand, device, sizeof(scommand)), getStateText(bitRead(blink_mask, device -1)));

  mqtt_publish_topic_P(5, S_RSLT_POWER);
}


void mqtt_connected()
{
  char stopic[TOPSZ];

  if (sysCfg.flag.mqtt_enabled) {

    // Satisfy iobroker (#299)
    mqtt_data[0] = '\0';
    mqtt_publish_topic_P(0, S_RSLT_POWER);

    getTopic_P(stopic, 0, sysCfg.mqtt_topic, PSTR("#"));
    mqttClient.subscribe(stopic);
    mqttClient.loop();  // Solve LmacRxBlk:1 messages
    if (strstr(sysCfg.mqtt_fulltopic, MQTT_TOKEN_TOPIC) != NULL) {
      getTopic_P(stopic, 0, sysCfg.mqtt_grptopic, PSTR("#"));
      mqttClient.subscribe(stopic);
      mqttClient.loop();  // Solve LmacRxBlk:1 messages
      fallbacktopic = 1;
      getTopic_P(stopic, 0, MQTTClient, PSTR("#"));
      fallbacktopic = 0;
      mqttClient.subscribe(stopic);
      mqttClient.loop();  // Solve LmacRxBlk:1 messages
    }
#ifdef USE_DOMOTICZ
    domoticz_mqttSubscribe();
#endif  // USE_DOMOTICZ
  }

  if (mqttflag) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_MODULE "\":\"%s\", \"" D_VERSION "\":\"%s\", \"" D_FALLBACKTOPIC "\":\"%s\", \"" D_CMND_GROUPTOPIC "\":\"%s\"}"),
      my_module.name, Version, MQTTClient, sysCfg.mqtt_grptopic);
    mqtt_publish_topic_P(2, PSTR(D_RSLT_INFO "1"));
#ifdef USE_WEBSERVER
    if (sysCfg.webserver) {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_WEBSERVER_MODE "\":\"%s\", \"" D_CMND_HOSTNAME "\":\"%s\", \"" D_CMND_IPADDRESS "\":\"%s\"}"),
        (2 == sysCfg.webserver) ? D_ADMIN : D_USER, Hostname, WiFi.localIP().toString().c_str());
      mqtt_publish_topic_P(2, PSTR(D_RSLT_INFO "2"));
    }
#endif  // USE_WEBSERVER
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_RESTARTREASON "\":\"%s\"}"),
      (getResetReason() == "Exception") ? ESP.getResetInfo().c_str() : getResetReason().c_str());
    mqtt_publish_topic_P(2, PSTR(D_RSLT_INFO "3"));
    if (sysCfg.tele_period) {
      tele_period = sysCfg.tele_period -9;
    }
    status_update_timer = 2;
#ifdef USE_DOMOTICZ
    domoticz_setUpdateTimer(2);
#endif  // USE_DOMOTICZ
  }
  mqttflag = 0;
}


////////////Nextioncode///////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Modify these values for your environment
//const char* sysCfg.sta_ssid[sysCfg.sta_active] = "YOUR_WIFI_NETWORK";  // your WiFi network name
//const char* sysCfg.sta_pwd[sysCfg.sta_active] = "YOUR_WIFI_PASSWORD";  // your WiFi network password
//const char* sysCfg.mqtt_host = "YOUR_MQTT_BROKER_IP";  // your MQTT broker IP address
//const char* sysCfg.mqtt_user = ""; // mqtt broker username, set to "" for no user
//const char* sysCfg.mqtt_pwd = ""; // mqtt broker password, set to "" for no password
const String mqttNode = "HA"; // your unique hostname for this device
const String mqttDiscoveryPrefix = "NEX"; // Home Assistant MQTT Discovery, see https://home-assistant.io/docs/mqtt/discovery/
////////////////////////////////////////////////////////////////////////////////////////////////////

// Serial debug output control, comment out to disable
//#define DEBUGSERIAL

// OK embedded guys don't read anything below because thar be strings errywhere (HEY I HAVE THE RAM OK)

// MQTT topic string definitions
const String mqttStateTopic = mqttDiscoveryPrefix + "/haswitchplate/" + mqttNode + "/state";
const String mqttCommandTopic = mqttDiscoveryPrefix + "/haswitchplate/" + mqttNode + "/command";
const String mqttSubscription = mqttCommandTopic + "/#";

// Home Assistant MQTT Discovery, see https://home-assistant.io/docs/mqtt/discovery/
// We'll create one binary_sensor device to track MQTT connectivity
//const String mqttDiscoBinaryStateTopic = mqttDiscoveryPrefix + "/binary_sensor/" + mqttNode + "/state";
//const String mqttDiscoBinaryConfigTopic = mqttDiscoveryPrefix + "/binary_sensor/" + mqttNode + "/config";

// And one light device to set dim values on the panel
//const String mqttDiscoLightSwitchCommandTopic = mqttDiscoveryPrefix + "/light/" + mqttNode + "/switch";
//const String mqttDiscoLightSwitchStateTopic = mqttDiscoveryPrefix + "/light/" + mqttNode + "/status";
//const String mqttDiscoLightBrightCommandTopic = mqttDiscoveryPrefix + "/light/" + mqttNode + "/brightness/set";
//const String mqttDiscoLightBrightStateTopic = mqttDiscoveryPrefix + "/light/" + mqttNode + "/brightness";
//const String mqttDiscoLightConfigTopic = mqttDiscoveryPrefix + "/light/" + mqttNode + "/config";
//const String mqttDiscoLightSubscription = mqttDiscoveryPrefix + "/light/" + mqttNode + "/#";

// The strings below will spill over the PubSubClient_MAX_PACKET_SIZE 128
//const String mqttDiscoBinaryConfigPayload = "{\"name\": \"" + mqttNode + "\", \"device_class\": \"connectivity\", \"state_topic\": \"" + mqttDiscoBinaryStateTopic + "\"}";
//const String mqttDiscoLightConfigPayload = "{\"name\": \"" + mqttNode + "\", \"command_topic\": \"" + mqttDiscoLightSwitchCommandTopic + "\", \"state_topic\": \"" + mqttDiscoLightSwitchStateTopic + "\", \"brightness_command_topic\": \"" + mqttDiscoLightBrightCommandTopic + "\", \"brightness_state_topic\": \"" + mqttDiscoLightBrightStateTopic + "\"}";

// global byte array to pass around data coming from the panel
byte nextionReturnBuffer[100];
int nextionReturnIndex = 0;

// Global string to hold topic string for incoming commands requesting .val
// or automatic .val requests from button presses
String mqttGetSubtopic;

////////////Nextioncodeends///////

void mqtt_reconnect()
{
  char stopic[TOPSZ];

  mqttcounter = sysCfg.mqtt_retry;

  if (!sysCfg.flag.mqtt_enabled) {
    mqtt_connected();
    return;
  }
#ifdef USE_EMULATION
  UDP_Disconnect();
#endif  // USE_EMULATION
  if (mqttflag > 1) {
#ifdef USE_MQTT_TLS
    addLog_P(LOG_LEVEL_INFO, S_LOG_MQTT, PSTR(D_FINGERPRINT));
    if (!espClient.connect(sysCfg.mqtt_host, sysCfg.mqtt_port)) {
      snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_MQTT D_TLS_CONNECT_FAILED_TO " %s:%d. " D_RETRY_IN " %d " D_UNIT_SECOND),
        sysCfg.mqtt_host, sysCfg.mqtt_port, mqttcounter);
      addLog(LOG_LEVEL_DEBUG);
      return;
    }
    if (espClient.verify(sysCfg.mqtt_fingerprint, sysCfg.mqtt_host)) {
      addLog_P(LOG_LEVEL_INFO, S_LOG_MQTT, PSTR(D_VERIFIED));
    } else {
      addLog_P(LOG_LEVEL_DEBUG, S_LOG_MQTT, PSTR(D_INSECURE));
    }
#endif  // USE_MQTT_TLS
    mqttClient.setCallback(mqttDataCb);
    mqttflag = 1;
    mqttcounter = 1;
    return;
  }

  addLog_P(LOG_LEVEL_INFO, S_LOG_MQTT, PSTR(D_ATTEMPTING_CONNECTION));

#ifndef USE_MQTT_TLS
#ifdef USE_DISCOVERY
#ifdef MQTT_HOST_DISCOVERY
  mdns_discoverMQTTServer();
#endif  // MQTT_HOST_DISCOVERY
#endif  // USE_DISCOVERY
#endif  // USE_MQTT_TLS
  mqttClient.setServer(sysCfg.mqtt_host, sysCfg.mqtt_port);

  getTopic_P(stopic, 2, sysCfg.mqtt_topic, S_LWT);
  snprintf_P(mqtt_data, sizeof(mqtt_data), S_OFFLINE);
  if (mqttClient.connect(MQTTClient, sysCfg.mqtt_user, sysCfg.mqtt_pwd, stopic, 1, true, mqtt_data)) {
    addLog_P(LOG_LEVEL_INFO, S_LOG_MQTT, PSTR(D_CONNECTED));
    ////////////Nextioncode///////////
    mqttClient.subscribe(mqttSubscription.c_str());
    mqttClient.publish(mqttCommandTopic.c_str(), "ONJI");

    // publish discovery messages for Home Assistant MQTT Discovery
    //mqttClient.publish(mqttDiscoBinaryConfigTopic.c_str(), mqttDiscoBinaryConfigPayload.c_str(), true);
    //mqttClient.publish(mqttDiscoBinaryStateTopic.c_str(), "ON", true);

    //mqttClient.publish(mqttDiscoLightConfigTopic.c_str(), mqttDiscoLightConfigPayload.c_str(), true);
    //mqttClient.publish(mqttDiscoLightSwitchStateTopic.c_str(), "ON", true);

    ////////////Nextioncodeends///////
    mqttcounter = 0;

    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR(D_ONLINE));
    mqtt_publish(stopic, true);
    mqtt_connected();
  } else {

    snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_MQTT D_CONNECT_FAILED_TO " %s:%d, rc %d. " D_RETRY_IN " %d " D_UNIT_SECOND),
      sysCfg.mqtt_host, sysCfg.mqtt_port, mqttClient.state(), mqttcounter);  //status codes are documented here http://pubsubclient.knolleary.net/api.html#state
    addLog(LOG_LEVEL_INFO);
  }
}

/********************************************************************************************/

boolean mqtt_command(boolean grpflg, char *type, uint16_t index, char *dataBuf, uint16_t data_len, int16_t payload, uint16_t payload16)
{
  boolean serviced = true;
  char stemp1[TOPSZ];
  char stemp2[10];
  char scommand[CMDSZ];
  uint16_t i;

  if (!strcasecmp_P(type, PSTR(D_CMND_MQTTHOST))) {
    if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_host))) {
      strlcpy(sysCfg.mqtt_host, (1 == payload) ? MQTT_HOST : dataBuf, sizeof(sysCfg.mqtt_host));
      restartflag = 2;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_MQTTHOST "\":\"%s\"}"), sysCfg.mqtt_host);
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_MQTTPORT))) {
    if (payload16 > 0) {
      sysCfg.mqtt_port = (1 == payload16) ? MQTT_PORT : payload16;
      restartflag = 2;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_MQTTPORT "\":%d}"), sysCfg.mqtt_port);
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_MQTTRETRY))) {
    if ((payload >= MQTT_RETRY_SECS) && (payload < 32001)) {
      sysCfg.mqtt_retry = payload;
      mqttcounter = sysCfg.mqtt_retry;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_MQTTRETRY "\":%d}"), sysCfg.mqtt_retry);
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_STATETEXT)) && (index > 0) && (index <= 4)) {
    if ((data_len > 0) && (data_len < sizeof(sysCfg.state_text[0]))) {
      for(i = 0; i <= data_len; i++) {
        if (dataBuf[i] == ' ') {
          dataBuf[i] = '_';
        }
      }
      strlcpy(sysCfg.state_text[index -1], dataBuf, sizeof(sysCfg.state_text[0]));
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATETEXT "%d\":\"%s\"}"), index, getStateText(index -1));
  }
#ifdef USE_MQTT_TLS
  else if (!strcasecmp_P(type, PSTR(D_CMND_MQTTFINGERPRINT))) {
    if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_fingerprint))) {
      strlcpy(sysCfg.mqtt_fingerprint, (!strcmp(dataBuf,"0")) ? "" : (1 == payload) ? MQTT_FINGERPRINT : dataBuf, sizeof(sysCfg.mqtt_fingerprint));
      restartflag = 2;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_MQTTFINGERPRINT "\":\"%s\"}"), sysCfg.mqtt_fingerprint);
  }
#endif
  else if (!grpflg && !strcasecmp_P(type, PSTR(D_CMND_MQTTCLIENT))) {
    if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_client))) {
      strlcpy(sysCfg.mqtt_client, (1 == payload) ? MQTT_CLIENT_ID : dataBuf, sizeof(sysCfg.mqtt_client));
      restartflag = 2;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_MQTTCLIENT "\":\"%s\"}"), sysCfg.mqtt_client);
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_MQTTUSER))) {
    if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_user))) {
      strlcpy(sysCfg.mqtt_user, (!strcmp(dataBuf,"0")) ? "" : (1 == payload) ? MQTT_USER : dataBuf, sizeof(sysCfg.mqtt_user));
      restartflag = 2;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_MQTTUSER "\":\"%s\"}"), sysCfg.mqtt_user);
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_MQTTPASSWORD))) {
    if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_pwd))) {
      strlcpy(sysCfg.mqtt_pwd, (!strcmp(dataBuf,"0")) ? "" : (1 == payload) ? MQTT_PASS : dataBuf, sizeof(sysCfg.mqtt_pwd));
      restartflag = 2;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_MQTTPASSWORD "\":\"%s\"}"), sysCfg.mqtt_pwd);
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_FULLTOPIC))) {
    if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_fulltopic))) {
      mqttfy(1, dataBuf);
      if (!strcmp(dataBuf, MQTTClient)) {
        payload = 1;
      }
      strlcpy(stemp1, (1 == payload) ? MQTT_FULLTOPIC : dataBuf, sizeof(stemp1));
      if (strcmp(stemp1, sysCfg.mqtt_fulltopic)) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), (sysCfg.flag.mqtt_offline) ? S_OFFLINE : "");
        mqtt_publish_topic_P(2, PSTR(D_LWT), true);  // Offline or remove previous retained topic
        strlcpy(sysCfg.mqtt_fulltopic, stemp1, sizeof(sysCfg.mqtt_fulltopic));
        restartflag = 2;
      }
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_FULLTOPIC "\":\"%s\"}"), sysCfg.mqtt_fulltopic);
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_PREFIX)) && (index > 0) && (index <= 3)) {
    if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_prefix[0]))) {
      mqttfy(0, dataBuf);
      strlcpy(sysCfg.mqtt_prefix[index -1], (1 == payload) ? (1==index)?SUB_PREFIX:(2==index)?PUB_PREFIX:PUB_PREFIX2 : dataBuf, sizeof(sysCfg.mqtt_prefix[0]));
//      if (sysCfg.mqtt_prefix[index -1][strlen(sysCfg.mqtt_prefix[index -1])] == '/') sysCfg.mqtt_prefix[index -1][strlen(sysCfg.mqtt_prefix[index -1])] = 0;
      restartflag = 2;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_PREFIX "%d\":\"%s\"}"), index, sysCfg.mqtt_prefix[index -1]);
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_GROUPTOPIC))) {
    if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_grptopic))) {
      mqttfy(0, dataBuf);
      if (!strcmp(dataBuf, MQTTClient)) {
        payload = 1;
      }
      strlcpy(sysCfg.mqtt_grptopic, (1 == payload) ? MQTT_GRPTOPIC : dataBuf, sizeof(sysCfg.mqtt_grptopic));
      restartflag = 2;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_GROUPTOPIC "\":\"%s\"}"), sysCfg.mqtt_grptopic);
  }
  else if (!grpflg && !strcasecmp_P(type, PSTR(D_CMND_TOPIC))) {
    if ((data_len > 0) && (data_len < sizeof(sysCfg.mqtt_topic))) {
      mqttfy(0, dataBuf);
      if (!strcmp(dataBuf, MQTTClient)) {
        payload = 1;
      }
      strlcpy(stemp1, (1 == payload) ? MQTT_TOPIC : dataBuf, sizeof(stemp1));
      if (strcmp(stemp1, sysCfg.mqtt_topic)) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), (sysCfg.flag.mqtt_offline) ? S_OFFLINE : "");
        mqtt_publish_topic_P(2, PSTR(D_LWT), true);  // Offline or remove previous retained topic
        strlcpy(sysCfg.mqtt_topic, stemp1, sizeof(sysCfg.mqtt_topic));
        restartflag = 2;
      }
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_TOPIC "\":\"%s\"}"), sysCfg.mqtt_topic);
  }
  else if (!grpflg && !strcasecmp_P(type, PSTR(D_CMND_BUTTONTOPIC))) {
    if ((data_len > 0) && (data_len < sizeof(sysCfg.button_topic))) {
      mqttfy(0, dataBuf);
      if (!strcmp(dataBuf, MQTTClient)) {
        payload = 1;
      }
      strlcpy(sysCfg.button_topic, (!strcmp(dataBuf,"0")) ? "" : (1 == payload) ? sysCfg.mqtt_topic : dataBuf, sizeof(sysCfg.button_topic));
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_BUTTONTOPIC "\":\"%s\"}"), sysCfg.button_topic);
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_SWITCHTOPIC))) {
    if ((data_len > 0) && (data_len < sizeof(sysCfg.switch_topic))) {
      mqttfy(0, dataBuf);
      if (!strcmp(dataBuf, MQTTClient)) {
        payload = 1;
      }
      strlcpy(sysCfg.switch_topic, (!strcmp(dataBuf,"0")) ? "" : (1 == payload) ? sysCfg.mqtt_topic : dataBuf, sizeof(sysCfg.switch_topic));
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_SWITCHTOPIC "\":\"%s\"}"), sysCfg.switch_topic);
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_BUTTONRETAIN))) {
    if ((payload >= 0) && (payload <= 1)) {
      strlcpy(sysCfg.button_topic, sysCfg.mqtt_topic, sizeof(sysCfg.button_topic));
      if (!payload) {
        for(i = 1; i <= Maxdevice; i++) {
          send_button_power(0, i, 9);  // Clear MQTT retain in broker
        }
      }
      sysCfg.flag.mqtt_button_retain = payload;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_BUTTONRETAIN "\":\"%s\"}"), getStateText(sysCfg.flag.mqtt_button_retain));
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_SWITCHRETAIN))) {
    if ((payload >= 0) && (payload <= 1)) {
      strlcpy(sysCfg.button_topic, sysCfg.mqtt_topic, sizeof(sysCfg.button_topic));
      if (!payload) {
        for(i = 1; i <= 4; i++) {
          send_button_power(1, i, 9);  // Clear MQTT retain in broker
        }
      }
      sysCfg.flag.mqtt_switch_retain = payload;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_SWITCHRETAIN "\":\"%s\"}"), getStateText(sysCfg.flag.mqtt_switch_retain));
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_POWERRETAIN))) {
    if ((payload >= 0) && (payload <= 1)) {
      if (!payload) {
        for(i = 1; i <= Maxdevice; i++) {  // Clear MQTT retain in broker
          getTopic_P(stemp1, 1, sysCfg.mqtt_topic, getPowerDevice(scommand, i, sizeof(scommand)));
          mqtt_data[0] = '\0';
          mqtt_publish(stemp1, sysCfg.flag.mqtt_power_retain);
        }
      }
      sysCfg.flag.mqtt_power_retain = payload;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_POWERRETAIN "\":\"%s\"}"), getStateText(sysCfg.flag.mqtt_power_retain));
  }
  else if (!strcasecmp_P(type, PSTR(D_CMND_SENSORRETAIN))) {
    if ((payload >= 0) && (payload <= 1)) {
      if (!payload) {
        mqtt_data[0] = '\0';
        mqtt_publish_topic_P(2, PSTR(D_RSLT_SENSOR), sysCfg.flag.mqtt_sensor_retain);
      }
      sysCfg.flag.mqtt_sensor_retain = payload;
    }
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_SENSORRETAIN "\":\"%s\"}"), getStateText(sysCfg.flag.mqtt_sensor_retain));
  }

#ifdef USE_DOMOTICZ
  else if (domoticz_command(type, index, dataBuf, data_len, payload)) {
    // Serviced
  }
#endif  // USE_DOMOTICZ
  else {
    serviced = false;
  }
  return serviced;
}

/********************************************************************************************/

/********************************************************************************************/
void mqttDataCb(char* topic, byte* data, unsigned int data_len)
{
  char *str;

  if (!strcmp(sysCfg.mqtt_prefix[0],sysCfg.mqtt_prefix[1])) {
    str = strstr(topic,sysCfg.mqtt_prefix[0]);
    if ((str == topic) && mqtt_cmnd_publish) {
      if (mqtt_cmnd_publish > 8) {
        mqtt_cmnd_publish -= 8;
      } else {
        mqtt_cmnd_publish = 0;
      }
      return;
    }
  }

  char topicBuf[TOPSZ];
  char dataBuf[data_len+1];
  char stemp1[TOPSZ];
  char *p;
  char *mtopic = NULL;
  char *type = NULL;
  byte otype = 0;
  byte ptype = 0;
  byte jsflg = 0;
  byte lines = 1;
  uint16_t i = 0;
  uint16_t grpflg = 0;
  uint16_t index;
  uint32_t address;

  ////////////Nextioncode///////////
// strTopic: NEX/haswitchplate/devicename/command/p[1].b[4].txt
  // data: "Lights On"
  // subTopic: p[1].b[4].txt

  String strTopic = topic;
  // convert the data into a string
  String strPayload;
  for (int i = 0; i < data_len; i++) {
    strPayload += (char)data[i];
  }

  // Incoming Namespace:
  // '[...]/device/command' -m '' = undefined
  // '[...]/device/command' -m 'dim 50' = sendNextionCmd("dim 50")
  // '[...]/device/command/page' -m '1' = sendNextionCmd("page 1")
  // '[...]/device/command/motionLatch' -m '120' = set motionLatch time to 120 seconds
  // '[...]/device/command/update' -m 'http://192.168.0.10/local/HASwitchPlate.tft' = startNextionOTA("http://192.168.0.10/local/HASwitchPlate.tft")
  // '[...]/device/command/p[1].b[4].txt' -m '' = getNextionAttr("p[1].b[4].txt")
  // '[...]/device/command/p[1].b[4].txt' -m '"Lights On"' = setNextionAttr("p[1].b[4].txt", "\"Lights On\"")

//  debugPrintln("MQTT IN:  '" + strTopic + "' : '" + strPayload + "'");

  // '[...]/device/command' -m '' = undefined
  if ((strTopic == mqttCommandTopic) && (strPayload == "")) {
    // currently undefined
  }

  // '[...]/device/command' -m 'dim 50' = sendNextionCmd("dim 50")
  else if ((strTopic == mqttCommandTopic)) {
    sendNextionCmd(strPayload);
  }

  // '[...]/device/command/page' -m '1' = sendNextionCmd("page 1")
  else if ((strTopic == (mqttCommandTopic + "/page"))) {
    sendNextionCmd("page " + strPayload);
  }

  // '[...]/device/command/p[1].b[4].txt' -m '' = getNextionAttr("p[1].b[4].txt")
  else if (strTopic.startsWith(mqttCommandTopic) && (strPayload == "")) {
    String subTopic = strTopic.substring(mqttCommandTopic.length() + 1);
    mqttGetSubtopic = "/" + subTopic;
    getNextionAttr(subTopic);
  }

  // '[...]/device/command/p[1].b[4].txt' -m '"Lights On"' = setNextionAttr("p[1].b[4].txt", "\"Lights On\"")
  else if (strTopic.startsWith(mqttCommandTopic)) {
    String subTopic = strTopic.substring(mqttCommandTopic.length() + 1);
    setNextionAttr(subTopic, strPayload);
  }

////////////Nextioncodeends///////

  strncpy(topicBuf, topic, sizeof(topicBuf));
  for (i = 0; i < data_len; i++) {
    if (!isspace(data[i])) {
      break;
    }
  }
  data_len -= i;
  memcpy(dataBuf, data +i, sizeof(dataBuf));
  dataBuf[sizeof(dataBuf)-1] = 0;

  snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_RESULT D_RECEIVED_TOPIC " %s, " D_DATA_SIZE " %d, " D_DATA " %s"),
    topicBuf, data_len, dataBuf);
  addLog(LOG_LEVEL_DEBUG_MORE);
//  if (LOG_LEVEL_DEBUG_MORE <= seriallog_level) Serial.println(dataBuf);

#ifdef USE_DOMOTICZ
  if (sysCfg.flag.mqtt_enabled) {
    if (domoticz_mqttData(topicBuf, sizeof(topicBuf), dataBuf, sizeof(dataBuf))) {
      return;
    }
  }
#endif  // USE_DOMOTICZ

  grpflg = (strstr(topicBuf, sysCfg.mqtt_grptopic) != NULL);
  fallbacktopic = (strstr(topicBuf, MQTTClient) != NULL);
  type = strrchr(topicBuf, '/') +1;  // Last part of received topic is always the command (type)

  index = 1;
  if (type != NULL) {
    for (i = 0; i < strlen(type); i++) {
      type[i] = toupper(type[i]);
    }
    while (isdigit(type[i-1])) {
      i--;
    }
    if (i < strlen(type)) {
      index = atoi(type +i);
    }
    type[i] = '\0';
  }

  snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_RESULT D_GROUP " %d, " D_INDEX " %d, " D_COMMAND " %s, " D_DATA " %s"),
    grpflg, index, type, dataBuf);
  addLog(LOG_LEVEL_DEBUG);

  if (type != NULL) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_COMMAND "\":\"" D_ERROR "\"}"));
    if (sysCfg.ledstate &0x02) {
      blinks++;
    }

    if (!strcmp(dataBuf,"?")) {
      data_len = 0;
    }
    int16_t payload = -99;               // No payload
    uint16_t payload16 = 0;
    long lnum = strtol(dataBuf, &p, 10);
    if (p != dataBuf) {
      payload = (int16_t) lnum;          // -32766 - 32767
      payload16 = (uint16_t) lnum;       // 0 - 65535
    }
    blogdelay = MIN_BACKLOG_DELAY;       // Reset backlog delay

    if (!strcasecmp_P(dataBuf, PSTR("OFF")) || !strcasecmp_P(dataBuf, PSTR(D_OFF)) || !strcasecmp_P(dataBuf, sysCfg.state_text[0]) || !strcasecmp_P(dataBuf, PSTR(D_FALSE)) || !strcasecmp_P(dataBuf, PSTR(D_STOP)) || !strcasecmp_P(dataBuf, PSTR(D_CELSIUS))) {
      payload = 0;
    }
    if (!strcasecmp_P(dataBuf, PSTR("ON")) || !strcasecmp_P(dataBuf, PSTR(D_ON)) || !strcasecmp_P(dataBuf, sysCfg.state_text[1]) || !strcasecmp_P(dataBuf, PSTR(D_TRUE)) || !strcasecmp_P(dataBuf, PSTR(D_START)) || !strcasecmp_P(dataBuf, PSTR(D_FAHRENHEIT)) || !strcasecmp_P(dataBuf, PSTR(D_USER))) {
      payload = 1;
    }
    if (!strcasecmp_P(dataBuf, PSTR("TOGGLE")) || !strcasecmp_P(dataBuf, PSTR(D_TOGGLE)) || !strcasecmp_P(dataBuf, sysCfg.state_text[2]) || !strcasecmp_P(dataBuf, PSTR(D_ADMIN))) {
      payload = 2;
    }
    if (!strcasecmp_P(dataBuf, PSTR("BLINK")) || !strcasecmp_P(dataBuf, PSTR(D_BLINK))) {
      payload = 3;
    }
    if (!strcasecmp_P(dataBuf, PSTR("BLINKOFF")) || !strcasecmp_P(dataBuf, PSTR(D_BLINKOFF))) {
      payload = 4;
    }

//    snprintf_P(log_data, sizeof(log_data), PSTR("RSLT: Payload %d, Payload16 %d"), payload, payload16);
//    addLog(LOG_LEVEL_DEBUG);

    if (!strcasecmp_P(type, PSTR(D_CMND_BACKLOG))) {
      if (data_len) {
        char *blcommand = strtok(dataBuf, ";");
        while (blcommand != NULL) {
          Backlog[blogidx] = String(blcommand);
          blogidx++;
/*
          if (blogidx >= MAX_BACKLOG) {
            blogidx = 0;
          }
*/
          blogidx &= 0xF;
          blcommand = strtok(NULL, ";");
        }
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_BACKLOG "\":\"" D_APPENDED "\"}"));
      } else {
        uint8_t blflag = (blogptr == blogidx);
        blogptr = blogidx;
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_BACKLOG "\":\"%s\"}"), blflag ? D_EMPTY : D_ABORTED);
      }
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_DELAY))) {
      if ((payload >= MIN_BACKLOG_DELAY) && (payload <= 3600)) {
        blogdelay = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_DELAY "\":%d}"), blogdelay);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_POWER)) && (index > 0) && (index <= Maxdevice)) {
      if ((payload < 0) || (payload > 4)) {
        payload = 9;
      }
      do_cmnd_power(index, payload);
      fallbacktopic = 0;
      return;
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_STATUS))) {
      if ((payload < 0) || (payload > MAX_STATUS)) {
        payload = 99;
      }
      publish_status(payload);
      fallbacktopic = 0;
      return;
    }
    else if ((sysCfg.module != MOTOR) && !strcasecmp_P(type, PSTR(D_CMND_POWERONSTATE))) {
      /* 0 = Keep relays off after power on
       * 1 = Turn relays on after power on
       * 2 = Toggle relays after power on
       * 3 = Set relays to last saved state after power on
       * 4 = Turn relays on and disable any relay control (used for Sonoff Pow to always measure power)
       */
      if ((payload >= 0) && (payload <= 4)) {
        sysCfg.poweronstate = payload;
        if (4 == sysCfg.poweronstate) {
          for (byte i = 1; i <= Maxdevice; i++) {
            do_cmnd_power(i, 1);
          }
        }
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_POWERONSTATE "\":%d}"), sysCfg.poweronstate);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_PULSETIME)) && (index > 0) && (index <= MAX_PULSETIMERS)) {
      if (data_len > 0) {
        sysCfg.pulsetime[index -1] = payload16;  // 0 - 65535
        pulse_timer[index -1] = 0;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_PULSETIME "%d\":%d}"), index, sysCfg.pulsetime[index -1]);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_BLINKTIME))) {
      if ((payload > 2) && (payload <= 3600)) {
        sysCfg.blinktime = payload;
        if (blink_timer) {
          blink_timer = sysCfg.blinktime;
        }
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_BLINKTIME "\":%d}"), sysCfg.blinktime);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_BLINKCOUNT))) {
      if (data_len > 0) {
        sysCfg.blinkcount = payload16;  // 0 - 65535
        if (blink_counter) {
          blink_counter = sysCfg.blinkcount *2;
        }
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_BLINKCOUNT "\":%d}"), sysCfg.blinkcount);
    }
    else if (sfl_flg && sl_command(type, index, dataBuf, data_len, payload)) {
      // Serviced
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_SAVEDATA))) {
      if ((payload >= 0) && (payload <= 3600)) {
        sysCfg.savedata = payload;
        savedatacounter = sysCfg.savedata;
      }
      if (sysCfg.flag.savestate) {
        sysCfg.power = power;
      }
      CFG_Save(0);
      if (sysCfg.savedata > 1) {
        snprintf_P(stemp1, sizeof(stemp1), PSTR(D_EVERY " %d " D_UNIT_SECOND), sysCfg.savedata);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_SAVEDATA "\":\"%s\"}"), (sysCfg.savedata > 1) ? stemp1 : getStateText(sysCfg.savedata));
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_SETOPTION)) && ((index >= 0) && (index <= 14)) || ((index > 31) && (index <= P_MAX_PARAM8 +31))) {
      if (index <= 31) {
        ptype = 0;   // SetOption0 .. 31
      } else {
        ptype = 1;   // SetOption32 ..
        index = index -32;
      }
      if (payload >= 0) {
        if (0 == ptype) {  // SetOption0 .. 31
          if (payload <= 1) {
            switch (index) {
              case 3:   // mqtt
                restartflag = 2;
              case 0:   // savestate
              case 1:   // button_restrict
              case 2:   // value_units
              case 4:   // mqtt_response
              case 8:   // temperature_conversion
              case 10:  // mqtt_offline
              case 11:  // button_swap
              case 12:  // stop_flash_rotate
              case 13:  // button_single
              case 14:  // interlock
                bitWrite(sysCfg.flag.data, index, payload);
            }
            if (12 == index) {  // stop_flash_rotate
              stop_flash_rotate = payload;
              CFG_Save(2);
            }
          }
        }
        else {  // SetOption32 ..
          switch (index) {
            case P_HOLD_TIME:
              if ((payload >= 1) && (payload <= 100)) {
                sysCfg.param[P_HOLD_TIME] = payload;
              }
              break;
            case P_MAX_POWER_RETRY:
              if ((payload >= 1) && (payload <= 250)) {
                sysCfg.param[P_MAX_POWER_RETRY] = payload;
              }
              break;
          }
        }
      }
      if (ptype) {
        snprintf_P(stemp1, sizeof(stemp1), PSTR("%d"), sysCfg.param[index]);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_SETOPTION "%d\":\"%s\"}"), (ptype) ? index +32 : index, (ptype) ? stemp1 : getStateText(bitRead(sysCfg.flag.data, index)));
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_TEMPERATURE_RESOLUTION))) {
      if ((payload >= 0) && (payload <= 3)) {
        sysCfg.flag.temperature_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_TEMPERATURE_RESOLUTION "\":%d}"), sysCfg.flag.temperature_resolution);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_HUMIDITY_RESOLUTION))) {
      if ((payload >= 0) && (payload <= 3)) {
        sysCfg.flag.humidity_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_HUMIDITY_RESOLUTION "\":%d}"), sysCfg.flag.humidity_resolution);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_PRESSURE_RESOLUTION))) {
      if ((payload >= 0) && (payload <= 3)) {
        sysCfg.flag.pressure_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_PRESSURE_RESOLUTION "\":%d}"), sysCfg.flag.pressure_resolution);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_POWER_RESOLUTION))) {
      if ((payload >= 0) && (payload <= 1)) {
        sysCfg.flag.wattage_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_POWER_RESOLUTION "\":%d}"), sysCfg.flag.wattage_resolution);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_VOLTAGE_RESOLUTION))) {
      if ((payload >= 0) && (payload <= 1)) {
        sysCfg.flag.voltage_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_VOLTAGE_RESOLUTION "\":%d}"), sysCfg.flag.voltage_resolution);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_ENERGY_RESOLUTION))) {
      if ((payload >= 0) && (payload <= 5)) {
        sysCfg.flag.energy_resolution = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_ENERGY_RESOLUTION "\":%d}"), sysCfg.flag.energy_resolution);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_MODULE))) {
      if ((payload > 0) && (payload <= MAXMODULE)) {
        payload--;
        byte new_modflg = (sysCfg.module != payload);
        sysCfg.module = payload;
        if (new_modflg) {
          for (byte i = 0; i < MAX_GPIO_PIN; i++) {
            sysCfg.my_module.gp.io[i] = 0;
          }
        }
        restartflag = 2;
      }
      snprintf_P(stemp1, sizeof(stemp1), modules[sysCfg.module].name);
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_MODULE "\":\"%d (%s)\"}"), sysCfg.module +1, stemp1);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_MODULES))) {
      for (byte i = 0; i < MAXMODULE; i++) {
        if (!jsflg) {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_MODULES "%d\":\""), lines);
        } else {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s, "), mqtt_data);
        }
        jsflg = 1;
        snprintf_P(stemp1, sizeof(stemp1), modules[i].name);
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s%d (%s)"), mqtt_data, i +1, stemp1);
        if ((strlen(mqtt_data) > 200) || (i == MAXMODULE -1)) {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s\"}"), mqtt_data);
          mqtt_publish_topic_P(5, type);
          jsflg = 0;
          lines++;
        }
      }
      mqtt_data[0] = '\0';
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_GPIO)) && (index < MAX_GPIO_PIN)) {
      mytmplt cmodule;
      memcpy_P(&cmodule, &modules[sysCfg.module], sizeof(cmodule));
      if ((GPIO_USER == cmodule.gp.io[index]) && (payload >= 0) && (payload < GPIO_SENSOR_END)) {
        for (byte i = 0; i < MAX_GPIO_PIN; i++) {
          if ((GPIO_USER == cmodule.gp.io[i]) && (sysCfg.my_module.gp.io[i] == payload)) {
            sysCfg.my_module.gp.io[i] = 0;
          }
        }
        sysCfg.my_module.gp.io[index] = payload;
        restartflag = 2;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{"));
      byte jsflg = 0;
      for (byte i = 0; i < MAX_GPIO_PIN; i++) {
        if (GPIO_USER == cmodule.gp.io[i]) {
          if (jsflg) {
            snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s, "), mqtt_data);
          }
          jsflg = 1;
          snprintf_P(stemp1, sizeof(stemp1), sensors[sysCfg.my_module.gp.io[i]]);
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s\"" D_CMND_GPIO "%d\":%d (%s)"), mqtt_data, i, sysCfg.my_module.gp.io[i], stemp1);
        }
      }
      if (jsflg) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
      } else {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_GPIO "\":\"" D_NOT_SUPPORTED "\"}"));
      }
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_GPIOS))) {
      for (byte i = 0; i < GPIO_SENSOR_END; i++) {
        if (!jsflg) {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_GPIOS "%d\":\""), lines);
        } else {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s, "), mqtt_data);
        }
        jsflg = 1;
        snprintf_P(stemp1, sizeof(stemp1), sensors[i]);
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s%d (%s)"), mqtt_data, i, stemp1);
        if ((strlen(mqtt_data) > 200) || (i == GPIO_SENSOR_END -1)) {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s\"}"), mqtt_data);
          mqtt_publish_topic_P(5, type);
          jsflg = 0;
          lines++;
        }
      }
      mqtt_data[0] = '\0';
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_PWM)) && (index > pwm_idxoffset) && (index <= 5)) {
      if ((payload >= 0) && (payload <= PWM_RANGE) && (pin[GPIO_PWM1 + index -1] < 99)) {
        sysCfg.pwmvalue[index -1] = payload;
        analogWrite(pin[GPIO_PWM1 + index -1], payload);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_PWM "\":{"));
      bool first = true;
      for (byte i = 0; i < 5; i++) {
        if(pin[GPIO_PWM1 + i] < 99) {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s%s\"" D_CMND_PWM "%d\":%d"), mqtt_data, first ? "" : ", ", i+1, sysCfg.pwmvalue[i]);
          first = false;
        }
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}}"),mqtt_data);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_COUNTER)) && (index > 0) && (index <= MAX_COUNTERS)) {
      if ((data_len > 0) && (pin[GPIO_CNTR1 + index -1] < 99)) {
        rtcMem.pCounter[index -1] = payload16;
        sysCfg.pCounter[index -1] = payload16;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_COUNTER "%d\":%d}"), index, rtcMem.pCounter[index -1]);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_COUNTERTYPE)) && (index > 0) && (index <= MAX_COUNTERS)) {
      if ((payload >= 0) && (payload <= 1) && (pin[GPIO_CNTR1 + index -1] < 99)) {
        bitWrite(sysCfg.pCounterType, index -1, payload &1);
        rtcMem.pCounter[index -1] = 0;
        sysCfg.pCounter[index -1] = 0;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_COUNTERTYPE "%d\":%d}"), index, bitRead(sysCfg.pCounterType, index -1));
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_COUNTERDEBOUNCE))) {
      if ((data_len > 0) && (payload16 < 32001)) {
        sysCfg.pCounterDebounce = payload16;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_COUNTERDEBOUNCE "\":%d}"), sysCfg.pCounterDebounce);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_SLEEP))) {
      if ((payload >= 0) && (payload < 251)) {
        if ((!sysCfg.sleep && payload) || (sysCfg.sleep && !payload)) {
          restartflag = 2;
        }
        sysCfg.sleep = payload;
        sleep = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_SLEEP "\":\"%d%s (%d%s)\"}"), sleep, (sysCfg.flag.value_units) ? " " D_UNIT_MILLISECOND : "", sysCfg.sleep, (sysCfg.flag.value_units) ? " " D_UNIT_MILLISECOND : "");
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_UPGRADE)) || !strcasecmp_P(type, PSTR(D_CMND_UPLOAD))) {
      // Check if the payload is numerically 1, and had no trailing chars.
      //   e.g. "1foo" or "1.2.3" could fool us.
      // Check if the version we have been asked to upgrade to is higher than our current version.
      //   We also need at least 3 chars to make a valid version number string.
      if (((1 == data_len) && (1 == payload)) || ((data_len >= 3) && newerVersion(dataBuf))) {
        otaflag = 3;
        snprintf_P(mqtt_data, sizeof(mqtt_data), "{\"" D_CMND_UPGRADE "\":\"" D_VERSION " %s " D_FROM " %s\"}", Version, sysCfg.otaUrl);
      } else {
        snprintf_P(mqtt_data, sizeof(mqtt_data), "{\"" D_CMND_UPGRADE "\":\"" D_ONE_OR_GT "\"}", Version);
      }
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_OTAURL))) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.otaUrl)))
        strlcpy(sysCfg.otaUrl, (1 == payload) ? OTA_URL : dataBuf, sizeof(sysCfg.otaUrl));
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_OTAURL "\":\"%s\"}"), sysCfg.otaUrl);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_SERIALLOG))) {
      if ((payload >= LOG_LEVEL_NONE) && (payload <= LOG_LEVEL_ALL)) {
        sysCfg.seriallog_level = payload;
        seriallog_level = payload;
        seriallog_timer = 0;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_SERIALLOG "\":\"%d (" D_ACTIVE " %d)\"}"), sysCfg.seriallog_level, seriallog_level);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_SYSLOG))) {
      if ((payload >= LOG_LEVEL_NONE) && (payload <= LOG_LEVEL_ALL)) {
        sysCfg.syslog_level = payload;
        syslog_level = (sysCfg.flag.emulation) ? 0 : payload;
        syslog_timer = 0;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_SYSLOG "\":\"%d (" D_ACTIVE " %d)\"}"), sysCfg.syslog_level, syslog_level);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_LOGHOST))) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.syslog_host))) {
        strlcpy(sysCfg.syslog_host, (1 == payload) ? SYS_LOG_HOST : dataBuf, sizeof(sysCfg.syslog_host));
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_LOGHOST "\":\"%s\"}"), sysCfg.syslog_host);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_LOGPORT))) {
      if (payload16 > 0) {
        sysCfg.syslog_port = (1 == payload16) ? SYS_LOG_PORT : payload16;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_LOGPORT "\":%d}"), sysCfg.syslog_port);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_IPADDRESS)) && (index > 0) && (index <= 4)) {
      if (parseIP(&address, dataBuf)) {
        sysCfg.ip_address[index -1] = address;
//        restartflag = 2;
      }
      snprintf_P(stemp1, sizeof(stemp1), PSTR(" (%s)"), WiFi.localIP().toString().c_str());
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_IPADDRESS "%d\":\"%s%s\"}"), index, IPAddress(sysCfg.ip_address[index -1]).toString().c_str(), (1 == index) ? stemp1:"");
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_NTPSERVER)) && (index > 0) && (index <= 3)) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.ntp_server[0]))) {
        strlcpy(sysCfg.ntp_server[index -1], (!strcmp(dataBuf,"0")) ? "" : (1 == payload) ? (1==index)?NTP_SERVER1:(2==index)?NTP_SERVER2:NTP_SERVER3 : dataBuf, sizeof(sysCfg.ntp_server[0]));
        for (i = 0; i < strlen(sysCfg.ntp_server[index -1]); i++) {
          if (sysCfg.ntp_server[index -1][i] == ',') {
            sysCfg.ntp_server[index -1][i] = '.';
          }
        }
        restartflag = 2;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_NTPSERVER "%d\":\"%s\"}"), index, sysCfg.ntp_server[index -1]);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_AP))) {
      if ((payload >= 0) && (payload <= 2)) {
        switch (payload) {
        case 0:  // Toggle
          sysCfg.sta_active ^= 1;
          break;
        case 1:  // AP1
        case 2:  // AP2
          sysCfg.sta_active = payload -1;
        }
        restartflag = 2;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_AP "\":\"%d (%s)\"}"), sysCfg.sta_active +1, sysCfg.sta_ssid[sysCfg.sta_active]);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_SSID)) && (index > 0) && (index <= 2)) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.sta_ssid[0]))) {
        strlcpy(sysCfg.sta_ssid[index -1], (1 == payload) ? (1 == index) ? STA_SSID1 : STA_SSID2 : dataBuf, sizeof(sysCfg.sta_ssid[0]));
        sysCfg.sta_active = index -1;
        restartflag = 2;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_SSID "%d\":\"%s\"}"), index, sysCfg.sta_ssid[index -1]);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_PASSWORD)) && (index > 0) && (index <= 2)) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.sta_pwd[0]))) {
        strlcpy(sysCfg.sta_pwd[index -1], (1 == payload) ? (1 == index) ? STA_PASS1 : STA_PASS2 : dataBuf, sizeof(sysCfg.sta_pwd[0]));
        sysCfg.sta_active = index -1;
        restartflag = 2;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_PASSWORD "%d\":\"%s\"}"), index, sysCfg.sta_pwd[index -1]);
    }
    else if (!grpflg && !strcasecmp_P(type, PSTR(D_CMND_HOSTNAME))) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.hostname))) {
        strlcpy(sysCfg.hostname, (1 == payload) ? WIFI_HOSTNAME : dataBuf, sizeof(sysCfg.hostname));
        if (strstr(sysCfg.hostname,"%")) {
          strlcpy(sysCfg.hostname, WIFI_HOSTNAME, sizeof(sysCfg.hostname));
        }
        restartflag = 2;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_HOSTNAME "\":\"%s\"}"), sysCfg.hostname);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_WIFICONFIG))) {
      if ((payload >= WIFI_RESTART) && (payload < MAX_WIFI_OPTION)) {
        sysCfg.sta_config = payload;
        wificheckflag = sysCfg.sta_config;
        snprintf_P(stemp1, sizeof(stemp1), wificfg[sysCfg.sta_config]);
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_WIFICONFIG "\":\"%s " D_SELECTED "\"}"), stemp1);
        if (WIFI_State() != WIFI_RESTART) {
//          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s after restart"), mqtt_data);
          restartflag = 2;
        }
      } else {
        snprintf_P(stemp1, sizeof(stemp1), wificfg[sysCfg.sta_config]);
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_WIFICONFIG "\":\"%d (%s)\"}"), sysCfg.sta_config, stemp1);
      }
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_FRIENDLYNAME)) && (index > 0) && (index <= 4)) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.friendlyname[0]))) {
        if (1 == index) {
          snprintf_P(stemp1, sizeof(stemp1), PSTR(FRIENDLY_NAME));
        } else {
          snprintf_P(stemp1, sizeof(stemp1), PSTR(FRIENDLY_NAME "%d"), index);
        }
        strlcpy(sysCfg.friendlyname[index -1], (1 == payload) ? stemp1 : dataBuf, sizeof(sysCfg.friendlyname[index -1]));
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_FRIENDLYNAME "%d\":\"%s\"}"), index, sysCfg.friendlyname[index -1]);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_SWITCHMODE)) && (index > 0) && (index <= 4)) {
      if ((payload >= 0) && (payload < MAX_SWITCH_OPTION)) {
        sysCfg.switchmode[index -1] = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_SWITCHMODE "%d\":%d}"), index, sysCfg.switchmode[index-1]);
    }
#ifdef USE_WEBSERVER
    else if (!strcasecmp_P(type, PSTR(D_CMND_WEBSERVER))) {
      if ((payload >= 0) && (payload <= 2)) {
        sysCfg.webserver = payload;
      }
      if (sysCfg.webserver) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_WEBSERVER "\":\"" D_ACTIVE_FOR " %s " D_ON_DEVICE " %s " D_WITH_IP_ADDRESS " %s\"}"),
          (2 == sysCfg.webserver) ? D_ADMIN : D_USER, Hostname, WiFi.localIP().toString().c_str());
      } else {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_WEBSERVER "\":\"%s\"}"), getStateText(0));
      }
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_WEBPASSWORD))) {
      if ((data_len > 0) && (data_len < sizeof(sysCfg.web_password))) {
        strlcpy(sysCfg.web_password, (!strcmp(dataBuf,"0")) ? "" : (1 == payload) ? WEB_PASSWORD : dataBuf, sizeof(sysCfg.web_password));
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_WEBPASSWORD "\":\"%s\"}"), sysCfg.web_password);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_WEBLOG))) {
      if ((payload >= LOG_LEVEL_NONE) && (payload <= LOG_LEVEL_ALL)) {
        sysCfg.weblog_level = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_WEBLOG "\":%d}"), sysCfg.weblog_level);
    }
#ifdef USE_EMULATION
    else if (!strcasecmp_P(type, PSTR(D_CMND_EMULATION))) {
      if ((payload >= 0) && (payload <= 2)) {
        sysCfg.flag.emulation = payload;
        restartflag = 2;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_EMULATION "\":%d}"), sysCfg.flag.emulation);
    }
#endif  // USE_EMULATION
#endif  // USE_WEBSERVER
    else if (!strcasecmp_P(type, PSTR(D_CMND_TELEPERIOD))) {
      if ((payload >= 0) && (payload < 3601)) {
        sysCfg.tele_period = (1 == payload) ? TELE_PERIOD : payload;
        if ((sysCfg.tele_period > 0) && (sysCfg.tele_period < 10)) {
          sysCfg.tele_period = 10;   // Do not allow periods < 10 seconds
        }
        tele_period = sysCfg.tele_period;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_TELEPERIOD "\":\"%d%s\"}"), sysCfg.tele_period, (sysCfg.flag.value_units) ? " " D_UNIT_SECOND : "");
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_RESTART))) {
      switch (payload) {
      case 1:
        restartflag = 2;
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_RESTART "\":\"" D_RESTARTING "\"}"));
        break;
      case 99:
        addLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_APPLICATION D_RESTARTING));
        ESP.restart();
        break;
      default:
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_RESTART "\":\"" D_ONE_TO_RESTART "\"}"));
      }
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_RESET))) {
      switch (payload) {
      case 1:
        restartflag = 211;
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_RESET "\":\"" D_RESET_AND_RESTARTING "\"}"));
        break;
      case 2:
        restartflag = 212;
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_RESET "\":\"" D_ERASE ", " D_RESET_AND_RESTARTING "\"}"));
        break;
      default:
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_RESET "\":\"" D_ONE_TO_RESET "\"}"));
      }
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_TIMEZONE))) {
      if ((data_len > 0) && (((payload >= -12) && (payload <= 12)) || (99 == payload))) {
        sysCfg.timezone = payload;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_TIMEZONE "\":%d}"), sysCfg.timezone);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_LEDPOWER))) {
      if ((payload >= 0) && (payload <= 2)) {
        sysCfg.ledstate &= 8;
        switch (payload) {
        case 0: // Off
        case 1: // On
          sysCfg.ledstate = payload << 3;
          break;
        case 2: // Toggle
          sysCfg.ledstate ^= 8;
          break;
        }
        blinks = 0;
        setLed(sysCfg.ledstate &8);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_LEDPOWER "\":\"%s\"}"), getStateText(bitRead(sysCfg.ledstate, 3)));
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_LEDSTATE))) {
      if ((payload >= 0) && (payload < MAX_LED_OPTION)) {
        sysCfg.ledstate = payload;
        if (!sysCfg.ledstate) {
          setLed(0);
        }
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_LEDSTATE "\":%d}"), sysCfg.ledstate);
    }
    else if (!strcasecmp_P(type, PSTR(D_CMND_CFGDUMP))) {
      CFG_Dump(dataBuf);
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_CFGDUMP "\":\"" D_DONE "\"}"));
    }
    else if (sysCfg.flag.mqtt_enabled && mqtt_command(grpflg, type, index, dataBuf, data_len, payload, payload16)) {
      // Serviced
    }
    else if (hlw_flg && hlw_command(type, index, dataBuf, data_len, payload)) {
      // Serviced
    }
    else if ((SONOFF_BRIDGE == sysCfg.module) && sb_command(type, index, dataBuf, data_len, payload)) {
      // Serviced
    }
#ifdef USE_I2C
    else if (i2c_flg && !strcasecmp_P(type, PSTR(D_CMND_I2CSCAN))) {
      i2c_scan(mqtt_data, sizeof(mqtt_data));
    }
#endif  // USE_I2C
#ifdef USE_IR_REMOTE
    else if ((pin[GPIO_IRSEND] < 99) && ir_send_command(type, index, dataBuf, data_len, payload)) {
      // Serviced
    }
#endif  // USE_IR_REMOTE
#ifdef DEBUG_THEO
    else if (!strcasecmp_P(type, PSTR(D_CMND_EXCEPTION))) {
      if (data_len > 0) {
        exception_tst(payload);
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_EXCEPTION "\":\"" D_DONE "\"}"));
    }
#endif  // DEBUG_THEO
    else {
      type = NULL;
    }
  }
  if (type == NULL) {
    blinks = 201;
    snprintf_P(topicBuf, sizeof(topicBuf), PSTR(D_COMMAND));
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_COMMAND "\":\"" D_UNKNOWN "\"}"));
    type = (char*)topicBuf;
  }
  if (mqtt_data[0] != '\0') {
    mqtt_publish_topic_P(5, type);
  }
  fallbacktopic = 0;
}

/********************************************************************************************/
////////////Nextioncode///////////
////////////////////////////////////////////////////////////////////////////////////////////////////
// Set the value of a Nextion component attribute
void setNextionAttr (String hmi_attribute, String hmi_value) {
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial.print(hmi_attribute);
  Serial.print("=");
  Serial.print(hmi_value);
  Serial.write(nextionSuffix, sizeof(nextionSuffix));
  Serial.flush();
//  debugPrintln("HMI OUT:  '" + hmi_attribute + "=" + hmi_value + "'");
  delay(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Get the value of a Nextion component attribute
void getNextionAttr (String hmi_attribute) {
  // This will only send the command to the panel requesting the attribute, the actual
  // return of that value will be handled by processNextionInput and placed into mqttGetSubtopic
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial.print("get " + hmi_attribute);
  Serial.write(nextionSuffix, sizeof(nextionSuffix));
  Serial.flush();
//  debugPrintln("HMI OUT:  'get " + hmi_attribute + "'");
  delay(1);
  processNextionInput();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Send a raw command to the Nextion panel
void sendNextionCmd (String nexcmd) {
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial.print(nexcmd);
  Serial.write(nextionSuffix, sizeof(nextionSuffix));
  Serial.flush();
//  debugPrintln("HMI OUT:  " + nexcmd);
  delay(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Send initialization commands to the Nextion panel at startup
void initializeNextion() {
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial.print("rest");
  Serial.write(nextionSuffix, sizeof(nextionSuffix));
  Serial.flush();
  delay(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle incoming serial data from the Nextion panel
void handleNextionInput() {
  // This will collect serial data from the panel and place it into the global buffer
  // nextionReturnBuffer[nextionReturnIndex] until it finds a string of 3 consecutive
  // 0xFF values or runs into the timeout
  bool nextionCommandIncoming = true; // we'll flip this to false when we receive 3 consecutive 0xFFs
  int nextionCommandTimeout = 1000; // timeout for receiving termination string in milliseconds
  int nextionTermByteCnt = 0; // counter for our 3 consecutive 0xFFs
  unsigned long nextionCommandTimer = millis(); // record current time for our timeout
  nextionReturnIndex = 0; // reset the global nextionReturnIndex back to zero
  String hmiDebug = "HMI IN: "; // assemble a string for debug output
  // Load the nextionBuffer until we receive a termination command or we hit our timeout
  while (nextionCommandIncoming && ((millis() - nextionCommandTimer) < nextionCommandTimeout)) {
    if (Serial.available()) {
      byte nextionCommandByte = Serial.read();
      delay(1);
      hmiDebug += (" 0x" + String(nextionCommandByte, HEX));
      // check to see if we have one of 3 consecutive 0xFF which indicates the end of a command
      if (nextionCommandByte == 0xFF) {
        nextionTermByteCnt++;
        if (nextionTermByteCnt >= 3) {
          nextionCommandIncoming = false;
        }
      }
      else {
        nextionTermByteCnt = 0;  // reset counter if a non-term byte was encountered
      }
      nextionReturnBuffer[nextionReturnIndex] = nextionCommandByte;
      nextionReturnIndex++;
    }
    else {
      delay (1);
    }
  }
//  debugPrintln(hmiDebug);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle incoming commands from the Nextion device
void processNextionInput() {
  // Command reference: https://www.itead.cc/wiki/Nextion_Instruction_Set#Format_of_Device_Return_Data
  // tl;dr, command byte, command data, 0xFF 0xFF 0xFF

  // Call handleNextionInput() to load nextionReturnBuffer[] and set nextionReturnIndex
  handleNextionInput();

  // Handle incoming touch command
  if (nextionReturnBuffer[0] == 0x65) {
    // 0x65+Page ID+Component ID+TouchEvent+End
    // Return this data when the touch event created by the user is pressed.
    // Definition of TouchEvent: Press Event 0x01, Release Event 0X00
    // Example: 0x65 0x00 0x02 0x01 0xFF 0xFF 0xFF
    // Meaning: Touch Event, Page 0, Object 2, Press
    String nextionPage = String(nextionReturnBuffer[1]);
    String nextionButtonID = String(nextionReturnBuffer[2]);
    byte nextionButtonAction = nextionReturnBuffer[3];

    if (nextionButtonAction == 0x01) {
//      debugPrintln("HMI IN:   [Button ON] 'p[" + nextionPage + "].b[" + nextionButtonID + "]'");
      String mqttButtonTopic = mqttStateTopic + "/p[" + nextionPage + "].b[" + nextionButtonID + "]";
//    debugPrintln("MQTT OUT: '" + mqttButtonTopic + "' : 'ON'");
      mqttClient.publish(mqttButtonTopic.c_str(), "ON");
    }
    if (nextionButtonAction == 0x00) {
//      debugPrintln("HMI IN:   [Button OFF] 'p[" + nextionPage + "].b[" + nextionButtonID + "]'");
      String mqttButtonTopic = mqttStateTopic + "/p[" + nextionPage + "].b[" + nextionButtonID + "]";
//      debugPrintln("MQTT OUT: '" + mqttButtonTopic + "' : 'OFF'");
      mqttClient.publish(mqttButtonTopic.c_str(), "OFF");
      // Now see if this object has a .val that might have been updated.
      // works for sliders, two-state buttons, etc, throws a 0x1A error for normal buttons
      // which we'll catch and ignore
      mqttGetSubtopic = "/p[" + nextionPage + "].b[" + nextionButtonID + "].val";
      getNextionAttr("p[" + nextionPage + "].b[" + nextionButtonID + "].val");
    }
  }
  // Handle incoming "sendme" page number
  else if (nextionReturnBuffer[0] == 0x66) {
    // 0x66+PageNum+End
    // Example: 0x66 0x02 0xFF 0xFF 0xFF
    // Meaning: page 2
    String nextionPage = String(nextionReturnBuffer[1]);
//    debugPrintln("HMI IN:   [sendme Page] '" + nextionPage + "'");
    String mqttPageTopic = mqttStateTopic + "/page";
//    debugPrintln("MQTT OUT: '" + mqttPageTopic + "' : '" + nextionPage + "'");
    mqttClient.publish(mqttPageTopic.c_str(), nextionPage.c_str());
  }
  // Handle touch coordinate data
  else if (nextionReturnBuffer[0] == 0x67) {
    // 0X67+Coordinate X High+Coordinate X Low+Coordinate Y High+Coordinate Y Low+TouchEvent+End
    // Example: 0X67 0X00 0X7A 0X00 0X1E 0X01 0XFF 0XFF 0XFF
    // Meaning: Coordinate (122,30), Touch Event: Press
    // issue Nextion command "sendxy=1" to enable this output
    uint16_t xCoord = nextionReturnBuffer[1];
    xCoord = xCoord * 256 + nextionReturnBuffer[2];
    uint16_t yCoord = nextionReturnBuffer[3];
    yCoord = yCoord * 256 + nextionReturnBuffer[4];
    String xyCoord = String(xCoord) + ',' + String(yCoord);
    byte nextionTouchAction = nextionReturnBuffer[5];
    if (nextionTouchAction == 0x01) {
//      debugPrintln("HMI IN:   [Touch ON] '" + xyCoord + "'");
      String mqttTouchTopic = mqttStateTopic + "/touchOn";
//      debugPrintln("MQTT OUT: '" + mqttTouchTopic + "' : '" + xyCoord + "'");
      mqttClient.publish(mqttTouchTopic.c_str(), xyCoord.c_str());
    }
    else if (nextionTouchAction == 0x00) {
//      debugPrintln("HMI IN:   [Touch OFF] '" + xyCoord + "'");
      String mqttTouchTopic = mqttStateTopic + "/touchOff";
//      debugPrintln("MQTT OUT: '" + mqttTouchTopic + "' : '" + xyCoord + "'");
      mqttClient.publish(mqttTouchTopic.c_str(), xyCoord.c_str());
    }
  }
  // Handle get string return
  else if (nextionReturnBuffer[0] == 0x70) {
    // 0x70+ASCII string+End
    // Example: 0x70 0x41 0x42 0x43 0x44 0x31 0x32 0x33 0x34 0xFF 0xFF 0xFF
    // Meaning: String data, ABCD1234
    String getString;
    // convert the data into a string
    for (int i = 1; i < nextionReturnIndex - 3; i++) {
      getString += (char)nextionReturnBuffer[i];
    }
//    debugPrintln("HMI IN:   [String Return] '" + getString + "'");
    // If there's no outstanding request for a value, publish to mqttStateTopic
    if (mqttGetSubtopic == "") {
//      debugPrintln("MQTT OUT: '" + mqttStateTopic + "' : '" + getString + "]");
      mqttClient.publish(mqttStateTopic.c_str(), getString.c_str());
    }
    // Otherwise, publish the to saved mqttGetSubtopic and then reset mqttGetSubtopic
    else {
      String mqttReturnTopic = mqttStateTopic + mqttGetSubtopic;
//      debugPrintln("MQTT OUT: '" + mqttReturnTopic + "' : '" + getString + "]");
      mqttClient.publish(mqttReturnTopic.c_str(), getString.c_str());
      mqttGetSubtopic = "";
    }
  }
  // Handle get int return
  else if (nextionReturnBuffer[0] == 0x71) {
    // 0x71+byte1+byte2+byte3+byte4+End (4 byte little endian)
    // Example: 0x71 0x7B 0x00 0x00 0x00 0xFF 0xFF 0xFF
    // Meaning: Integer data, 123
    unsigned long getInt = nextionReturnBuffer[4];
    getInt = getInt * 256 + nextionReturnBuffer[3];
    getInt = getInt * 256 + nextionReturnBuffer[2];
    getInt = getInt * 256 + nextionReturnBuffer[1];
    String getString = String(getInt);
//    debugPrintln("HMI IN:   [Int Return] '" + getString + "'");
    // If there's no outstanding request for a value, published to mqttStateTopic
    if (mqttGetSubtopic == "") {
      mqttClient.publish(mqttStateTopic.c_str(), getString.c_str());
    }
    // Otherwise, publish the to saved mqttGetSubtopic and then reset mqttGetSubtopic
    else {
      String mqttReturnTopic = mqttStateTopic + mqttGetSubtopic;
      mqttClient.publish(mqttReturnTopic.c_str(), getString.c_str());
      mqttGetSubtopic = "";
    }
  }
  // Catch 0x1A error, possibly from .val query against things that might not support that request
  if (nextionReturnBuffer[0] == 0x1A) {
    // 0x1A+End
    // ERROR: Variable name invalid
    // We'll be triggering this a lot due to requesting .val on every component that sends us a Touch Off
    // Just reset mqttGetSubtopic and move on with life.
    mqttGetSubtopic = "";
  }
}
////////////Nextioncodeends///////
/********************************************************************************************/
boolean send_button_power(byte key, byte device, byte state)
{
// key 0 = button_topic
// key 1 = switch_topic
// state 0 = off
// state 1 = on
// state 2 = toggle
// state 3 = hold
// state 9 = clear retain flag

  char stopic[TOPSZ];
  char scommand[CMDSZ];
  char stemp1[10];
  boolean result = false;

  char *key_topic = (key) ? sysCfg.switch_topic : sysCfg.button_topic;
  if (sysCfg.flag.mqtt_enabled && mqttClient.connected() && (strlen(key_topic) != 0) && strcmp(key_topic, "0")) {
    if (!key && (device > Maxdevice)) {
      device = 1;
    }

//    snprintf_P(stemp1, sizeof(stemp1), PSTR("%d"), device);
//    snprintf_P(scommand, sizeof(scommand), PSTR("POWER%s"), (key || (Maxdevice > 1)) ? stemp1 : "");
//    getTopic_P(stopic, 0, key_topic, scommand);

    getTopic_P(stopic, 0, key_topic, getPowerDevice(scommand, device, sizeof(scommand), key));

    if (9 == state) {
      mqtt_data[0] = '\0';
    } else {
      if ((!strcmp(sysCfg.mqtt_topic, key_topic) || !strcmp(sysCfg.mqtt_grptopic, key_topic)) && (2 == state)) {
        state = ~(power >> (device -1)) & 0x01;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), getStateText(state));
    }
#ifdef USE_DOMOTICZ
    if (!(domoticz_button(key, device, state, strlen(mqtt_data)))) {
      mqtt_publish_sec(stopic, (key) ? sysCfg.flag.mqtt_switch_retain : sysCfg.flag.mqtt_button_retain);
    }
#else
    mqtt_publish_sec(stopic, (key) ? sysCfg.flag.mqtt_switch_retain : sysCfg.flag.mqtt_button_retain);
#endif  // USE_DOMOTICZ
    result = true;
  }
  return result;
}

void do_cmnd_power(byte device, byte state)
{
// device  = Relay number 1 and up
// state 0 = Relay Off
// state 1 = Relay On (turn off after sysCfg.pulsetime * 100 mSec if enabled)
// state 2 = Toggle relay
// state 3 = Blink relay
// state 4 = Stop blinking relay
// state 6 = Relay Off and no publishPowerState
// state 7 = Relay On and no publishPowerState
// state 9 = Show power state

  uint8_t publishPower = 1;
  if ((6 == state) || (7 == state)) {
    state &= 1;
    publishPower = 0;
  }
  if ((device < 1) || (device > Maxdevice)) {
    device = 1;
  }
  byte mask = 0x01 << (device -1);
  pulse_timer[(device -1)&3] = 0;
  if (state <= 2) {
    if ((blink_mask & mask)) {
      blink_mask &= (0xFF ^ mask);  // Clear device mask
      mqtt_publishPowerBlinkState(device);
    }
    if (sysCfg.flag.interlock && !interlockmutex) {  // Clear all but masked relay
      interlockmutex = 1;
      for (byte i = 0; i < Maxdevice; i++) {
        byte imask = 0x01 << i;
        if ((power & imask) && (mask != imask)) {
          do_cmnd_power(i +1, 0);
        }
      }
      interlockmutex = 0;
    }
    switch (state) {
    case 0: { // Off
      power &= (0xFF ^ mask);
      break; }
    case 1: // On
      power |= mask;
      break;
    case 2: // Toggle
      power ^= mask;
    }
    setRelay(power);
#ifdef USE_DOMOTICZ
    domoticz_updatePowerState(device);
#endif  // USE_DOMOTICZ
    pulse_timer[(device -1)&3] = (power & mask) ? sysCfg.pulsetime[(device -1)&3] : 0;
  }
  else if (3 == state) { // Blink
    if (!(blink_mask & mask)) {
      blink_powersave = (blink_powersave & (0xFF ^ mask)) | (power & mask);  // Save state
      blink_power = (power >> (device -1))&1;  // Prep to Toggle
    }
    blink_timer = 1;
    blink_counter = ((!sysCfg.blinkcount) ? 64000 : (sysCfg.blinkcount *2)) +1;
    blink_mask |= mask;  // Set device mask
    mqtt_publishPowerBlinkState(device);
    return;
  }
  else if (4 == state) { // No Blink
    byte flag = (blink_mask & mask);
    blink_mask &= (0xFF ^ mask);  // Clear device mask
    mqtt_publishPowerBlinkState(device);
    if (flag) {
      do_cmnd_power(device, (blink_powersave >> (device -1))&1);  // Restore state
    }
    return;
  }
  if (publishPower) {
    mqtt_publishPowerState(device);
  }
}

void stop_all_power_blink()
{
  byte mask;

  for (byte i = 1; i <= Maxdevice; i++) {
    mask = 0x01 << (i -1);
    if (blink_mask & mask) {
      blink_mask &= (0xFF ^ mask);  // Clear device mask
      mqtt_publishPowerBlinkState(i);
      do_cmnd_power(i, (blink_powersave >> (i -1))&1);  // Restore state
    }
  }
}

void do_cmnd(char *cmnd)
{
  char stopic[CMDSZ];
  char svalue[INPUT_BUFFER_SIZE];
  char *start;
  char *token;

  token = strtok(cmnd, " ");
  if (token != NULL) {
    start = strrchr(token, '/');   // Skip possible cmnd/sonoff/ preamble
    if (start) {
      token = start +1;
    }
  }
  snprintf_P(stopic, sizeof(stopic), PSTR("/%s"), (token == NULL) ? "" : token);
  token = strtok(NULL, "");
  snprintf_P(svalue, sizeof(svalue), (token == NULL) ? "" : token);
  mqttDataCb(stopic, (byte*)svalue, strlen(svalue));
}

void publish_status(uint8_t payload)
{
  uint8_t option = 1;

  // Workaround MQTT - TCP/IP stack queueing when SUB_PREFIX = PUB_PREFIX
  if (!strcmp(sysCfg.mqtt_prefix[0],sysCfg.mqtt_prefix[1]) && (!payload)) {
    option++;
  }

  if ((!sysCfg.flag.mqtt_enabled) && (6 == payload)) {
    payload = 99;
  }
  if ((!hlw_flg) && ((8 == payload) || (9 == payload))) {
    payload = 99;
  }

  if ((0 == payload) || (99 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS "\":{\"" D_CMND_MODULE "\":%d, \"" D_CMND_FRIENDLYNAME "\":\"%s\", \"" D_CMND_TOPIC "\":\"%s\", \"" D_CMND_BUTTONTOPIC "\":\"%s\", \"" D_CMND_POWER "\":%d, \"" D_CMND_POWERONSTATE "\":%d, \"" D_CMND_LEDSTATE "\":%d, \"" D_CMND_SAVEDATA "\":%d, \"" D_SAVESTATE "\":%d, \"" D_CMND_BUTTONRETAIN "\":%d, \"" D_CMND_POWERRETAIN "\":%d}}"),
      sysCfg.module +1, sysCfg.friendlyname[0], sysCfg.mqtt_topic, sysCfg.button_topic, power, sysCfg.poweronstate, sysCfg.ledstate, sysCfg.savedata, sysCfg.flag.savestate, sysCfg.flag.mqtt_button_retain, sysCfg.flag.mqtt_power_retain);
    mqtt_publish_topic_P(option, PSTR(D_CMND_STATUS));
  }

  if ((0 == payload) || (1 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS1_PARAMETER "\":{\"" D_BAUDRATE "\":%d, \"" D_CMND_GROUPTOPIC "\":\"%s\", \"" D_CMND_OTAURL "\":\"%s\", \"" D_UPTIME "\":%d, \"" D_CMND_SLEEP "\":%d, \"" D_BOOTCOUNT "\":%d, \"" D_SAVECOUNT "\":%d, \"" D_SAVEADDRESS "\":\"%X\"}}"),
      Baudrate, sysCfg.mqtt_grptopic, sysCfg.otaUrl, uptime, sysCfg.sleep, sysCfg.bootcount, sysCfg.saveFlag, CFG_Address());
    mqtt_publish_topic_P(option, PSTR(D_CMND_STATUS "1"));
  }

  if ((0 == payload) || (2 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS2_FIRMWARE "\":{\"" D_VERSION "\":\"%s\", \"" D_BUILDDATETIME "\":\"%s\", \"" D_BOOTVERSION "\":%d, \"" D_COREVERSION "\":\"%s\", \"" D_SDKVERSION "\":\"%s\"}}"),
      Version, getBuildDateTime().c_str(), ESP.getBootVersion(), ESP.getCoreVersion().c_str(), ESP.getSdkVersion());
    mqtt_publish_topic_P(option, PSTR(D_CMND_STATUS "2"));
  }

  if ((0 == payload) || (3 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS3_LOGGING "\":{\"" D_CMND_SERIALLOG "\":%d, \"" D_CMND_WEBLOG "\":%d, \"" D_CMND_SYSLOG "\":%d, \"" D_CMND_LOGHOST "\":\"%s\", \"" D_CMND_LOGPORT "\":%d, \"" D_CMND_SSID "1\":\"%s\", \"" D_CMND_SSID "2\":\"%s\", \"" D_CMND_TELEPERIOD "\":%d, \"" D_CMND_SETOPTION "\":\"%08X\"}}"),
      sysCfg.seriallog_level, sysCfg.weblog_level, sysCfg.syslog_level, sysCfg.syslog_host, sysCfg.syslog_port, sysCfg.sta_ssid[0], sysCfg.sta_ssid[1], sysCfg.tele_period, sysCfg.flag.data);
    mqtt_publish_topic_P(option, PSTR(D_CMND_STATUS "3"));
  }

  if ((0 == payload) || (4 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS4_MEMORY "\":{\"" D_PROGRAMSIZE "\":%d, \"" D_FREEMEMORY "\":%d, \"" D_HEAPSIZE "\":%d, \"" D_PROGRAMFLASHSIZE "\":%d, \"" D_FLASHSIZE "\":%d, \"" D_FLASHMODE "\":%d}}"),
      ESP.getSketchSize()/1024, ESP.getFreeSketchSpace()/1024, ESP.getFreeHeap()/1024, ESP.getFlashChipSize()/1024, ESP.getFlashChipRealSize()/1024, ESP.getFlashChipMode());
    mqtt_publish_topic_P(option, PSTR(D_CMND_STATUS "4"));
  }

  if ((0 == payload) || (5 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS5_NETWORK "\":{\"" D_CMND_HOSTNAME "\":\"%s\", \"" D_CMND_IPADDRESS "\":\"%s\", \"" D_GATEWAY "\":\"%s\", \"" D_SUBNETMASK "\":\"%s\", \"" D_DNSSERVER "\":\"%s\", \"" D_MAC "\":\"%s\", \"" D_CMND_WEBSERVER "\":%d, \"" D_CMND_WIFICONFIG "\":%d}}"),
      Hostname, WiFi.localIP().toString().c_str(), IPAddress(sysCfg.ip_address[1]).toString().c_str(), IPAddress(sysCfg.ip_address[2]).toString().c_str(), IPAddress(sysCfg.ip_address[3]).toString().c_str(),
      WiFi.macAddress().c_str(), sysCfg.webserver, sysCfg.sta_config);
    mqtt_publish_topic_P(option, PSTR(D_CMND_STATUS "5"));
  }

  if (((0 == payload) || (6 == payload)) && sysCfg.flag.mqtt_enabled) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS6_MQTT "\":{\"" D_CMND_MQTTHOST "\":\"%s\", \"" D_CMND_MQTTPORT "\":%d, \"" D_CMND_MQTTCLIENT D_MASK "\":\"%s\", \"" D_CMND_MQTTCLIENT "\":\"%s\", \"" D_CMND_MQTTUSER "\":\"%s\", \"MAX_PACKET_SIZE\":%d, \"KEEPALIVE\":%d}}"),
      sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.mqtt_client, MQTTClient, sysCfg.mqtt_user, MQTT_MAX_PACKET_SIZE, MQTT_KEEPALIVE);
    mqtt_publish_topic_P(option, PSTR(D_CMND_STATUS "6"));
  }

  if ((0 == payload) || (7 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS7_TIME "\":{\"" D_UTC_TIME "\":\"%s\", \"" D_LOCAL_TIME "\":\"%s\", \"" D_STARTDST "\":\"%s\", \"" D_ENDDST "\":\"%s\", \"" D_CMND_TIMEZONE "\":%d}}"),
      rtc_time(0).c_str(), rtc_time(1).c_str(), rtc_time(2).c_str(), rtc_time(3).c_str(), sysCfg.timezone);
    mqtt_publish_topic_P(option, PSTR(D_CMND_STATUS "7"));
  }

  if (hlw_flg) {
    if ((0 == payload) || (8 == payload)) {
      hlw_mqttStatus();
      mqtt_publish_topic_P(option, PSTR(D_CMND_STATUS "8"));
    }

    if ((0 == payload) || (9 == payload)) {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS9_MARGIN "\":{\"" D_CMND_POWERLOW "\":%d, \"" D_CMND_POWERHIGH "\":%d, \"" D_CMND_VOLTAGELOW "\":%d, \"" D_CMND_VOLTAGEHIGH "\":%d, \"" D_CMND_CURRENTLOW "\":%d, \"" D_CMND_CURRENTHIGH "\":%d}}"),
        sysCfg.hlw_pmin, sysCfg.hlw_pmax, sysCfg.hlw_umin, sysCfg.hlw_umax, sysCfg.hlw_imin, sysCfg.hlw_imax);
      mqtt_publish_topic_P(option, PSTR(D_CMND_STATUS "9"));
    }
  }

  if ((0 == payload) || (10 == payload)) {
    uint8_t djson = 0;
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS10_SENSOR "\":"));
    sensors_mqttPresent(&djson);
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
    mqtt_publish_topic_P(option, PSTR(D_CMND_STATUS "10"));
  }

  if ((0 == payload) || (11 == payload)) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_STATUS D_STATUS11_STATUS "\":"));
    state_mqttPresent();
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
    mqtt_publish_topic_P(option, PSTR(D_CMND_STATUS "11"));
  }

}

void state_mqttPresent()
{
  char stemp1[16];

  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s{\"" D_TIME "\":\"%s\", \"" D_UPTIME "\":%d"), mqtt_data, getDateTime().c_str(), uptime);
#ifdef USE_ADC_VCC
  dtostrfd((double)ESP.getVcc()/1000, 3, stemp1);
  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s, \"" D_VCC "\":%s"), mqtt_data, stemp1);
#endif
  for (byte i = 0; i < Maxdevice; i++) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s, \"%s\":\"%s\""), mqtt_data, getPowerDevice(stemp1, i +1, sizeof(stemp1)), getStateText(bitRead(power, i)));
  }
  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s, \"" D_WIFI "\":{\"" D_AP "\":%d, \"" D_SSID "\":\"%s\", \"" D_RSSI "\":%d, \"" D_APMAC_ADDRESS "\":\"%s\"}}"),
    mqtt_data, sysCfg.sta_active +1, sysCfg.sta_ssid[sysCfg.sta_active], WIFI_getRSSIasQuality(WiFi.RSSI()), WiFi.BSSIDstr().c_str());
}

void sensors_mqttPresent(uint8_t* djson)
{
  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s{\"" D_TIME "\":\"%s\""), mqtt_data, getDateTime().c_str());
  for (byte i = 0; i < 4; i++) {
    if (pin[GPIO_SWT1 +i] < 99) {
      boolean swm = ((FOLLOW_INV == sysCfg.switchmode[i]) || (PUSHBUTTON_INV == sysCfg.switchmode[i]) || (PUSHBUTTONHOLD_INV == sysCfg.switchmode[i]));
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s, \"" D_SWITCH "%d\":\"%s\""), mqtt_data, i +1, getStateText(swm ^ lastwallswitch[i]));
      *djson = 1;
    }
  }
  counter_mqttPresent(djson);
#ifndef USE_ADC_VCC
  if (pin[GPIO_ADC0] < 99) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s, \"" D_ANALOG_INPUT0 "\":%d"), mqtt_data, getAdc0());
    *djson = 1;
  }
#endif
  if (SONOFF_SC == sysCfg.module) {
    sc_mqttPresent(djson);
  }
  if (pin[GPIO_DSB] < 99) {
#ifdef USE_DS18B20
    dsb_mqttPresent(djson);
#endif  // USE_DS18B20
#ifdef USE_DS18x20
    ds18x20_mqttPresent(djson);
#endif  // USE_DS18x20
  }
#ifdef USE_DHT
  if (dht_flg) {
    dht_mqttPresent(djson);
  }
#endif  // USE_DHT
#ifdef USE_I2C
  if (i2c_flg) {
#ifdef USE_SHT
    sht_mqttPresent(djson);
#endif  // USE_SHT
#ifdef USE_HTU
    htu_mqttPresent(djson);
#endif  // USE_HTU
#ifdef USE_BMP
    bmp_mqttPresent(djson);
#endif  // USE_BMP
#ifdef USE_BH1750
    bh1750_mqttPresent(djson);
#endif  // USE_BH1750
  }
#endif  // USE_I2C
  if (strstr_P(mqtt_data, PSTR(D_TEMPERATURE))) {
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s, \"" D_TEMPERATURE_UNIT "\":\"%c\""), mqtt_data, tempUnit());
  }
  snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s}"), mqtt_data);
}

/********************************************************************************************/

void every_second()
{
  if (blockgpio0) {
    blockgpio0--;
  }

  for (byte i = 0; i < MAX_PULSETIMERS; i++) {
    if (pulse_timer[i] > 111) {
      pulse_timer[i]--;
    }
  }

  if (seriallog_timer) {
    seriallog_timer--;
    if (!seriallog_timer) {
      if (seriallog_level) {
        addLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_APPLICATION D_SERIAL_LOGGING_DISABLED));
      }
      seriallog_level = 0;
    }
  }

  if (syslog_timer) {  // Restore syslog level
    syslog_timer--;
    if (!syslog_timer) {
      syslog_level = (sysCfg.flag.emulation) ? 0 : sysCfg.syslog_level;
      if (sysCfg.syslog_level) {
        addLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_APPLICATION D_SYSLOG_LOGGING_REENABLED));  // Might trigger disable again (on purpose)
      }
    }
  }

#ifdef USE_DOMOTICZ
  domoticz_mqttUpdate();
#endif  // USE_DOMOTICZ

  if (status_update_timer) {
    status_update_timer--;
    if (!status_update_timer) {
      for (byte i = 1; i <= Maxdevice; i++) {
        mqtt_publishPowerState(i);
      }
    }
  }

  if (sysCfg.tele_period) {
    tele_period++;
    if (tele_period == sysCfg.tele_period -1) {
      if (pin[GPIO_DSB] < 99) {
#ifdef USE_DS18B20
        dsb_readTempPrep();
#endif  // USE_DS18B20
#ifdef USE_DS18x20
        ds18x20_search();      // Check for changes in sensors number
        ds18x20_convert();     // Start Conversion, takes up to one second
#endif  // USE_DS18x20
      }
#ifdef USE_DHT
      if (dht_flg) {
        dht_readPrep();
      }
#endif  // USE_DHT
#ifdef USE_I2C
      if (i2c_flg) {
#ifdef USE_SHT
        sht_detect();
#endif  // USE_SHT
#ifdef USE_HTU
        htu_detect();
#endif  // USE_HTU
#ifdef USE_BMP
        bmp_detect();
#endif  // USE_BMP
#ifdef USE_BH1750
        bh1750_detect();
#endif  // USE_BH1750
      }
#endif  // USE_I2C
    }
    if (tele_period >= sysCfg.tele_period) {
      tele_period = 0;

      mqtt_data[0] = '\0';
      state_mqttPresent();
      mqtt_publish_topic_P(2, PSTR(D_RSLT_STATE));

      uint8_t djson = 0;
      mqtt_data[0] = '\0';
      sensors_mqttPresent(&djson);
      if (djson) {
        mqtt_publish_topic_P(2, PSTR(D_RSLT_SENSOR), sysCfg.flag.mqtt_sensor_retain);
      }

      if (hlw_flg) {
        hlw_mqttPresent(1);
      }
    }
  }

  if (hlw_flg) {
    hlw_margin_chk();
  }

  if ((2 == rtcTime.Minute) && uptime_flg) {
    uptime_flg = false;
    uptime++;
    snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_TIME "\":\"%s\", \"" D_UPTIME "\":%d}"), getDateTime().c_str(), uptime);
    mqtt_publish_topic_P(2, PSTR(D_RSLT_UPTIME));
  }
  if ((3 == rtcTime.Minute) && !uptime_flg) {
    uptime_flg = true;
  }
}

/*********************************************************************************************\
 * Button handler with single press only or multi-press and hold on all buttons
\*********************************************************************************************/

void button_handler()
{
  uint8_t button = NOT_PRESSED;
  uint8_t butt_present = 0;
  uint8_t flag = 0;
  char scmnd[20];

  for (byte i = 0; i < Maxdevice; i++) {
    button = NOT_PRESSED;
    butt_present = 0;

    if (!i && ((SONOFF_DUAL == sysCfg.module) || (CH4 == sysCfg.module))) {
      butt_present = 1;
      if (ButtonCode) {
        snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_BUTTON " " D_CODE " %04X"), ButtonCode);
        addLog(LOG_LEVEL_DEBUG);
        button = PRESSED;
        if (0xF500 == ButtonCode) {                     // Button hold
          holdbutton[i] = (sysCfg.param[P_HOLD_TIME] * (STATES / 10)) -1;
        }
        ButtonCode = 0;
      }
    } else {
      if ((pin[GPIO_KEY1 +i] < 99) && !blockgpio0) {
        butt_present = 1;
        button = digitalRead(pin[GPIO_KEY1 +i]);
      }
    }

    if (butt_present) {
      if (SONOFF_4CHPRO == sysCfg.module) {
        if (holdbutton[i]) {
          holdbutton[i]--;
        }
        flag = 0;
        if ((PRESSED == button) && (NOT_PRESSED == lastbutton[i])) {
          snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_BUTTON " %d " D_LEVEL_10), i +1);
          addLog(LOG_LEVEL_DEBUG);
          holdbutton[i] = STATES;
          flag = 1;
        }
        if ((NOT_PRESSED == button) && (PRESSED == lastbutton[i])) {
          snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_BUTTON " %d " D_LEVEL_01), i +1);
          addLog(LOG_LEVEL_DEBUG);
          if (!holdbutton[i]) {                           // Do not allow within 1 second
            flag = 1;
          }
        }
        if (flag) {
          if (!send_button_power(0, i +1, 2)) {           // Execute Toggle command via MQTT if ButtonTopic is set
            do_cmnd_power(i +1, 2);                       // Execute Toggle command internally
          }
        }
      } else {
        if ((PRESSED == button) && (NOT_PRESSED == lastbutton[i])) {
          if (sysCfg.flag.button_single) {                // Allow only single button press for immediate action
            snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_BUTTON " %d " D_IMMEDIATE), i +1);
            addLog(LOG_LEVEL_DEBUG);
            if (!send_button_power(0, i +1, 2)) {         // Execute Toggle command via MQTT if ButtonTopic is set
              do_cmnd_power(i +1, 2);                     // Execute Toggle command internally
            }
          } else {
            multipress[i] = (multiwindow[i]) ? multipress[i] +1 : 1;
            snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_BUTTON " %d " D_MULTI_PRESS " %d"), i +1, multipress[i]);
            addLog(LOG_LEVEL_DEBUG);
            multiwindow[i] = STATES /2;                   // 0.5 second multi press window
          }
          blinks = 201;
        }

        if (NOT_PRESSED == button) {
          holdbutton[i] = 0;
        } else {
          holdbutton[i]++;
          if (sysCfg.flag.button_single) {                // Allow only single button press for immediate action
            if (holdbutton[i] == sysCfg.param[P_HOLD_TIME] * (STATES / 10) * 4) {  // Button hold for four times longer
//              sysCfg.flag.button_single = 0;
              snprintf_P(scmnd, sizeof(scmnd), PSTR(D_CMND_SETOPTION "13 0"));  // Disable single press only
              do_cmnd(scmnd);
            }
          } else {
            if (holdbutton[i] == sysCfg.param[P_HOLD_TIME] * (STATES / 10)) {      // Button hold
              multipress[i] = 0;
              if (!sysCfg.flag.button_restrict) {         // No button restriction
                snprintf_P(scmnd, sizeof(scmnd), PSTR(D_CMND_RESET " 1"));
                do_cmnd(scmnd);
              } else {
                send_button_power(0, i +1, 3);            // Execute Hold command via MQTT if ButtonTopic is set
              }
            }
          }
        }

        if (!sysCfg.flag.button_single) {                 // Allow multi-press
          if (multiwindow[i]) {
            multiwindow[i]--;
          } else {
            if (!restartflag && !holdbutton[i] && (multipress[i] > 0) && (multipress[i] < MAX_BUTTON_COMMANDS +3)) {
              flag = 0;
              if (multipress[i] < 3) {                    // Single or Double press
                if ((SONOFF_DUAL == sysCfg.module) || (CH4 == sysCfg.module)) {
                  flag = 1;
                } else  {
                  flag = (sysCfg.flag.button_swap +1 == multipress[i]);
                  multipress[i] = 1;
                }
              }
              if (flag && send_button_power(0, i + multipress[i], 2)) {  // Execute Toggle command via MQTT if ButtonTopic is set
                // Success
              } else {
                if (multipress[i] < 3) {                  // Single or Double press
                  if (WIFI_State()) {                     // WPSconfig, Smartconfig or Wifimanager active
                    restartflag = 1;
                  } else {
                    do_cmnd_power(i + multipress[i], 2);  // Execute Toggle command internally
                  }
                } else {                                  // 3 - 7 press
                  if (!sysCfg.flag.button_restrict) {
                    snprintf_P(scmnd, sizeof(scmnd), commands[multipress[i] -3]);
                    do_cmnd(scmnd);
                  }
                }
              }
              multipress[i] = 0;
            }
          }
        }
      }
    }
    lastbutton[i] = button;
  }
}

/*********************************************************************************************\
 * Switch handler
\*********************************************************************************************/

void switch_handler()
{
  uint8_t button = NOT_PRESSED;
  uint8_t switchflag;

  for (byte i = 0; i < 4; i++) {
    if (pin[GPIO_SWT1 +i] < 99) {

      if (holdwallswitch[i]) {
        holdwallswitch[i]--;
        if (0 == holdwallswitch[i]) {
          send_button_power(1, i +1, 3);         // Execute command via MQTT
        }
      }

      button = digitalRead(pin[GPIO_SWT1 +i]);
      if (button != lastwallswitch[i]) {
        switchflag = 3;
        switch (sysCfg.switchmode[i]) {
        case TOGGLE:
          switchflag = 2;                        // Toggle
          break;
        case FOLLOW:
          switchflag = button & 0x01;    // Follow wall switch state
          break;
        case FOLLOW_INV:
          switchflag = ~button & 0x01;   // Follow inverted wall switch state
          break;
        case PUSHBUTTON:
          if ((PRESSED == button) && (NOT_PRESSED == lastwallswitch[i])) {
            switchflag = 2;              // Toggle with pushbutton to Gnd
          }
          break;
        case PUSHBUTTON_INV:
          if ((NOT_PRESSED == button) && (PRESSED == lastwallswitch[i])) {
            switchflag = 2;              // Toggle with releasing pushbutton from Gnd
          }
          break;
        case PUSHBUTTONHOLD:
          if ((PRESSED == button) && (NOT_PRESSED == lastwallswitch[i])) {
            holdwallswitch[i] = sysCfg.param[P_HOLD_TIME] * (STATES / 10);
          }
          if ((NOT_PRESSED == button) && (PRESSED == lastwallswitch[i]) && (holdwallswitch[i])) {
            holdwallswitch[i] = 0;
            switchflag = 2;              // Toggle with pushbutton to Gnd
          }
          break;
        case PUSHBUTTONHOLD_INV:
          if ((NOT_PRESSED == button) && (PRESSED == lastwallswitch[i])) {
            holdwallswitch[i] = sysCfg.param[P_HOLD_TIME] * (STATES / 10);
          }
          if ((PRESSED == button) && (NOT_PRESSED == lastwallswitch[i]) && (holdwallswitch[i])) {
            holdwallswitch[i] = 0;
            switchflag = 2;             // Toggle with pushbutton to Gnd
          }
          break;
        }

        if (switchflag < 3) {
          if (!send_button_power(1, i +1, switchflag)) {  // Execute command via MQTT
            do_cmnd_power(i +1, switchflag);              // Execute command internally (if i < Maxdevice)
          }
        }

        lastwallswitch[i] = button;
      }
    }
  }
}

/*********************************************************************************************\
 * State loop
\*********************************************************************************************/

void stateloop()
{
  uint8_t power_now;

  timerxs = millis() + (1000 / STATES);
  state++;

/*-------------------------------------------------------------------------------------------*\
 * Every second
\*-------------------------------------------------------------------------------------------*/

  if (STATES == state) {
    state = 0;
    every_second();
  }

/*-------------------------------------------------------------------------------------------*\
 * Every 0.1 second
\*-------------------------------------------------------------------------------------------*/

  if (!(state % (STATES/10))) {

    if (mqtt_cmnd_publish) {
      mqtt_cmnd_publish--;  // Clean up
    }

    if (latching_relay_pulse) {
      latching_relay_pulse--;
      if (!latching_relay_pulse) {
        setLatchingRelay(0, 0);
      }
    }

    for (byte i = 0; i < MAX_PULSETIMERS; i++) {
      if ((pulse_timer[i] > 0) && (pulse_timer[i] < 112)) {
        pulse_timer[i]--;
        if (!pulse_timer[i]) {
          do_cmnd_power(i +1, 0);
        }
      }
    }

    if (blink_mask) {
      blink_timer--;
      if (!blink_timer) {
        blink_timer = sysCfg.blinktime;
        blink_counter--;
        if (!blink_counter) {
          stop_all_power_blink();
        } else {
          blink_power ^= 1;
          power_now = (power & (0xFF ^ blink_mask)) | ((blink_power) ? blink_mask : 0);
          setRelay(power_now);
        }
      }
    }

    // Backlog
    if (blogdelay) {
      blogdelay--;
    }
    if ((blogptr != blogidx) && !blogdelay && !blogmutex) {
      blogmutex = 1;
      do_cmnd((char*)Backlog[blogptr].c_str());
      blogmutex = 0;
      blogptr++;
/*
    if (blogptr >= MAX_BACKLOG) {
      blogptr = 0;
    }
*/
      blogptr &= 0xF;
    }
  }

/*-------------------------------------------------------------------------------------------*\
 * Every 0.05 second
\*-------------------------------------------------------------------------------------------*/

  button_handler();
  switch_handler();

  if (sfl_flg) {  // Sonoff B1, AiLight, Sonoff led or BN-SZ01
    sl_animate();
  }

/*-------------------------------------------------------------------------------------------*\
 * Every 0.2 second
\*-------------------------------------------------------------------------------------------*/

  if (!(state % ((STATES/10)*2))) {
    if (blinks || restartflag || otaflag) {
      if (restartflag || otaflag) {
        blinkstate = 1;   // Stay lit
      } else {
        blinkstate ^= 1;  // Blink
      }
      if ((!(sysCfg.ledstate &0x08)) && ((sysCfg.ledstate &0x06) || (blinks > 200) || (blinkstate))) {
        setLed(blinkstate);
      }
      if (!blinkstate) {
        blinks--;
        if (200 == blinks) {
          blinks = 0;
        }
      }
    } else {
      if (sysCfg.ledstate &0x01) {
        boolean tstate = power;
        if ((SONOFF_TOUCH == sysCfg.module) || (SONOFF_T11 == sysCfg.module) || (SONOFF_T12 == sysCfg.module) || (SONOFF_T13 == sysCfg.module)) {
          tstate = (!power) ? 1 : 0;
        }
        setLed(tstate);
      }
    }
  }

/*-------------------------------------------------------------------------------------------*\
 * Every second at 0.2 second interval
\*-------------------------------------------------------------------------------------------*/

  switch (state) {
  case (STATES/10)*2:
    if (otaflag && (blogptr == blogidx)) {
      otaflag--;
      if (2 == otaflag) {
        otaretry = OTA_ATTEMPTS;
        ESPhttpUpdate.rebootOnUpdate(false);
        CFG_Save(1);  // Free flash for OTA update
      }
      if (otaflag <= 0) {
#ifdef USE_WEBSERVER
        if (sysCfg.webserver) {
          stopWebserver();
        }
#endif  // USE_WEBSERVER
        otaflag = 92;
        otaok = 0;
        otaretry--;
        if (otaretry) {
//          snprintf_P(log_data, sizeof(log_data), PSTR("OTA: Attempt %d"), OTA_ATTEMPTS - otaretry);
//          addLog(LOG_LEVEL_INFO);
          otaok = (HTTP_UPDATE_FAILED != ESPhttpUpdate.update(sysCfg.otaUrl));
          if (!otaok) {
            otaflag = 2;
          }
        }
      }
      if (90 == otaflag) {     // Allow MQTT to reconnect
        otaflag = 0;
        if (otaok) {
          setFlashModeDout();  // Force DOUT for both ESP8266 and ESP8285
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR(D_SUCCESSFUL ". " D_RESTARTING));
        } else {
          snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR(D_FAILED " %s"), ESPhttpUpdate.getLastErrorString().c_str());
        }
        restartflag = 2;       // Restart anyway to keep memory clean webserver
        mqtt_publish_topic_P(1, PSTR(D_CMND_UPGRADE));
      }
    }
    break;
  case (STATES/10)*4:
    if (rtc_midnight_now()) {
      counter_savestate();
    }
    if (savedatacounter && (blogptr == blogidx)) {
      savedatacounter--;
      if (savedatacounter <= 0) {
        if (sysCfg.flag.savestate) {
          byte mask = 0xFF;
          for (byte i = 0; i < MAX_PULSETIMERS; i++) {
            if ((sysCfg.pulsetime[i] > 0) && (sysCfg.pulsetime[i] < 30)) {
              mask &= ~(1 << i);
            }
          }
          if (!((sysCfg.power &mask) == (power &mask))) {
            sysCfg.power = power;
          }
        }
        CFG_Save(0);
        savedatacounter = sysCfg.savedata;
      }
    }
    if (restartflag && (blogptr == blogidx)) {
      if (211 == restartflag) {
        CFG_Default();
        restartflag = 2;
      }
      if (212 == restartflag) {
        CFG_Erase();
        CFG_Default();
        restartflag = 2;
      }
      if (sysCfg.flag.savestate) {
        sysCfg.power = power;
      }
      if (hlw_flg) {
        hlw_savestate();
      }
      counter_savestate();
      CFG_Save(0);
      restartflag--;
      if (restartflag <= 0) {
        addLog_P(LOG_LEVEL_INFO, PSTR(D_LOG_APPLICATION D_RESTARTING));
        ESP.restart();
      }
    }
    break;
  case (STATES/10)*6:
    WIFI_Check(wificheckflag);
    wificheckflag = WIFI_RESTART;
    break;
  case (STATES/10)*8:
    if (WL_CONNECTED == WiFi.status()) {
      if (sysCfg.flag.mqtt_enabled) {
        if (!mqttClient.connected()) {
          if (!mqttcounter) {
            mqtt_reconnect();
          } else {
            mqttcounter--;
          }
        }
      } else {
        if (!mqttcounter) {
          mqtt_reconnect();
        }
      }
    }
    break;
  }
}

/********************************************************************************************/

void serial()
{
  ////////////Nextioncode///////////
  // Process user input from HMI
	processNextionInput();
  ////////////Nextioncodeends///////
  while (Serial.available()) {
    yield();
    SerialInByte = Serial.read();

/*-------------------------------------------------------------------------------------------*\
 * Sonoff dual 19200 baud serial interface
\*-------------------------------------------------------------------------------------------*/

    if (Hexcode) {
      Hexcode--;
      if (Hexcode) {
        ButtonCode = (ButtonCode << 8) | SerialInByte;
        SerialInByte = 0;
      } else {
        if (SerialInByte != 0xA1) {
          ButtonCode = 0;                    // 0xA1 - End of Sonoff dual button code
        }
      }
    }
    if (0xA0 == SerialInByte) {              // 0xA0 - Start of Sonoff dual button code
      SerialInByte = 0;
      ButtonCode = 0;
      Hexcode = 3;
    }

/*-------------------------------------------------------------------------------------------*\
 * Sonoff bridge 19200 baud serial interface
\*-------------------------------------------------------------------------------------------*/

    if (sb_serial()) {
      SerialInByteCounter = 0;
      Serial.flush();
      return;
    }

/*-------------------------------------------------------------------------------------------*/

    if (SerialInByte > 127) {                // binary data...
      SerialInByteCounter = 0;
      Serial.flush();
      return;
    }
    if (isprint(SerialInByte)) {
      if (SerialInByteCounter < INPUT_BUFFER_SIZE) {  // add char to string if it still fits
        serialInBuf[SerialInByteCounter++] = SerialInByte;
      } else {
        SerialInByteCounter = 0;
      }
    }

/*-------------------------------------------------------------------------------------------*\
 * Sonoff SC 19200 baud serial interface
\*-------------------------------------------------------------------------------------------*/

    if (SerialInByte == '\x1B') {            // Sonoff SC status from ATMEGA328P
      serialInBuf[SerialInByteCounter] = 0;  // serial data completed
      sc_rcvstat(serialInBuf);
      SerialInByteCounter = 0;
      Serial.flush();
      return;
    }

/*-------------------------------------------------------------------------------------------*/

    else if (SerialInByte == '\n') {
      serialInBuf[SerialInByteCounter] = 0;  // serial data completed
      seriallog_level = (sysCfg.seriallog_level < LOG_LEVEL_INFO) ? LOG_LEVEL_INFO : sysCfg.seriallog_level;
      snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_COMMAND "%s"), serialInBuf);
      addLog(LOG_LEVEL_INFO);
      do_cmnd(serialInBuf);
      SerialInByteCounter = 0;
      Serial.flush();
      return;
    }
  }
}

/********************************************************************************************/

void GPIO_init()
{
  uint8_t mpin;
  mytmplt def_module;

  if (!sysCfg.module || (sysCfg.module >= MAXMODULE)) {
    sysCfg.module = MODULE;
  }

  memcpy_P(&def_module, &modules[sysCfg.module], sizeof(def_module));
//  sysCfg.my_module.flag = def_module.flag;
  strlcpy(my_module.name, def_module.name, sizeof(my_module.name));
  for (byte i = 0; i < MAX_GPIO_PIN; i++) {
    if (sysCfg.my_module.gp.io[i] > GPIO_NONE) {
      my_module.gp.io[i] = sysCfg.my_module.gp.io[i];
    }
    if ((def_module.gp.io[i] > GPIO_NONE) && (def_module.gp.io[i] < GPIO_USER)) {
      my_module.gp.io[i] = def_module.gp.io[i];
    }
  }

  for (byte i = 0; i < GPIO_MAX; i++) {
    pin[i] = 99;
  }
  for (byte i = 0; i < MAX_GPIO_PIN; i++) {
    mpin = my_module.gp.io[i];

//  snprintf_P(log_data, sizeof(log_data), PSTR("DBG: gpio pin %d, mpin %d"), i, mpin);
//  addLog(LOG_LEVEL_DEBUG);

    if (mpin) {
      if ((mpin >= GPIO_REL1_INV) && (mpin <= GPIO_REL4_INV)) {
        rel_inverted[mpin - GPIO_REL1_INV] = 1;
        mpin -= 4;
      }
      else if ((mpin >= GPIO_LED1_INV) && (mpin <= GPIO_LED4_INV)) {
        led_inverted[mpin - GPIO_LED1_INV] = 1;
        mpin -= 4;
      }
#ifdef USE_DHT
      else if ((mpin >= GPIO_DHT11) && (mpin <= GPIO_DHT22)) {
        if (dht_setup(i, mpin)) {
          dht_flg = 1;
          mpin = GPIO_DHT11;
        } else {
          mpin = 0;
        }
      }
#endif  // USE_DHT
    }
    if (mpin) {
      pin[mpin] = i;
    }
  }

  if (2 == pin[GPIO_TXD]) {
    Serial.set_tx(2);
  }

  analogWriteRange(PWM_RANGE);  // Default is 1023 (Arduino.h)
  analogWriteFreq(PWM_FREQ);    // Default is 1000 (core_esp8266_wiring_pwm.c)

  Maxdevice = 1;
  if (SONOFF_BRIDGE == sysCfg.module) {
    Baudrate = 19200;
  }
  if (SONOFF_DUAL == sysCfg.module) {
    Maxdevice = 2;
    Baudrate = 19200;
  }
  else if (CH4 == sysCfg.module) {
    Maxdevice = 4;
    Baudrate = 19200;
  }
  else if (SONOFF_SC == sysCfg.module) {
    Maxdevice = 0;
    Baudrate = 19200;
  }
  else if (SONOFF_BN == sysCfg.module) {   // Single color led (White)
    sfl_flg = 1;
  }
  else if (SONOFF_LED == sysCfg.module) {  // Dual color led (White warm and cold)
    sfl_flg = 2;
  }
  else if (AILIGHT == sysCfg.module) {     // RGBW led
    sfl_flg = 4;
  }
  else if (SONOFF_B1 == sysCfg.module) {   // RGBWC led
    sfl_flg = 5;
  }
  else {
    Maxdevice = 0;
    for (byte i = 0; i < 4; i++) {
      if (pin[GPIO_REL1 +i] < 99) {
        pinMode(pin[GPIO_REL1 +i], OUTPUT);
        Maxdevice++;
      }
//      if (pin[GPIO_KEY1 +i] < 99) {
//        pinMode(pin[GPIO_KEY1 +i], (16 == pin[GPIO_KEY1 +i]) ? INPUT_PULLDOWN_16 : INPUT_PULLUP);
//      }
    }
  }
  for (byte i = 0; i < 4; i++) {
    if (pin[GPIO_KEY1 +i] < 99) {
      pinMode(pin[GPIO_KEY1 +i], (16 == pin[GPIO_KEY1 +i]) ? INPUT_PULLDOWN_16 : INPUT_PULLUP);
    }
    if (pin[GPIO_LED1 +i] < 99) {
      pinMode(pin[GPIO_LED1 +i], OUTPUT);
      digitalWrite(pin[GPIO_LED1 +i], led_inverted[i]);
    }
    if (pin[GPIO_SWT1 +i] < 99) {
      pinMode(pin[GPIO_SWT1 +i], (16 == pin[GPIO_SWT1 +i]) ? INPUT_PULLDOWN_16 :INPUT_PULLUP);
      lastwallswitch[i] = digitalRead(pin[GPIO_SWT1 +i]);  // set global now so doesn't change the saved power state on first switch check
    }
  }

#ifdef USE_WS2812
  if (!sfl_flg && (pin[GPIO_WS2812] < 99)) {
    Maxdevice++;
    sfl_flg = 3;
  }
#endif  // USE_WS2812
  if (sfl_flg) {                // Sonoff B1, AiLight, Sonoff Led or BN-SZ01, WS2812
    if (sfl_flg < 3) {
      pwm_idxoffset = sfl_flg;  // 1 for BN-SZ01, 2 for Sonoff Led
    }
    sl_init();
  }
  for (byte i = pwm_idxoffset; i < 5; i++) {
    if (pin[GPIO_PWM1 +i] < 99) {
      pwm_flg = 1;
      pinMode(pin[GPIO_PWM1 +i], OUTPUT);
      analogWrite(pin[GPIO_PWM1 +i], sysCfg.pwmvalue[i]);
    }
  }

  if (EXS_RELAY == sysCfg.module) {
    setLatchingRelay(0,2);
    setLatchingRelay(1,2);
  }
  setLed(sysCfg.ledstate &8);

#ifdef USE_IR_REMOTE
  if (pin[GPIO_IRSEND] < 99) {
    ir_send_init();
  }
#endif // USE_IR_REMOTE

  counter_init();

  hlw_flg = ((pin[GPIO_HLW_SEL] < 99) && (pin[GPIO_HLW_CF1] < 99) && (pin[GPIO_HLW_CF] < 99));
  if (hlw_flg) {
    hlw_init();
  }

#ifdef USE_DHT
  if (dht_flg) {
    dht_init();
  }
#endif  // USE_DHT

#ifdef USE_DS18x20
  if (pin[GPIO_DSB] < 99) {
    ds18x20_init();
  }
#endif  // USE_DS18x20

#ifdef USE_I2C
  i2c_flg = ((pin[GPIO_I2C_SCL] < 99) && (pin[GPIO_I2C_SDA] < 99));
  if (i2c_flg) {
    Wire.begin(pin[GPIO_I2C_SDA], pin[GPIO_I2C_SCL]);
  }
#endif  // USE_I2C
}

extern "C" {
extern struct rst_info resetInfo;
}

void setup()
{
  byte idx;

  Serial.begin(Baudrate);
  delay(10);
  Serial.println();
  seriallog_level = LOG_LEVEL_INFO;  // Allow specific serial messages until config loaded


  snprintf_P(Version, sizeof(Version), PSTR("%d.%d.%d"), VERSION >> 24 & 0xff, VERSION >> 16 & 0xff, VERSION >> 8 & 0xff);
  if (VERSION & 0x1f) {
    idx = strlen(Version);
    Version[idx] = 96 + (VERSION & 0x1f);
    Version[idx +1] = 0;
  }
  CFG_Load();
  CFG_Delta();

  osw_init();

  seriallog_level = sysCfg.seriallog_level;
  seriallog_timer = SERIALLOG_TIMER;
#ifndef USE_EMULATION
  sysCfg.flag.emulation = 0;
#endif  // USE_EMULATION
  syslog_level = (sysCfg.flag.emulation) ? 0 : sysCfg.syslog_level;
  stop_flash_rotate = sysCfg.flag.stop_flash_rotate;
  savedatacounter = sysCfg.savedata;
  sleep = sysCfg.sleep;

  sysCfg.bootcount++;
  snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_BOOT_COUNT " %d"), sysCfg.bootcount);
  addLog(LOG_LEVEL_DEBUG);

  GPIO_init();
  ////////////Nextioncode///////////
  // initialize display
  initializeNextion();
  ////////////Nextioncodeends///////
  if (Serial.baudRate() != Baudrate) {
    if (seriallog_level) {
      snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_SET_BAUDRATE_TO " %d"), Baudrate);
      addLog(LOG_LEVEL_INFO);
    }
    delay(100);
    Serial.flush();
    Serial.begin(Baudrate);
    delay(10);
    Serial.println();
  }

  if (strstr(sysCfg.hostname, "%")) {
    strlcpy(sysCfg.hostname, WIFI_HOSTNAME, sizeof(sysCfg.hostname));
    snprintf_P(Hostname, sizeof(Hostname)-1, sysCfg.hostname, sysCfg.mqtt_topic, ESP.getChipId() & 0x1FFF);
  } else {
    snprintf_P(Hostname, sizeof(Hostname)-1, sysCfg.hostname);
  }
  WIFI_Connect();

  getClient(MQTTClient, sysCfg.mqtt_client, sizeof(MQTTClient));

  if (MOTOR == sysCfg.module) {
    sysCfg.poweronstate = 1;  // Needs always on else in limbo!
  }
  if (4 == sysCfg.poweronstate) {  // Allways on
    setRelay(power);
  } else {
    if ((resetInfo.reason == REASON_DEFAULT_RST) || (resetInfo.reason == REASON_EXT_SYS_RST)) {
      switch (sysCfg.poweronstate) {
      case 0:  // All off
        power = 0;
        setRelay(power);
        break;
      case 1:  // All on
        power = (1 << Maxdevice) -1;
        setRelay(power);
        break;
      case 2:  // All saved state toggle
        power = sysCfg.power & ((1 << Maxdevice) -1) ^ 0xFF;
        if (sysCfg.flag.savestate) {
          setRelay(power);
        }
        break;
      case 3:  // All saved state
        power = sysCfg.power & ((1 << Maxdevice) -1);
        if (sysCfg.flag.savestate) {
          setRelay(power);
        }
        break;
      }
    } else {
      power = sysCfg.power & ((1 << Maxdevice) -1);
      if (sysCfg.flag.savestate) {
        setRelay(power);
      }
    }
  }

  // Issue #526
  for (byte i = 0; i < Maxdevice; i++) {
    if ((pin[GPIO_REL1 +i] < 99) && (digitalRead(pin[GPIO_REL1 +i]))) {
      bitSet(power, i);
      pulse_timer[i] = sysCfg.pulsetime[i];
    }
  }

  blink_powersave = power;

  if (SONOFF_SC == sysCfg.module) {
    sc_init();
  }

  rtc_init();

  snprintf_P(log_data, sizeof(log_data), PSTR(D_PROJECT " %s %s (" D_CMND_TOPIC " %s, " D_FALLBACK " %s, " D_CMND_GROUPTOPIC " %s) " D_VERSION " %s"),
    PROJECT, sysCfg.friendlyname[0], sysCfg.mqtt_topic, MQTTClient, sysCfg.mqtt_grptopic, Version);
  addLog(LOG_LEVEL_INFO);
}

void loop()
{
  osw_loop();

#ifdef USE_WEBSERVER
  pollDnsWeb();
#endif  // USE_WEBSERVER

#ifdef USE_EMULATION
  if (sysCfg.flag.emulation) {
    pollUDP();
  }
#endif  // USE_EMULATION

  if (millis() >= timerxs) {
    stateloop();
  }
  if (sysCfg.flag.mqtt_enabled) {
    mqttClient.loop();
  }
  if (Serial.available()){
    serial();
  }

//  yield();     // yield == delay(0), delay contains yield, auto yield in loop
  delay(sleep);  // https://github.com/esp8266/Arduino/issues/2021
}
