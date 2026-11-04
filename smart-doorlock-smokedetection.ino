
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>

// ========== LCD ==========
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ========== PIN CONNECTIONS ==========
const int SERVO_PIN = 10;
const int IR_PIN = 11;
const int GAS_PIN = A0;
const int BUZZER_PIN = 12;

const int RED_PIN = A1;
const int GREEN_PIN = A2;
const int BLUE_PIN = A3;

// Change to LOW if your IR sensor detects with LOW.
const int IR_DETECTED_LEVEL = HIGH;

// ========== SERVO ==========
Servo doorServo;

// ========== KEYPAD ==========
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8, 9};

Keypad keypad = Keypad(
  makeKeymap(keys),
  rowPins,
  colPins,
  ROWS,
  COLS
);

// ========== PASSWORD ==========
String correctPassword = "2222";
String enteredPassword = "";

int wrongAttempts = 0;
const int MAX_ATTEMPTS = 3;

// ========== GAS SENSOR ==========
const int GAS_ALARM_THRESHOLD = 400;
const int GAS_CLEAR_THRESHOLD = 350;

int gasValue = 0;
bool gasEmergency = false;

// ========== TIMERS ==========
unsigned long stateTime = 0;
unsigned long lastGasCheck = 0;
unsigned long lastCountdownUpdate = 0;
unsigned long buzzerStartTime = 0;
unsigned long buzzerDuration = 0;
unsigned long lockoutStartTime = 0;
unsigned long lastIRDebug = 0;

bool buzzerTimed = false;

// ========== SYSTEM STATES ==========
enum SystemState {
  WAITING,
  PASSWORD_ENTRY,
  WRONG_PASSWORD,
  UNLOCKED,
  GOODBYE,
  LOCKOUT
};

SystemState currentState = WAITING;

// ========== RGB LED ==========
// Assumes common cathode RGB LED.

void setLED(bool red, bool green, bool blue) {
  digitalWrite(RED_PIN, red ? HIGH : LOW);
  digitalWrite(GREEN_PIN, green ? HIGH : LOW);
  digitalWrite(BLUE_PIN, blue ? HIGH : LOW);
}

// ========== BUZZER ==========

void beepFor(unsigned long duration) {
  tone(BUZZER_PIN, 1000);
  buzzerStartTime = millis();
  buzzerDuration = duration;
  buzzerTimed = true;
}

void updateBuzzer() {
  if (buzzerTimed &&
      millis() - buzzerStartTime >= buzzerDuration) {
    noTone(BUZZER_PIN);
    buzzerTimed = false;
  }
}

// ========== LCD FUNCTIONS ==========

void screenOff() {
  lcd.clear();
  lcd.noBacklight();
}

void showPasswordScreen() {
  lcd.backlight();
  lcd.clear();
  lcd.print("HELLO WELCOME");
  lcd.setCursor(0, 1);
  lcd.print("ENTER PASSWORD");

  enteredPassword = "";
}

// Return to black-screen waiting mode
void returnToWaiting() {
  enteredPassword = "";

  setLED(false, true, false);

  screenOff();

  currentState = WAITING;

  Serial.println("Waiting for next person...");
}

// ========== SETUP ==========

