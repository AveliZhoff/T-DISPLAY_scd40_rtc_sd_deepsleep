#define DEVICE_NUM 1 // Номер девайса.
#define REFRESH_INTERVAL 600 // Интервал отправки данных с датчика.
#define REFRESH_INTERVAL_WHEN_CHARGER_ON 60 // Интервал отправки данных с датчика или с SD карты при включенной зарядке.
#define MAX_MESUREMENT_SEND_AT_A_TIME 12 // Количество показаний, считываемых с карты за раз.

// Button
// #define BUTTON_PIN_LEFT 0
#define BUTTON_PIN_RIGHT 35

// Display
#include <SPI.h>
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
byte backLightPort = 4;
int max_x = 240;
int max_y = 135;
int x;
int y;
boolean keyWakeup = false;
RTC_DATA_ATTR boolean displayState = false;
String fileDisplayState = "displayState";

// Глубокий сон
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
RTC_DATA_ATTR int TIME_TO_SLEEP = REFRESH_INTERVAL;  /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR unsigned int bootCount = 0;

// WIFI
// Credential
#include "WiFiCredentials.h"
#include <WiFiMulti.h>
#include "ip.cpp"
WiFiMulti WiFiMulti;

#define NUMITEMS(arr) (sizeof(arr) / sizeof((arr)[0]))
byte WiFiExitCode = true;
String WiFiAP = "NONE";
String WiFiIP = "NONE";
String hostname;
int checkIntrval = 1;
int checkTimes = 10000;

// Voltage mesurement
const uint8_t ADC_PIN = 34;
const uint8_t ADC_EN = 14;
RTC_DATA_ATTR float usbVoltage = 0;
RTC_DATA_ATTR float batteryVoltage = 0;
boolean ADC_EN_State = LOW;
boolean isChargerConnected;
RTC_DATA_ATTR boolean isChargerConnectedPrev = false;
const float usbVoltageMin = 4;

// I2C 
#include <Wire.h>

// SCD40
#include "SparkFun_SCD4x_Arduino_Library.h"
SCD4x SCD40;
RTC_DATA_ATTR int CO2;
RTC_DATA_ATTR float TEMP;
RTC_DATA_ATTR float HUM;
const float kAvarage = 0.5;
boolean isSCD40ready = true;
RTC_DATA_ATTR unsigned long SCD40_lastSendTime = 4294967295UL;

// NTP
const long  gmtOffset = 3;
const long  gmtOffset_sec = gmtOffset*60*60;
const int   daylightOffset_sec = 0;
boolean isWriteToFile = false;
const char* ntpServers[] = {
  "192.168.1.252",
  "192.168.1.254",
  "pool.ntp.org",
  "time.nist.gov",
  "time.google.com",
  "ru.pool.ntp.org",
  "de.pool.ntp.org",
  "0.openwrt.pool.ntp.org",
  "1.openwrt.pool.ntp.org",
  "2.openwrt.pool.ntp.org",
  "3.openwrt.pool.ntp.org",
};

// REALTIME
#include "RTClib.h"
#include "time.h"
RTC_DS3231 rtc;
DateTime now ;
char datestr[11];
char timestr[9];

// SD MISO: 26, MOSI: 15, SCK: 13, CS: 2
#include "SD.h"
SPIClass spiSD(HSPI);
#define SD_CS       2
#define SDSPEED 1000000 //HZ
uint32_t cardSize;
String catalogNameMesurement = "/mesurement";
String catalogConfig = "/config";

