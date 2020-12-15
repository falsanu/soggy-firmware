/**
 * Soggy Firmware v.0.2.0
 */

#include "config.h"


#include <ArduinoJson.h> // https://arduinojson.org/v6/how-to/use-arduinojson-with-httpclient/
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_NeoPixel.h>

#define TRIGGER 4 // D1
#define ECHO 5		//  D2
#define SOILPIN A0
#define PUMP 16			// D0
#define STATUSLED 2 // -> D4

Adafruit_NeoPixel strip = Adafruit_NeoPixel(1, STATUSLED, NEO_RGB);

/*
   Bluetooth Stack INIT BEGIN
*/
// DSD Tech BT-PCB has to be powered with 5V!

#include <SparkFun_HM1X_Bluetooth_Arduino_Library.h>
#include <SoftwareSerial.h>
#define HM1X_HARDWARE_SERIAL_ENABLED // Enable hardware serial

#define INT_RX 14 // D5
#define INT_TX 12 // D6
bool BT_ENABLED = false;
SoftwareSerial hm13Serial(INT_RX, INT_TX); // RX, TX on Arduino

/*
   Bluetooth Stack INIT END
*/

// Soil Moisture Setup
const int airValue = 859;
const int waterValue = 457;

int soilMoistureValue = 0;
int soilMoisturePercent = 0;

// WIFI Setup
const char *ssid = SSID_NAME;
const char *wifiPassword = WIFI_PW;

// API Setup
const String loginEndpoint = LOGIN_ENDPOINT;
const String dataEndpoint = DATA_ENDPOINT;
const String lowWaterEndpoint = LOWWATER_ENDPOINT;
const String loginUsername= USER;
const String loginPasswort = PASS;
String jwt = "";


// Measuring Setup
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

// Pump Setup
const int moistureThreshold = 75; //all values below this trigger the pump to pump
const int pumpDuration = 1000;

// Water Level Setup
long distance;
long waterHeight = 20;
long minWaterHeight = 2;
float waterLevel;				 // in percent
float minWaterLevel = 5; //percent

// Plant Setup
const char *plantID = "5fd7fb5b281b39048b97bc21";

long calcWaterLevel() {
  long wh = ((waterHeight - minWaterHeight - distance) * 100) / (waterHeight - minWaterHeight);
  return wh;
}

void setupLED()
{
  strip.begin();
  strip.clear();
  strip.setPixelColor(0, 0, 255, 0);

  strip.setBrightness(64);
  strip.show();
}
void setupBluetooth()
{
  if (BT_ENABLED)
  {
    Serial.println("Starting Bluetooth");
    hm13Serial.begin(9600);
    Serial.println("Ready to Bluetooth!");
  }
}

