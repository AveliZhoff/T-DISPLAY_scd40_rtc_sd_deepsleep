void SPIinit () {
    Serial.println("Init SPI only..");
    SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, -1);
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_BL, OUTPUT);
    tft.setRotation(1);
}
