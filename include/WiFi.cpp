void WiFiSetup () {
    WiFi.setHostname(hostname.c_str()); 
    WiFi.disconnect(true);
    Serial.print("Number of Wifi AP: "); Serial.print(NUMITEMS(apList));
    for (int x = 0; x < NUMITEMS(apList); x++) {
        WiFiMulti.addAP(apList[x].ssid, apList[x].password);
        Serial.print(", ");
        Serial.print(x);
        Serial.print(" -> ");
        Serial.print(apList[x].ssid);
    }
    Serial.println('.');
}

void wifi () {
    tft.setTextSize(1); tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setCursor(x, y); tft.print("WiFi");
    WiFiExitCode = WiFiMulti.run();
    Serial.print("WiFiExitCode: "); Serial.print(WiFiExitCode);
    if (WiFiExitCode==WL_CONNECTED) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        WiFiAP = WiFi.SSID();
        IPAddress IP = WiFi.localIP();
        WiFiIP = IpAddress2String(IP);
        Serial.print(", WIFI AP: "); Serial.print(WiFiAP); Serial.print(", WIFI IP: "); Serial.println(WiFiIP);
        // WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, dns1, dns2);
        // Serial.print("Попытка утсновить DNS. WiFiExitCode: "); Serial.println(WiFiExitCode);
    } else {
        Serial.print(", WIFI не подключен");
        tft.setTextColor(TFT_RED, TFT_BLACK);
        isWriteToFile = true;
    }
    tft.setCursor(x, y); tft.print("WiFi");
}
