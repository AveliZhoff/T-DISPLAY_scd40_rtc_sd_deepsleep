void scd40Setup () {
  if (SCD40.begin() == false) {
    Serial.println(F("SCD40 Sensor not detected. Please check wiring."));
  } else {
    if (bootCount==1) {
      if (SCD40.stopPeriodicMeasurement() == true)
      {
        Serial.println(F("SCD40 periodic measurement is disabled!"));
      }  
      Serial.println(F("SCD40 starting the self-test. This will take 10 seconds to complete..."));
      bool success = SCD40.performSelfTest();
      Serial.print(F("SCD40 yhe self test was "));
      if (success == false)
        Serial.print(F("not "));
      Serial.println(F("successful"));

      Serial.println(F("SCD40 starting the factory reset. This will take 1200ms to complete..."));
      success = SCD40.performFactoryReset();
      Serial.print(F("SCD40 the factory reset was "));
      if (success == false)
        Serial.print(F("not "));
      Serial.println(F("successful"));
    }
    if (SCD40.startPeriodicMeasurement() == true)
    {
      Serial.println(F("SCD40 periodic measurement is enabled!"));
    }
  }
}

void scd40 (){
  tft.setTextSize(1); tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setCursor(x, y); tft.print("SCD40");

  while (!SCD40.readMeasurement()) {
    delay(checkIntrval);
    checkTimes = checkTimes - 1;
    if (checkTimes < 0) {
      Serial.println("SCD40 prepare timeout reached.");
      isSCD40ready = false;
      break;}
  }

  if (isSCD40ready) {
    Serial.println("------------------SCD40------------------");
    word colorCO2;
    word colorTEMP;
    word colorHUM;

    if (bootCount == 1) {
      CO2 = SCD40.getCO2();
      TEMP = SCD40.getTemperature();
      HUM = SCD40.getHumidity();
    } else {
      CO2 += int((SCD40.getCO2() - CO2) * kAvarage);
      TEMP += (SCD40.getTemperature() - TEMP) * kAvarage;
      HUM += (SCD40.getHumidity() - HUM)* kAvarage;
    }

    tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setCursor(x, y); tft.print("SCD40");
    
    
    if (CO2 < 800) { colorCO2 = TFT_GREEN; } else if (CO2 < 1200) { colorCO2 = TFT_YELLOW; } else { colorCO2 = TFT_RED; }
    if (TEMP < 18) { colorTEMP = TFT_BLUE; } else if (TEMP < 24) { colorTEMP = TFT_GREEN; } else { colorTEMP = TFT_RED; }
    if (HUM < 30) { colorHUM = TFT_RED; } else if (HUM < 40) { colorHUM = TFT_YELLOW; } else if (HUM < 60) { colorHUM = TFT_GREEN; } else if (HUM < 70) { colorHUM = TFT_YELLOW; } else { colorHUM = TFT_RED; }

    displaySCD40 (colorCO2, colorTEMP, colorHUM);

    createdAt = String(datestr) + ' ' + String(timestr);
    mesurement[0] = "{\"created_at\": \"" + createdAt + " " + timez + "\", \"field1\": \"" + CO2 + "\", \"field2\":\"" + TEMP + "\", \"field3\":\"" + HUM + "\", \"field4\":\"" + batteryVoltage + "\", \"field5\":\"" + usbVoltage + "\", \"field6\":\"" + bootCount + "\"}";
    Serial.print("MesurementCount: "); Serial.print(fileCount); Serial.print(" , json: "); Serial.println(mesurement[0]);

    if (SCD40.stopPeriodicMeasurement() == true) {
        Serial.println(F("SCD40 periodic measurement is disabled!"));
    }

  } else {
    tft.setTextSize(1); tft.setTextColor(TFT_GREEN, TFT_RED); tft.setCursor(x, y); tft.print("SCD40");
    Serial.println(F("SCD40 is not ready."));
  }
}
