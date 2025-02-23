// Libaries
#include "config.h" // Konfigurationsdatei für Netzwerkeinstellungen & Iot Hub
#include <Arduino.h> // Arduino Standard-Bibliothek (Pin-, Serial-Funktionen, etc.)
#include <TFT_eSPI.h> // Bibliothek für das TFT-Display des Wio Terminals
#include "DHT.h" // Bibliothek für Grove Temperatur- & Feuchtigkeitssensor (DHT)
#include <rpcWiFi.h> // WLAN-Verbindung für das Wio Terminal
#include <SD.h> // Zugriff auf die SD-Karte
#include <WiFiUdp.h> // UDP-Protokoll für Netzwerkkommunikation (NTP)
#include <Wire.h> // I2C-Bibliothek für die Kommunikation mit Sensoren
#include <Adafruit_VL53L0X.h> // Entfernungssensor
#include "lcd_backlight.hpp" // Steuerung der LCD-Hintergrundbeleuchtung
#include <AzureIoTHub.h> // Azure IoT Hub SDK für Cloud-Anbindung
#include <AzureIoTProtocol_MQTT.h> // MQTT-Protokoll für Azure IoT Hub
#include <iothubtransportmqtt.h> // MQTT-Transport für IoT-Hub-Kommunikation
#include <ArduinoJson.h> // JSON-Bibliothek für Datenserialisierung
#include "DateTime.h" // Bibliothek für Datum- und Zeitverwaltung
#include <time.h> // Standard-Zeitbibliothek für Zeitfunktionen
#include "samd/NTPClientAz.h" // NTP (Network Time Protocol) für Zeitsynchronisation
#include <sys/time.h> // Systembibliothek für Echtzeituhr (RTC)
#include <RTC_SAMD51.h> // Echtzeituhr für den SAMD51 Mikrocontroller (Wio Terminal)
#include <TinyGPS++.h> // GPS-Bibliothek für die Verarbeitung von GPS-Daten

// Definitionen
#define MOISTURE_PIN A2 // Pin A2 für den Feuchtigkeitssensor am rechten Grove-Anschluss
#define RELAY_PIN D6 // Pin D6, für Relai
#define DHT_PIN 0 // Grove-Analoganschluss für den DHT-Sensor (Standard ist D0 oder A0)
#define DHT_TYPE DHT11 // Typ des DHT-Sensors
#define SDCARD_SS_PIN // CS-Pin für die SD-Karte
#define DIST_THRESHOLD 100  
#define TFT_DARKORANGE  0xFCC0 //Farbdefinition Orange
#define TFT_DARKYELLOW  0xCD00 //Farbdefinition Gelb
#define WIO_KEY_A BUTTON_3 // Knopf A
#define WIO_KEY_B BUTTON_2 // Knopf B
#define WIO_KEY_C BUTTON_1 // Knopf C

// Objekte definieren
File dataFile; // Dateiobjekt für das Speichern der Daten
DHT dht(DHT_PIN, DHT_TYPE); // DHT-Sensor-Objekt erstellen
TFT_eSPI tft = TFT_eSPI();  // Display-Objekt erstellen
RTC_SAMD51 rtc; // RTC-Objekt erstellen
Adafruit_VL53L0X lox = Adafruit_VL53L0X(); // Instanz für Distanz-Sensor 
static LCDBackLight backLight; //Objekt für die Hintergrundbeleuchtung
IOTHUB_DEVICE_CLIENT_LL_HANDLE _device_ll_handle; //Iot Hub
TinyGPSPlus gps; //Objekt für GPS Sensor

// Konfiguration für NTP
WiFiUDP _udp;
NTPClientAz ntpClient;
time_t epochTime = (time_t)-1;

