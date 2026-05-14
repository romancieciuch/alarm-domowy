//////////////////////////////////////////////////////////////////////////////////////
// Projekt: Alarm domowy

// Licencja: Creative Commons Zero (CC0)

// Wokwi: https://wokwi.com/projects/463992320389011457
// Repozytorium GitHub: https://github.com/romancieciuch/alarm-domowy
// Film oprowadzający po projekcie: https://youtu.be/s39St69Hleg

// Wymagania:
// - Płytka Arduino
// - Płytka stykowa
// - Buzzer
// - PIR
// - RTC
// - 4 x Rezystory 330 R
// - Dioda LED RGB wspólna katoda
// - Klawiatura 4 x 4
// - Wyświetlacz LED 2 x 16 I2C
// - Kable połączeniowe

// Klawisze:
// 0-9 -> 0-9
// A   -> Uzbrój alarm
// B   -> Nowe hasło
// C   -> Usuń wpisywane dane
// D   -> Reset do ustawień fabrycznych
// *   -> Informacja o klawiszach
// #   -> Zatwierdzenie lub powrót do głównego ekranu, kiedy nieuzbrojony


//////////////////////////////////////////////////////////////////////////////////////
// LED
#define RED_PIN   11
#define GREEN_PIN 10
#define BLUE_PIN  9
unsigned long t_led = 0;
int led_state = 0;
int pwm = 0;
int dir = 1;


//////////////////////////////////////////////////////////////////////////////////////
// Buzzer
#define BUZZER_PIN 13
unsigned long t_buzzer = 0;


//////////////////////////////////////////////////////////////////////////////////////
// RTC
#include <Wire.h>
#include <RTClib.h> // Doinstalować bibliotekę RTClib z zależnościami

#define RTC_ON true

// Ustawienie czasu potrzebne jest PRZY PIERWSZYM WGRANIU
// Potem ustaw wartość na false i wgraj jeszcze raz
#define RESET_TIME false

// Wybór urządzenia RTC
RTC_DS1307 rtc;
// RTC_DS3231 rtc;

unsigned long t_rtc = 0;


//////////////////////////////////////////////////////////////////////////////////////
// Czujnik ruchu
#define PIR_PIN 12
unsigned long t_pir = 0;


//////////////////////////////////////////////////////////////////////////////////////
// Klawiatura
#define NO_KEY '-'
const byte keypad_rows[4] = {A3, A2, A1, A0};
const byte keypad_cols[4] = {2, 3, 4, 5};
String phrase = "";
String default_passcode = "1234";
String passcode = default_passcode;

char keypad_keys[4][4] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};


//////////////////////////////////////////////////////////////////////////////////////
// LCD
#include <Wire.h>
#include <LiquidCrystal_I2C.h> // Doinstalować bibliotekę LiquidCrystal_I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);


//////////////////////////////////////////////////////////////////////////////////////
// Alarm
enum stanyAlarmu {
  WYLACZONY,
  UZBROJONY,
  WYKRYTO_RUCH,
  ALARM,
  ZMIANA_HASLA
};
stanyAlarmu STAN_ALARMU = WYLACZONY;
unsigned long czas_na_wpisanie_kodu = 10000;
unsigned long t_alarm = 0;


//////////////////////////////////////////////////////////////////////////////////////
// Pamięć EEPROM
#include <EEPROM.h>
#define ADDR_STATE 100
#define STATE_OFF 0
#define STATE_ARMED 1


void setup () {
  Serial.begin(9600);

  setup_led();
  setup_buzzer();
  if (RTC_ON) setup_rtc();
  setup_pir();
  setup_keypad();
  setup_lcd();

  led_ready();

  init_pin(default_passcode);
  passcode = load_pin();

  setup_state();
}