// HTTP thingSpeak
#include <HTTPClient.h>
unsigned long myChannelNumber;
char * myWriteAPIKey;
String jsonPayload;
String createdAt;
String mesurement[MAX_MESUREMENT_SEND_AT_A_TIME + 1];
String filenames[MAX_MESUREMENT_SEND_AT_A_TIME + 1];
byte maxMesurementAtTime = MAX_MESUREMENT_SEND_AT_A_TIME;
byte fileCount = 0;
String const timez = "+0300";

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "SD.cpp"
#include "SPI.cpp"
#include "display.cpp"
#include "scd40.cpp"
#include "WiFi.cpp"
#include "voltage.cpp"
#include "deepSleep.cpp"
#include "NTP.cpp"
#include "thingSpeak.cpp"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  // pinMode(BUTTON_PIN_LEFT, INPUT);
  pinMode(BUTTON_PIN_RIGHT, INPUT);

  ++bootCount;

  // Serial.begin(115200);

  wakeupReason();

  voltageSetup();

 //Get usb voltage
  voltage();

  spisdInit();
  // READ Config
  sdReadDisplayState();

  displaySetup();

  Wire.begin(21, 22);
  scd40Setup();
  
  deepSleepSetup();

  WiFiSetup();

  // REALTIME
  rtc.begin();
  //rtc.adjust(DateTime(2000, 1, 1, 0, 0, 0));

  tft.setTextSize(1); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setCursor(10, 120); tft.print("boot: "); tft.print(bootCount);
  Serial.println("Boot number: " + String(bootCount));

  switch (DEVICE_NUM) {
    case 1:
      hostname = "esp32-scd40-01";
      myChannelNumber = 1448907;
      myWriteAPIKey = "6JHWQUA3D77IGQAW";
      break;
    case 2:
      hostname = "esp32-scd40-02";
      myChannelNumber = 2398760;
      myWriteAPIKey = "CK60HE8LCLTCELI0";
      break;
    case 3:
      hostname = "esp32-scd40-03";
      myChannelNumber = 2401929;
      myWriteAPIKey = "QEKNQE7HLVMRXRX4";
      break;
    default:
      break;
  }

  // displayState = digitalRead(BUTTON_PIN_LEFT);
  // Serial.print("displayState: "); Serial.println(displayState);
  // displayState = digitalRead(BUTTON_PIN_RIGHT);
  // Serial.print("displayState: "); Serial.println(displayState);

  if (digitalRead(BUTTON_PIN_RIGHT) == false) {
    displaySCD40 (TFT_BLACK, TFT_BLACK, TFT_BLACK);
    displayState = !displayState;
    sdWriteDisplayState();
    tft.setTextSize(3); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setCursor(30, 50);
    if (displayState == true) {
      tft.print("DISPLAY ON");
      delay(500);
      tft.setTextColor(TFT_BLACK, TFT_BLACK); tft.setCursor(30, 50); tft.print("DISPLAY ON");
    } else {
      tft.print("DISPLAY OFF");
      delay(500);
      tft.setTextColor(TFT_BLACK, TFT_BLACK); tft.setCursor(30, 50); tft.print("DISPLAY OFF");
    }
    displaySCD40 (TFT_DARKGREY, TFT_DARKGREY, TFT_DARKGREY);

  }
  Serial.print("displayState: "); Serial.println(displayState);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {

  // RTC
  now = rtc.now();
  snprintf(datestr, 11, "%04d-%02d-%02d", now.year(), now.month(), now.day() );
  snprintf(timestr, 9, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  Serial.print("RTC: "); Serial.print(datestr); Serial.print("  "); Serial.println(timestr);
  tft.setTextSize(2);

  if (bootCount == 1) {
    if (rtc.lostPower()) {displayRTC(TFT_RED);} else {displayRTC(TFT_GREEN);}
  }

  // WIFI
  x=10; y=20;
  wifi();

  // NTP
  x=50; y=20;
  ntpGet();

  if (bootCount == 1) {
    now = rtc.now();
    snprintf(datestr, 11, "%04d-%02d-%02d", now.year(), now.month(), now.day() );
    snprintf(timestr, 9, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    if (rtc.lostPower()) {displayRTC(TFT_RED);} else {displayRTC(TFT_GREEN);}
    delay(1000);
  }

  // SD READ
  x=10; y=40;
  sdReadFromFile();

  //Get battery voltage
  voltage();

  if (bootCount == 1) {
    displayRTC(TFT_BLACK);
  }

  // CO2
  x=10; y=60;
  scd40();

  // THINGSPEAK
  x=10; y=80;
  jsonHttpPost();

  // SD WRITE
  x=10; y=100;
  sdWriteToFile();

  if (isChargerConnected == true) {
    TIME_TO_SLEEP = REFRESH_INTERVAL_WHEN_CHARGER_ON;
  } else {
    TIME_TO_SLEEP = REFRESH_INTERVAL;
    delay(1000);
    Serial.println("TFT sleep now");
    displaySleep();
    backLightOff();
  }
  deepSleepSetupOnlyTimer();
  Serial.print("TIME_TO_SLEEP: "); Serial.println(TIME_TO_SLEEP);

  if (displayState == false) {
    delay(1000);
    Serial.println("TFT sleep now");
    displaySleep();
    backLightOff();
  }

  isChargerConnectedPrev = isChargerConnected;

  Serial.println("ESP32 sleep now");
  Serial.flush();
  esp_deep_sleep_start();
}
