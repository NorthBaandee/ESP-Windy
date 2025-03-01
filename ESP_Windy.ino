
#include <Wire.h>
#include <Adafruit_INA226.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

#define DUMP_LOAD_PIN 12

Adafruit_INA226 ina226;
WebServer server(80);

// Default settings
struct Settings {
  float absorptionVoltage = 34.0;   // Absorption voltage (34V for 20 NiFe cells)
  float floatVoltage = 32.0;        // Float voltage
  float hysteresis = 0.5;           // Hysteresis voltage
  bool manualOverride = false;      // Manual control override
  bool manualDumpLoad = false;      // Manual dump load state
};
Settings settings;

// WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

void setup() {
  Serial.begin(115200);
  pinMode(DUMP_LOAD_PIN, OUTPUT);

  // Initialize INA226
  if (!ina226.begin(0x40, &Wire, 0.01, 20)) { // 0.01 ohm shunt, 20A max
    Serial.println("Failed to find INA226 chip");
    while (1);
  }
  ina226.setAveragingCount(INA226_AVERAGES_16);
  ina226.setConversionTime(INA226_CONV_TIME_1100US);

  // Load settings from EEPROM
  EEPROM.begin(sizeof(settings));
  EEPROM.get(0, settings);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup web server
  server.on("/", handleRoot);
  server.on("/settings", handleSettings);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

void loop() {
  server.handleClient();
  
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    
    float busVoltage = ina226.getBusVoltage();
    float current = ina226.getCurrent();
    static bool dumpActive = false;

    // Control logic
    if (settings.manualOverride) {
      dumpActive = settings.manualDumpLoad;
    } else {
      if (busVoltage >= settings.absorptionVoltage) {
        dumpActive = true;
      } else if (busVoltage <= (settings.absorptionVoltage - settings.hysteresis)) {
        dumpActive = false;
      }
    }
    
    digitalWrite(DUMP_LOAD_PIN, dumpActive ? HIGH : LOW);
    
    // Logging
    Serial.printf("Voltage: %.2fV, Current: %.2fA, Dump Load: %s\n",
                  busVoltage, current, dumpActive ? "ON" : "OFF");
  }
}

void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body {font-family: Arial; margin: 20px;}";
  html += "h1 {color: #444;} .data {background: #f0f0f0; padding: 10px;}</style></head>";
  html += "<body><h1>Wind Turbine Controller</h1><div class='data'>";
  html += "<p>Battery Voltage: " + String(ina226.getBusVoltage(), 2) + " V</p>";
  html += "<p>Charge Current: " + String(ina226.getCurrent(), 2) + " A</p>";
  html += "<p>Dump Load State: " + String(digitalRead(DUMP_LOAD_PIN) ? "ON" : "OFF") + "</p>";
  html += "<p><a href='/settings'><button>Settings</button></a></p></div></body></html>";
  server.send(200, "text/html", html);
}

void handleSettings() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body {font-family: Arial; margin: 20px;} form {background: #f0f0f0; padding: 20px;}";
  html += "input[type=number] {width: 100px; margin: 5px 0;} button {margin-top: 10px;}</style></head>";
  html += "<body><h1>Controller Settings</h1><form action='/save' method='post'>";
  html += "Absorption Voltage (V):<br><input type='number' step='0.1' name='abs' value='" + String(settings.absorptionVoltage, 1) + "'><br>";
  html += "Float Voltage (V):<br><input type='number' step='0.1' name='float' value='" + String(settings.floatVoltage, 1) + "'><br>";
  html += "Hysteresis (V):<br><input type='number' step='0.1' name='hyst' value='" + String(settings.hysteresis, 1) + "'><br>";
  html += "<input type='checkbox' name='override' " + (settings.manualOverride ? "checked" : "") + "> Manual Override<br>";
  html += "<input type='checkbox' name='manual' " + (settings.manualDumpLoad ? "checked" : "") + "> Manual Dump Load<br>";
  html += "<input type='submit' value='Save'></form></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("abs")) settings.absorptionVoltage = server.arg("abs").toFloat();
  if (server.hasArg("float")) settings.floatVoltage = server.arg("float").toFloat();
  if (server.hasArg("hyst")) settings.hysteresis = server.arg("hyst").toFloat();
  
  settings.manualOverride = server.hasArg("override");
  settings.manualDumpLoad = server.hasArg("manual");

  EEPROM.put(0, settings);
  EEPROM.commit();
  
  server.sendHeader("Location", "/");
  server.send(303);
}
```

**Key Features:**
1. Monitors battery voltage and charge current using INA226
2. Automatic dump load control with hysteresis
3. Manual override via web interface
4. Settings persistence using EEPROM
5. Responsive web interface accessible via WiFi

**Setup Instructions:**
1. Replace `YOUR_SSID` and `YOUR_PASSWORD` with your WiFi credentials
2. Connect components:
   - INA226 to I2C pins (SDA=21, SCL=22 typically)
   - Dump load control to GPIO12
   - Wind turbine input and battery connections to INA226
3. Ensure proper shunt resistor (0.01Ω, 20A+ rating) for current sensing
4. Size dump load appropriately for your turbine's maximum power

**Web Interface:**
- Access via ESP32's IP address
- View real-time voltage and current
- Adjust voltage settings and control modes
- Manual override capability

**Safety Considerations:**
- Ensure proper heat dissipation for dump load
- Use appropriately rated components for your system voltage/current
- Consider adding hardware-based overvoltage protection as backup
- Secure WiFi network for production use

This implementation provides basic charge control with web-based monitoring and configuration. For production use, additional safety features and input validation should be added.