void loop () {

  char key = get_key_pressed();
  if (key != NO_KEY) {
    buzzer_beep();

    switch (key) {
      // Uzbrajanie
      case 'A':
        if (STAN_ALARMU == WYLACZONY) {
          phrase = "";

          STAN_ALARMU = UZBROJONY;
          save_state(STATE_ARMED);
          log("UZBROJONY");

          lcd_locked();
          buzzer_beep();
        }

        break;

      // Zmiana hasła
      case 'B':
        if (STAN_ALARMU == WYLACZONY) {
          phrase = "";

          STAN_ALARMU = ZMIANA_HASLA;
          log("ZMIANA_HASLA");

          lcd_password_change();
          buzzer_beep();
        }
        break;

      case 'C':
        phrase = "";
        log("USUNIECIE WPROWADZANYCH ZNAKOW");

        if (STAN_ALARMU == UZBROJONY)
          lcd_locked();

        if (STAN_ALARMU == ZMIANA_HASLA)
          lcd_password_change();

        if (STAN_ALARMU == ALARM)
          lcd_alarm();

        if (STAN_ALARMU == WYKRYTO_RUCH)
          lcd_motion_detected();

        break;

      // Reset do ustawień fabrycznych
      case 'D':
        if (STAN_ALARMU == WYLACZONY) {
          phrase = "";

          passcode = default_passcode;
          save_pin(passcode);

          log("RESET DO USTAWIEN FABRYCZNYCH");
          log("NOWE HASLO " + default_passcode);

          lcd_factory_defaults();

          buzzer_beep();
          buzzer_beep();
          buzzer_beep();
          buzzer_beep();
        }
        break;

      // Informacja o klawiszach
      case '*':
        lcd_keys_info();
        break;

      // Zapis zmiany hasła lub sprawdzenie hasła uzbrojonego alarmu
      case '#':

        if (STAN_ALARMU == WYLACZONY) {
          lcd_home();

        } else if (STAN_ALARMU == ZMIANA_HASLA) {

          if (phrase.length() < 4) {
            log("ZA KROTKIE HASLO: " + passcode);
            lcd_password_too_short();
            phrase = "";
            break;
          }

          passcode = phrase;
          save_pin(passcode);

          STAN_ALARMU = WYLACZONY;
          log("NOWE HASLO " + passcode);

          lcd_password_changed();

          buzzer_beep();
          buzzer_beep();
          buzzer_beep();
          buzzer_beep();

        } else if (STAN_ALARMU == UZBROJONY || STAN_ALARMU == WYKRYTO_RUCH || STAN_ALARMU == ALARM) {

          if (phrase == passcode) {
            led_ready();

            STAN_ALARMU = WYLACZONY;
            save_state(STATE_OFF);
            log("POPRAWNE HASLO -> ALARM WYLACZONY");

            lcd_home();

            buzzer_beep();
            buzzer_beep();

          } else {

            STAN_ALARMU = ALARM;
            log("NIEPOPRAWNE HASLO -> ALARM");

            lcd_alarm();
          }

        }

        phrase = "";
        break;

      default:
        phrase += key;
        Serial.println(phrase);

        if (STAN_ALARMU == UZBROJONY)
          lcd_locked();

        if (STAN_ALARMU == ZMIANA_HASLA)
          lcd_password_change();

        if (STAN_ALARMU == ALARM)
          lcd_alarm();

        break;
    }
  }


  if (STAN_ALARMU == UZBROJONY) {
    led_armed();
  }


  bool motion_detected = pir();
  if (STAN_ALARMU == UZBROJONY && motion_detected) {
    t_alarm = millis();

    STAN_ALARMU = WYKRYTO_RUCH;
    log("WYKRYTO_RUCH");

    lcd_motion_detected();
  }


  if (STAN_ALARMU == WYKRYTO_RUCH) {
    led_motion_detected();

    if (millis() - t_alarm >= czas_na_wpisanie_kodu) {
      STAN_ALARMU = ALARM;
      log("ALARM");

      lcd_alarm();
    }
  }


  if (STAN_ALARMU == ALARM) {
    led_alarm();
    buzzer_alarm();
  }

}


void setup_lcd () {
  lcd.init();
  lcd.backlight();

  lcd_reset();
  lcd_home();
}

void lcd_reset () {
  lcd.setCursor(0, 0);
  lcd.print("                ");

  lcd.setCursor(0, 1);
  lcd.print("                ");
}

void lcd_home () {
  lcd_reset();

  lcd.setCursor(0, 0);
  lcd.print("   Cerber 1.0   ");

  lcd.setCursor(0, 1);
  lcd.print(" [ODBLOKOWANY]  ");
}

void lcd_keys_info () {
  lcd_reset();

  lcd.setCursor(0, 0);
  lcd.print("A: Uzb | B: Has");

  lcd.setCursor(0, 1);
  lcd.print("C: Usu | D: Fabr");
}

void lcd_locked () {
  lcd_reset();

  lcd.setCursor(0, 0);
  lcd.print("  [UZBROJONY] ");

  lcd.setCursor(0, 1);
  lcd.print("Haslo: ");
  lcd.print(text_to_stars(phrase));
}

void lcd_password_change () {
  lcd_reset();

  lcd.setCursor(0, 0);
  lcd.print(" [ZMIANA HASLA] ");

  lcd.setCursor(0, 1);
  lcd.print("Haslo: ");
  lcd.print(text_to_stars(phrase));
}

void lcd_password_changed () {
  lcd_reset();

  lcd.setCursor(0, 0);
  lcd.print(" [ZMIANA HASLA] ");

  lcd.setCursor(0, 1);
  lcd.print("Haslo zmienione");
}

void lcd_password_too_short () {
  lcd_reset();

  lcd.setCursor(0, 0);
  lcd.print(" [ZMIANA HASLA] ");

  lcd.setCursor(0, 1);
  lcd.print("Haslo min. 4 zn.");
}