void readBluetooth()
{
  if (BT_ENABLED)
  {
    // put your main code here, to run repeatedly:
    //sendSensorData(plantID, 12, 100, false);
    //login();
    hm13Serial.listen(); // listen the HM10 port
    while (hm13Serial.available() > 0)
    { // if HM10 sends something then read
      Serial.write(hm13Serial.read());
    }

    if (Serial.available())
    { // Read user input if available.
      hm13Serial.write(Serial.read());
    }
  }
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(9600);

  Serial.println("##################################################");
  Serial.println("                 SOGGY V 0.2.0 Starting           ");
  Serial.println("##################################################");
  setupLED();
  //setupBluetooth();

  pinMode(TRIGGER, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(PUMP, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);

  WiFi.begin(ssid, wifiPassword);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    strip.clear();
    strip.setPixelColor(0, 26, 136, 214); //warm yellow
    strip.show();

    delay(500);
    Serial.print(".");
    strip.clear();
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
}

void measuringLED()
{
  strip.clear();
  strip.setBrightness(64);
  strip.setPixelColor(0, 214, 170, 26); //warm yellow
  strip.show();
}
void operationalLED()
{
  strip.clear();
  strip.setBrightness(64);
  strip.setPixelColor(0, 89, 214, 26); // warm green
  strip.show();
}
void greenLED()
{
  strip.clear();
  strip.setBrightness(255);
  strip.setPixelColor(0, 0, 255, 0); // hot green
  strip.show();
}
void pumpLED()
{
  strip.clear();
  strip.setBrightness(64);
  strip.setPixelColor(0, 0, 206, 252); // cool blue
  strip.show();
}

void errorLED()
{
  strip.clear();
  strip.setBrightness(64);
  strip.setPixelColor(0, 214, 26, 51); // funky red
  strip.show();
}


void logRequest(int code, String url) {
  Serial.print(code);
  Serial.print(" -- ");
  Serial.println(url);
}



void login()
{
  DynamicJsonDocument doc(2048);
  HTTPClient http;
  http.begin(loginEndpoint);
  http.addHeader("Content-Type", "application/json");
  String data = "{\"username\":\"" + loginUsername + "\", \"password\":\"" + loginPasswort + "\"}";
  int httpResponseCode = http.POST(data);
  logRequest(httpResponseCode, loginEndpoint);
  if (httpResponseCode == 200)
  {
    deserializeJson(doc, http.getStream());
    jwt = doc["token"].as<String>();
    Serial.println(doc.as<String>());
  }
  else
  {
    Serial.print("Could not login");
  }
}



void sendSensorData(String plantID, int soilMoisturePercent, long waterLevel, bool willWater)
{
  HTTPClient http;
  http.begin(dataEndpoint);
  http.addHeader("Content-Type", "application/json");
  // here we could check for header initially (to save one request)
  http.addHeader("Authorization", jwt);

  String data = "{\"plantId\":\"" + plantID + "\",\"hygro\":\"" + soilMoisturePercent + "\",\"waterLevel\":\"" + waterLevel + "\",\"willWater\":\"" + willWater + "\"}";
  int httpResponseCode = http.POST(data);
  
  logRequest(httpResponseCode, dataEndpoint);

  if (httpResponseCode == 401)
  { // Unauthorized
    Serial.print("Authorization failed");
    login();
    sendSensorData(plantID, soilMoisturePercent, distance, willWater);
  }
  else
  {
    if (httpResponseCode != 200)
    {
      greenLED();
      delay(500);
      operationalLED();
    }
    else
    {
      errorLED();
      delay(500);
    }
  }
}

// Inform Server about low waterlevel
void sendLowWaterRequest(String plantID, long waterLevel)
{
  HTTPClient http;
  http.begin(lowWaterEndpoint);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", jwt);
  String data = "{\"plantId\":\"" + plantID + "\",\"waterLevel\":\"" + waterLevel + "\"}";
  int httpResponseCode = http.POST(data);

  logRequest(httpResponseCode, lowWaterEndpoint);
  if (httpResponseCode != 200)
  {
    errorLED();
  }
  else
  {

    greenLED();
    delay(500);
    operationalLED();
  }
}



void loop()
{
  //readBluetooth();

  //measure every {timerDelay} seconds
  if ((millis() - lastTime) > timerDelay)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      bool willWater = false;

      measuringLED();

      measureDistance();

      measureSoil();

      if (soilMoisturePercent < moistureThreshold)
      {
        willWater = true;
        waterLevel = calcWaterLevel();
        Serial.print("WaterLevel : ");
        Serial.print(waterLevel);
        Serial.print(" -- ");
        Serial.print(minWaterLevel);
        Serial.println("minWaterlevel");
        if (waterLevel > minWaterLevel)
        {
          activatePump();
        }
        else
        {
          // low water send notification
          sendLowWaterRequest(plantID, waterLevel);
        }
      }

      sendSensorData(plantID, soilMoisturePercent, waterLevel, willWater);
    }
    else
    {
      Serial.println("WiFi Disconnected");
      errorLED();
    }
    lastTime = millis();
  }
}


void measureSoil()
{
  // Measure Soil
  soilMoistureValue = analogRead(SOILPIN); //put Sensor insert into soil
  soilMoisturePercent = map(soilMoistureValue, airValue, waterValue, 0, 100);
  Serial.print("Value:");
  Serial.print(soilMoistureValue);
  Serial.print(" / ");
  Serial.print(soilMoisturePercent);
  Serial.println("%");
}
void measureDistance()
{
  long duration;
  digitalWrite(TRIGGER, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIGGER, LOW);
  duration = pulseIn(ECHO, HIGH);
  distance = (duration / 2) / 29.1;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println("cm ");
}

void activatePump()
{
  pumpLED();
  digitalWrite(PUMP, HIGH);
  delay(pumpDuration); // wait for a pump duration befor switching it off
  digitalWrite(PUMP, LOW);
  operationalLED();
}
