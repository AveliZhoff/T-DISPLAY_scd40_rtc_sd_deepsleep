void voltageSetup () {
    pinMode(ADC_PIN, INPUT);
    pinMode(ADC_EN, OUTPUT);
}

void voltage () {
    if (ADC_EN_State == 0) {
        usbVoltage = (float)(analogRead(ADC_PIN)) * 7.26 / 4095;
        Serial.print("usbVoltage: "); Serial.println(usbVoltage);
        if (usbVoltage < usbVoltageMin) {
            isChargerConnected = false;
        } else {
            isChargerConnected = true;
        }
        Serial.print("isChargerConnected: "); Serial.println(isChargerConnected);
    } else {
        batteryVoltage = (float)(analogRead(ADC_PIN)) * 7.26 / 4095;
        Serial.print("batteryVoltage: "); Serial.println(batteryVoltage);
    }
    ADC_EN_State = !ADC_EN_State;
    digitalWrite(ADC_EN, ADC_EN_State);
}