// Variablen definieren
int lastMoistureValue = -1; // Speichern des vorherigen Feuchtigkeitswerts
float lastTemperature = -1000; // Speichern des vorherigen Temperaturwerts
float lastHumidity = -1000; // Speichern des vorherigen Feuchtigkeitswerts
unsigned long previousTimeUpdate = 0; // Letzte Aktualisierung der Uhrzeit
unsigned long previousSensorUpdate = 0; // Letzte Aktualisierung der Sensorwerte
const unsigned long timeInterval = 1000; // Intervall für Zeitaktualisierung (1 Sekunde)
const unsigned long sensorInterval = 4000; // Intervall für Sensoraktualisierung (4 Sekunden)
const unsigned long displayTimeout = 20000; // Intervall für die Anzeige von Sensorwerten (20 Sekunden)
const unsigned long IoTHubTimeout = 60000; // Intervall für die Iot Hub (60 Sekunden)
unsigned long previousIoTHubUpdate = 0; // Letzte Sendung der Sensorwerte an Iot Hub
bool isDisplayingSensorValues = false; // Variable für Sensor-Werte Aktualisierung
unsigned long displayUpdateTime = 0; // Variable für Display Aktualisierung
bool firstMainScreen = false; // Variable erster MainScreen
unsigned long previousFlowerUpdate = 0; // Variable vorherige Standby-Screen Aktualisierung
int plantMode; // Variable für die Pflanzen Modis

// Schwellenwerte für die Modi
struct MoistureThreshold {
    int low;
    int high;
};
MoistureThreshold thresholds[3] = {
    {10, 200},    // Wenig Wasserbedarf
    {201, 400},  // Mittel Wasserbedarf
    {401, 600}   // Viel Wasserbedarf
};

// WLAN Verbindung
void connectToWiFi() {
    int maxRetries = 2; // Maximale Anzahl an Versuchen
    int retryCount = 0; // Zähler für Versuche

    tft.fillScreen(TFT_BLACK);            
    tft.setTextDatum(MC_DATUM);           
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawString("Verbinde mit WLAN...", 160, 100);
    Serial.println("Verbinde mit WLAN...");
    WiFi.begin(ssid, password);

    // Wiederhole Verbindung bis Maximum erreicht ist
    while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
        delay(500);
        Serial.print(".");
        tft.print(".");
        retryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN);
        tft.setTextSize(3);
        tft.drawString("WLAN verbunden!", 160, 100);
        Serial.println("\nWLAN verbunden!");
    } else {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED);
        tft.setTextSize(2);
        tft.drawString("WLAN fehlgeschlagen!", 160, 100);
        Serial.println("\nWLAN fehlgeschlagen!");
    }
    delay(2000); 
}

// Aktuelle Uhrzeit setzen
static void initTime()
{
    ntpClient.begin();

    while (true)
    {
        epochTime = ntpClient.getEpochTime("0.pool.ntp.org");

        if (epochTime == (time_t)-1)
        {
            Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
            delay(2000);
        }
        else
        {
            Serial.print("Fetched NTP epoch time is: ");

            char buff[32];
            sprintf(buff, "%.f", difftime(epochTime, (time_t)0));
            Serial.println(buff);
            break;
        }
    }

    ntpClient.end();
    epochTime += 3600; // Offset für Zeitzone hinzufügen
    struct timeval tv;
    tv.tv_sec = epochTime;
    tv.tv_usec = 0;

    settimeofday(&tv, NULL);

    // RTC synchronisieren
    rtc.adjust(DateTime(epochTime));
    Serial.println("RTC erfolgreich synchronisiert!");
}


// Funktion zum Zeichnen eines Kreisbogens
void drawArc(int x, int y, int r, int startAngle, int endAngle, uint16_t color, int thickness) {
    for (int i = startAngle; i <= endAngle; i++) {
    float angle = i * 3.14159 / 180.0;
    int x0 = x + (int)(cos(angle) * r);
    int y0 = y - (int)(sin(angle) * r);
    tft.drawPixel(x0, y0, color);

        for (int j = -thickness / 2; j < thickness / 2; j++) {
            tft.drawPixel(x0 + j, y0, color);
        }
    }
}

