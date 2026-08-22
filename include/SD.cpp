void sdListDir(fs::FS &fs, const char * dirname, uint8_t levels) {
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                sdListDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: "); Serial.print(file.name()); Serial.print("  SIZE: "); Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void sdCreateCatalog (String catalogName) {
  if (!SD.exists(catalogName)) {
    Serial.print("Catalog: "); Serial.print(catalogName); Serial.println(" Does not exist. Try to create.");
    if (SD.mkdir(catalogName)) {
      Serial.print("Catalog: "); Serial.print(catalogName); Serial.println(" Created !");
    } else {
      Serial.print("Error while create the catalog: "); Serial.print(catalogName);
    }
  } else {
    Serial.print("Catalog: "); Serial.print(catalogName); Serial.println(" Exist, use it.");
  }
}


void sdWriteDisplayState () {
  Serial.println("-------------writeDisplayState------------");

  sdCreateCatalog(catalogConfig);

  String fileName = catalogConfig + "/" + fileDisplayState;
  String fileContent = String(displayState);
  Serial.print("Filename: "); Serial.print(fileName); Serial.print(" ,  FileContent: "); Serial.print(fileContent);

  File myFile = SD.open(fileName, FILE_WRITE);
  if (myFile) {
    Serial.print(", Writing to file... ");
    myFile.print(fileContent);
    myFile.close();
    Serial.println("done.");
  } else {
    Serial.println("Error opening file to write.");
  }
}

void sdReadDisplayState () {
    Serial.println("--------------readDisplayState------------");

    sdCreateCatalog(catalogConfig);

    String fileName = catalogConfig + "/" + fileDisplayState;
    String fileContent;
    if (SD.exists(fileName)) {
    Serial.print("Found config file: "); Serial.print(fileName); Serial.print(" try to load.. ");
    File file = SD.open(fileName);
    if (file) {
      while (file.available()) {
        fileContent += (char)file.read();
      }
      Serial.print("Done."); Serial.print(" Content from file: "); Serial.println(fileContent);
      if (fileContent == "1") {displayState = true;} else {displayState = false;}
    }
  } else {
    Serial.print("No config file: "); Serial.print(fileName); Serial.println(" found.");
  }
}

void sdWriteToFile () {
  if (isWriteToFile) {
    tft.setTextSize(1); tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setCursor(x, y); tft.print("SDW");
    Serial.println("---------------writeToFile---------------");
    char datestr[11];
    char timestr[9];
    snprintf(datestr, 11, "%04d-%02d-%02d", now.year(), now.month(), now.day() );
    snprintf(timestr, 9, "%02d_%02d_%02d", now.hour(), now.minute(), now.second());
    // Serial.println(datestr);
    // Serial.println(timestr);

    sdCreateCatalog(catalogNameMesurement);

    String fileName = catalogNameMesurement + "/" + String(datestr) + ' ' + String(timestr);
    String fileContent = mesurement[0];
    Serial.print("Filename: "); Serial.print(fileName); Serial.print(" ,  FileContent: "); Serial.println(fileContent);

    File myFile = SD.open(fileName, FILE_WRITE);
    if (myFile) {
      Serial.print("Writing to file...");
      myFile.print(fileContent);
      myFile.close();
      Serial.println(" done.");
      tft.setTextSize(1); tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setCursor(x, y); tft.print("SDW");
    } else {
      Serial.println(" Error opening file to write.");
      tft.setTextSize(1); tft.setTextColor(TFT_RED, TFT_BLACK); tft.setCursor(x, y); tft.print("SDW");
    }
  }
}

void sdReadFromFile () {
  if (WiFiExitCode==WL_CONNECTED) {
    Serial.println("---------------readFromFile---------------");
    tft.setTextSize(1); tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setCursor(x, y); tft.print("SDR");
    sdCreateCatalog(catalogNameMesurement);
    File root = SD.open(catalogNameMesurement);
    if (!root) {
      Serial.println("Failed to open directory");
      tft.setTextColor(TFT_RED, TFT_BLACK); tft.setCursor(x, y); tft.print("SDR");
      return;
    }
    if (!root.isDirectory()) {
      Serial.println("Not a directory");
      tft.setTextColor(TFT_RED, TFT_BLACK); tft.setCursor(x, y); tft.print("SDR");
      return;
    }

    for (int i = 1; i <= maxMesurementAtTime; i++) {

      File file = root.openNextFile();
      if (file) {
          if (file.isDirectory()) {
              Serial.print("DIR : "); Serial.println(file.name());
          } else {
   
              String fileContent = "";
              while (file.available()) {
                fileContent += (char)file.read();
              }

              mesurement[i] = fileContent;
              fileCount++;
              Serial.print("MesurementCount: "); Serial.print(i); Serial.print(" , json: "); Serial.println(mesurement[i]);
              
              filenames[i] = file.name();
            }
      } else {
        Serial.println("No more files found or error opening file.");
        root.close();
        break;
      }

      file.close();

    }
    root.close();
    tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setCursor(x, y); tft.print("SDR");
  }
}

void sdDeleteFiles () {
  for (int i = 1; i <= fileCount; i++) {
    File root = SD.open(catalogNameMesurement);
    if (!root) {
      Serial.println("Failed to open directory");
      return;
    }
    if (!root.isDirectory()) {
      Serial.println("Not a directory");
      return;
    }

    String filename = catalogNameMesurement + "/" + filenames[i];
    if (!SD.remove(filename)){
      Serial.print("Error delete file: "); Serial.println(filenames[i]);
    } else {
      Serial.print("File deleted: "); Serial.println(filenames[i]);
    }

    root.close();
  }
}

void spisdInit () {
    spiSD.begin(13,26,15,2);
    if (!SD.begin(SD_CS, spiSD, SDSPEED)) {
        Serial.println("SDCard: MOUNT FAIL");
    } else {
        cardSize = SD.cardSize() / (1024 * 1024);
        String str = "SDCard Size: " + String(cardSize) + "MB";
        Serial.println(str);

        // List Dir.
        // sdListDir(SD, "/", 2);
    }
}

