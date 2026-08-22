void jsonHttpPost () {
  if (WiFiExitCode==WL_CONNECTED) {
    Serial.println("------------------thingSpeak------------------");

    String myWriteAPIKeyStr(myWriteAPIKey);
    String mesurementAll;
    int beginCycle = 0;

    Serial.print("unixtime: "); Serial.println(now.unixtime());
    Serial.print("SCD40_lastSendTime: "); Serial.println(SCD40_lastSendTime);
    Serial.print("SCD40_lastSendTime: "); Serial.println(SCD40_lastSendTime + REFRESH_INTERVAL);
    Serial.print("Осталось секунд до следующей отправки: ");
    Serial.println(SCD40_lastSendTime + REFRESH_INTERVAL - now.unixtime());
    if (SCD40_lastSendTime + REFRESH_INTERVAL > now.unixtime()) {
      Serial.println("Отключаем отправку данных с датчика.");
      if (fileCount == 0) {
        Serial.println("Отключаем отправку полностью.");
        // return false;
        return;
      }
      beginCycle = 1;
    } else {
      SCD40_lastSendTime = now.unixtime();
    }
    Serial.print("beginCycle: "); Serial.println(beginCycle);

    for (int i = beginCycle; i <= fileCount; i++) {
      if ( i == beginCycle) {
        mesurementAll = mesurement[i];
      } else {
        mesurementAll = mesurementAll + "," + mesurement[i];
      }
    }

    jsonPayload = "{ \"write_api_key\": \"" + myWriteAPIKeyStr + "\", \"updates\": [" + mesurementAll + "] }";
    Serial.print("jsonPayload: "); Serial.println(jsonPayload);

    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setCursor(x, y); tft.print("TS");
 
    String myChannelNumberStr(myChannelNumber);
    String url = "http://api.thingspeak.com/channels/" + myChannelNumberStr + "/bulk_update.json";
    Serial.print("Отправка на: "); Serial.println(url);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.print(httpResponseCode); Serial.print(" "); Serial.println(response);
        if ( httpResponseCode == 202 ) {
          tft.setTextColor(TFT_GREEN, TFT_BLACK);
          tft.setCursor(x, y); tft.print("TS");
          sdDeleteFiles();
        } else {
          tft.setTextColor(TFT_RED, TFT_BLACK);
          tft.setCursor(x, y); tft.print("TS");
          isWriteToFile = true;
        }
    } else {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setCursor(x, y); tft.print("TS");
      Serial.print("Error on sending POST: "); Serial.println(httpResponseCode);
      isWriteToFile = true;
    }

    http.end();

  }
}
