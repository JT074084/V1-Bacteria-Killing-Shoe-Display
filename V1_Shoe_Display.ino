#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <AccelStepper.h>
#include <NeoPixelBus.h>

// WiFi
const char *ssid = "YOUR_SSID_HERE";
const char *password = "YOUR_PASSWORD_HERE";

const int LED = 2;

WebServer server(80);

// Servo
const int servoPin = 13;

Servo largeServo;

int currentAngle = 45;
int targetAngle = 0;

// LA Bed
const int pulPin = 23;
const int dirPin = 14;
const int enablePin = 27;

AccelStepper bedStepper(AccelStepper::DRIVER, pulPin, dirPin);

const int limitOutside = 19;
const int limitInside = 16;

// Fans
const int insideFan = 25;
const int outsideFan = 21;

// Display LEDs
const uint16_t TotalLeds = 68;
const uint16_t ActiveLeds = 68;
const uint8_t DataPin = 5;

NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> strip(TotalLeds, DataPin);

// UVC LEDS
const int uvcLEDs = 26;

// Shoe Sensor
const int shoeSensor = 18;

// Color State Enum
enum class ColorState { RED, GREEN, WHITE, YELLOW, NONE }; // This is basically a list, with each color having a number, like white == 0.
volatile ColorState mainColor = ColorState::NONE; // Shared variable 

// Mutex for critical sections
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // This is like making a room and setting the door to unlocked so only one task at a time can enter and change stuff

// Universal Button
const int button = 32;

// == GLOBAL VARIABLE
volatile bool disinfectTaskActive = false;
volatile bool disinfectTaskComplete = false;

// Task Handles
// == DEBUG TASK HANDLES ==
volatile bool debugMode = false; 
TaskHandle_t debugTaskHandle = NULL;
// == NORMAL OP TASK HANDLES ==
// Subtasks
TaskHandle_t ledSwitcherTaskHandle = NULL;
TaskHandle_t disinfectingTaskHandle = NULL;
TaskHandle_t electronicsCoolingTaskHandle = NULL;
// Main Normal Task
TaskHandle_t normalTaskHandle = NULL;
// == SWITCHER TASK HANDLES ==
TaskHandle_t switcherTaskHandle = NULL;

// == TASK FUNCTION ==
// Switcher Task
void switcherTask(void *parameter) {
  bool lastButtonState = HIGH;
  bool buttonState = HIGH;
  unsigned long pressStartTime = 0;
  bool hasSwitched = false;

  for (;;) {
    buttonState = digitalRead(button);

    if (lastButtonState == HIGH && buttonState == LOW) {
      pressStartTime = millis();
      hasSwitched = false;
    }

    if (buttonState == LOW && !hasSwitched && (millis() - pressStartTime >= 5000)) {
      debugMode = !debugMode;
      hasSwitched = true;
      pressStartTime = millis() + 60000;
      if (debugMode) {
        // Delete Subtasks
        if (ledSwitcherTaskHandle != NULL) {
          vTaskDelete(ledSwitcherTaskHandle);
          ledSwitcherTaskHandle = NULL;
        }
        if (electronicsCoolingTaskHandle != NULL) {
          // Turn off Fan
          coolingFanOff();
          vTaskDelete(electronicsCoolingTaskHandle);
          electronicsCoolingTaskHandle = NULL;
        }
        if (disinfectingTaskHandle != NULL) {
          vTaskDelete(disinfectingTaskHandle);
          disinfectingTaskHandle = NULL;
        }
        // Delete Main Tasks
        if (normalTaskHandle != NULL) {
          vTaskDelete(normalTaskHandle);
          normalTaskHandle = NULL;
        }
        xTaskCreatePinnedToCore(
          debugTask,
          "Debug Task",
          10000,
          NULL,
          5,
          &debugTaskHandle,
          1
        );
      } else {
        if (debugTaskHandle != NULL) {
          vTaskDelete(debugTaskHandle);
          debugTaskHandle = NULL;
        }
        xTaskCreatePinnedToCore(
          normalTask,
          "Normal Task",
          10000,
          NULL,
          5,
          &normalTaskHandle,
          1
        );
      }
    }

    lastButtonState = buttonState;
    vTaskDelay(50 / portTICK_PERIOD_MS);  // Add 50ms delay for debouncing
  }; 
}


