// my epic smart switch firmware
// version 40.0 i guess

#include <ESP8266WiFi.h>
#include <ESP8266WDT.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <time.h>
#include <Ticker.h>

// ------ MQTT STUFF ------
const char* mqtt_server = "eda1d166.ala.eu-central-1.emqxsl.com";
const int   mqtt_port = 8883;
const char* mqtt_user = "Giorgi_bedroom_switch";
const char* mqtt_pass = "gbjpX2vKTBv96Dg";

char deviceID[20];
char topicStatus[50];
char topicCommand[50];

// ------ PINS ------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

const int SERVO_PIN      = D4;
const int PIR_PIN        = D5;
const int ON_OFF_BTN_PIN = D6;
const int MENU_BTN_PIN   = D7;

// ------ TIMINGS AND STUFF ------
const unsigned long BUTTON_DEBOUNCE_MS = 50;
const unsigned long LONG_PRESS_MS = 1000;
const unsigned long SCREEN_SLEEP_TIMEOUT_MS = 30000; // 30 secs
const unsigned long NTP_SYNC_INTERVAL_S = 3600; // sync time every hour
const unsigned long EEPROM_SAVE_DELAY_MS = 15000; // wait 15s to save settings

// ------ SETTINGS THAT GET SAVED ------
struct Configuration {
    uint32_t magic_key; // to check if eeprom is fresh
    unsigned int SERVO_ON_ANGLE = 135;
    unsigned int SERVO_OFF_ANGLE = 22;
    unsigned long HESITATION_DURATION_S = 20;
    unsigned int SUNSET_OFFSET_MINUTES = 10;
    unsigned long MANUAL_OVERRIDE_TIMEOUT_M = 1;
    bool motionSensorEnabled = true;
    char owmCity[32] = "Bristol";
    char owmCountryCode[4] = "GB";
    bool quietHoursEnabled = false;
    int quietHoursStart = 2300;
    int quietHoursEnd = 600;
    unsigned long motionTimeout_M = 7;
    bool alarmEnabled = false;
    int alarmTime = 700;
    byte alarmDays = 0b0111110; // M T W T F
};
Configuration config;
bool configIsDirty = false;

// ------ GLOBAL OBJECTS AND VARS ------
Servo myServo;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);
Ticker timeBasedScheduler, statusPublisher, configSaveTicker, ntpSyncTicker;
WiFiManager wifiManager;

// NTP clock state
time_t lastNtpSyncTime = 0;
unsigned long lastNtpSyncMillis = 0;

// UI state
enum DisplayScreen { SCREEN_HOME, SCREEN_MAIN_MENU, SCREEN_CONTROL, SCREEN_INFO, SCREEN_TOGGLE_MOTION, SCREEN_CONFIRM_RESET };
DisplayScreen currentScreen = SCREEN_HOME;
int menuSelection = 0;
bool displayNeedsUpdate = true;
bool displayIsAsleep = false;
unsigned long lastUiInteractionTime = 0;

// System state
enum SystemMode { AUTO, MANUAL, BEDTIME, HESITATION, AWAY, DAY_IDLE } currentMode = DAY_IDLE;
volatile bool lightState = false;
bool alarmTriggeredToday = false;
unsigned long lastMotionTime = 0, hesitationStartTime = 0, manualModeStartTime = 0;
uint32_t autoStartTimeSunset = 0, autoEndTimeSunrise = 0;

// Button state
struct ButtonState {
    const uint8_t PIN;
    volatile bool pressed = false;
    volatile unsigned long pressTime = 0;
};
ButtonState onOffButton = {ON_OFF_BTN_PIN};
ButtonState menuButton = {MENU_BTN_PIN};


// ------ ICONS ------
const unsigned char wifi_icon_16x8[] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x07, 0xf8, 0x1f, 0xfc, 0x7f, 0xfe, 0xf8, 0x1f, 0xf0, 0x0f, 0x60, 0x06 };
const unsigned char app_icon_16x8[] PROGMEM = { 0x3f, 0xfc, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x7f, 0xfe, 0x3f, 0xfc };