// Funktion zum Zeichnen der Blume für Standby-Screen
void drawSunflower(String mood, uint16_t faceColor) {
    tft.fillScreen(TFT_BLACK);

    int centerX = 160; // Horizontal zentriert
    int centerY = 90; // Vertikal zentriert
    int faceRadius = 50; // Radius des Gesichts

    for (int angle = 0; angle < 360; angle += 30) {
    float rad = angle * 3.14159 / 180.0;
    int xOffset = (int)(cos(rad) * 60);
    int yOffset = (int)(sin(rad) * 60);

    tft.fillCircle(centerX + xOffset,centerY + yOffset,20,TFT_YELLOW);
    }

    // Gesicht
    tft.fillCircle(centerX, centerY, faceRadius, faceColor);

    // Augen
    tft.fillCircle(centerX - 25, centerY - 12, 6, TFT_BLACK); // Linkes Auge
    tft.fillCircle(centerX + 25, centerY - 12, 6, TFT_BLACK); // Rechtes Auge

    // Mund
    int mouthY = centerY + 20;  
    int mouthRadius = 25;      

    if (mood == "happy") {
    // Lachender Mund (oben gewölbt)
    drawArc(centerX, mouthY, mouthRadius, 180, 360, TFT_BLACK, 4);
    } 
    else if (mood == "sad") {
    // Trauriger Mund (unten gewölbt)
    drawArc(centerX, mouthY + 10, mouthRadius, 0, 180, TFT_BLACK, 4);
    } 
    else {
    // Neutraler Mund (gerade Linie)
    tft.fillRect(centerX - 25, mouthY - 2, 50, 4, TFT_BLACK);
    }

    // Stiel
    int stemWidth = 12; 
    int stemX = centerX - stemWidth / 2;
    int stemY = centerY + faceRadius; 
    int stemHeight = 100; 

    tft.fillRect(stemX, stemY, stemWidth, stemHeight, TFT_GREEN);

    // Blätter
    int leafCenterY = stemY + stemHeight / 2 + 10; 
    int leafOffsetX = 40;   
    int leafRadiusX = 30;   
    int leafRadiusY = 15;   

    // Linkes Blatt
    tft.fillEllipse(centerX - leafOffsetX, leafCenterY, leafRadiusX, leafRadiusY, TFT_GREEN);
    // Rechtes Blatt
    tft.fillEllipse(centerX + leafOffsetX, leafCenterY, leafRadiusX, leafRadiusY, TFT_GREEN);
}

// Funktion zum Aktualisieren der Anzeige-Zeit
void updateTimeDisplay() {
    DateTime now = rtc.now();
    char timeBuffer[16];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    tft.fillRect(200, 60, 100, 30, TFT_BLACK);
    tft.setCursor(200, 60);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.println(timeBuffer);
}

// Funktion für das Ein- und Ausschalten des Relai
void controlRelayBasedOnMoisture(int moistureValue) {
    MoistureThreshold currentThreshold = thresholds[plantMode - 1];
    if (moistureValue <= currentThreshold.low) { 
        digitalWrite(RELAY_PIN, HIGH);  // Relais einschalten
    } else if (moistureValue > currentThreshold.low && moistureValue <= currentThreshold.high) {
        digitalWrite(RELAY_PIN, LOW);  // Relais ausschalten
    } else {
        digitalWrite(RELAY_PIN, LOW);  // Relais ausschalten
    }
}

// Funktion zum Aktualisieren der Sensorwerte
void updateSensorData(int moistureValue, float temperature, float humidity) {
    if (moistureValue != lastMoistureValue || firstMainScreen) {
        tft.fillRect(200, 100, 80, 30, TFT_BLACK);
        tft.setCursor(200, 100);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.print(moistureValue);
        lastMoistureValue = moistureValue;
    }

    if (temperature != lastTemperature || firstMainScreen) {
        tft.fillRect(200, 140, 100, 30, TFT_BLACK);
        tft.setCursor(200, 140);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.print(temperature);
        tft.print(" C");
        lastTemperature = temperature;
    }

    if (humidity != lastHumidity || firstMainScreen) {
        tft.fillRect(200, 180, 100, 30, TFT_BLACK);
        tft.setCursor(200, 180);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.print(humidity);
        tft.print(" %");
        lastHumidity = humidity;
    }

    tft.fillRect(0, 200, 320, 40, TFT_BLACK);
    String statusMessage;
    MoistureThreshold currentThreshold = thresholds[plantMode - 1];

    // Anzeige der drei Status
    if (moistureValue <= currentThreshold.low) {
        statusMessage = "Giessen";
        tft.setTextColor(TFT_RED);
    } else if (moistureValue > currentThreshold.low && moistureValue <= currentThreshold.high) {
        statusMessage = "Bald Giessen";
        tft.setTextColor(TFT_YELLOW);
    } else {
        statusMessage = "Alles Gut";
        tft.setTextColor(TFT_GREEN);
    }

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(3);
    tft.drawString(statusMessage, 160, 220);

    // Relai
    controlRelayBasedOnMoisture(moistureValue);
}