// == NORMAL OPERATIONS TASKS
// SUBTASKS
// LED Controls Task
void ledSwitchTask(void *parameter) {
  int lightLevel = 0;
  const int unit = 2.55;
  const int levels = 100;
  const int time = 10;
  
  for (;;) {
    ColorState currentColor; // This just creates a local variable for the ColorState class
    // Safely read mainColor
    portENTER_CRITICAL(&mux);
    currentColor = mainColor;
    portEXIT_CRITICAL(&mux);

    // RED
    if (currentColor == ColorState::RED) {
      for (int a = 0; a < levels; a++) {
        for (uint16_t i = 0; i < TotalLeds; i++) {
          strip.SetPixelColor(i, RgbColor(lightLevel,0,0));
        }
        strip.Show();
        lightLevel += unit;
        vTaskDelay(time / portTICK_PERIOD_MS);
      }
      for (int a = 0; a < levels; a++) {
        for (uint16_t i = 0; i < TotalLeds; i++) {
          strip.SetPixelColor(i, RgbColor(lightLevel,0,0));
        }
        strip.Show();
        lightLevel -= unit;
        vTaskDelay(time / portTICK_PERIOD_MS);
      }
    // GREEN
    } else if (currentColor == ColorState::GREEN) {
      for (int a = 0; a < levels; a++) {
        for (uint16_t i = 0; i < TotalLeds; i++) {
          strip.SetPixelColor(i, RgbColor(0,lightLevel,0));
        }
        strip.Show();
        lightLevel += unit;
        vTaskDelay(time / portTICK_PERIOD_MS);
      }
      for (int a = 0; a < levels; a++) {
        for (uint16_t i = 0; i < TotalLeds; i++) {
          strip.SetPixelColor(i, RgbColor(0,lightLevel,0));
        }
        strip.Show();
        lightLevel -= unit;
        vTaskDelay(time / portTICK_PERIOD_MS);
      }
    // WHITE
    } else if (currentColor == ColorState::WHITE) {
      for (int a = 0; a < levels; a++) {
        for (uint16_t i = 0; i < TotalLeds; i++) {
          strip.SetPixelColor(i, RgbColor(lightLevel,lightLevel,lightLevel));
        }
        strip.Show();
        lightLevel += unit;
        vTaskDelay(time / portTICK_PERIOD_MS);
      }
      for (int a = 0; a < levels; a++) {
        for (uint16_t i = 0; i < TotalLeds; i++) {
          strip.SetPixelColor(i, RgbColor(lightLevel,lightLevel,lightLevel));
        }
        strip.Show();
        lightLevel -= unit;
        vTaskDelay(time / portTICK_PERIOD_MS);
      }
    // YELLOW
    } else if (currentColor == ColorState::YELLOW) {
      for (int a = 0; a < levels; a++) {
        for (uint16_t i = 0; i < TotalLeds; i++) {
          strip.SetPixelColor(i, RgbColor(lightLevel,lightLevel,0));
        }
        strip.Show();
        lightLevel += unit;
        vTaskDelay(time / portTICK_PERIOD_MS);
      }
      for (int a = 0; a < levels; a++) {
        for (uint16_t i = 0; i < TotalLeds; i++) {
          strip.SetPixelColor(i, RgbColor(lightLevel,lightLevel,0));
        }
        strip.Show();
        lightLevel -= unit;
        vTaskDelay(time / portTICK_PERIOD_MS);
      }
    }
  }
}
// Electronics Cooling Task
void electronicsCoolingTask(void *parameter) {
  for (;;) {
    vTaskDelay(600000 / portTICK_PERIOD_MS);
    coolingFanOn();
    vTaskDelay(300000 / portTICK_PERIOD_MS);
    coolingFanOff();
  }
}
// Disinfecting Task
void disinfectingTask(void *parameter) {
  // Sequence Test
  // Switch LED Color
  portENTER_CRITICAL(&mux);
  mainColor = ColorState::YELLOW;
  portEXIT_CRITICAL(&mux);

  // Actual Disinfecting Stuff
  chamberFanOn();
  for (int i = 0; i < 10; i++) {
    uvcOn();
    vTaskDelay(60000 / portTICK_PERIOD_MS);
    uvcOff();
    vTaskDelay(180000 / portTICK_PERIOD_MS);
  }
  vTaskDelay(1200000 / portTICK_PERIOD_MS);
  chamberFanOff();

  // == END OF DISINFECTING TASK ==
  // Set Disinfect Task to Unactive
  disinfectTaskActive = false;
  // Set task complete to true
  disinfectTaskComplete = true;

  // Delete Task
  if (disinfectingTaskHandle != NULL) {
    vTaskDelete(disinfectingTaskHandle);
    disinfectingTaskHandle = NULL;
  }
}
// MAIN NORMAL TASK
void normalTask(void *parameter) {
  // Setup
  // Create LED Task
  xTaskCreatePinnedToCore(
    ledSwitchTask,
    "Led Switcher Task",
    10000,
    NULL,
    5,
    &ledSwitcherTaskHandle,
    0
  );
  // Create Cooling Fan Task
  xTaskCreatePinnedToCore(
    electronicsCoolingTask,
    "Electronics Colling Task",
    10000,
    NULL,
    10,
    &electronicsCoolingTaskHandle,
    0
  );

  // Button press detection
  unsigned long pressStartTime = 0;
  bool buttonPressed = false;
  const unsigned long longPressTime = 5000;
  unsigned long buttonDuration = 0;

  bool doorOpened = false;

  bool phaseActive = false;

  // Door Open Timing
  unsigned long openTime = 0;
  const unsigned long closeLimit = 30000;

  // Shoe In Bay Check
  bool shoeInBay = false;

  bool noShoe = false;


  // Loop
  for (;;) {
    unsigned long currentTime = millis();

    // Auto Close
    if (doorOpened && (currentTime - openTime > closeLimit)) {
      // Change Color
      portENTER_CRITICAL(&mux);
      mainColor = ColorState::RED;
      portEXIT_CRITICAL(&mux);
      slideInBed();
      closeDoor();
      doorOpened = false;
      Serial.println("Auto-closed after timeout");
    }

    int buttonState = digitalRead(button);

    // Capture First Inital Press
    if (buttonState == LOW && !buttonPressed) {
      buttonPressed = true;
      pressStartTime = currentTime;
    }

    // See how long the press is
    if (buttonState == HIGH && buttonPressed) {
      buttonDuration = currentTime - pressStartTime;
      buttonPressed = false;

      // if the press is a short one, then we'll either open or close the doors
      if (buttonDuration < longPressTime) {  // Short press
        if (!doorOpened) {
          // First check to see if a disinfecting task was active and if so delete it
          if (disinfectTaskActive && disinfectingTaskHandle != NULL) {
            disinfectTaskActive = false;
            vTaskDelete(disinfectingTaskHandle);
            disinfectingTaskHandle = NULL;
            // Turn of LEDs and FANS
            uvcOff();
            chamberFanOff();
          }
          noShoe = false;
          disinfectTaskComplete = false;
          // Open sequence
          portENTER_CRITICAL(&mux);
          mainColor = ColorState::GREEN;
          portEXIT_CRITICAL(&mux);
          openDoor();
          slideOutBed();
          doorOpened = true;
          openTime = currentTime;
          Serial.println("Opened on short press");
        } else {
          // Close sequence
          portENTER_CRITICAL(&mux);
          mainColor = ColorState::RED;
          portEXIT_CRITICAL(&mux);
          slideInBed();
          closeDoor();
          doorOpened = false;
          Serial.println("Closed on short press");
        }
      } else {
        Serial.println("Entering Debug Mode (long press)");
      }

      buttonDuration = 0;  // Reset for next use
    }
    // Shoe Sensor Test
    if (digitalRead(shoeSensor) == LOW && !doorOpened && !disinfectTaskActive && !disinfectTaskComplete) {
      // Set Disinfect Task to Active
      disinfectTaskActive = true;
      // Setup Disinfecting Task
      xTaskCreatePinnedToCore(
        disinfectingTask,
        "Shoe Disinfecting Task",
        10000,
        NULL,
        20,
        &disinfectingTaskHandle,
        1
      );
    } else if (digitalRead(shoeSensor) == LOW && !doorOpened && !disinfectTaskActive && disinfectTaskComplete) {
      // Change Light Color to White
      portENTER_CRITICAL(&mux);
      mainColor = ColorState::WHITE;
      portEXIT_CRITICAL(&mux);
      // Reset Task Complete
      //disinfectTaskComplete = false;
    }

    // If no shoe is in the bay after closing then switch the LED color back to white
    if (digitalRead(shoeSensor) == HIGH && noShoe == false && !doorOpened) {
      portENTER_CRITICAL(&mux);
      mainColor = ColorState::WHITE;
      portEXIT_CRITICAL(&mux);
      noShoe = true;
    }
  }
}
// Debug Task
void debugTask(void *parameter) {
  // == HTTP REQUEST SETUP ==
  server.on("/doorOpen", debugOpenDoor);
  server.on("/doorClose", debugCloseDoor);
  server.on("/bedSlideIn", debugSlideInBed);
  server.on("/bedSlideOut", debugSlideOutBed);
  server.on("/chamberFanOn", debugChamberFanOn);
  server.on("/chamberFanOff", debugChamberFanOff);
  server.on("/coolingFanOn", debugCoolingFanOn);
  server.on("/coolingFanOff", debugCoolingFanOff);
  server.on("/uvcLEDsOn", debugUvcOn);
  server.on("/uvcLEDsOff", debugUvcOff);
  server.begin();

  // LED Update
  for (uint16_t i = 0; i < TotalLeds; i++) {
    strip.SetPixelColor(i, RgbColor(50,50,255));
    strip.Show();
    vTaskDelay(25 / portTICK_PERIOD_MS);
  }

  // Loop
  for (;;) {
    // Must Haves
    server.handleClient();
    ArduinoOTA.handle();
  }
}

