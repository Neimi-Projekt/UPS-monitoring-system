#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// LCDs Konfiguration
LiquidCrystal_I2C lcd1(0x26, 20, 4);  // Temperatur
LiquidCrystal_I2C lcd2(0x27, 20, 4);  // Versorgungsspannung

// Temperatursensor Konfiguration
#define ONE_WIRE_BUS 32
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);  // Übergibt den Bus an die Sensor-Bibliothek

#define AC_STATUS_PIN 14

#define SIM_RX 16          // Empfangs-Pin des ESP32
#define SIM_TX 17          // Sende-Pin des ESP32
HardwareSerial sim800(2);  // Nutzt den Serial-Port 2 des ESP32

const char* TELEFONNUMMER = "+4915757181343";

bool smsTemperaturGesendet = false;    // Verhindert wiederholte SMS bei Übertemperatur
bool smsStromausfallGesendet = false;  // Verhindert wiederholte SMS bei Netzausfall

void setup() {

  // LCDs initialisieren
  Wire.begin();
  lcd1.init();
  lcd1.backlight();
  lcd2.init();
  lcd2.backlight();

  // Startmeldung auf LCDs
  lcd1.setCursor(0, 0);
  lcd1.print("Systemtemperatur");
  lcd1.setCursor(0, 2);
  lcd1.print("System startet...");

  lcd2.setCursor(0, 0);
  lcd2.print("Versorgungsspannung");
  lcd2.setCursor(0, 2);
  lcd2.print("System startet...");

  sensors.begin();  // Temperatursensor starten


  pinMode(AC_STATUS_PIN, INPUT_PULLDOWN);  // der Pin ist LOW, wenn kein Signal anliegt

  sim800.begin(9600, SERIAL_8N1, SIM_RX, SIM_TX);  // SIM800C Modul starten
  delay(4000);

  lcd1.clear();
  lcd2.clear();
}

void loop() {
  // 1. Netzversorgung Überwachen
  bool stromDa = (digitalRead(AC_STATUS_PIN) == HIGH);

  // LCD für Netzversorgung aktualisieren
  lcd2.setCursor(0, 0);
  lcd2.print("Versorgungsspannung");

  lcd2.setCursor(0, 1);
  lcd2.print("                    ");

  lcd2.setCursor(0, 2);

  if (stromDa) {
    lcd2.print("ist AN 230 V      ");
    smsStromausfallGesendet = false;  // Merker zurücksetzen (System ist wieder okay)
  } else {
    lcd2.print("ist AUS 0 V       ");

    // SMS bei Netzausfall senden
    if (!smsStromausfallGesendet) {
      sendeSMS("Achtung! Versorgungsspannung ist Aus (USV-Nr. 0314)");
      smsStromausfallGesendet = true;  // Merker setzen: SMS wurde versendet!
    }
  }

  lcd2.setCursor(0, 3);
  lcd2.print("                    ");

  // Tem. Überwachen
  sensors.requestTemperatures();                  // Befehl an Sensor: "Miss jetzt!
  float temperatur = sensors.getTempCByIndex(0);  // Ergebnis abholen

  // 85°C Startwert ignorieren
  if (temperatur > 84.9 && temperatur < 85.1) {
    lcd1.setCursor(0, 0);
    lcd1.print("Systemtemperatur");
    lcd1.setCursor(0, 1);
    lcd1.print("                    ");
    lcd1.setCursor(0, 2);
    lcd1.print("Messung laeuft...  ");
    lcd1.setCursor(0, 3);
    lcd1.print("                    ");
    delay(1000);
    return;  // Diese Messung überspringen und Loop neu starten
  }

  // LCD für Temperatur aktualisieren
  lcd1.setCursor(0, 0);
  lcd1.print("Systemtemperatur");

  lcd1.setCursor(0, 1);
  lcd1.print("                    ");

  lcd1.setCursor(0, 2);  // Zeile 3: Status + Temperatur

  char displayText[21];  // Vorlage für die Display-Ausgabe

  if (temperatur <= 25.0) {  // %4.1f = % Zahl, 4 Stellen, ein nachkommastell., float Kommazahl

    snprintf(displayText, sizeof(displayText), "ist NORMAL %4.1f C    ", temperatur);
    lcd1.print(displayText);
    smsTemperaturGesendet = false;  // Reset für nächsten Alarm
  } else if (temperatur < 30.0) {

    snprintf(displayText, sizeof(displayText), "ist INSTABIL %4.1f C  ", temperatur);
    lcd1.print(displayText);
    smsTemperaturGesendet = false;
  } else {
    //
    snprintf(displayText, sizeof(displayText), "ist HOCH %4.1f C      ", temperatur);
    lcd1.print(displayText);

    // SMS bei hoher Temperatur senden
    if (!smsTemperaturGesendet) {
      sendeSMS("Achtung! Systemtemperatur ist Hoch (USV-Nr. 0314)");
      smsTemperaturGesendet = true;  // Merker setzen, damit SMS nur 1x kommt
    }
  }
  lcd1.setCursor(0, 3);
  lcd1.print("                    ");

  delay(1000);
}

void sendeSMS(const char* nachricht) {  // SMS-Versand

  // AT-Befehle, um das Modem zu steuern
  sim800.println("AT"); 
  delay(1000);
  sim800.println("AT+CMGF=1");  // set sms system into text mode
  delay(1000);

  sim800.print("AT+CMGS=\"");  // Befehl zum SMS-Senden einleiten
  sim800.print(TELEFONNUMMER);
  sim800.println("\"");
  delay(1000);

  // Nachricht senden
  sim800.print(nachricht); // Schreibt den Text der SMS in das SIM-Modul
  delay(500);

  // SMS abschicken (Ctrl+Z = ASCII 26)
  sim800.write(26); // Der "Absende-Knopf" 
  delay(500);

}