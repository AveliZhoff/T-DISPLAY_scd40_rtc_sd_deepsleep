void ntpGet () {
    if (rtc.lostPower()) {
        tft.setCursor(x, y);
        tft.setTextSize(1); tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setCursor(x, y); tft.print("NTP");

        struct tm timeinfo;
        for (int i = 0; i < sizeof(ntpServers)/sizeof(ntpServers[0]); i++) {

            configTime(gmtOffset_sec, daylightOffset_sec, ntpServers[i]);

            Serial.printf("Попытка подключения к NTP серверу: %s", ntpServers[i]);
            if (getLocalTime(&timeinfo)) {
                Serial.println(" Время успешно получено");

                tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setCursor(x, y); tft.print("NTP");
                //   Serial.println(&timeinfo, "%A, %B %d., %Y %m %d %H:%M:%S");
                Serial.print("NTP: ");
                Serial.print(1900 + timeinfo.tm_year); Serial.print(" "); Serial.print(timeinfo.tm_mon + 1); Serial.print(" "); Serial.print(timeinfo.tm_mday); Serial.print(" ");
                Serial.print(timeinfo.tm_hour); Serial.print(" "); Serial.print(timeinfo.tm_min); Serial.print(" "); Serial.println(timeinfo.tm_sec);
                
                rtc.adjust(DateTime(1900 + timeinfo.tm_year, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));

                break;
            } else {
                Serial.println(" Ошибка");
            }
            Serial.println("NTP: Failed to obtain time");
            tft.setTextColor(TFT_RED, TFT_BLACK); tft.setCursor(x, y); tft.print("NTP");
        }
    }
}
