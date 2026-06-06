#include <WiFi.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const int fanPin = 5;
const int tachPin = 18;
const int sensorPin = 32;
const int mosfetPin = 4;

//Wifi Setup
const char* ssid = "Wifi Name";
const char* password = "Wifi Password";
WiFiServer server(80);

// DHT Setup
#define DHTTYPE DHT11
DHT dht(sensorPin, DHTTYPE);

// PWM Configuration
const int freq = 25000;
const int resolution = 8;

// Tachometer
volatile unsigned long pulseCount = 0;
unsigned long lastRPMTime = 0;
unsigned int rpm = 0;

// Timer for millis() function
unsigned long lastControlTime = 0;
unsigned long lastDisplayTime = 0;
const unsigned long CONTROL_INTERVAL = 2000;
const unsigned long DISPLAY_INTERVAL = 500;

// Temperature hysteresis
const float highLimit = 25.0;
const float lowLimit = 24.0;
bool fanON = false;
float temperature = 0.0;
int speed = 0;

// Interrupt Service Routine
void IRAM_ATTR countPulse() {
  pulseCount++;
}

// Fan speed
int calculateFanSpeed(float temp) {
  if (temp < highLimit)       return 100;
  if (temp < (highLimit + 1)) return 150;
  if (temp < (highLimit + 2)) return 200;
  return 255;
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(mosfetPin, OUTPUT);
  digitalWrite(mosfetPin, LOW); // start OFF

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED allocation failed"));
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);

  // PWM
  ledcAttach(fanPin, freq, resolution);

  // Configure Tachometer Interrupt
  pinMode(tachPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(tachPin), countPulse, FALLING);
  
  lastRPMTime = millis();

  //Connect Wifi
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  unsigned long currentTime = millis();

  //Calculates RPM every 1 second
  if (currentTime - lastRPMTime >= 1000) {
    noInterrupts();
    unsigned long pulses = pulseCount;
    pulseCount = 0;
    interrupts();

    unsigned long elapsedTime = currentTime - lastRPMTime;
    lastRPMTime = currentTime;

    // RPM = (Pulses / time_in_seconds) * 60 seconds / 2 pulses per revolution
    rpm = (pulses * 30000UL) / (unsigned long)elapsedTime; 
  }

  //Reads DHT Sensor every 2 seconds
  if (currentTime - lastControlTime >= CONTROL_INTERVAL) {
    lastControlTime = currentTime;

    float rawTemp = dht.readTemperature();
    if (!isnan(rawTemp)) {
      temperature = rawTemp;

      if (temperature < lowLimit) fanON = false;
      if (temperature > highLimit) fanON = true;

      digitalWrite(mosfetPin, fanON ? HIGH : LOW);

      if (fanON) {
        speed = calculateFanSpeed(temperature);
        ledcWrite(fanPin, speed);
      } 
      else {
        speed = 0;
        ledcWrite(fanPin, 0);
      }
    }
  }

  // Display
  if (currentTime - lastDisplayTime >= DISPLAY_INTERVAL) {
    lastDisplayTime = currentTime;

    // Serial Telemetry
    Serial.print("Temp: "); Serial.print(temperature);
    Serial.print(" C | Speed: "); Serial.print(speed);
    Serial.print(" | RPM: "); Serial.println(rpm);

    // OLED Update
    display.clearDisplay();
    display.setTextSize(1);
    
    display.setCursor(0, 0);
    display.print("Temp: "); display.print(temperature); display.print(" C");
    
    display.setCursor(0, 16);
    display.print("PWM:  "); display.print(speed);
    
    display.setCursor(0, 32);
    display.print("RPM:  "); display.print(rpm);

    display.setCursor(0, 48);
    display.print("IP: ");
    display.print(WiFi.localIP());
    
    display.display();
  }

  if (client) {
    Serial.println("New Client Connected.");
    String request = "";

    if (client.available()) {
      request = client.readStringUntil('\r');
    }

    while (client.connected() && client.available()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") { 
        break; // Headers ended
      }
    }

    //For live data
    if (request.indexOf("/data") != -1) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Connection: close");
      client.println();

      client.print("{\"temp\":");
      client.print(temperature);
      client.print(",\"rpm\":");
      client.print(rpm);
      client.print(",\"pwm\":");
      client.print(speed);
      client.println("}");
    } 

    //Web page
    else {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type: text/html");
      client.println("Connection: close");
      client.println();

      client.println("<!DOCTYPE html><html>");

      client.println("<head>");
      client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
      client.println("<title>ESP32 Fan Dashboard</title>");

      client.println("<style>");

      client.println("body {");
      client.println("  font-family: Arial;");
      client.println("  background:#111;");
      client.println("  color:white;");
      client.println("  text-align:center;");
      client.println("  margin:0;");
      client.println("  padding:20px;");
      client.println("}");

      client.println("h2 {");
      client.println("  font-size: 34px;");
      client.println("  margin-bottom: 20px;");
      client.println("}");

      client.println(".card {");
      client.println("  background:#222;");
      client.println("  margin:15px auto;");
      client.println("  padding:22px;");
      client.println("  width:90%;");
      client.println("  max-width:380px;");
      client.println("  border-radius:16px;");
      client.println("  box-shadow:0 6px 14px rgba(0,0,0,0.6);");
      client.println("}");

      client.println("h3 {");
      client.println("  font-size: 22px;");
      client.println("  margin-bottom: 8px;");
      client.println("}");

      client.println(".value {");
      client.println("  font-size: 32px;");
      client.println("  font-weight: bold;");
      client.println("  color:#00ffcc;");
      client.println("}");

      client.println("</style>");

      client.println("</head>");

      client.println("<body>");

      client.println("<h2>ESP32 Fan Dashboard</h2>");

      client.println("<div class='card'>");
      client.println("<h3>Temperature</h3>");
      client.println("<div class='value'><span id='t'>--</span> C</div>");
      client.println("</div>");

      client.println("<div class='card'>");
      client.println("<h3>RPM</h3>");
      client.println("<div class='value'><span id='r'>--</span></div>");
      client.println("</div>");

      client.println("<div class='card'>");
      client.println("<h3>PWM</h3>");
      client.println("<div class='value'><span id='p'>--</span></div>");
      client.println("</div>");

      // LIVE UPDATE SCRIPT
      client.println("<script>");
      client.println("setInterval(() => {");
      client.println("  fetch('/data')");
      client.println("    .then(r => r.json())");
      client.println("    .then(d => {");
      client.println("      document.getElementById('t').innerText = d.temp;");
      client.println("      document.getElementById('r').innerText = d.rpm;");
      client.println("      document.getElementById('p').innerText = d.pwm;");
      client.println("    }).catch(err => console.log('Fetch error:', err));");
      client.println("}, 1000);");
      client.println("</script>");

      client.println("</body></html>");
    }

    delay(1);
    client.stop();
    Serial.println("Client Disconnected.");
  }
}