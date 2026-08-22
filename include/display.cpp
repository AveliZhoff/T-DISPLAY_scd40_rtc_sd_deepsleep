#include <TFT_eSPI.h> 

void displaySleep () {
  Serial.println("displaySleep..");
  digitalWrite(TFT_CS, LOW);
  digitalWrite(TFT_DC, LOW);
  SPI.transfer(0x10);
  digitalWrite(TFT_CS, HIGH);
}

void displayWakeup () {
  Serial.println("displayWakeup..");
  digitalWrite(TFT_CS, LOW);
  digitalWrite(TFT_DC, LOW);
  SPI.transfer(0x11);
  digitalWrite(TFT_CS, HIGH);
}

void backLightOn () {
  Serial.println("backLightOn..");
  gpio_hold_dis((gpio_num_t)backLightPort);
  digitalWrite(backLightPort, HIGH);
  gpio_hold_en((gpio_num_t)backLightPort);
}

void backLightOff () {
  Serial.println("backLightOff..");
  gpio_hold_dis((gpio_num_t)backLightPort);
  digitalWrite(backLightPort, LOW);
  gpio_hold_en((gpio_num_t)backLightPort);
}

void displayInit () {
  Serial.println("DisplayInit..");
  tft.init();
  tft.setSwapBytes(true);
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, max_x, max_y, TFT_BLUE);
  backLightOn();
}

void backLightSaveState () {
  Serial.println("BackLightSaveState..");
  gpio_pad_select_gpio(backLightPort);
  gpio_set_direction((gpio_num_t)backLightPort, GPIO_MODE_OUTPUT);
  gpio_hold_en((gpio_num_t)backLightPort);
  gpio_deep_sleep_hold_en();
}

void prepareText () {
  tft.setTextSize(1);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  x=10; y=20; tft.setCursor(x, y); tft.print("WiFi"); x=50; y=20; tft.setCursor(x, y); tft.print("NTP");
  x=10; y=40; tft.setCursor(x, y); tft.print("SDR");
  x=10; y=60; tft.setCursor(x, y); tft.print("SCD40");
  x=10; y=80; tft.setCursor(x, y); tft.print("TS");
  x=10; y=100; tft.setCursor(x, y); tft.print("SDW");
}

void displaySCD40 (word color1, word color2, word color3) {
  tft.setTextSize(5); tft.setTextColor(color1, TFT_BLACK); tft.setCursor(80, 30); tft.print(CO2); tft.print(" ");
  tft.setTextSize(2);
  tft.setTextColor(color2, TFT_BLACK); tft.setCursor(70, 80); tft.print("t: "); tft.print(TEMP); tft.print("C");
  tft.setTextColor(color3, TFT_BLACK); tft.setCursor(70, 110); tft.print("h: "); tft.print(HUM); tft.print("%");
}

void displayRTC (word color) {
  tft.setTextColor(color, TFT_BLACK); tft.setTextSize(3);
  tft.setCursor(40, 40); tft.print(datestr);
  tft.setCursor(50, 80); tft.print(timestr);
}

void displaySetup () {
  if (bootCount == 1) {
    displayInit();
  }

  if (keyWakeup == true) {
    if (isChargerConnected == true) {
      if (isChargerConnectedPrev == false) {
        displayInit();
      } else {
        SPIinit();
        backLightOn();
        displayWakeup();
      }
    } else {
      displayInit();
    }
  } 
  else {
    if (displayState == true ) {
      if (isChargerConnected == true) {
        if (isChargerConnectedPrev == false) {
          displayInit();
        } else {
          SPIinit();
          backLightOn();
          displayWakeup();
        }
      }
    }
  }

  if (bootCount != 1) {
    displaySCD40 (TFT_DARKGREY, TFT_DARKGREY, TFT_DARKGREY);
  }

  //Сохранение стояние порта подсветки дисплея при глубоком сне ESP32
  backLightSaveState();

  prepareText();
}