// Funktion zum schreiben der Daten auf die SD-Karte
void logDataToCSV(int moistureValue, float temperature, float humidity) {
    dataFile = SD.open("sensors.csv", FILE_WRITE); // Datei im Anhängemodus öffnen

    if (dataFile) {
        DateTime now = rtc.now(); // Aktuelle Uhrzeit abrufen
        char timeBuffer[16];
        snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

        // Datenzeile in die CSV schreiben
        dataFile.print(timeBuffer); // Zeit
        dataFile.print(",");
        dataFile.print(moistureValue); // Feuchtigkeit
        dataFile.print(",");
        dataFile.print(temperature); // Temperatur
        dataFile.print(",");
        dataFile.println(humidity); // Luftfeuchtigkeit
        dataFile.close(); // Datei schließen
    } else {
        Serial.println("Fehler beim Öffnen der CSV-Datei!");
    }
}

// Funktion für den Hauptbildschirm
void mainScreen() {
    // Anzeige definieren
    backLight.setBrightness(100);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);   
    tft.setTextSize(3);           
    tft.setTextColor(TFT_WHITE);
    tft.drawString("AquaBotanica", 160, 20);  
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 60);
    tft.println("Uhrzeit:");
    tft.setCursor(10, 100);
    tft.println("Pflanze F.:");
    tft.setCursor(10, 140);
    tft.println("Temperatur:");
    tft.setCursor(10, 180);
    tft.println("Luft F.:");

    // Aktualiserung der Sensordaten, Zeit und Relai
    int moistureValue = analogRead(MOISTURE_PIN);
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    updateSensorData(moistureValue, temperature, humidity);
    updateTimeDisplay();
    controlRelayBasedOnMoisture(moistureValue);
    isDisplayingSensorValues = true;
}

// Funktion für den Standby-Screen
void showStandbyScreen() {
    tft.fillScreen(TFT_BLACK); 
    backLight.setBrightness(20);
    tft.setCursor(10, 10);
    drawSunflower("neutral", TFT_DARKYELLOW);
}

// Funktion für IoT Hub Verbindung Prüfung
static void connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void *user_context)
{
    if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
    {
        Serial.println("The device client is connected to IoT Hub");
    }
    else
    {
        Serial.println("The device client has been disconnected");
        Serial.printf("Device client disconnected. Reason: %d\n", reason);
    }
}

// Funktion für die IoT Hub Verbindung
void connectIoTHub()
{
    IoTHub_Init();

    _device_ll_handle = IoTHubDeviceClient_LL_CreateFromConnectionString(CONNECTION_STRING, MQTT_Protocol);
    
    if (_device_ll_handle == NULL)
    {
        Serial.println("Failure creating IoTHub device. Hint: Check your connection string.");
        return;
    }
    
    IoTHubDeviceClient_LL_SetConnectionStatusCallback(_device_ll_handle, connectionStatusCallback, NULL);
}

// Funktion zum senden der Daten an den IoT Hub
void sendTelemetry(const char *telemetry)
{
    IOTHUB_MESSAGE_HANDLE message_handle = IoTHubMessage_CreateFromString(telemetry);
    if (message_handle == NULL) {
    Serial.println("Failed to create IoT Hub message!");
    return;
}
    IoTHubDeviceClient_LL_SendEventAsync(_device_ll_handle, message_handle, NULL, NULL);
    IoTHubMessage_Destroy(message_handle);
 
}