// ------ FUNCTION PROTOTYPES (makes the compiler happy) ------
const char* modeToString(SystemMode mode);
void logMessage(const char* msg);
void factoryReset();
void syncNtpTime();
time_t getCurrentTime();
bool isDayTime();
void setScreen(DisplayScreen newScreen);
void drawHomeScreen();
void drawMenu(const char* title, const char* items[], int numItems);
void handleDisplay();
void wakeDisplay();
void checkButtons();
void publishStatus();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnect();
void loadConfiguration();
void saveConfiguration();
void commitConfigToEEPROM();
void masterScheduler();
void runStateMachine();
void handleAutoMode();
void handleHesitationMode();
void checkWakeUpAlarm();
void checkManualOverrideTimeout();
void areInQuietHours();
void setLightState(bool newState);
void setSystemMode(SystemMode newMode);
void fetchWeatherData();
void setup_OTA();

// ======================= SETUP =======================
void setup() {
    Serial.begin(115200);
    logMessage("\n\nBooting up...");
    pinMode(ON_OFF_BTN_PIN, INPUT_PULLUP);
    pinMode(MENU_BTN_PIN, INPUT_PULLUP);

    // hold the on/off button while it boots to factory reset
    if (digitalRead(ON_OFF_BTN_PIN) == LOW) {
        delay(3000);
        if (digitalRead(ON_OFF_BTN_PIN) == LOW) { factoryReset(); }
    }
    WDT.begin(5000); // watchdog so it doesn't crash forever

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        logMessage("display failed lol");
        for(;;);
    }
    display.drawBitmap(32, 16, app_icon_16x8, 16, 8, 1); display.drawBitmap(80, 16, wifi_icon_16x8, 16, 8, 1);
    display.display();
    delay(2000);

    EEPROM.begin(sizeof(Configuration));
    loadConfiguration();
    snprintf(deviceID, sizeof(deviceID), "SwitchMote-%06X", ESP.getChipId());
    snprintf(topicStatus, sizeof(topicStatus), "switchmote/%s/status", deviceID);
    snprintf(topicCommand, sizeof(topicCommand), "switchmote/%s/command", deviceID);

    wifiManager.setConnectTimeout(15); wifiManager.setConfigPortalTimeout(180);
    // this html is for the wifi setup page
    const char* custom_head = R"rawliteral(<meta name=viewport content="width=device-width,initial-scale=1"><style>body{background-color:#1f1f1f;color:#efefef;font-family:-apple-system,sans-serif;text-align:center}h1{color:#5c97ff;margin-top:30px}.c,.b{border-radius:8px;border:1px solid #555;padding:12px;margin-bottom:10px;width:80%;max-width:400px;box-sizing:border-box}.b{background-color:#5c97ff;color:#fff;text-decoration:none;font-weight:700;font-size:1.1em}input{background-color:#333;color:#fff}a{color:#5c97ff}</style><h1>Smart Light Setup</h1><p>Please connect this device to your home Wi-Fi network.</p>)rawliteral";
    wifiManager.setCustomHeadElement(custom_head);
    wifiManager.setAPCallback([](WiFiManager *myWiFiManager) { logMessage("CONFIG MODE: Connect to 'SmartLight_Setup'."); displayNeedsUpdate = true; });
    if (!wifiManager.autoConnect("SmartLight_Setup")) {
        logMessage("portal timed out, rebooting");
        delay(3000);
        ESP.restart();
    }

    char buffer[64]; snprintf(buffer, sizeof(buffer), "WiFi connected! IP: %s", WiFi.localIP().toString().c_str()); logMessage(buffer);

    wifiClient.setInsecure();
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    setup_OTA();

    if(WiFi.status() == WL_CONNECTED) {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        syncNtpTime();
        fetchWeatherData();
    }

    lightState = true; setLightState(false); // set to off
    timeBasedScheduler.attach(1, masterScheduler);
    statusPublisher.attach(60, publishStatus);
    ntpSyncTicker.attach(NTP_SYNC_INTERVAL_S, syncNtpTime);
    lastUiInteractionTime = millis();
}

// ======================= MAIN LOOP =======================
void loop() {
    WDT.feed(); // pet the watchdog
    ArduinoOTA.handle(); // for wireless updates
    checkButtons();
    runStateMachine();
    handleDisplay();
    if (WiFi.status() == WL_CONNECTED) {
        if (!mqttClient.connected()) { reconnect(); }
        mqttClient.loop();
    }
}