void lcd_factory_defaults () {
  lcd_reset();

  lcd.setCursor(0, 0);
  lcd.print("[RES DO UST FAB]");

  lcd.setCursor(0, 1);
  lcd.print("Haslo: ");
  lcd.print(default_passcode);
}

void lcd_alarm () {
  lcd_reset();

  lcd.setCursor(0, 0);
  lcd.print("[!!! ALARM !!!]");

  lcd.setCursor(0, 1);
  lcd.print("Haslo: ");
  lcd.print(text_to_stars(phrase));
}

void lcd_motion_detected () {
  lcd_reset();

  lcd.setCursor(0, 0);
  lcd.print(" [WYKRYTO RUCH] ");

  lcd.setCursor(0, 1);
  lcd.print("Haslo: ");
  lcd.print(text_to_stars(phrase));
}

String text_to_stars (String text) {
  String pass = "";

  for (int i = 0; i < text.length(); i++) {
    pass += "*";
  }

  return pass;
}

void setup_keypad () {
  for (int i = 0; i < 4; i++) {
    pinMode(keypad_rows[i], INPUT_PULLUP);
    pinMode(keypad_cols[i], OUTPUT);
    digitalWrite(keypad_cols[i], HIGH);
  }
}

char get_key_pressed () {
  for (int c = 0; c < 4; c++) {
    digitalWrite(keypad_cols[c], LOW);

    for (int r = 0; r < 4; r++) {
      if (digitalRead(keypad_rows[r]) == LOW) {
        delay(50);

        while (digitalRead(keypad_rows[r]) == LOW);
        digitalWrite(keypad_cols[c], HIGH);
        return keypad_keys[r][c];
      }
    }

    digitalWrite(keypad_cols[c], HIGH);
  }

  return NO_KEY;
}


void setup_pir () {
  pinMode(PIR_PIN, INPUT);
  Serial.println("PIR OK");
}

bool pir () {
  if (millis() - t_pir < 1000) return false;

  t_pir = millis();
  int motion = digitalRead(PIR_PIN);

  if (motion == HIGH) {
    Serial.println("RUCH!");
    return true;
  }

  return false;
}


void setup_rtc () {

  if (!rtc.begin()) {
    Serial.println("Nie wykryto RTC");
    while (1);
  }

  if (RESET_TIME)
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  Serial.println("RTC OK");
}

String get_time () {
  if (!RTC_ON) return "";

  DateTime now = rtc.now();
  String t = "";

  t += now.year();
  t += "-";
  t += now.month();
  t += "-";
  t += now.day();
  t += " ";
  t += now.hour();
  t += ":";
  t += now.minute();
  t += ":";
  t += now.second();

  return t;
}


void setup_buzzer () {
  pinMode(BUZZER_PIN, OUTPUT);
}

void buzzer_alarm () {
  if (millis() - t_buzzer < 500) return;

  t_buzzer = millis();

  static bool state = false;
  state = !state;

  if (state) tone(BUZZER_PIN, 1000);
  else noTone(BUZZER_PIN);
}

void buzzer_beep () {
  tone(BUZZER_PIN, 2200);
  delay(50);
  noTone(BUZZER_PIN);
}


void setup_led () {
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  set_led(255, 255, 255);
}

void led_motion_detected () {
  set_led(255, 20, 0);
}

void led_armed () {
  if (millis() - t_led < 1500) return;
  t_led = millis();

  static bool state = false;
  state = !state;

  set_led(state ? 255 : 0, state ? 255 : 0, 0);
}

void led_alarm () {
  if (millis() - t_led < 80) return;

  t_led = millis();

  static bool state = false;
  state = !state;

  set_led(state ? 255 : 0, 0, 0);
}

void led_ready () {
  set_led(255, 255, 255);
}

void set_led (int r, int g, int b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}


void init_pin (String pin) {
  if (EEPROM.read(0) == 0xFF) { // EEPROM pusty
    save_pin(pin);
  }
}

void save_pin (String pin) {
  for (int i = 0; i < pin.length(); i++) {
    EEPROM.write(i, pin[i]);
  }
  EEPROM.write(pin.length(), '\0');
}

String load_pin () {
  String pin = "";
  char c;

  for (int i = 0; i < 10; i++) {
    c = EEPROM.read(i);
    if (c == '\0') break;
    pin += c;
  }

  return pin;
}


void setup_state () {
  byte saved = load_state();

  if (saved == STATE_ARMED) {
    STAN_ALARMU = UZBROJONY;
    log("RESTORE: UZBROJONY");

    lcd_locked();
  } else {
    STAN_ALARMU = WYLACZONY;
  }
}

byte load_state () {
  return EEPROM.read(ADDR_STATE);
}

void save_state (byte state) {
  EEPROM.write(ADDR_STATE, state);
}


void log (String text) {
  Serial.println(get_time() + " " + text);
}