// Setup Funktion beim Starten des Wio Terminals
void setup() {
    Serial.begin(115200); // Serial Monitor starten
    Serial1.begin(9600); // Serial Monitor GPS
    pinMode(WIO_KEY_A, INPUT_PULLUP); // Taste A 
    pinMode(WIO_KEY_B, INPUT_PULLUP); // Taste B 
    pinMode(WIO_KEY_C, INPUT_PULLUP); // Taste C 
    pinMode(MOISTURE_PIN, INPUT); // Feuchtigkeitssensor als Eingang konfigurieren
    pinMode(RELAY_PIN, OUTPUT); // Relais-Pin als Ausgang konfigurieren
    pinMode(WIO_MIC, INPUT); // Mikrofon als Eingang festlegen
    digitalWrite(PIN_WIRE_SCL, LOW); // Relais im Default-Zustand auf LOW setzen (ausgeschaltet)
    plantMode = 1; // Pflanzmodus auf Standardwert setzen
    
    // Display initialisieren
    tft.begin();
    tft.setRotation(3); // Querformat
    tft.fillScreen(TFT_BLACK); // Hintergrundfarbe auf Schwarz setzen

    // DHT-Sensor initialisieren
    dht.begin();
    
    // RTC initialisieren
    if (!rtc.begin()) {
        Serial.println("RTC nicht gefunden!");
        while (1);
    }

    // Näherungssensor initialisieren
    if (!lox.begin()) {
        Serial.println("VL53L0X Sensor konnte nicht initialisiert werden.");
        while (1);
    } else {
        Serial.println("VL53L0X Sensor initialisiert.");
    }

    // Hintergrundbeleuchtung initialisieren
    backLight.initialize();

    // SD-Karte initialisieren
    if (!SD.begin(SDCARD_SS_PIN)) {
        Serial.println("SD-Karte konnte nicht initialisiert werden!");
        while (1);
    }
    Serial.println("SD-Karte erfolgreich initialisiert!");

    // CSV-Datei erstellen/öffnen und Kopfzeilen schreiben
    if (!SD.exists("sensors.csv")) {
        // Datei existiert nicht, neu erstellen und Header schreiben
        dataFile = SD.open("sensors.csv", FILE_WRITE);
        if (dataFile) {
            dataFile.println("Zeit,Feuchtigkeit_Pflanze,Temperatur,Luftfeuchtigkeit");
            dataFile.close();
            Serial.println("Kopfzeile in CSV-Datei geschrieben.");
        } else {
            Serial.println("Konnte CSV-Datei nicht erstellen!");
            while (1);
        }
    } else {
        Serial.println("CSV-Datei existiert bereits, kein Header geschrieben.");
    }
    
    // WLAN initialisieren
    connectToWiFi();
    // Zeit initialisieren
    initTime();
    // Iot Hub Verbindung initialisieren
    connectIoTHub();
    delay(2000);
    tft.fillScreen(TFT_BLACK); // Bildschirm erneut leeren für Sensoranzeige
    // Hauptbildschirm laden
    mainScreen();
}

// Funktion für die Pflanzen Modis bzw. Anzeige bei Modi-Wechsel
void displayModeInfo() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(3);

    String modeText1 = "Modus:";
    String modeText2;
    if (plantMode == 1) {
        modeText2 = "Wenig Wasser";
    } else if (plantMode == 2) {
        modeText2 = "Mittel Wasser";
    } else if (plantMode == 3) {
        modeText2 = "Viel Wasser";
    }
    tft.drawString(modeText1, 160, 110);
    tft.drawString(modeText2, 160, 140);
    delay(1000);
    mainScreen();
}