// --- DISPLAY & UI ---
void setScreen(DisplayScreen newScreen) { if (currentScreen != newScreen) { currentScreen = newScreen; menuSelection = 0; displayNeedsUpdate = true; } }
void drawHomeScreen() { display.clearDisplay(); display.setTextColor(WHITE); time_t now = getCurrentTime(); if (now > 0) { struct tm* timeinfo = localtime(&now); char timeBuf[6]; snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min); display.setTextSize(2); display.setCursor(38, 0); display.print(timeBuf); } else { display.setTextSize(1); display.setCursor(25, 4); display.print("Syncing Time..."); } display.drawBitmap(0, 0, wifi_icon_16x8, 16, 8, WiFi.status() == WL_CONNECTED ? 1 : 0); display.drawBitmap(112, 0, app_icon_16x8, 16, 8, mqttClient.connected() ? 1 : 0); display.setTextSize(1); display.setCursor(0, 22); display.print(F("Mode: ")); display.print(modeToString(currentMode)); display.setCursor(0, 32); display.print(F("Motion: ")); display.print(config.motionSensorEnabled ? "On" : "Off"); display.setTextSize(2); display.setCursor(5, 50); display.print(F("Light is ")); display.print(lightState ? F("ON") : F("OFF")); }
void drawMenu(const char* title, const char* items[], int numItems) { display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE); display.setCursor(0, 0); display.println(title); display.drawLine(0, 10, 127, 10, WHITE); for (int i = 0; i < numItems; i++) { display.setCursor(5, 18 + i * 12); if (i == menuSelection) { display.print(F("> ")); } display.println(items[i]); } }
void handleDisplay() { if (millis() - lastUiInteractionTime > SCREEN_SLEEP_TIMEOUT_MS && !displayIsAsleep) { display.ssd1306_command(SSD1306_DISPLAYOFF); displayIsAsleep = true; } if (!displayNeedsUpdate) return; const char* mainMenuItems[] = {"<- Back to Home", "Live Control", "Toggle Motion", "Device Info", "Factory Reset"}; const char* controlMenuItems[] = {"<- Back to Menu", "Set Auto", "Set Bedtime", "Set Away"}; const char* confirmMenuItems[] = {"NO (Cancel)", "YES (Reset)"}; switch (currentScreen) { case SCREEN_HOME: drawHomeScreen(); break; case SCREEN_MAIN_MENU: drawMenu("--- Main Menu ---", mainMenuItems, 5); break; case SCREEN_CONTROL: drawMenu("--- Control ---", controlMenuItems, 4); break; case SCREEN_TOGGLE_MOTION: config.motionSensorEnabled = !config.motionSensorEnabled; saveConfiguration(); setScreen(SCREEN_MAIN_MENU); break; case SCREEN_INFO: display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE); display.setCursor(0, 0); display.println(F("--- Device Info ---")); display.setCursor(0, 12); display.println(deviceID); display.setCursor(0, 22); display.println(WiFi.localIP().toString()); display.setCursor(0, 32); display.print(F("Ver: ")); display.println(FW_VERSION); break; case SCREEN_CONFIRM_RESET: drawMenu("Reset ALL settings?", confirmMenuItems, 2); break; } display.display(); displayNeedsUpdate = false; }


