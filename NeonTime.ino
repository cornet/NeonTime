#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// https://github.com/tzapu/WiFiManager + deps
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

// https://github.com/datacute/DoubleResetDetector
#include <DoubleResetDetector.h>

#include <NTPClient.h>
#include <TimeLib.h>
#include <Timezone.h>

// https://github.com/thebigpotatoe/Effortless-SPIFFS
#include <Effortless_SPIFFS.h>

// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

// File to save update interval to
#define UPDATE_INTERVAL_FILE "/update_interval.txt"
#define INITIAL_UPDATE_DELAY_FILE "/initial_update.txt"
#define CLOCK_BAUD_RATE_FILE "/baud_rate.txt"

// How often to update clock in seconds
int updateIntervalMin = 1;

// How long to delay before doing initial update
int initialUpdateDelaySec = 10;

// Clock serial baud rate
int clockBaudRate = 2400;

// Detect resetting of device twice within a timout.
// We will use this to enter configuration mode.
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

// NTP returns UTC so we track both UTC & UK Time.
time_t UKTime, Utc;

// Europe/London timezone definition.
TimeChangeRule BST = {"BST", Last, Sun, Mar, 1, 60};
TimeChangeRule GMT = {"GMT", Last, Sun, Oct, 2, 0};
Timezone UK(BST, GMT);

// Used to send/receive UDP messages.
WiFiUDP ntpUDP;

// NTP Client instance.
NTPClient timeClient(ntpUDP);

// Track last time we updated clock
unsigned long prevUpdateMillis = 0;

// Flag so we know when we've done the intial update
// to prevent issues when millis() rolls over.
bool initialUpdateDone = false;

// Flag for saving data
bool shouldSaveConfig = false;

// Filesystem for storing the config
eSPIFFS fileSystem;

// Callback notifying us of the need to save config
void saveConfigCallback () {
  shouldSaveConfig = true;
}

void setup() {
  // Turn the onboard LED on to indicate we are in setup mode.
  // It will get turned off once Serial1 is enabled as they share the same pin.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // USB Serial for debug
  Serial.begin(9600);

  read_config();

  // WifiManager returns a char[] so we need to pass
  // it one as default.
  char curUpdateInterval[11];
  sprintf(curUpdateInterval, "%d", updateIntervalMin);

  char curInitialUpdateDelay[11];
  sprintf(curInitialUpdateDelay, "%d", initialUpdateDelaySec);

  char curClockBaudRate[11];
  sprintf(curClockBaudRate, "%d", clockBaudRate);

  // Define custom input paramters
  WiFiManagerParameter custom_update_interval(
      "update_interval", "Update Interval",
      curUpdateInterval, sizeof(curUpdateInterval)
      );

  WiFiManagerParameter custom_initial_update_delay(
      "initial_update_delay", "Intial Update Delay",
      curInitialUpdateDelay, sizeof(curInitialUpdateDelay)
      );

  WiFiManagerParameter custom_baud_rate(
      "baud_rate", "Baud Rate",
      curClockBaudRate, sizeof(curClockBaudRate)
      );

  // Define custom header paramters
  WiFiManagerParameter custom_update_interval_header(
      "<b>Update Interval (minutes)</b>"
  );

  WiFiManagerParameter custom_initial_update_delay_header(
      "<b>Initial Update Delay (seconds)</b>"
  );

  WiFiManagerParameter custom_baud_rate_header(
      "<b>Baud Rate</b>"
  );

  // Configure WiFi
  WiFiManager wifiManager;

  // Reset Save Settings
  // wifiManager.resetSettings();

  // Set config save callback function
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Add custom parameters
  wifiManager.addParameter(&custom_initial_update_delay_header);
  wifiManager.addParameter(&custom_initial_update_delay);

  wifiManager.addParameter(&custom_update_interval_header);
  wifiManager.addParameter(&custom_update_interval);

  wifiManager.addParameter(&custom_baud_rate_header);
  wifiManager.addParameter(&custom_baud_rate);

  // Launch config portal wifi settings if reset twice in 10s
  if (drd.detectDoubleReset()) {
    wifiManager.startConfigPortal("NeonTime");
  } else {
    // This launches the config portal if:
    // - There are no Wifi network saved
    // - It can't connect to any saved Wifi networks
    wifiManager.autoConnect("NeonTime");
  }

  // If config has changed the write it out to filesystem
  if (shouldSaveConfig) {
    Serial.println("Config changed, saving...");
    if (fileSystem.saveToFile(UPDATE_INTERVAL_FILE, custom_update_interval.getValue())) {
      Serial.println("Successfully wrote update interval");
    }

    if (fileSystem.saveToFile(INITIAL_UPDATE_DELAY_FILE, custom_initial_update_delay.getValue())) {
      Serial.println("Successfully wrote intial update delay");
    }

    if (fileSystem.saveToFile(CLOCK_BAUD_RATE_FILE, custom_baud_rate.getValue())) {
      Serial.println("Successfully wrote clock baud rate");
    }

    // Read config again to get correct value
    read_config();
  }

  // Connected to the Clock (Pin D4)
  Serial1.begin(clockBaudRate);

  // Start the NTP client
  timeClient.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  // Run inital update
  if ((!initialUpdateDone) && (currentMillis >= (initialUpdateDelaySec * 1000))) {
    update_clock();
    initialUpdateDone = true;
    prevUpdateMillis = currentMillis;
  }

  // Run periodic update
  if (currentMillis - prevUpdateMillis >= (updateIntervalMin * 60 * 1000)) {
    update_clock();
    prevUpdateMillis = currentMillis;
  }

  // Update the double reset detector
  drd.loop();
}

void read_config() {
  // Read all the configuration files from the filesystem

  if (fileSystem.openFromFile(UPDATE_INTERVAL_FILE, updateIntervalMin)) {
    Serial.print("Successfully read updated interval: ");
  } else {
    Serial.print("Failed to read update interval, using previous value of: ");
  }
  Serial.println(updateIntervalMin);

  if (fileSystem.openFromFile(INITIAL_UPDATE_DELAY_FILE, initialUpdateDelaySec)) {
    Serial.print("Successfully read inital update delay: ");
  } else {
    Serial.print("Failed to read initial update delay, using previous value of: ");
  }
  Serial.println(initialUpdateDelaySec);

  if (fileSystem.openFromFile(CLOCK_BAUD_RATE_FILE, clockBaudRate)) {
    Serial.print("Successfully read clock baud rate: ");
  } else {
    Serial.print("Failed to read clock baud rate, using previous value of: ");
  }
  Serial.println(clockBaudRate);
}

void update_clock() {
    timeClient.update();
    setTime(UK.toLocal(timeClient.getEpochTime()));

    // timeset = 081220212123
    //           DDMMYYHHMMSS
    Serial.printf("%02d%02d%02d%02d%02d%02d", day(), month(), year() % 100, hour(), minute(), second());
    Serial1.printf("\x1b\x53%02d%02d%02d%02d%02d%02d\x0d", day(), month(), year() % 100, hour(), minute(), second());
}
