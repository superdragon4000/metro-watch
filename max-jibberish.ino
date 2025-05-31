#include <LedControl.h>
#include <RTClib.h>

// Pins for MAX7219: DIN, CLK, CS (LOAD)
LedControl lc = LedControl(11, 13, 5, 1); // DIN, CLK, CS, 1 device

// Buttons
#define BUTTON_MODE 2
#define BUTTON_UP   3
#define BUTTON_DOWN 4

RTC_DS3231 rtc;

// Constants
const int HOURS_IN_DAY = 24;
const int MINUTES_IN_HOUR = 60;
const unsigned long SCREEN_OFF_DELAY = 10000;
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long HOLD_DELAY = 500;
const unsigned long REPEAT_RATE = 100;
const unsigned long BLINK_INTERVAL = 1000;
const unsigned long MODE_HOLD_DURATION = 1000; // 1 second hold for timer

// Screen & timer state
bool screenOn = true;
bool showTime = true;
bool wasScreenOff = true;

unsigned long lastInteraction = 0;
unsigned long lastBlink = 0;
bool colonState = true;

// Timer
unsigned long startTime = 0;
int timerSeconds = 0;
bool timerRunning = false;

// Button hold
unsigned long buttonHoldTime = 0;
unsigned long lastRepeat = 0;
bool isButtonPressed = false;

// Button states
bool lastButtonState = HIGH;
bool modeButtonHeld = false; // Tracks if BUTTON_MODE is held

void setup() {
  Serial.begin(9600);

  lc.shutdown(0, false);   // Wake up MAX7219
  lc.setIntensity(0, 15);  // Brightness max
  lc.clearDisplay(0);      // Clear display

  pinMode(BUTTON_MODE, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop() {
  DateTime now = rtc.now();

  handleButtons();

  // Auto screen off after inactivity
  if ((millis() - lastInteraction > SCREEN_OFF_DELAY) && !timerRunning) {
    screenOn = false;
    showTime = true; // Switch back to clock mode when idle
  }

  if (screenOn) {
    if (wasScreenOff) {
      displayJibberish();
      wasScreenOff = false;
    }

    if (showTime) {
      updateDisplay(now.hour(), now.minute());
    } else {
      if (timerRunning) {
        unsigned long currentTime = millis();
        int remaining = timerSeconds - (currentTime - startTime) / 1000;
        if (remaining > 0) {
          int minutes = remaining / MINUTES_IN_HOUR;
          int seconds = remaining % MINUTES_IN_HOUR;
          updateDisplay(minutes, seconds);
        } else {
          timerRunning = false;
          showTime = true; // Timer done, back to clock
        }
      }
    }
  } else {
    lc.clearDisplay(0); // Screen off
    wasScreenOff = true;
  }

  if (screenOn && (millis() - lastBlink >= BLINK_INTERVAL)) {
    lastBlink = millis();
    colonState = !colonState;
  }
}

// Handle button logic
void handleButtons() {
  static unsigned long modeButtonPressTime = 0;

  // Handle BUTTON_MODE logic
  if (digitalRead(BUTTON_MODE) == LOW) { // Button pressed
    if (!modeButtonHeld) {
      modeButtonPressTime = millis();
      modeButtonHeld = true;
    }

    // Check for hold
    if (millis() - modeButtonPressTime >= MODE_HOLD_DURATION) {
      if (!timerRunning) {
        // Start timer on hold when not running
        startTimer(5 * MINUTES_IN_HOUR); // 5 minutes
        showTime = false;
      } else {
        // Return to clock on hold if timer is running
        timerRunning = false;
        showTime = true;
      }
    }
  } else { // Button released
    if (modeButtonHeld) {
      modeButtonHeld = false;

      // Check for short press
      if ((millis() - modeButtonPressTime) < MODE_HOLD_DURATION) {
        if (!screenOn) {
          // Short press wakes up the display
          screenOn = true;
          lastInteraction = millis();
        }
      }
    }
  }

  // BUTTON_UP and BUTTON_DOWN logic (unchanged)
  if (digitalRead(BUTTON_UP) == LOW) {
    handleButtonPress("hours", 1);
  } else if (digitalRead(BUTTON_DOWN) == LOW) {
    handleButtonPress("minutes", 1);
  } else {
    resetButtonState();
  }
}

void handleButtonPress(String unit, int adjustment) {
  if (!isButtonPressed) {
    adjustTime(unit, adjustment);
    isButtonPressed = true;
    buttonHoldTime = millis();
    lastRepeat = millis();
    lastInteraction = millis();
  } else if ((millis() - buttonHoldTime >= HOLD_DELAY) && (millis() - lastRepeat >= REPEAT_RATE)) {
    adjustTime(unit, adjustment);
    lastRepeat = millis();
  }
}

void resetButtonState() {
  if (isButtonPressed) {
    isButtonPressed = false;
    buttonHoldTime = 0;
  }
}

// Start a timer
void startTimer(int seconds) {
  startTime = millis();
  timerSeconds = seconds;
  timerRunning = true;
}

// Adjust time (hours or minutes)
void adjustTime(String unit, int adjustment) {
  DateTime now = rtc.now();
  int hours = now.hour();
  int minutes = now.minute();

  if (unit == "hours") {
    hours = (hours + adjustment + HOURS_IN_DAY) % HOURS_IN_DAY; // Wrap hours
  } else if (unit == "minutes") {
    minutes = (minutes + adjustment + MINUTES_IN_HOUR) % MINUTES_IN_HOUR; // Wrap minutes
  }

  rtc.adjust(DateTime(now.year(), now.month(), now.day(), hours, minutes, now.second()));
}

// Update display (showing HH:MM or MM:SS)
void updateDisplay(int high, int low) {
  int h1 = high / 10;
  int h0 = high % 10;
  int l1 = low / 10;
  int l0 = low % 10;

  lc.setDigit(0, 0, h1, false);
  lc.setDigit(0, 1, h0, colonState); // blinking colon
  lc.setDigit(0, 2, l1, false);
  lc.setDigit(0, 3, l0, false);
}

// Display random numbers for 1 second
void displayJibberish() {
  unsigned long start = millis();
  while (millis() - start < 1000) {
    int randomValue = random(0, 9999);
    lc.setDigit(0, 0, (randomValue / 1000) % 10, false);
    lc.setDigit(0, 1, (randomValue / 100) % 10, false);
    lc.setDigit(0, 2, (randomValue / 10) % 10, false);
    lc.setDigit(0, 3, randomValue % 10, false);
    delay(50);
  }
}