// --- BUTTONS ---
void wakeDisplay() { if (displayIsAsleep) { display.ssd1306_command(SSD1306_DISPLAYON); displayIsAsleep = false; lastUiInteractionTime = millis(); displayNeedsUpdate = true; } }
void checkButtons() {
    static unsigned long lastPress = 0;
    if (millis() - lastPress < BUTTON_DEBOUNCE_MS) return;

    // on/off button logic
    if (digitalRead(onOffButton.PIN) == LOW) {
        lastPress = millis();
        wakeDisplay();
        if (displayIsAsleep) return; // first press just wakes screen
        logMessage("ON/OFF button pressed");
        setSystemMode(MANUAL);
        manualModeStartTime = millis();
        setLightState(!lightState);
        lastUiInteractionTime = millis();
    }

    // menu button logic (this part is kinda complex)
    static unsigned long menuPressTime = 0;
    static bool menuWasPressed = false;
    bool menuReading = (digitalRead(menuButton.PIN) == LOW);

    if (menuReading && !menuWasPressed) { // pressed
        menuPressTime = millis();
        menuWasPressed = true;
    } else if (!menuReading && menuWasPressed) { // released
        wakeDisplay();
        if (displayIsAsleep) { menuWasPressed = false; return; }
        lastUiInteractionTime = millis();
        if (millis() - menuPressTime < LONG_PRESS_MS) { // this was a short press
            logMessage("Menu button SHORT press");
            switch (currentScreen) {
                case SCREEN_HOME: setScreen(SCREEN_MAIN_MENU); break;
                case SCREEN_MAIN_MENU: menuSelection = (menuSelection + 1) % 5; displayNeedsUpdate = true; break;
                case SCREEN_CONTROL: menuSelection = (menuSelection + 1) % 4; displayNeedsUpdate = true; break;
                case SCREEN_INFO: setScreen(SCREEN_MAIN_MENU); break;
                case SCREEN_CONFIRM_RESET: menuSelection = (menuSelection + 1) % 2; displayNeedsUpdate = true; break;
            }
        }
        menuWasPressed = false;
    } else if (menuReading && menuWasPressed) { // held down
        if (millis() - menuPressTime > LONG_PRESS_MS) {
            wakeDisplay();
            if (displayIsAsleep) return;
            logMessage("Menu button LONG press");
            lastUiInteractionTime = millis();
            switch (currentScreen) {
                case SCREEN_HOME: setScreen(SCREEN_MAIN_MENU); break;
                case SCREEN_MAIN_MENU:
                    if (menuSelection == 0) setScreen(SCREEN_HOME);
                    if (menuSelection == 1) setScreen(SCREEN_CONTROL);
                    if (menuSelection == 2) setScreen(SCREEN_TOGGLE_MOTION);
                    if (menuSelection == 3) setScreen(SCREEN_INFO);
                    if (menuSelection == 4) setScreen(SCREEN_CONFIRM_RESET);
                    break;
                case SCREEN_CONTROL:
                    if (menuSelection == 0) setScreen(SCREEN_MAIN_MENU);
                    if (menuSelection == 1) { setSystemMode(AUTO); setScreen(SCREEN_HOME); }
                    if (menuSelection == 2) { setSystemMode(BEDTIME); setScreen(SCREEN_HOME); }
                    if (menuSelection == 3) { setSystemMode(AWAY); setScreen(SCREEN_HOME); }
                    break;
                case SCREEN_INFO: setScreen(SCREEN_MAIN_MENU); break;
                case SCREEN_CONFIRM_RESET:
                    if (menuSelection == 0) setScreen(SCREEN_MAIN_MENU); // NO
                    if (menuSelection == 1) factoryReset(); // YES
                    break;
            }
            menuWasPressed = false; // reset
        }
    }
}

// --- TIMEKEEPING ---
void syncNtpTime() { time_t now = time(nullptr); if (now > 1609459200) { lastNtpSyncTime = now; lastNtpSyncMillis = millis(); logMessage("NTP time synced."); displayNeedsUpdate = true; } else { logMessage("NTP sync failed, will retry."); } }
time_t getCurrentTime() { if (lastNtpSyncTime == 0) return 0; return lastNtpSyncTime + ((millis() - lastNtpSyncMillis) / 1000); }

