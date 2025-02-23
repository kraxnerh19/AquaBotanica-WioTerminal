# AquaBotanica - Wio Terminal Project

Dieses Projekt nutzt das Wio Terminal um Umgebungsdaten wie Temperatur, Luftfeuchtigkeit und Bodenfeuchtigkeit zu messen und anzuzeigen, sowie die Daten an die Azure Cloud zu übertragen.

## Aufbau Wio Terminal
   
### **1. Bodenfeuchtigkeitssensor**

| **Sensor-Pin** | **Funktion**         | **Wio Terminal-Anschluss** |
|----------------|----------------------|----------------------------|
| **VCC**        | Versorgungsspannung  | **Pin 2 (5V)**             |
| **GND**        | Masse                | **Pin 6 (GND)**            |
| **SIG**        | Analogsignal         | **Pin 16 (A2)**            |

### **2. DHT-Sensor**

Schließe den Sensor an den analogen Grove-Port (rechter Grove-Port A0) des Wio Terminals an.

### **3. RTC Sensor**

| **RTC-Pin** | **Funktion**         | **Wio Terminal-Anschluss** |
|-------------|----------------------|----------------------------|
| **VCC**     | Versorgungsspannung  | **Pin 1 (3.3V)**           |
| **GND**     | Masse                | **Pin 9 (GND)**            |
| **SDA**     | I²C-Datenleitung     | **Pin 3 (SDA)**            |
| **SCL**     | I²C-Taktleitung      | **Pin 5 (SCL)**            |

### **4. Relai**

| **Sensor-Pin** | **Funktion**         | **Wio Terminal-Anschluss** |
|-------------|----------------------|----------------------------|
| **VCC**     | Versorgungsspannung  | **Pin 17 (3.3V)**          |
| **GND**     | Masse                | **Pin 34 (GND)**           |
| **SIG**     | Digitalsignal        | **Pin 33 (D6)**            |

### **5. Time of Flight Distance Sensor**

Schließe den Sensor an den I²C Grove-Port (linker Grove-Port) des Wio Terminals an.

### **4. GPS Sensor**

| **Sensor-Pin** | **Funktion**         | **Wio Terminal-Anschluss** |
|-------------|----------------------|----------------------------|
| **VCC**     | Versorgungsspannung  | **Pin 4 (5V)**             |
| **GND**     | Masse                | **Pin 14 (GND)**           |
| **RX**      | Daten senden         | **Pin 8 (TX)**             |
| **TX**      | Daten empfangen      | **Pin 10 (RX)**            |

---

## Vorbereitung

1. **Vergewissere dich, dass PlatformIO installiert ist.**
   - Installiere PlatformIO in [Visual Studio Code](https://platformio.org/install/ide?install=vscode).

2. **Schließe das Wio Terminal mit einem USB-Kabel an deinen Computer an.**

3. **Klone das Repository:**
   ```bash
   git clone https://github.com/kraxnerh19/AquaBotanica-WioTerminal.git

4. **Umgebungsvariablen definieren:**

   Öffne die Datei `config.h` im Projektverzeichnis.

   Ändere die WLAN-Logindaten und Azure IoT Hub Konfiguration in den folgenden Zeilen, um deine SSID und dein Passwort sowie den IoT Hub Connection String einzutragen:

   ```cpp
   const char *ssid = "<dein-WLAN-Name>";
   const char *password = "<dein-WLAN-Passwort>";
   const char *CONNECTION_STRING = "<dein-IoT-Hub Connection String>";

  ---

## Projekt auf das Wio Terminal hochladen
1. **Upload-Modus aktivieren:**

   Schalte das Terminal über den Schieberegler rechts ein und drücke diesen 2-mal nach unten, die blaue LED sollte pulsieren.
3. **Build Projekt:**

   Zunächst muss das Projekt auf Fehler überprüft werden.
   ```bash
   pio run
5. **Upload Projekt:**

   Lade das Projekt aufs WIO Terminal hoch, um es zu nutzen.
   ```bash
   pio run --target upload
   
   