// Loop Funktion wird laufend ausgeführt und liest Werte bzw. für Aktionen aus
void loop() {
    
    // Variablen definieren
    unsigned long currentMillis = millis(); // Aktuelle Zeit in Millisekunden
    uint16_t distance = lox.readRange(); // Distance lesen
    int micValue = analogRead(WIO_MIC); // Mikrofonwert lesen

    // Knopf A gedrückt -> Modus 1
    if (digitalRead(WIO_KEY_A) == LOW) {
        plantMode = 1;
        displayModeInfo();
        firstMainScreen = true;
        mainScreen();
    }

    // Knopf B gedrückt -> Modus 2
    if (digitalRead(WIO_KEY_B) == LOW) {
        plantMode = 2;
        displayModeInfo();
        firstMainScreen = true;
        mainScreen();
    }

    // Knopf C gedrückt -> Modus 3
    if (digitalRead(WIO_KEY_C) == LOW) {
        plantMode = 3;
        displayModeInfo();
        firstMainScreen = true;
        mainScreen();
    }

    // Main-Screen und Standby-Screen Anzeige
    if (distance <= DIST_THRESHOLD || micValue > 650) {
        if (!isDisplayingSensorValues) {
            displayUpdateTime = currentMillis;  // Timer starten, wenn der Abstand oder Mikrowert unter bzw. über der Schwelle liegt
            isDisplayingSensorValues = true;
            firstMainScreen = true;
            mainScreen(); // Hauptbildschirm anzeigen
        }
    } else {
        if (isDisplayingSensorValues && currentMillis - displayUpdateTime >= displayTimeout) {
            showStandbyScreen(); // Standby-Bildschirm anzeigen
            isDisplayingSensorValues = false;
            firstMainScreen = false;
        }
    }

    // Relai 
    if (!isDisplayingSensorValues && currentMillis - previousSensorUpdate >= sensorInterval) {
        previousSensorUpdate = currentMillis;
        int moistureValue = analogRead(MOISTURE_PIN);
        controlRelayBasedOnMoisture(moistureValue);
    }

    // Uhrzeit jede Sekunde aktualisieren
    if (isDisplayingSensorValues && currentMillis - previousTimeUpdate >= timeInterval) {
        previousTimeUpdate = currentMillis;
        updateTimeDisplay();
    }

    // Sensorwerte alle 4 Sekunden aktualisieren
    if (isDisplayingSensorValues && currentMillis - previousSensorUpdate >= sensorInterval) {
        previousSensorUpdate = currentMillis;
        firstMainScreen = false;
        int moistureValue = analogRead(MOISTURE_PIN);
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();

        if (!isnan(temperature) && !isnan(humidity)) {
            updateSensorData(moistureValue, temperature, humidity);
            controlRelayBasedOnMoisture(moistureValue);
        } else {
            Serial.println("Fehler beim Lesen eines Sensors!");
        }
    }

    // Standby Screen
    if (!isDisplayingSensorValues && currentMillis - previousFlowerUpdate >= sensorInterval) {
        previousFlowerUpdate = currentMillis;

        int moistureValue = analogRead(MOISTURE_PIN);
        uint16_t faceColor = TFT_DARKORANGE;
        MoistureThreshold currentThreshold = thresholds[plantMode - 1];

        if (moistureValue > currentThreshold.high) { // In Ordnung
            drawSunflower("happy", faceColor);
        } else if (moistureValue > currentThreshold.low) { // Bald gießen
            drawSunflower("neutral", faceColor);
        } else { // Sofort gießen
            drawSunflower("sad", faceColor);
        }
    }

    // Daten auf SD Karte schreiben alle 4 Sekunden
    if (currentMillis - previousSensorUpdate >= sensorInterval) {
        previousSensorUpdate = currentMillis;

        int moistureValue = analogRead(MOISTURE_PIN);
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();

        if (!isnan(temperature) && !isnan(humidity)) {
            logDataToCSV(moistureValue, temperature, humidity);
        } else {
            Serial.println("Fehler beim Lesen eines Sensors!");
        }
    }
    
    // IoT Hub
    if (currentMillis - previousIoTHubUpdate >= IoTHubTimeout) {
        previousIoTHubUpdate = currentMillis;

        while (Serial1.available() > 0)
        {
            char c = Serial1.read(); // Zeichen vom GPS-Modul lesen
            gps.encode(c); // GPS-Daten verarbeiten
        }

        // Standardwert: FH Joanneum 
        double latitude = 47.06895;
        double longitude = 15.40643;

        if (gps.location.isValid())
        {
            latitude = gps.location.lat();
            longitude = gps.location.lng();
        }

        // Sensordaten abrufen
        int moistureValue = analogRead(MOISTURE_PIN);
        float temperature = dht.readTemperature();
        float humidity = dht.readHumidity();

        // JSON dokument mit Daten erstellen
        DynamicJsonDocument doc(1024);
        doc["deviceId"] = "Wio";
        doc["temperature"] = temperature;
        doc["humidity"] = humidity;
        doc["moisture"] = moistureValue;
        doc["latitude"] = latitude;
        doc["longitude"] = longitude;
        
        // Daten als String
        String telemetry;
        serializeJson(doc, telemetry);

        // Daten an Iot Hub senden
        Serial.print("Sending telemetry: ");
        Serial.println(telemetry.c_str());
        sendTelemetry(telemetry.c_str());
           
    }
    IoTHubDeviceClient_LL_DoWork(_device_ll_handle); 
}