// --- CORE LOGIC & STATE MACHINE ---
void setSystemMode(SystemMode newMode) { if (currentMode == newMode) return; currentMode = newMode; char buffer[48]; snprintf(buffer, sizeof(buffer), "System mode set to: %s", modeToString(currentMode)); logMessage(buffer); if (currentMode == HESITATION) hesitationStartTime = millis(); if (currentMode != MANUAL) manualModeStartTime = 0; if (currentMode == BEDTIME || currentMode == AWAY) { setLightState(false); } displayNeedsUpdate = true; publishStatus(); }
void setLightState(bool newState) { if (lightState == newState) return; lightState = newState; if (!myServo.attached()) myServo.attach(SERVO_PIN); myServo.write(lightState ? config.SERVO_ON_ANGLE : config.SERVO_OFF_ANGLE); Ticker tempTicker; tempTicker.once_ms(1000, [] { myServo.detach(); }); logMessage(lightState ? "Light -> ON" : "Light -> OFF"); displayNeedsUpdate = true; publishStatus(); }
void saveConfiguration() { configIsDirty = true; configSaveTicker.once_ms(EEPROM_SAVE_DELAY_MS, commitConfigToEEPROM); logMessage("Config updated, saving soon."); display.clearDisplay();display.setTextSize(2);display.setCursor(25,25);display.print("Saving...");display.display();delay(1000); publishStatus(); }
void commitConfigToEEPROM() { if (configIsDirty) { logMessage("Saving config to EEPROM..."); EEPROM.put(0, config); if (EEPROM.commit()) { logMessage("Save successful."); configIsDirty = false; } else { logMessage("EEPROM save failed!"); } } }
void factoryReset() { logMessage("FACTORY RESETTING"); Configuration defaultConfig; config = defaultConfig; config.magic_key = 123456789; commitConfigToEEPROM(); wifiManager.resetSettings(); logMessage("WiFi erased, rebooting..."); display.clearDisplay();display.setTextSize(2);display.setCursor(20,25);display.print("Resetting...");display.display(); delay(2000); ESP.restart(); }
void masterScheduler(){ checkManualOverrideTimeout(); time_t now_ts = getCurrentTime(); if (now_ts == 0) return; struct tm* timeinfo = localtime(&now_ts); checkWakeUpAlarm(); if (timeinfo->tm_hour == 3 && timeinfo->tm_min == 0) { if (currentMode != BEDTIME && currentMode != AWAY) { logMessage("3 AM Failsafe: BEDTIME mode"); setSystemMode(BEDTIME); } } if (isDayTime()) { if (currentMode == AUTO || currentMode == HESITATION) { logMessage("Sunrise, Day Idle mode."); setSystemMode(DAY_IDLE); } } else { if (currentMode == DAY_IDLE) { logMessage("Sunset, Auto mode."); setSystemMode(AUTO); } } displayNeedsUpdate = true; }
void runStateMachine(){ switch (currentMode) { case AUTO: handleAutoMode(); break; case HESITATION: handleHesitationMode(); break; default: break; } }
void handleAutoMode(){ if (!config.motionSensorEnabled || currentMode != AUTO) return; if (areInQuietHours()) { if(lightState) { logMessage("Quiet hours, turning light off."); setLightState(false); } return; } if (digitalRead(PIR_PIN) == HIGH) { lastMotionTime = millis(); if (!lightState) { logMessage("Motion detected, light ON"); setLightState(true); } } else { if (lightState && (millis() - lastMotionTime > (config.motionTimeout_M * 60 * 1000L))) { logMessage("Timeout, hesitating..."); setSystemMode(HESITATION); } } }
void checkWakeUpAlarm(){ time_t now_ts = getCurrentTime(); if (now_ts == 0 || !config.alarmEnabled) return; struct tm* timeinfo = localtime(&now_ts); if (timeinfo->tm_hour == 0 && timeinfo->tm_min <= 1) { alarmTriggeredToday = false; } if (alarmTriggeredToday) return; int dayOfWeek = timeinfo->tm_wday; int currentTime = timeinfo->tm_hour * 100 + timeinfo->tm_min; if (currentTime >= config.alarmTime && currentTime < config.alarmTime + 5) { if ((config.alarmDays >> dayOfWeek) & 1) { logMessage("Wake-Up Alarm!"); setSystemMode(MANUAL); manualModeStartTime = 0; setLightState(true); alarmTriggeredToday = true; } } }
const char* modeToString(SystemMode mode) { const char* modes[] = {"AUTO", "MANUAL", "BEDTIME", "HESITATION", "AWAY", "DAY IDLE"}; return modes[mode]; }
bool isDayTime() { time_t now_ts = getCurrentTime(); if (now_ts == 0 || autoStartTimeSunset == 0) return true; return (now_ts >= autoEndTimeSunrise && now_ts < autoStartTimeSunset); }
void logMessage(const char* msg) { Serial.println(msg); }
void loadConfiguration() { EEPROM.get(0, config); if (config.magic_key != 123456789) { logMessage("EEPROM empty, loading defaults"); Configuration defaultConfig; config = defaultConfig; config.magic_key = 123456789; configIsDirty = true; commitConfigToEEPROM(); } else { logMessage("Loaded config from EEPROM."); } }
bool areInQuietHours() { time_t now_ts = getCurrentTime(); if (now_ts == 0 || !config.quietHoursEnabled) return false; struct tm* timeinfo = localtime(&now_ts); int currentTime = timeinfo->tm_hour * 100 + timeinfo->tm_min; if (config.quietHoursStart > config.quietHoursEnd) return (currentTime >= config.quietHoursStart || currentTime < config.quietHoursEnd); else return (currentTime >= config.quietHoursStart && currentTime < config.quietHoursEnd); }
void handleHesitationMode(){ if (digitalRead(PIR_PIN) == HIGH) { logMessage("Hesitation cancelled by motion."); lastMotionTime = millis(); setSystemMode(AUTO); return; } if (millis() - hesitationStartTime > (config.HESITATION_DURATION_S * 1000L)) { logMessage("Hesitation over. Light OFF."); setLightState(false); setSystemMode(isDayTime() ? DAY_IDLE : AUTO); } }
void checkManualOverrideTimeout() { if (currentMode == MANUAL && manualModeStartTime > 0) { if (millis() - manualModeStartTime > (config.MANUAL_OVERRIDE_TIMEOUT_M * 60 * 1000L)) { logMessage("Manual Override expired."); setSystemMode(isDayTime() ? DAY_IDLE : AUTO); } } }
void fetchWeatherData() { if (WiFi.status() != WL_CONNECTED) return; WiFiClient client; HTTPClient http; http.setTimeout(8000); char buffer[128]; snprintf(buffer, sizeof(buffer), "http://api.openweathermap.org/data/2.5/weather?q=%s,%s&appid=%s", config.owmCity, config.owmCountryCode, "e20c6a9a37404d9726371c943d7da0ba"); if (http.begin(client, buffer)) { snprintf(buffer, sizeof(buffer), "Fetching weather for %s...", config.owmCity); logMessage(buffer); int httpCode = http.GET(); if (httpCode == HTTP_CODE_OK) { DynamicJsonDocument doc(2048); DeserializationError error = deserializeJson(doc, http.getString()); if (!error) { autoEndTimeSunrise = doc["sys"]["sunrise"].as<uint32_t>() + doc["timezone"].as<long>(); autoStartTimeSunset = doc["sys"]["sunset"].as<uint32_t>() + doc["timezone"].as<long>() + (config.SUNSET_OFFSET_MINUTES * 60); logMessage("Weather data updated."); } else logMessage("JSON Deserialization failed!"); } else { snprintf(buffer, sizeof(buffer), "HTTP GET failed, error: %d", httpCode); logMessage(buffer); } http.end(); } else logMessage("HTTP connection failed!"); }
void setup_OTA() { ArduinoOTA.onStart([] { logMessage("OTA Update Started..."); }); ArduinoOTA.onEnd([] { logMessage("OTA Update Finished!"); }); ArduinoOTA.onProgress([](unsigned int, unsigned int) {}); ArduinoOTA.onError([](ota_error_t) { logMessage("OTA Error"); }); ArduinoOTA.setHostname("ota_hostname_placeholder"); ArduinoOTA.begin(); } // idk what to put for hostname
void reconnect() { static unsigned long lastReconnectAttempt = 0; if (millis() - lastReconnectAttempt > 5000) { lastReconnectAttempt = millis(); logMessage("Attempting MQTT connection..."); displayNeedsUpdate = true; if (mqttClient.connect(deviceID, mqtt_user, mqtt_pass)) { logMessage("MQTT connected!"); mqttClient.subscribe(topicCommand); publishStatus(); } else { char buffer[64]; snprintf(buffer, sizeof(buffer), "MQTT failed, rc=%d", mqttClient.state()); logMessage(buffer); } } }
void mqttCallback(char* topic, byte* payload, unsigned int length) { char p[length + 1]; memcpy(p, payload, length); p[length] = '\0'; logMessage("--- MQTT Command Received ---"); logMessage(p); DynamicJsonDocument doc(1024); if (deserializeJson(doc, p)) { logMessage("JSON parsing failed!"); return; } const char* action = doc["action"]; if (!action) { logMessage("'action' key missing!"); return; } if (strcmp(action, "setLight") == 0) { logMessage("Action: setLight"); setSystemMode(MANUAL); manualModeStartTime = 0; setLightState(strcmp(doc["value"], "on") == 0); } else if (strcmp(action, "setMode") == 0) { logMessage("Action: setMode"); const char* value = doc["value"]; if (strcmp(value, "auto") == 0) setSystemMode(AUTO); else if (strcmp(value, "bedtime") == 0) setSystemMode(BEDTIME); else if (strcmp(value, "away") == 0) setSystemMode(AWAY); } else if (strcmp(action, "reboot") == 0) { logMessage("Action: reboot"); delay(1000); ESP.restart(); } else if (strcmp(action, "factory_reset") == 0) { logMessage("Action: factory_reset"); factoryReset(); } else if (strcmp(action, "calibrate") == 0) { logMessage("Action: calibrate"); int angle = doc["angle"]; if (!myServo.attached()) myServo.attach(SERVO_PIN); myServo.write(constrain(angle, 0, 180)); } else if (strcmp(action, "saveConfig") == 0) { logMessage("Action: saveConfig"); JsonObject data = doc["data"]; config.motionSensorEnabled = data["motionEnabled"]; config.SERVO_ON_ANGLE = constrain(data["servoOn"].as<int>(), 0, 180); config.SERVO_OFF_ANGLE = constrain(data["servoOff"].as<int>(), 0, 180); config.HESITATION_DURATION_S = constrain(data["hesitationS"].as<int>(), 0, 300); config.SUNSET_OFFSET_MINUTES = constrain(data["sunsetOffsetM"].as<int>(), -120, 120); config.MANUAL_OVERRIDE_TIMEOUT_M = data["manualTimeoutM"]; strlcpy(config.owmCity, data["city"], sizeof(config.owmCity)); strlcpy(config.owmCountryCode, data["country"], sizeof(config.owmCountryCode)); config.quietHoursEnabled = data["quietHoursEnabled"]; config.quietHoursStart = data["quietHoursStart"]; config.quietHoursEnd = data["quietHoursEnd"]; config.motionTimeout_M = data["motionTimeoutM"]; config.alarmEnabled = data["alarmEnabled"]; config.alarmTime = data["alarmTime"]; config.alarmDays = data["alarmDays"]; saveConfiguration(); } else if (strcmp(action, "getStatus") == 0) { logMessage("Action: getStatus"); publishStatus(); } else { logMessage("Unknown action!"); } logMessage("--- MQTT Command Done ---"); }
void publishStatus() { if (!mqttClient.connected()) return; DynamicJsonDocument doc(2048); doc["lightState"] = lightState; doc["systemMode"] = modeToString(currentMode); doc["uptime"] = millis(); doc["wifiSignal"] = WiFi.RSSI(); doc["freeHeap"] = ESP.getFreeHeap(); time_t now = getCurrentTime(); if (now > 0) { struct tm* timeinfo = localtime(&now); char timeBuf[9]; snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec); doc["rtcTime"] = timeBuf; } else { doc["rtcTime"] = "N/A"; } doc["motionEnabled"] = config.motionSensorEnabled; doc["servoOn"] = config.SERVO_ON_ANGLE; doc["servoOff"] = config.SERVO_OFF_ANGLE; doc["hesitationS"] = config.HESITATION_DURATION_S; doc["sunsetOffsetM"] = config.SUNSET_OFFSET_MINUTES; doc["manualTimeoutM"] = config.MANUAL_OVERRIDE_TIMEOUT_M; doc["city"] = config.owmCity; doc["country"] = config.owmCountryCode; doc["quietHoursEnabled"] = config.quietHoursEnabled; doc["quietHoursStart"] = config.quietHoursStart; doc["quietHoursEnd"] = config.quietHoursEnd; doc["motionTimeoutM"] = config.motionTimeout_M; doc["alarmEnabled"] = config.alarmEnabled; doc["alarmTime"] = config.alarmTime; doc["alarmDays"] = config.alarmDays; doc["version"] = FW_VERSION; String response; serializeJson(doc, response); mqttClient.publish(topicStatus, response.c_str(), true); }