void setup() {

  Serial.begin(9600);

  lcd.init();
  lcd.backlight();

  doorServo.attach(SERVO_PIN);
  doorServo.write(0);

  pinMode(IR_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  setLED(false, false, false);

  lcd.clear();
  lcd.print("SMART DOOR LOCK");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  setLED(false, false, true);

  delay(2000);

  // Start with LCD black
  returnToWaiting();

  Serial.println("System ready.");
  Serial.println("IR sensor monitoring started.");
}

// ========== MAIN LOOP ==========

void loop() {

  unsigned long now = millis();

  char key = keypad.getKey();

  updateBuzzer();

  // ========== IR SENSOR DEBUG ==========

  if (now - lastIRDebug >= 500) {
    lastIRDebug = now;

    Serial.print("IR sensor output: ");
    Serial.println(digitalRead(IR_PIN));
  }

  // ========== GAS SENSOR ==========

  if (now - lastGasCheck >= 500) {

    lastGasCheck = now;

    gasValue = analogRead(GAS_PIN);

    Serial.print("Gas reading: ");
    Serial.println(gasValue);

    // Gas detected
    if (!gasEmergency &&
        gasValue >= GAS_ALARM_THRESHOLD) {

      gasEmergency = true;

      doorServo.write(90);

      buzzerTimed = false;
      tone(BUZZER_PIN, 1000);

      lcd.backlight();
      lcd.clear();
      lcd.print("GAS ALERT!");
      lcd.setCursor(0, 1);
      lcd.print("EVACUATE NOW!");

      setLED(true, false, false);

      Serial.println("GAS EMERGENCY!");
    }

    // Gas returns to clear threshold
    if (gasEmergency &&
        gasValue <= GAS_CLEAR_THRESHOLD) {

      gasEmergency = false;

      noTone(BUZZER_PIN);
      buzzerTimed = false;

      doorServo.write(0);

      Serial.println("Gas level normal.");

      returnToWaiting();
    }
  }

  // Emergency overrides normal operation
  if (gasEmergency) {
    return;
  }

  // ========== SYSTEM STATES ==========

  switch (currentState) {

    // ----- WAITING FOR PERSON -----

    case WAITING:

      if (digitalRead(IR_PIN) == IR_DETECTED_LEVEL) {

        showPasswordScreen();

        setLED(false, false, true);

        currentState = PASSWORD_ENTRY;

        Serial.println("PERSON DETECTED!");
      }

      break;


    // ----- PASSWORD ENTRY -----

    case PASSWORD_ENTRY:

      if (key) {

        Serial.print("Key pressed: ");
        Serial.println(key);

        // Clear password
        if (key == '*') {

          enteredPassword = "";

          lcd.setCursor(0, 1);
          lcd.print("                ");
        }

        // Submit password
        else if (key == '#') {

          if (enteredPassword == correctPassword) {

            wrongAttempts = 0;

            lcd.clear();
            lcd.print("THANK YOU!");
            lcd.setCursor(0, 1);
            lcd.print("WELCOME IN");

            doorServo.write(90);

            setLED(false, true, false);

            beepFor(200);

            stateTime = now;

            currentState = UNLOCKED;

            Serial.println("Access granted.");
          }

          else {

            wrongAttempts++;

            lcd.backlight();
            lcd.clear();
            lcd.print("WRONG PASSWORD");
            lcd.setCursor(0, 1);
            lcd.print("ATTEMPTS: ");
            lcd.print(wrongAttempts);
            lcd.print("/3");

            setLED(true, false, false);

            // One-second warning
            beepFor(1000);

            stateTime = now;

            currentState = WRONG_PASSWORD;

            Serial.println("Wrong password.");
          }

          enteredPassword = "";
        }

        // Accept numbers
        else if (key >= '0' && key <= '9') {

          if (enteredPassword.length() < 4) {

            enteredPassword += key;

            lcd.setCursor(0, 1);
            lcd.print("                ");

            lcd.setCursor(0, 1);

            for (int i = 0;
                 i < enteredPassword.length();
                 i++) {
              lcd.print('*');
            }
          }
        }
      }

      break;


    // ----- WRONG PASSWORD -----

    case WRONG_PASSWORD:

      // Start lockout after 3 wrong attempts
      if (wrongAttempts >= MAX_ATTEMPTS &&
          now - stateTime >= 1500) {

        lockoutStartTime = now;
        lastCountdownUpdate = 0;

        lcd.backlight();
        lcd.clear();
        lcd.print("TOO MANY TRIES");
        lcd.setCursor(0, 1);
        lcd.print("Wait 60 seconds");

        setLED(true, false, false);

        // Two-second lockout warning
        beepFor(2000);

        currentState = LOCKOUT;

        Serial.println("60-second lockout started.");
      }

      // If fewer than 3 wrong attempts,
      // return directly to password entry.
      // NEVER turn the LCD black here.
      else if (wrongAttempts < MAX_ATTEMPTS &&
               now - stateTime >= 1500) {

        showPasswordScreen();

        setLED(false, false, true);

        currentState = PASSWORD_ENTRY;

        Serial.println("Try password again.");
      }

      break;


    // ----- 60-SECOND LOCKOUT -----

    case LOCKOUT: {

      unsigned long elapsed = now - lockoutStartTime;

      if (elapsed >= 60000UL) {

        wrongAttempts = 0;
        enteredPassword = "";

        Serial.println("Lockout ended.");

        returnToWaiting();

      }

      else if (now - lastCountdownUpdate >= 1000) {

        lastCountdownUpdate = now;

        int secondsLeft = 60 - (elapsed / 1000);

        lcd.backlight();
        lcd.clear();
        lcd.print("LOCKED OUT!");

        lcd.setCursor(0, 1);
        lcd.print("Wait ");
        lcd.print(secondsLeft);
        lcd.print(" seconds");

        setLED(true, false, false);
      }

      break;
    }


    // ----- DOOR UNLOCKED FOR 5 SECONDS -----

    case UNLOCKED:

      if (now - stateTime >= 5000) {

        // Lock the door
        doorServo.write(0);

        Serial.println("Door locked.");

        // Show goodbye message after locking
        lcd.clear();
        lcd.print("GOODBYE!");
        lcd.setCursor(0, 1);
        lcd.print("HAVE A NICE DAY");

        setLED(false, true, false);

        stateTime = now;

        currentState = GOODBYE;
      }

      break;


    // ----- GOODBYE MESSAGE FOR 2 SECONDS -----

    case GOODBYE:

      if (now - stateTime >= 2000) {

        Serial.println("Goodbye message finished.");

        // Turn LCD black after goodbye
        returnToWaiting();
      }

      break;

  }
}
