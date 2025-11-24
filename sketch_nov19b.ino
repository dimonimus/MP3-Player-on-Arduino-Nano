#include <SoftwareSerial.h>
#include "DFRobotDFPlayerMini.h"
#include "U8glib.h"

U8GLIB_SH1106_128X64 u8g(U8G_I2C_OPT_NO_ACK);
SoftwareSerial mySerial(2, 3);
DFRobotDFPlayerMini myDFPlayer;

// === Пины ===
const uint8_t BTN_NEXT       = 4;
const uint8_t BTN_VOL_UP     = 5;
const uint8_t BTN_VOL_DOWN   = 6;
const uint8_t BTN_PLAY_PAUSE = 7;
const uint8_t LED_PIN        = 8;

// === Состояние плеера ===
int current_track = 1;
unsigned long track_start_time = 0;
unsigned long track_played_seconds = 0;
bool is_playing = false;

int current_volume = 15;
const int VOL_SHORT = 2;
const int VOL_LONG  = 5;

// === Кнопки громкости ===
unsigned long btn_up_time = 0;
unsigned long btn_down_time = 0;
bool btn_up_active = false;
bool btn_down_active = false;

// === LED ===
unsigned long led_timer = 0;
bool led_state = false;

// === Ключевой флаг: только что отправили команду play ===
bool just_sent_play_command = false;

// === Обновление экрана ===
void updateDisplay() {
  u8g.firstPage();
  do {
    u8g.setFont(u8g_font_6x10);
    u8g.setDefaultForegroundColor();

    u8g.drawStr(0, 10, "MP3 Player");

    char trackStr[30];
    sprintf(trackStr, "Track: %04d %s", current_track, is_playing ? "Playing" : "Paused");
    u8g.drawStr(0, 22, trackStr);

    unsigned long elapsed = track_played_seconds;
    if (is_playing) {
      elapsed += (millis() - track_start_time) / 1000;
    }
    char timeStr[20];
    sprintf(timeStr, "%02lu:%02lu", elapsed / 60, elapsed % 60);
    u8g.drawStr(0, 35, timeStr);

    char volStr[20];
    sprintf(volStr, "Vol: %02d", current_volume);
    u8g.drawStr(0, 52, volStr);

    u8g.drawFrame(42, 46, 60, 8);
    int vol_bar = map(current_volume, 0, 30, 0, 58);
    u8g.drawBox(43, 47, vol_bar, 6);

  } while (u8g.nextPage());
}

// === Громкость ===
void setVolume(int vol) {
  vol = constrain(vol, 0, 30);
  if (vol != current_volume) {
    current_volume = vol;
    myDFPlayer.volume(current_volume);
    updateDisplay();
  }
}

void startNewTrack() {
  myDFPlayer.stop();
  delay(50);
  while (myDFPlayer.available()) myDFPlayer.read();  

  Serial.print(F("Playing track: "));
  Serial.println(current_track);

  myDFPlayer.playMp3Folder(current_track);

  just_sent_play_command = true;  

  track_start_time = millis();
  track_played_seconds = 0;
  is_playing = true;

  updateDisplay();
}

void togglePlayPause() {
  if (is_playing) {
    myDFPlayer.pause();
    track_played_seconds += (millis() - track_start_time) / 1000;
    Serial.println(F("Paused"));
  } else {
    if (track_played_seconds == 0 && track_start_time == 0) {
      startNewTrack();
      return;
    } else {
      myDFPlayer.start();
      track_start_time = millis();
      Serial.println(F("Resumed"));
    }
  }
  is_playing = !is_playing;
  updateDisplay();
}

// === LED ===
void updateLED() {
  unsigned long now = millis();
  if (is_playing) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    if (track_played_seconds == 0) {
      if (now - led_timer >= 1000) {
        led_timer = now;
        led_state = !led_state;
        digitalWrite(LED_PIN, led_state);
      }
    } else {
      if (now - led_timer >= 200) {
        led_timer = now;
        led_state = !led_state;
        digitalWrite(LED_PIN, led_state);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  mySerial.begin(9600);

  pinMode(BTN_NEXT,       INPUT_PULLUP);
  pinMode(BTN_VOL_UP,     INPUT_PULLUP);
  pinMode(BTN_VOL_DOWN,   INPUT_PULLUP);
  pinMode(BTN_PLAY_PAUSE, INPUT_PULLUP);
  pinMode(LED_PIN,        OUTPUT);

  if (!myDFPlayer.begin(mySerial, true, false)) {
    Serial.println(F("DFPlayer не найден!"));
    while (true) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(300);
    }
  }

  myDFPlayer.volume(current_volume);
  myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);

  Serial.println(F("MP3 Player готов!"));
  led_timer = millis();
  updateDisplay();
}

void loop() {
  bool next  = digitalRead(BTN_NEXT)       == LOW;
  bool up    = digitalRead(BTN_VOL_UP)     == LOW;
  bool down  = digitalRead(BTN_VOL_DOWN)   == LOW;
  bool play  = digitalRead(BTN_PLAY_PAUSE) == LOW;

  static bool last_next = false;
  static bool last_play = false;

  if (next && !last_next) {
    current_track++;
    if (current_track > 9999) current_track = 1;
    startNewTrack();
  }
  last_next = next;

  // Play / Pause
  if (play && !last_play) {
    togglePlayPause();
  }
  last_play = play;

  // Громкость +
  if (up && !btn_up_active) {
    btn_up_active = true;
    btn_up_time = millis();
    setVolume(current_volume + VOL_SHORT);
  }
  if (up && btn_up_active && (millis() - btn_up_time >= 500)) {
    btn_up_time = millis();
    setVolume(current_volume + VOL_LONG);
  }
  if (!up) btn_up_active = false;

  // Громкость -
  if (down && !btn_down_active) {
    btn_down_active = true;
    btn_down_time = millis();
    setVolume(current_volume - VOL_SHORT);
  }
  if (down && btn_down_active && (millis() - btn_down_time >= 500)) {
    btn_down_time = millis();
    setVolume(current_volume - VOL_LONG);
  }
  if (!down) btn_down_active = false;

  if (myDFPlayer.available()) {
    if (just_sent_play_command) {
      while (myDFPlayer.available()) myDFPlayer.read();
      just_sent_play_command = false;  
    } else {
      uint8_t type = myDFPlayer.readType();

      if (type == DFPlayerPlayFinished && is_playing) {
        Serial.println(F("Track finished -> next"));
        track_played_seconds += (millis() - track_start_time) / 1000;
        current_track++;
        if (current_track > 9999) current_track = 1;
        startNewTrack();
      }
    }
  }

  updateLED();
  updateDisplay();
  delay(50);
}