// == FUNCTIONS == 
// == DOOR CONTROLS ==
// Debug Open Door
void debugOpenDoor() {
  openDoor();
  server.send(200, "text/plain", "Door is open");
}
// Open Servo Door
void openDoor() {
  targetAngle = 0;
  while (currentAngle > targetAngle) {
    currentAngle--;
    largeServo.write(currentAngle);
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

// Debug Close Door
void debugCloseDoor() {
  closeDoor();
  server.send(200, "text/plain", "Door is closed");
}
// Close Servo Door
void closeDoor() {
  targetAngle = 45;
  while (currentAngle < targetAngle) {
    currentAngle++;
    largeServo.write(currentAngle);
    vTaskDelay(30 / portTICK_PERIOD_MS);
  }
}

// == BED CONTROLS ==
// Debug Slide Bed In
void debugSlideInBed() {
  slideInBed();
  server.send(200, "text/plain", "The Bed Has Slid In");
}
// Slide Bed In
void slideInBed() {
  digitalWrite(enablePin, LOW); // Turn On Driver
  bool limitHit = false;
  // Set Direction
  long moveDistance = 179081;
  bedStepper.move(moveDistance);

  // Move until the limit switch is pressed
  while (bedStepper.distanceToGo() != 0 && !limitHit) {
    bedStepper.run();
    if (digitalRead(limitInside) == LOW) {
      limitHit = true;
    }
    taskYIELD();
  }

  if (limitHit) {
    digitalWrite(enablePin, HIGH); // Emergency Stop
  }
  bedStepper.setCurrentPosition(0);
}

// Debug Slide Bed Out
void debugSlideOutBed() {
  slideOutBed();
  server.send(200, "text/plain", "The Bed Has Slid Out");
}
// Slide Bed Out
void slideOutBed() {
  digitalWrite(enablePin, LOW); // Turn On Driver
  bool limitHit = false;
  // Set Direction
  long moveDistance = -179081;
  bedStepper.move(moveDistance);

  // Move until the limit switch is pressed
  while (bedStepper.distanceToGo() != 0 && !limitHit) {
    bedStepper.run();
    if (digitalRead(limitOutside) == LOW) {
      limitHit = true;
    }
    taskYIELD();
  }
  
  if (limitHit) {
    digitalWrite(enablePin, HIGH); // Emergency Stop
  }
  bedStepper.setCurrentPosition(0);
}

// == FAN CONTROLS ==
// Chamber Fan
// Debug Chamber Fan On
void debugChamberFanOn() {
  chamberFanOn();
  server.send(200, "text/plain", "Chamber Fan is On");
}
// Chamber Fan On
void chamberFanOn() {
  digitalWrite(insideFan, HIGH);
}
// Debug Chamber Fan Off
void debugChamberFanOff() {
  chamberFanOff();
  server.send(200, "text/plain", "Chamber Fan if Off");
}
// Chamber Fan Off
void chamberFanOff() {
  digitalWrite(insideFan, LOW);
}

// Cooling Fan
// Debug Cooling Fan On
void debugCoolingFanOn() {
  coolingFanOn();
  server.send(200, "text/plain", "Cooling Fan is On");
}
// Cooling Fan On
void coolingFanOn() {
  digitalWrite(outsideFan, HIGH);
}
// Debug Cooling Fan Off
void debugCoolingFanOff() {
  coolingFanOff();
  server.send(200, "text/plain", "Cooling Fan is Off");
}
// Cooling Fan Off
void coolingFanOff() {
  digitalWrite(outsideFan, LOW);
}

// UVC LEDs
// Debug UVC LEDs On
void debugUvcOn() {
  uvcOn();
  server.send(200, "text/plin", "The UVC LEDs Are On");
}
// UVC LEDs On
void uvcOn() {
  digitalWrite(uvcLEDs, HIGH);
}

// Debug UVC LEDs Off
void debugUvcOff() {
  uvcOff();
  server.send(200, "text/plain", "The UVC LEDs Are Off");
}
// UVC LEDs Off
void uvcOff() {
  digitalWrite(uvcLEDs, LOW);
}

// == SETUP ==
void setup() {
  Serial.begin(115200);
  // == PIN MODES ==
  pinMode(LED, OUTPUT);
  pinMode(servoPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  pinMode(limitOutside, INPUT_PULLUP);
  pinMode(limitInside, INPUT_PULLUP);
  pinMode(button, INPUT_PULLUP);
  pinMode(insideFan, OUTPUT);
  pinMode(outsideFan, OUTPUT);
  pinMode(uvcLEDs, OUTPUT);
  pinMode(shoeSensor, INPUT_PULLUP);

  // UVC LEDs
  digitalWrite(uvcLEDs, LOW);

  // FAN TESTING
  digitalWrite(insideFan, LOW);
  digitalWrite(outsideFan, LOW);

  // == SERVO SETUP ==
  largeServo.attach(servoPin);
  largeServo.write(currentAngle);

  // == WIFI SETUP ==
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // == OTA UPDATES SETUP ==
  // Setup OTA
  ArduinoOTA.onStart([]() {
    Serial.println("OTA: Start updating");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA: End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // == LINEAR ACTUATOR SETUP
  // Set maximum speed and acceleration
  bedStepper.setMaxSpeed(40000.0);
  bedStepper.setAcceleration(40000.0); 
  digitalWrite(enablePin, HIGH); // Turn off driver to save power

  // == DISPLAY LED SETUP ==
  strip.Begin();
  strip.Show();
  portENTER_CRITICAL(&mux);
  mainColor = ColorState::WHITE;
  portEXIT_CRITICAL(&mux);

  // == INITIAL TASK SETUP ==
  // Switcher Task
  xTaskCreatePinnedToCore(
    switcherTask, // Function
    "Switcher Task", // Name
    10000, // Size
    NULL, // Parameters
    1, // Prority
    &switcherTaskHandle, // Task Handle
    0 // Core
  );
  // Normal Operations Task
  xTaskCreatePinnedToCore(
    normalTask,
    "Normal Task",
    10000,
    NULL,
    5,
    &normalTaskHandle,
    1
  );

  // LED Update Test
  digitalWrite(LED, HIGH);
  delay(1000);
  digitalWrite(LED, LOW);
}

void loop() {
}
