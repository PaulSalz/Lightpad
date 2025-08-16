

// -------------------- Konfiguration -------------------------------------------------------------------------------------

  #include <Wire.h>
  #include <Adafruit_MCP23X17.h>
  #include <WS2812Serial.h>
  #define FASTLED_OVERCLOCK 1.2
  #define FASTLED_ALLOW_INTERRUPTS 1
  #define FASTLED_INTERRUPT_RETRY_COUNT 4
  #define FASTLED_OVERCLOCK_SUPPRESS_WARNING
  #include <FastLED.h>
  #include <WS2812Serial.h>
  #include "Adafruit_DRV2605.h"
  #include <Control_Surface.h>
  #define IR_INPUT_PIN 33
  #include "TinyIRReceiver.hpp"

  Adafruit_MCP23X17 mcp[8];
  #define MCP_COUNT 8
  USBMIDI_Interface midi;
  Adafruit_DRV2605 drv;
  // ---- DRV2605 low-level read/write ----
  #define DRV2605_ADDR 0x5A
  #define REG_STATUS 0x00
  #define REG_MODE 0x01
  #define REG_FEEDBACK 0x1A  // Bit7 selects LRA(1)/ERM(0)
  #define REG_CONTROL3 0x1D  // enthält ERM_OpenLoop-Bit
  #define REG_RATED_VOLTAGE 0x16
  #define REG_OVERDRIVE_CLAMP 0x17


// -------------------- MIDI-CONFIG -----------------------------------------------------------------------------------------

  //const MIDIAddress note = MIDI_Notes::C[4]; // C4 is middle C
  const uint8_t velocity = 127;      // 127 is maximum velocity -> value
  constexpr uint8_t BASE_NOTE = 60;  // C4
  cs::Channel CHANNEL_switch = cs::Channel{ 0 };

  // Hilfsfunktion: Note-Adresse für einen Encoder berechnen
  static inline cs::MIDIAddress encoderAddr(uint8_t row, uint8_t enc_i, uint8_t base, auto channel_x) {
    // row: 1..3, enc_i: 0..7 -> 8 Noten je Reihe
    uint8_t note = base + (row - 1) * 8 + enc_i;  // achte auf 0..127
    return cs::MIDIAddress{ note, channel_x };
  }
  static inline cs::MIDIAddress encoderAddr_2(uint8_t row, uint8_t enc_i, uint8_t base, auto channel_x) {
    // row: 1..3, enc_i: 0..7 -> 8 Noten je Reihe
    uint8_t note = base + (row - 1) * 16 + enc_i;  // achte auf 0..127
    return cs::MIDIAddress{ note, channel_x };
  }
  inline void decodeCC(uint8_t cc, uint8_t &row, uint8_t &enc_i) {
    uint8_t offset = cc - BASE_NOTE;  // 0..23
    row = (offset / 8) + 1;           // 1..3
    enc_i = offset % 8;               // 0..7
  }

// -------------------- LED-CONFIG -------------------------------------------------------------------------------------------

  #define NUM_LEDS_main 64
  #define NUM_LEDS_enc 576
  #define NUM_LEDS_enc_back 96
  #define DATA_PIN_main 1
  #define DATA_PIN_enc 8
  #define DATA_PIN_enc_back 17
  const int colorOrder = WS2812_GRB;

  byte drawingMemory1[NUM_LEDS_enc * 3] = { 0 };
  DMAMEM byte displayMemory1[NUM_LEDS_enc * 12] = { 0 };
  byte drawingMemory2[NUM_LEDS_main * 3] = { 0 };
  DMAMEM byte displayMemory2[NUM_LEDS_main * 12] = { 0 };
  byte drawingMemory3[NUM_LEDS_enc_back * 3] = { 0 };
  DMAMEM byte displayMemory3[NUM_LEDS_enc_back * 12] = { 0 };
  WS2812Serial led_enc(NUM_LEDS_enc, displayMemory1, drawingMemory1, DATA_PIN_enc, colorOrder);
  WS2812Serial led_eb(NUM_LEDS_enc_back, displayMemory3, drawingMemory3, DATA_PIN_enc_back, colorOrder);
  WS2812Serial led_rest(NUM_LEDS_main, displayMemory2, drawingMemory2, DATA_PIN_main, colorOrder);

  uint32_t color_back, color_main, color_rgb, color_rgb2, cl_wh, cl_wh_dimm, color_main_bright, color_back_bright, color_rain, color_back_gr, color_back_bright_gr,color_var_hell,col_l,col_r,col_m, color_back_clipping;
  constexpr uint32_t c_wh_dimm = 0x303030;
  constexpr uint32_t c_wh_bright = 0xFFFFFF;
  constexpr uint32_t c_blu = 0x0000FF;
  int max_power_back_dimm = 50;  //init wert
  int max_power_back = 15;       //init wert
  uint8_t max_hell_power = 255;
  uint8_t max_hell_usb = 60;
  uint8_t color_shift_my = 0;
  uint8_t color_clipping_hue = 0;
  uint8_t color_shift_rain = 0;
  uint8_t hell_up_down = 15;
  uint8_t delta_var = 1;
  uint8_t delta_var_clip =1;
  constexpr uint8_t HUES_enc[8] = { 0, 16, 33, 96, 134, 160, 194, 222 };  // Farben: Rot, Orange, Gelb, Grün, Aqua, Blau, Lila, Magenta
  constexpr bool enc_upper[24] = { 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1 };
  constexpr bool enc_lower[24] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
  constexpr bool enc_left[24] = { 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  constexpr bool enc_right[24] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0 };
  constexpr bool enc_mid[24] = { 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0 };
  constexpr bool enc_korn[24] = { 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1 };
  constexpr bool enc_unrg[24] = { 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1 };
  constexpr uint8_t led_rev[24] = { 0, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };  //constexpr uint8_t led_rev[24] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23};
  constexpr bool enc_corner[24] = { 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0 };
  uint8_t pattern[24];
  constexpr uint8_t N_RING = 24;
  static uint8_t ramp_200_to_0[N_RING];
  static uint8_t ramp_255_to_0[N_RING];
  static uint8_t ramp_255_to_70[N_RING];
  static uint8_t ramp_200_to_130[N_RING];
  static uint8_t ramp_255_to_20[N_RING];
  static uint8_t ramp_255_to_10[N_RING];
  static uint8_t ramp_255_to_120[N_RING];


// -------------------- VARIABLEN-REST --------------------------------------------------------------------------------------

  elapsedMillis since_last_led_change;
  elapsedMillis since_last_led;
  elapsedMillis since_last_counter;
  long since_last_touch = 0;
  bool idle_b_prev=false;
  constexpr int interval_touched = 10000;
  constexpr int interval_blink = 2000;
  constexpr int interval_show = 25;
  constexpr int interval_col_btn = 6;
  bool idle_b = false;
  bool touched_b = false;
  bool ledState = LOW;
  int loop_counter = 0;
  long loop_timer = 0;
  uint32_t fx_fx = 1;
  bool last_button1, last_button2;
  volatile bool rubber_button_changed[3][2] = { false };  //INT
  bool button_state[3][16] = { false };
  constexpr uint8_t UNUSED = 255;
  constexpr const uint8_t CHIP_OF_PCB[3] = { 6, 7, 0 };  //main, key, nav
  constexpr const uint8_t KEY_COUNT[3] = { 16, 16, 8 };  // NAV hat 8 normale Keys + 5 Sonderkeys (9..13)
  constexpr const uint8_t LED_shift[3] = { 0, 48, 16 };
  uint8_t sth_changed = true;  //damit dfirekt gezeichnet wird
  uint16_t IR_aAddress;
  uint8_t IR_aCommand;
  bool IR_isRepeat;
  bool is_prev=false;
  auto channel_ir = cs::Channel{ 9 };
  auto addr_ir_0 = cs::MIDIAddress{ 0, channel_ir };
  auto addr_ir_1 = cs::MIDIAddress{ 1, channel_ir };
  auto addr_ir_2 = cs::MIDIAddress{ 2, channel_ir };
  auto addr_ir_3 = cs::MIDIAddress{ 3, channel_ir };
  auto addr_ir_4 = cs::MIDIAddress{ 4, channel_ir };
  auto addr_ir_5 = cs::MIDIAddress{ 5, channel_ir };
  auto addr_ir_6 = cs::MIDIAddress{ 6, channel_ir };
  auto addr_ir_7 = cs::MIDIAddress{ 7, channel_ir };
  auto addr_ir_8 = cs::MIDIAddress{ 8, channel_ir };
  auto addr_ir_9 = cs::MIDIAddress{ 9, channel_ir };
  uint8_t main_mode = 0;
  static uint32_t lastStepUs_nav = 0;

  constexpr uint8_t led_key[16] = { 0, 1, 2, 3,
                                    7, 6, 5, 4,
                                    8, 9, 10, 11,
                                    15, 14, 13, 12 };

  constexpr uint8_t led_nav[8] = {0,      1,
                                  31,     26,  //24 leds für weiteren encoder dazwischen
                                  30,29,28,27,};

  constexpr uint8_t led_nav_enc[4] = { 2, 8, 14, 20 };

  constexpr uint8_t led_main[16] = { 0, 1, 2, 3, 4, 5, 6, 7,  //von 0..16 beim durchgehen, welcher pin
                                    8, 9, 10, 11, 12, 13, 14, 15 };



  constexpr uint8_t but_key[16] = { 8, 9, 10, 11,  //von 0..16 beim durchgehen, welcher pin
                                    12, 13, 14, 15,
                                    0, 1, 2, 3,
                                    4, 5, 6, 7 };

  constexpr uint8_t but_nav[16] = { UNUSED, 6, 5, 4, 2, 0, 1, UNUSED, UNUSED, 9, 10, 11, 12, 13, 3, 7 };  //links oben nach rechts unten    //navigation encoder->  9mid  10up  12down  11left  13right  //enca 3(0)  encb 8(8)

  constexpr uint8_t but_main[16] = { 4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8, 0, 1, 2, 3 };  //links oben nach rechts unten

  constexpr const uint8_t *B2K[3] = { but_main, but_key, but_nav };
  constexpr const uint8_t *K2L_u8[3] = { led_main, led_key, led_nav };

  const int MIN_VALUE = 0;
  const int MAX_VALUE = 508;  //anpassen led min max

  const int MIN_VALUE_nav = 0;
  const int MAX_VALUE_nav = 400;  //anpassen led min max
  int counter_nav = { 0 };
  int counter_last_nav = { 0 };
  int counter_last_nav_midi = 0;
  uint8_t lastAB_nav = { 0 };
  volatile bool nav_changed = { false };
  constexpr uint8_t DEBOUNCE_MS = 10;  // 5–10 ms passt meist
  static bool debounce_pending[3] = { false, false, false };
  static uint32_t debounce_due[3] = { 0, 0, 0 };
  static uint16_t debounce_snap1[3] = { 0xFFFF, 0xFFFF, 0xFFFF };




// -------------------- VARIABLEN-ENCODER -----------------------------------------------------------------------------------

  // MCP-Pins für Encoder-Kanal A/B
  constexpr uint8_t enc_rot_pins_A[8] = { 6, 4, 2, 0, 15, 13, 11, 9 };
  constexpr uint8_t enc_rot_pins_B[8] = { 7, 5, 3, 1, 14, 12, 10, 8 };
  // MCP-Pins für Buttons der encoder
  constexpr uint8_t enc_but_pins_4[8] = { 7, 6, 5, 4, 3, 2, 1, 0 };
  constexpr uint8_t enc_but_pins_5[16] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
  // Teensy-INT-Pins
  constexpr uint8_t mcpIntA[8] = { 21, 23, 41, 36, 38, 32, 28, 30 };
  constexpr uint8_t mcpIntB[8] = { 20, 22, 40, 35, 37, 31, 27, 29 };

  constexpr uint16_t enc_led[3][8] = { { 24, 0, 168, 144, 312, 288, 456, 432 },
                                      { 72, 48, 216, 192, 360, 336, 504, 480 },
                                      { 120, 96, 264, 240, 408, 384, 552, 528 } };

  constexpr uint8_t enc_used_licht[3][8] = { { 1, 1, 0, 1, 1, 0, 1, 1 },
                                            { 0, 0, 0, 1, 1, 0, 1, 1 },
                                            { 1, 1, 0, 1, 1, 0, 1, 1 } };

  constexpr uint8_t enc_used_details[3][8] = { { 1, 0, 1, 1, 0, 1, 0, 1 },
                                              { 1, 0, 1, 1, 0, 1, 0, 1 },
                                              { 1, 0, 1, 1, 0, 1, 0, 1 } };

  constexpr uint8_t enc_used_cg[3][8] = { { 1, 0, 1, 0, 1, 0, 1, 1 },
                                          { 1, 0, 1, 0, 1, 0, 1, 1 },
                                          { 1, 0, 1, 0, 1, 0, 1, 1 } };

  constexpr uint8_t NUM_ENCODER_CHIPS = 3 + 1;
  constexpr uint8_t ENC_PER_CHIP = 8;
  bool enc_dirty = false;
  uint8_t enc_mode = 0;
  constexpr uint8_t MAX_MODES = 4;
  volatile bool encoderChanged[NUM_ENCODER_CHIPS][2] = { false };
  volatile bool button_changed[NUM_ENCODER_CHIPS][2] = { false };
  int counter[MAX_MODES][NUM_ENCODER_CHIPS][ENC_PER_CHIP] = { 0 };
  int counter_last[MAX_MODES][NUM_ENCODER_CHIPS][ENC_PER_CHIP] = { 0 };
  bool button_enc_last[NUM_ENCODER_CHIPS][ENC_PER_CHIP * 2] = { false };
  int counter_last_midi[MAX_MODES][NUM_ENCODER_CHIPS][ENC_PER_CHIP] = { 0 };
  uint8_t lastAB[NUM_ENCODER_CHIPS][ENC_PER_CHIP] = { 0 };
  // pro Encoder die Zeit der letzten Flanke merken
  static uint32_t lastStepUs[NUM_ENCODER_CHIPS][ENC_PER_CHIP] = { { 0 } };
  // simple Wheel-Acceleration (Grenzen gerne feinjustieren)
  // Quadratur-Übergangstabelle (prev<<2 | curr) -> delta
  // +1: 00->01->11->10->00,  -1: umgekehrt, 0: ungültig/Prell
  static constexpr int8_t quadLUT[16] = {
    0, -1, +1, 0,
    +1, 0, 0, -1,
    -1, 0, 0, +1,
    0, +1, -1, 0
  };


// -------------------- ENCODER-HANDLE ---------------------------------------------------------------------------------------
  static inline uint8_t accelMultiplier(uint32_t dt_us) {
    if (dt_us < 1800) return 8;  // <1.0 ms pro Flanke -> sehr schnell
    if (dt_us < 2400) return 4;  // <2.5 ms
    if (dt_us < 2800) return 2;  // <6.0 ms
    return 1;                    // sonst normal
  }

  void encoder_handle() {
    for (uint8_t row = 1; row <= 3; ++row) {
      if (encoderChanged[row][0] || encoderChanged[row][1]) {  //eine der beiden bänke hat int getriggert
        encoderChanged[row][0] = false;
        encoderChanged[row][1] = false;
        since_last_touch = millis();
        idle_b = false;
        touched_b = true;
        uint16_t gpio = mcp[row].readGPIOAB();  //alle pins vom mcp auslesen

        for (uint8_t enc_i = 0; enc_i < ENC_PER_CHIP; ++enc_i) {
          uint8_t a = bitRead(gpio, enc_rot_pins_A[enc_i]);
          uint8_t b = bitRead(gpio, enc_rot_pins_B[enc_i]);
          uint8_t curr = (a << 1) | b;
          uint8_t prev = lastAB[row][enc_i];

          int8_t delta = quadLUT[(prev << 2) | curr];  // -1,0,+1 pro Flanke

          if (delta) {
            // Encoder 4..7 haben invertierte Richtung
            int8_t dir = (enc_i > 3) ? -1 : +1;  //richtung invertieren
            // Zeit seit letzter Flanke messen
            uint32_t now = micros();
            uint32_t dt = lastStepUs[row][enc_i] ? (now - lastStepUs[row][enc_i]) : 0xFFFFFFFF;
            lastStepUs[row][enc_i] = now;
            uint8_t mult = accelMultiplier(dt);

            counter[enc_mode][row][enc_i] += dir * delta * mult;

            // Begrenzen
            if (counter[enc_mode][row][enc_i] > MAX_VALUE) counter[enc_mode][row][enc_i] = MAX_VALUE;
            if (counter[enc_mode][row][enc_i] < MIN_VALUE) counter[enc_mode][row][enc_i] = MIN_VALUE;

            //midi senden wenn änderung groß genug -> eine rastung
            uint8_t v = counter[enc_mode][row][enc_i] / 4;
            if (v != counter_last_midi[enc_mode][row][enc_i]) {
              counter_last_midi[enc_mode][row][enc_i] = v;
              auto addr = encoderAddr(row, enc_i, BASE_NOTE, CHANNEL_switch);
              midi.sendControlChange(addr, v);
              //Serial.printf("row=%u enc=%u cc=%u val=%u\n", row, enc_i, addr.getAddress(), v);
              //Serial.printf("Counter: %d,   Counter/4: %d,  mult: %d \n", counter[enc_mode][row][enc_i], counter[enc_mode][row][enc_i]/4, mult);
            }
          }
          lastAB[row][enc_i] = curr;
        }
        mcp[row].clearInterrupts();
      }
    }
  }


// -------------------- BUTTON/NAV-HANDLE ------------------------------------------------------------------------------------
  bool button_used(uint8_t row, uint8_t enc) {
    if (enc_mode == 0 && enc_used_licht[row][enc] == 1) {
      return true;
    }
    if (enc_mode == 1) {
      return true;
    }
    if (enc_mode == 2 && enc_used_details[row][enc] == 1) {
      return true;
    }
    if (enc_mode == 3 && enc_used_cg[row][enc] == 1) {
      return true;
    }
    return false;
  }
  void encoder_button_handle() {
    for (uint8_t row_i = 1; row_i <= 3; row_i++) {  // --- Buttons verarbeiten: je Reihe ein Flag, einmal Ports lesen ---
      if (row_i == 1) {
        if (button_changed[1][0]) {
          button_changed[1][0] = false;
          since_last_touch = millis();
          idle_b = false;
          touched_b = true;
          // Einmal den ganzen 16-Bit Port lesen, dann pro Taste auswerten
          uint16_t gpio = mcp[4].readGPIOAB();
          for (int enc_i = 0; enc_i < 8; enc_i++) {
            bool currentbtn = bitRead(gpio, enc_but_pins_4[enc_i]);  // Pullup: LOW = gedrückt
            if (!currentbtn != button_enc_last[row_i][enc_i]) {
              button_enc_last[row_i][enc_i] = !currentbtn;
              auto addr = encoderAddr(row_i, enc_i, BASE_NOTE + 25 + 16, CHANNEL_switch);
              if (!currentbtn) {
                if (button_used(row_i - 1, enc_i)) {
                  fill_1_enc(row_i, enc_i, 0);
                  led_enc.show();
                  fill_1_enc(row_i, enc_i, color_main);
                  led_enc.show();
                  fill_1_enc(row_i, enc_i, 0);
                  led_enc.show();
                  counter_last[enc_mode][row_i][enc_i] = -1;
                }
                midi.sendControlChange(addr, 127);
              } else {
                midi.sendControlChange(addr, 0);
              }
              //Serial.printf("row= %u    enc= %u     cc=%u  btn: %d \n" , row_i, enc_i, addr.getAddress(), !currentbtn);
            }
          }
          mcp[4].clearInterrupts();
        }
      } 
      else if (row_i == 2) {
        if (button_changed[2][0]) {
          button_changed[2][0] = false;
          since_last_touch = millis();
          idle_b = false;
          touched_b = true;
          uint16_t gpio = mcp[5].readGPIOAB();
          for (int enc_i = 0; enc_i < 8; enc_i++) {
            bool currentbtn = bitRead(gpio, enc_but_pins_5[enc_i]);  // 0..7
            if (!currentbtn != button_enc_last[row_i][enc_i]) {
              button_enc_last[row_i][enc_i] = !currentbtn;
              auto addr = encoderAddr(row_i, enc_i, BASE_NOTE + 25 + 16, CHANNEL_switch);
              if (!currentbtn) {
                if (button_used(row_i - 1, enc_i)) {
                  fill_1_enc(row_i, enc_i, 0);
                  led_enc.show();
                  fill_1_enc(row_i, enc_i, color_main);
                  led_enc.show();
                  fill_1_enc(row_i, enc_i, 0);
                  led_enc.show();
                  counter_last[enc_mode][row_i][enc_i] = -1;
                }
                midi.sendControlChange(addr, 127);
              } else {
                midi.sendControlChange(addr, 0);
              }
              //Serial.printf("row= %u    enc= %u     cc=%u  btn: %d \n" , row_i, enc_i, addr.getAddress(), !currentbtn);
            }
          }
          mcp[5].clearInterrupts();
        }
      } else if (row_i == 3) {
        if (button_changed[3][0]) {
          button_changed[3][0] = false;
          since_last_touch = millis();
          idle_b = false;
          touched_b = true;
          uint16_t gpio = mcp[5].readGPIOAB();
          for (uint8_t enc_i = 8; enc_i < 16; enc_i++) {
            bool currentbtn = bitRead(gpio, enc_but_pins_5[enc_i]);  // 8..15
            if (!currentbtn != button_enc_last[row_i][enc_i]) {
              button_enc_last[row_i][enc_i] = !currentbtn;
              auto addr = encoderAddr(row_i, enc_i - 8, BASE_NOTE + 25 + 16, CHANNEL_switch);
              if (!currentbtn) {
                if (button_used(row_i - 1, enc_i - 8)) {
                  fill_1_enc(row_i, enc_i - 8, 0);
                  led_enc.show();
                  fill_1_enc(row_i, enc_i - 8, color_main);
                  led_enc.show();
                  fill_1_enc(row_i, enc_i - 8, 0);
                  led_enc.show();
                  counter_last[enc_mode][row_i][enc_i - 8] = -1;
                }
                midi.sendControlChange(addr, 127);
              } else {
                midi.sendControlChange(addr, 0);
              }
              //Serial.printf("row= %u    enc= %u     cc=%u  btn: %d \n" , row_i, enc_i, addr.getAddress(), !currentbtn);
            }
          }
          mcp[5].clearInterrupts();
        }
      }
    }
  }
  void handle_pcb(uint8_t pcb) {
    uint8_t chip = CHIP_OF_PCB[pcb];
    // Phase 1: Interrupt kam -> erste Probe + Recheck terminieren
    if (rubber_button_changed[pcb][0] || rubber_button_changed[pcb][1]) {
      rubber_button_changed[pcb][0] = false;
      rubber_button_changed[pcb][1] = false;
      since_last_touch = millis();
      idle_b = false;
      touched_b = true;

      debounce_snap1[pcb] = mcp[chip].readGPIOAB();  // erste Probe
      debounce_due[pcb] = millis() + DEBOUNCE_MS;    // Zeitpunkt für Recheck
      debounce_pending[pcb] = true;                  // Recheck aktivieren
      mcp[chip].clearInterrupts();
      return;  // noch NICHT übernehmen!
    }

    // Phase 2: Recheck fällig?
    if (debounce_pending[pcb] && (int32_t)(millis() - debounce_due[pcb]) >= 0) {
      uint16_t readout = mcp[chip].readGPIOAB();  // zweite Probe
      if (readout == debounce_snap1[pcb]) {       // stabil -> jetzt übernehmen
        const uint8_t *b2k = B2K[pcb];
        for (uint8_t bit = 0; bit < 16; ++bit) {
          uint8_t key = b2k[bit];
          if (key == UNUSED) continue;
          bool pressed = !bitRead(readout, bit);  // Pullup: LOW=pressed
          if (button_state[pcb][key] != pressed) {
            button_state[pcb][key] = pressed;
            sth_changed = true;  // LEDs später updaten
            //if (pressed) Serial.printf("pcb:%u chip:%u key:%u bit:%u gpio:%u\n", pcb, chip, key, bit, readout);
            auto addr = encoderAddr_2(pcb + 1, key, 0, CHANNEL_switch);
            if (!pressed) {
              //counter[row_i][enc_i - 8] = 0;
              midi.sendControlChange(addr, 127);
            } else {
              midi.sendControlChange(addr, 0);
            }
          }
        }
        debounce_pending[pcb] = false;  // fertig entprellt
      } else {
        // noch prellend -> neue Probe merken, erneuter Recheck
        debounce_snap1[pcb] = readout;
        debounce_due[pcb] = millis() + DEBOUNCE_MS;
      }
      mcp[chip].clearInterrupts();
    }
  }
  static inline uint8_t accelMultiplier_nav(uint32_t dt_us) {
    if (dt_us < 10000) return 8;  // <1.0 ms pro Flanke -> sehr schnell
    if (dt_us < 20000) return 4;  // <2.5 ms
    if (dt_us < 30000) return 2;  // <6.0 ms
    return 1;                     // sonst normal
  }
  int test_int=0;//debounce encoder nav

  void handle_nav() {
    int pcb = 2;  //nav
    uint8_t chip = CHIP_OF_PCB[pcb];
    if (nav_changed) {
      nav_changed = false;
      since_last_touch = millis();
      idle_b = false;
      touched_b = true;
      debounce_snap1[pcb] = mcp[chip].readGPIOAB();  // erste Probe                           
      debounce_due[pcb] = millis() + DEBOUNCE_MS;    // Zeitpunkt für Recheck
      debounce_pending[pcb] = true;                  // Recheck aktivieren
      //encoder
      uint8_t a = bitRead(debounce_snap1[pcb], 0);
      uint8_t b = bitRead(debounce_snap1[pcb], 8);
      uint8_t curr = (a << 1) | b;
      uint8_t prev = lastAB_nav;
      int8_t delta = quadLUT[(prev << 2) | curr];  // -1,0,+1 pro Flanke
      if (delta) {
        sth_changed = true;
        uint32_t now = micros();
        uint32_t dt = lastStepUs_nav ? (now - lastStepUs_nav) : 0xFFFFFFFF;
        lastStepUs_nav = now;
        uint8_t mult = accelMultiplier_nav(dt);

        counter_nav -= delta * mult;
        if(!button_state[0][7]){
          test_int+=delta;
          if(delta == 1 && test_int >= 2){
          test_int=0;
          midi.sendControlChange(cs::MIDIAddress{ 125, CHANNEL_switch }, 127);
          midi.sendControlChange(cs::MIDIAddress{ 125, CHANNEL_switch }, 0);
          }
          else if(delta == -1 && test_int <= -2){
            test_int=0; 
            midi.sendControlChange(cs::MIDIAddress{ 126, CHANNEL_switch }, 127);
            midi.sendControlChange(cs::MIDIAddress{ 126, CHANNEL_switch }, 0);
          }
        }     
        //Serial.printf("counter: %d,  delta: %d   speed: %d,    dt: %d,    map: %d, max_hell_dimm: %d, max_power_back: %d \n",counter_nav,delta,mult, dt,map(counter_nav, 0, MAX_VALUE_nav, 0, 255), max_power_back_dimm, max_power_back);
        if (counter_nav > MAX_VALUE_nav) counter_nav = MAX_VALUE_nav;
        if (counter_nav < MIN_VALUE_nav) counter_nav = MIN_VALUE_nav;
        //midi senden wenn änderung groß genug -> eine rastung
        uint8_t v = counter_nav / 4;
        if (v != counter_last_nav_midi && !button_state[0][7]){
          counter_last_nav_midi = v;
          midi.sendControlChange(cs::MIDIAddress{ 127, CHANNEL_switch }, counter_nav);
        }
      }
      lastAB_nav = curr;
      mcp[chip].clearInterrupts();
    }

    if (debounce_pending[pcb] && (int32_t)(millis() - debounce_due[pcb]) >= 0) {
      //buttons
      uint16_t readout = mcp[chip].readGPIOAB();  // zweite Probe
      if (readout == debounce_snap1[pcb]) {       // stabil -> jetzt übernehmen
        const uint8_t *b2k = B2K[pcb];
        for (uint8_t bit = 0; bit < 16; ++bit) {
          uint8_t key = b2k[bit];
          if (key == UNUSED) continue;
          bool pressed = !bitRead(readout, bit);  // Pullup → LOW = gedrückt
          if (button_state[pcb][key] != pressed) {
            button_state[pcb][key] = pressed;
            sth_changed = true;
            auto addr = encoderAddr_2(pcb + 1, key, 0, CHANNEL_switch);
            if (!pressed) {
              //counter[row_i][enc_i - 8] = 0;
              midi.sendControlChange(addr, 127);
            } else {
              midi.sendControlChange(addr, 0);
            }
          }
        }
      }
    }
  }
  void handle_rest() {
    handle_pcb(0);
    handle_pcb(1);
    handle_nav();
  }

// -------------------- HILFSZEUG -------------------------------------------------------------------------------------------

  void seedRNG() {
    randomSeed((uint32_t)micros() ^ ((uint32_t)analogRead(A0) << 16));
  }
  static inline uint8_t cap8(uint16_t v, uint8_t cap) {
    return (v > cap) ? cap : (uint8_t)v;
  }
  static inline void fillRamp(uint8_t *arr, uint8_t from, uint8_t to) {
    for (uint8_t i = 0; i < N_RING; ++i) {
      // gewichtetes Mittel: ((from*(N-1-i) + to*i) / (N-1))
      uint16_t num = (uint16_t)from * (N_RING - 1 - i) + (uint16_t)to * i;
      arr[i] = num / (N_RING - 1);
    }
  }
  static void initRamps() {
    fillRamp(ramp_200_to_0,   200, 0);
    fillRamp(ramp_255_to_0,   255, 0);
    fillRamp(ramp_255_to_70,  255, 70);
    fillRamp(ramp_200_to_130, 200, 130);
    fillRamp(ramp_255_to_20,  255, 20);
    fillRamp(ramp_255_to_10,  255, 10);
    fillRamp(ramp_255_to_120, 255, 120);
  }
  // rounded mapping (no overflow up to 16-bit counter)
  uint8_t map_u16_to_0_24(uint16_t v, uint16_t MAX_VAL) {
    return (uint32_t(v) * 24u + (MAX_VAL/2)) / uint32_t(MAX_VAL);
  }
  void fill01_24_exact(uint8_t out[24], uint8_t k) {
    for (uint8_t i = 0; i < 24; ++i) {
      uint8_t slotsLeft = 24 - i;
      if (k && (uint8_t)random(slotsLeft) < k) {
        out[i] = 1;
        --k;
      } else {
        out[i] = 0;
      }
    }
  }
  void fillK_24_values(uint8_t out[24], uint8_t k, uint8_t maxVal, bool allowZero = false) {
    // Alles erstmal auf 0
    for (uint8_t i = 0; i < 24; ++i) out[i] = 0;

    if (k == 0 || (maxVal == 0 && !allowZero)) return;  // nichts zu tun
    if (k > 24) k = 24;
    // (maxVal ist ohnehin uint8_t, also 0..255)

    // Einmal 0..23 aufbauen und per partieller Fisher-Yates-Shuffle k eindeutige Indizes ziehen
    uint8_t idx[24];
    for (uint8_t i = 0; i < 24; ++i) idx[i] = i;

    for (uint8_t i = 0; i < k; ++i) {
      uint8_t j = i + (uint8_t)random(24 - i);  // zufällig aus i..23
      uint8_t tmp = idx[i];
      idx[i] = idx[j];
      idx[j] = tmp;
    }

    // k gezogene Positionen belegen
    for (uint8_t t = 0; t < k; ++t) {
      uint8_t pos = idx[t];
      uint8_t val;
      if (allowZero) {
        // 0..maxVal inkl.
        val = (uint8_t)random((uint16_t)maxVal + 1);
      } else {
        // 1..maxVal (falls maxVal==0 oben schon abgefangen)
        val = (uint8_t)(1 + random(maxVal));
      }
      out[pos] = val;
    }
  }     
  void handleReceivedTinyIRData(uint16_t aAddress, uint8_t aCommand, bool isRepeat) {
    /*
      Serial.print(F("A=0x"));
      Serial.print(aAddress, HEX);
      Serial.print(F(" C=0x"));
      Serial.print(aCommand, HEX);
      Serial.print(F(" R="));
      Serial.print(isRepeat);
      Serial.println();
    */
    IR_aAddress = aAddress;
    IR_aCommand = aCommand;
    IR_isRepeat = isRepeat;
    if (IR_aCommand == 0x1 && !isRepeat) {//1TV
      main_mode = 0;
      enc_mode = 0;
      since_last_touch = millis();
      idle_b = false;
      touched_b = true;
      draw_enc_setup(enc_mode, color_main, color_back);
      CHANNEL_switch = cs::Channel{ enc_mode };
      haptic_play(86);
    } 
    else if (IR_aCommand == 0x4 && !isRepeat  && digitalReadFast(5)) {//2TV
      main_mode = 1;
      since_last_touch = millis();
      idle_b = false;      
      haptic_play(86);
    } 
    else if (IR_aCommand == 0xD && !isRepeat) {//4TV
      main_mode = 3;
      since_last_touch = millis();
      idle_b = false;
      idleTwinklesReset();
      touched_b = false;
      haptic_play(86);
    } 
    else if (IR_aCommand == 0x1A && !isRepeat) {//UTV    
      haptic_play(86);
    } 
    else if (IR_aCommand == 0x9 && !isRepeat) {//DTV
      haptic_play(86);
    } 
    else if (IR_aCommand == 0x3 && !isRepeat) {//OFF
      haptic_play(7);
      since_last_touch = millis();
      idle_b = false;
      if(is_prev){
        is_prev=false;
        midi.sendControlChange(addr_ir_6, 127);
        midi.sendControlChange(addr_ir_6, 0);
      }
      else{
        is_prev=true;
        midi.sendControlChange(addr_ir_7, 127);
        midi.sendControlChange(addr_ir_7, 0);
      }      
    } 
    else if (IR_aCommand == 0xA && !isRepeat) {//1PC
      haptic_play(7);
      since_last_touch = millis();
      idle_b = false;
      midi.sendControlChange(addr_ir_2, 127);
      midi.sendControlChange(addr_ir_2, 0);
    } 
    else if (IR_aCommand == 0x1B && !isRepeat) {//3PC
      haptic_play(7);
      since_last_touch = millis();
      idle_b = false;
      midi.sendControlChange(addr_ir_3, 127);
      midi.sendControlChange(addr_ir_3, 0);
    } 
    else if (IR_aCommand == 0x1F && !isRepeat) {//UPC
      haptic_play(7);
      since_last_touch = millis();
      idle_b = false;
      //next
      midi.sendControlChange(addr_ir_0, 127);
      midi.sendControlChange(addr_ir_0, 0);
    } 
    else if (IR_aCommand == 0x0 && !isRepeat) {//2PC
      haptic_play(7);
      since_last_touch = millis();
      idle_b = false;
      midi.sendControlChange(addr_ir_4, 127);
      midi.sendControlChange(addr_ir_4, 0);
    } 
    else if (IR_aCommand == 0x5 && !isRepeat) {//4PC
      haptic_play(7);
      since_last_touch = millis();
      idle_b = false;
      midi.sendControlChange(addr_ir_5, 127);
      midi.sendControlChange(addr_ir_5, 0);
    } 
    else if (IR_aCommand == 0x19 && !isRepeat) {//DPC
      haptic_play(7);
      since_last_touch = millis();
      idle_b = false;
      //prev
      midi.sendControlChange(addr_ir_1, 127);
      midi.sendControlChange(addr_ir_1, 0);
    } 
    else if (IR_aCommand == 0x2 && !isRepeat) {//3TV
      main_mode = 2;
      since_last_touch = millis();
      idle_b = false;
      idleTwinklesReset();
      haptic_play(86);
    }
  }
  void haptic_play(uint8_t fx) {
    drv.setWaveform(0, fx);  // Slot 0: Effekt
    drv.setWaveform(0, fx);  // Slot 0: Effekt
    drv.setWaveform(1, 0);   // Slot 1: Ende
    drv.go();
  }
  void led_blink() {
    if (since_last_led_change >= interval_blink) {
      since_last_led_change = since_last_led_change - interval_blink;
      if (ledState == LOW) {
        ledState = HIGH;
      } else {
        ledState = LOW;
      }
      digitalWrite(LED_BUILTIN, ledState);
      //Serial.print("Wert_1: "); Serial.print(counter);Serial.print("Wert_2: "); Serial.println(counter[2][0]);
      Serial.printf("CPU speed: %d MHz   Temp: %.1f C  \n", F_CPU_ACTUAL / 1000000, tempmonGetTemp());
    }
  }
  void print_startup_info() {
    Serial.println("Start");
    Serial.print("*********************************************\n");
    Serial.print("* TeensyParallel.ino                        *\n");
    Serial.print("*********************************************\n");
    Serial.printf(
      "CPU speed: %d MHz   Temp: %.1f C   Serial baud: %.1f MHz\n",
      F_CPU_ACTUAL / 1000000, tempmonGetTemp(),
      tempmonGetTemp() * 9.0 / 5.0 + 32, 800000 * 1.6 / 1000000.0);
  }
  void dump_last_crash() {
    if (CrashReport) {
      Serial.println("CrashReport:");
      Serial.println(CrashReport);
    }
  }
  bool fastRead(uint8_t pin) {
    return digitalReadFast(pin);
  }  // Teensy-Core, extrem schnell
  uint16_t capToMax(uint32_t v, uint32_t max) {
    return (v > max) ? max : v;
  }
  void init_I2C_Peripherals() {
    Wire.begin();
    Wire.setClock(400000);
    //Wire.setClock(1000000);
    Wire2.begin();
    delay(200);
    // MCP23017 von 0x20 bis (0x20 + MCP_COUNT - 1) initialisieren
    for (uint8_t addr = 0x20; addr <= (0x20 + MCP_COUNT - 1); addr++) {
      if (!mcp[addr - 0x20].begin_I2C(addr, &Wire)) {
        Serial.print("MCP23017 0x");
        Serial.print(addr, HEX);
        Serial.println(" nicht gefunden!");
        while (1)
          ;  // abbrechen
      } else {
        Serial.print("MCP23017 0x");
        Serial.print(addr, HEX);
        Serial.println(" gestartet.");
      }
      delay(50);
    }
    // DRV2605 initialisieren
    if (!drv.begin(&Wire2)) {
      Serial.println("DRV2605 nicht gefunden!");
      while (1) delay(10);
    } else {
      Serial.println("DRV2605 gestartet.");
    }
    delay(50);

    for (uint8_t i = 0; i < MCP_COUNT; i++) {
      init_MCP_pins(mcp[i]);
    }
    attachTeensyInterrupts();

    delay(50);
    //pins/ int resetten
    for (uint8_t i = 0; i < MCP_COUNT; i++) {
      mcp[i].readGPIOAB();
      mcp[i].clearInterrupts();
    }

    for (uint8_t row = 1; row <= 3; ++row) {
      uint16_t gpio = mcp[row].readGPIOAB();
      for (uint8_t enc = 0; enc < ENC_PER_CHIP; ++enc) {
        uint8_t a = bitRead(gpio, enc_rot_pins_A[enc]);
        uint8_t b = bitRead(gpio, enc_rot_pins_B[enc]);
        lastAB[row][enc] = (a << 1) | b;  // AB als 2-Bit Zustand
      }
    }
  }
  void init_MCP_pins(Adafruit_MCP23X17 &ref_mcp) {
    // Interrupt-Konfiguration: kein Mirror, kein Open-Drain, active-low
    ref_mcp.setupInterrupts(false, false, LOW);
    // Alle 16 Pins als Input mit Pullup
    for (uint8_t p = 0; p < 16; p++) {
      ref_mcp.pinMode(p, INPUT_PULLUP);
      ref_mcp.setupInterruptPin(p, CHANGE);
    }
  }
  void attachTeensyInterrupts() {
    for (uint8_t i = 0; i < MCP_COUNT; i++) {
      pinMode(mcpIntA[i], INPUT_PULLUP);
      pinMode(mcpIntB[i], INPUT_PULLUP);
    }
    // ----------------- A-ENCODER --------------------------------------------------------------------
    attachInterrupt(digitalPinToInterrupt(mcpIntA[0]), onEncoderInterrupt_A1, FALLING);
    attachInterrupt(digitalPinToInterrupt(mcpIntA[1]), onEncoderInterrupt_A2, FALLING);
    attachInterrupt(digitalPinToInterrupt(mcpIntA[2]), onEncoderInterrupt_A3, FALLING);
    // ----------------- B-ENCODER --------------------------------------------------------------------
    attachInterrupt(digitalPinToInterrupt(mcpIntB[0]), onEncoderInterrupt_B1, FALLING);
    attachInterrupt(digitalPinToInterrupt(mcpIntB[1]), onEncoderInterrupt_B2, FALLING);
    attachInterrupt(digitalPinToInterrupt(mcpIntB[2]), onEncoderInterrupt_B3, FALLING);

    // ----------------- BUTTONS-ENCODER --------------------------------------------------------------------
    attachInterrupt(digitalPinToInterrupt(mcpIntA[3]), onButtonInterrupt, FALLING);
    attachInterrupt(digitalPinToInterrupt(mcpIntA[4]), onButtonInterrupt2a, FALLING);
    attachInterrupt(digitalPinToInterrupt(mcpIntB[4]), onButtonInterrupt2b, FALLING);

    // ----------------- PUSH_BUTTONS-REST --------------------------------------------------------------------
    attachInterrupt(digitalPinToInterrupt(mcpIntA[5]), onButtonInterrupt_main_a, FALLING);
    attachInterrupt(digitalPinToInterrupt(mcpIntA[6]), onButtonInterrupt_key_a, FALLING);
    attachInterrupt(digitalPinToInterrupt(mcpIntA[7]), onButtonInterrupt_nav_a, CHANGE);

    attachInterrupt(digitalPinToInterrupt(mcpIntB[5]), onButtonInterrupt_main_b, FALLING);
    attachInterrupt(digitalPinToInterrupt(mcpIntB[6]), onButtonInterrupt_key_b, FALLING);
    attachInterrupt(digitalPinToInterrupt(mcpIntB[7]), onButtonInterrupt_nav_b, CHANGE);
  }


// -------------------- LED-STUFF -------------------------------------------------------------------------------------------

  uint32_t getPixelColor(const byte *buf, int index) {
    const int off = index * 3;
    uint8_t r = buf[off + 0];
    uint8_t g = buf[off + 1];
    uint8_t b = buf[off + 2];
    return ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
  }
  uint32_t CRGB_to_u32(const CRGB &c) {
    return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
  }
  uint32_t CHSV_to_u32(const CHSV &hsv) {
    CRGB rgb;
    hsv2rgb_rainbow(hsv, rgb);
    return CRGB_to_u32(rgb);
  }
  void u32_to_rgb(uint32_t c, uint8_t &r, uint8_t &g, uint8_t &b) {
    r = (c >> 16) & 0xFF;
    g = (c >> 8) & 0xFF;
    b = (c)&0xFF;
  }
  void all_Led_set(uint32_t color) {
    for (int i = 0; i < led_enc.numPixels(); i++) led_enc.setPixel(i, color);
    for (int i = 0; i < led_rest.numPixels(); i++) led_rest.setPixel(i, color);
  }
  void welcome_led() {
    all_Led_set(0);
    led_enc.show();
    for (byte i = 0; i < 24; i++) {
      for (byte row_i = 1; row_i <= 3; row_i++) {
        for (byte enc_i = 0; enc_i < 8; enc_i++) {
          led_enc.setPixel(enc_led[row_i - 1][enc_i] + i, color_back);
        }
      }
      delay(10);
      led_enc.show();
    }
    draw_enc_setup(0, color_main, color_back);
    enc_dirty = true;
    
  }
  void rainbow() {
    for (int i = 0; i < 96; i++) {
      color_rain = CRGB_to_u32(CHSV(color_shift_rain, 255, capToMax(255, max_power_back)));
      led_eb.setPixel(i, color_rain);
    }
    led_eb.show();
  }
  void clearAll() {
    for (int i = 0; i < led_enc.numPixels(); i++) led_enc.setPixel(i, 0);
    for (int i = 0; i < led_rest.numPixels(); i++) led_rest.setPixel(i, 0);
    for (int i = 0; i < led_eb.numPixels(); i++) led_eb.setPixel(i, 0);
  }
  void fill_1_enc(uint8_t row, uint8_t enc, uint32_t color) {
    for (byte i = 0; i < 24; i++) {
      led_enc.setPixel((enc_led[row - 1][enc]) + i, color);
    }
  }
  void ring_gradient24_rgb(WS2812Serial &strip, int start, uint32_t c1, uint32_t c2) {
    constexpr int N = 24;
    uint8_t r1, g1, b1, r2, g2, b2;
    u32_to_rgb(c1, r1, g1, b1);
    u32_to_rgb(c2, r2, g2, b2);

    for (int i = 0; i < N; ++i) {
      // gewichtete Mischung: ((a*(N-1-i) + b*i) / (N-1))
      uint16_t w1 = (N - 1 - i);
      uint16_t w2 = i;
      uint8_t r = (uint16_t(r1) * w1 + uint16_t(r2) * w2) / (N - 1);
      uint8_t g = (uint16_t(g1) * w1 + uint16_t(g2) * w2) / (N - 1);
      uint8_t b = (uint16_t(b1) * w1 + uint16_t(b2) * w2) / (N - 1);
      uint32_t c = (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
      strip.setPixel(start + i, c);
    }
  }
  void ring_2_halves(uint8_t row, uint8_t enc, uint32_t color, uint32_t color2) {
    for (byte i = 0; i < 13; i++) {
      led_enc.setPixel((enc_led[row - 1][enc]) + i, color);
    }
    for (byte i = 13; i < 24; i++) {
      led_enc.setPixel((enc_led[row - 1][enc]) + i, color2);
    }
  }
  void ring_3_halves(uint8_t row, uint8_t enc, uint32_t color, uint32_t color2, uint32_t color3) {
    uint16_t start_pos = (enc_led[row - 1][enc]);
    for (int i = start_pos; i < start_pos + 24; i++) {
      if (enc_upper[i - start_pos]) {
        led_enc.setPixel(i, color2);
      }
    }
    for (int i = start_pos; i < start_pos + 24; i++) {
      if (enc_lower[i - start_pos]) {
        led_enc.setPixel(i, color2);
      }
    }
    for (int i = start_pos; i < start_pos + 24; i++) {
      if (enc_left[i - start_pos]) {
        led_enc.setPixel(i, color);
      }
    }
    for (int i = start_pos; i < start_pos + 24; i++) {
      if (enc_right[i - start_pos]) {
        led_enc.setPixel(i, color3);
      }
    }
  }
  void draw_enc_1_col(uint32_t color, uint32_t color2) {
    for (byte row_i = 1; row_i <= 3; row_i++) {
      for (byte enc_i = 0; enc_i < 8; enc_i++) {
        if (counter[enc_mode][row_i][enc_i] != counter_last[enc_mode][row_i][enc_i]) {
          counter_last[enc_mode][row_i][enc_i] = counter[enc_mode][row_i][enc_i];
          draw_enc_ptr(row_i - 1, enc_i, map_u16_to_0_24(counter[enc_mode][row_i][enc_i], MAX_VALUE), color, color2);
          enc_dirty = true;
        }
      }
    }
  }
  void draw_enc_ptr(uint8_t row, uint8_t enc, int val, uint32_t color, uint32_t color2) {
    int value = val < 0 ? 0 : (val > 23 ? 23 : val);  //begrenzen auf min, max
    uint16_t start_pos = enc_led[row - 1][enc];
    for (int i = start_pos; i < start_pos + 24; i++) led_enc.setPixel(i, color2);
    //counter
    led_enc.setPixel(start_pos + led_rev[value], color);
  }
  void draw_enc_ptr_sat(uint8_t row, uint8_t enc, int val, uint32_t color_ptr, uint8_t enc_i) {
    int value = val < 0 ? 0 : (val > 23 ? 23 : val);  //begrenzen auf min, max
    uint32_t color_verlauf;
    uint16_t start_pos = enc_led[row - 1][enc];
    for (int i = start_pos, j = 0; j < 24; ++i, ++j) {
      uint8_t var_hell = cap8(ramp_200_to_130[j], (uint8_t)max_power_back);
      uint8_t var_sat  = ramp_255_to_10[j];
      uint32_t color_verlauf = CHSV_to_u32(CHSV(HUES_enc[enc_i], var_sat, var_hell));
      led_enc.setPixel(i, color_verlauf);
    }
    //counter beleuchten
    led_enc.setPixel(start_pos + led_rev[value], color_ptr);
  }
  void draw_enc_ptr_hell(uint8_t row, uint8_t enc, int val, uint32_t color_ptr, uint8_t enc_i) {
    int value = val < 0 ? 0 : (val > 23 ? 23 : val);  //begrenzen auf min, max
    uint32_t color_verlauf;
    uint16_t start_pos = enc_led[row - 1][enc];
    for (int i = start_pos, j = 0; j < 24; ++i, ++j) {
      uint8_t var_hell = cap8(ramp_200_to_0[j], (uint8_t)max_power_back);
      uint32_t color_verlauf = CHSV_to_u32(CHSV(HUES_enc[enc_i], 255, var_hell));
      led_enc.setPixel(i, color_verlauf);
    }
    //counter beleuchten
    led_enc.setPixel(start_pos + led_rev[value], color_ptr);
  }
  void draw_enc_hell_var(int start_pos, uint8_t hue_var) {
    uint32_t color_verlauf;
    for (int i = start_pos, j = 0; j < 24; ++i, ++j) {
      uint8_t var_hell = cap8(ramp_255_to_0[j], (uint8_t)max_power_back);
      uint8_t var_sat  = ramp_255_to_20[j];
      uint32_t color_verlauf = CHSV_to_u32(CHSV(hue_var, var_sat, var_hell));
      led_enc.setPixel(i, color_verlauf);
    }
  }
  void draw_enc_saet(int start_pos) {
    uint32_t color_verlauf;
    for (int i = start_pos, j = 0; j < 24; ++i, ++j) {
      uint8_t var_hell = cap8(ramp_255_to_70[j], (uint8_t)max_power_back);
      uint8_t var_sat  = ramp_255_to_0[j];
      uint8_t hue      = (uint8_t)(j * 8 + 60); // vormals (i - start_pos) * 8 + 60
      uint32_t color_verlauf = CHSV_to_u32(CHSV(hue, var_sat, var_hell));
      led_enc.setPixel(i, color_verlauf);
    }
  }
  void draw_enc_saet_var(int start_pos, uint8_t hue_var) {
    uint32_t color_verlauf;
    for (int i = start_pos, j = 0; j < 24; ++i, ++j) {
      uint8_t var_hell = cap8(ramp_255_to_120[j], (uint8_t)max_power_back);
      uint8_t var_sat  = ramp_255_to_0[j];
      uint32_t color_verlauf = CHSV_to_u32(CHSV(hue_var, var_sat, var_hell));
      led_enc.setPixel(i, color_verlauf);
    }
  }
  void draw_enc_dyn(int start_pos) {
    uint32_t color_verlauf;
    for (int i = start_pos; i < start_pos + 24; i++) {
      uint8_t var_hell = capToMax(255, max_power_back);
      color_verlauf = CHSV_to_u32(CHSV(HUES_enc[(i - start_pos) / 3], 255, var_hell));
      led_enc.setPixel(i, color_verlauf);
    }
  }
  void draw_enc_switching(uint8_t row, uint8_t enc, uint32_t color, uint32_t color2) {
    for (byte i = 0; i < 24; i++) {
      led_enc.setPixel((enc_led[row - 1][enc]) + i, color);
    }
    for (byte i = 0; i < 24; i += 2) {
      led_enc.setPixel((enc_led[row - 1][enc]) + i, color2);
    }
  }
  void draw_enc_switching_hell(uint8_t row, uint8_t enc, uint8_t color, uint8_t sat, uint8_t color2, uint8_t sat2) {
    for (byte i = 0; i < 24; i += 2) {
      uint32_t col_1 = CRGB_to_u32(CHSV(color, sat, capToMax(map(i, 0, 24, 255, 0), max_power_back)));
      led_enc.setPixel((enc_led[row - 1][enc]) + i, col_1);
      uint32_t col_2 = CRGB_to_u32(CHSV(color2, sat2, capToMax(map(i, 0, 24, 255, 0), max_power_back)));
      led_enc.setPixel((enc_led[row - 1][enc]) + i + 1, col_2);
    }
  }
  void draw_enc_switching_rgb(uint8_t row, uint8_t enc, uint32_t color) {
    for (byte i = 0; i < 24; i++) {
      led_enc.setPixel((enc_led[row - 1][enc]) + i, color);
    }
    for (byte i = 0; i < 24; i += 2) {
      uint32_t col_rgb = CRGB_to_u32(CHSV(HUES_enc[i / 3], 255, capToMax(255, max_power_back)));
      led_enc.setPixel((enc_led[row - 1][enc]) + i, col_rgb);
    }
  }
  void draw_enc_rgb_rnd(uint8_t row, uint8_t enc) {
    fillK_24_values(pattern, 24, 255, true);
    for (byte i = 0; i < 24; i++) {
      led_enc.setPixel((enc_led[row - 1][enc]) + i, CRGB_to_u32(CHSV(pattern[i], 255, capToMax(255, max_power_back))));
    }
  }
  void draw_enc_switching_rgb_rnd(uint8_t row, uint8_t enc, uint32_t color) {
    fillK_24_values(pattern, 24, 8, true);
    for (byte i = 0; i < 24; i++) {
      led_enc.setPixel((enc_led[row - 1][enc]) + i, CRGB_to_u32(CHSV(HUES_enc[pattern[i]], 255, capToMax(255, max_power_back))));
    }
    for (byte i = 0; i < 24; i += 2) {
      led_enc.setPixel((enc_led[row - 1][enc]) + i, color);
    }
  }
  void draw_enc_switching_rgb_rnd_hell(uint8_t row, uint8_t enc) {
    fillK_24_values(pattern, 24, 255, false);
    for (byte i = 0; i < 24; i++) {
      led_enc.setPixel((enc_led[row - 1][enc]) + i, CRGB_to_u32(CHSV(pattern[i], 255, capToMax(pattern[i], max_power_back))));
    }
  }
  void draw_enc_hell(uint8_t row, uint8_t enc, uint32_t color) {
    uint32_t color_verlauf;
    uint16_t start_pos = enc_led[row - 1][enc];
    for (int i = start_pos; i < start_pos + 24; i++) {
      uint8_t var_hell = capToMax(ramp_200_to_0[i - start_pos], max_power_back);
      color_verlauf = CHSV_to_u32(CHSV(0, 0, var_hell));
      led_enc.setPixel(i, color_verlauf);
    }
  }  
  void draw_upper_half(int start_pos, uint32_t color, uint32_t color2) {
    for (int i = start_pos; i < start_pos + 24; i++) {
      if (enc_upper[i - start_pos]) {
        led_enc.setPixel(i, color);
      } else {
        led_enc.setPixel(i, color2);
      }
    }
  }
  void draw_lower_half(int start_pos, uint32_t color, uint32_t color2) {
    for (int i = start_pos; i < start_pos + 24; i++) {
      if (enc_lower[i - start_pos] == 1) {
        led_enc.setPixel(i, color);
      } else {
        led_enc.setPixel(i, color2);
      }
    }
  }
  void draw_left_half(int start_pos, uint32_t color, uint32_t color2) {
    for (int i = start_pos; i < start_pos + 24; i++) {
      if (enc_left[i - start_pos]) {
        led_enc.setPixel(i, color);
      } else {
        led_enc.setPixel(i, color2);
      }
    }
  }
  void draw_right_half(int start_pos, uint32_t color, uint32_t color2) {
    for (int i = start_pos; i < start_pos + 24; i++) {
      if (enc_right[i - start_pos]) {
        led_enc.setPixel(i, color);
      } else {
        led_enc.setPixel(i, color2);
      }
    }
  }
  void draw_mid_half(int start_pos, uint32_t color, uint32_t color2) {
    for (int i = start_pos; i < start_pos + 24; i++) {
      if (enc_mid[i - start_pos]) {
        led_enc.setPixel(i, color);
      } else {
        led_enc.setPixel(i, color2);
      }
    }
  }
  void draw_mid_korn(int start_pos, uint32_t color, uint32_t color2) {
    for (int i = start_pos; i < start_pos + 24; i++) {
      if (enc_korn[i - start_pos]) {
        led_enc.setPixel(i, color);
      } else {
        led_enc.setPixel(i, color2);
      }
    }
  }
  void draw_corner(int start_pos, uint32_t color, uint32_t color2) {
    for (int i = start_pos; i < start_pos + 24; i++) {
      led_enc.setPixel(i, enc_corner[i - start_pos] ? color : color2);
    }
  }
  void draw_unrg_arr(int start_pos, uint32_t color, uint32_t color2) {
    uint32_t col_b = CRGB_to_u32(CHSV(160, 255, capToMax(255, max_power_back)));
    for (int i = start_pos; i < start_pos + 24; i++) {
      led_enc.setPixel(i, enc_unrg[i - start_pos] ? color : color2);
    }
    led_enc.setPixel(start_pos, col_b);
  }
  void draw_rnd_k(int start_pos, uint32_t color, uint32_t color2, uint8_t k) {
    fill01_24_exact(pattern, k);
    for (int i = start_pos; i < start_pos + 24; i++) {
      led_enc.setPixel(i, pattern[i - start_pos] ? color : color2);
    }
  }
  void led_nav_draw(int start_pos, int val, uint32_t color, uint32_t color2) {
    int value = val < 0 ? 0 : (val > 23 ? 23 : val);  //begrenzen auf min, max
    //alle schwarz zeichnen
    //fill_solid(&big[start_pos], 24, color2);
    for (int i = start_pos; i < start_pos + 24; i++) led_rest.setPixel(i, color2);
    //nur count beleuchten
    //big[start_pos+value] = color;
    led_rest.setPixel(start_pos + value, color);
  }
  void draw_nav_butt() {
    uint32_t color_read = 0;
    for (uint8_t key = 10; key <= 13; key++) {
      uint8_t ledIdx = led_nav_enc[key - 10];  // Index auf led_main
      ledIdx += LED_shift[2];
      if (ledIdx >= NUM_LEDS_main) continue;
      color_read = getPixelColor(drawingMemory2, ledIdx);
      led_rest.setPixel(ledIdx, button_state[2][key] ? c_wh_bright : color_read);
    }
    if (button_state[2][9]) {
      for (int i = (2 + LED_shift[2]); i < (24 + 2 + LED_shift[2]); i++) {
        led_rest.setPixel(i, c_wh_bright);
      }
    }
  }
  void draw_enc_hsv() {
    for (byte row_i = 1; row_i <= 3; row_i++) {
      //hue
      if (row_i == 3) {
        for (byte enc_i = 0; enc_i < 8; enc_i++) {
          if (counter[enc_mode][row_i][enc_i] != counter_last[enc_mode][row_i][enc_i]) {
            counter_last[enc_mode][row_i][enc_i] = counter[enc_mode][row_i][enc_i];
            //Serial.printf("counter_enc: %d,  counter/4: %d   map: %d \n",counter[row_i][enc_i],  counter[row_i][enc_i]/4,map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24));
            uint32_t color_enc = CRGB_to_u32(CHSV(HUES_enc[enc_i], 255, capToMax(255, max_power_back)));
            draw_enc_ptr(row_i, enc_i, map_u16_to_0_24(counter[enc_mode][row_i][enc_i], MAX_VALUE), cl_wh_dimm, color_enc);
            enc_dirty = true;
          }
        }
      }
      //sat
      if (row_i == 2) {
        for (byte enc_i = 0; enc_i < 8; enc_i++) {
          if (counter[enc_mode][row_i][enc_i] != counter_last[enc_mode][row_i][enc_i]) {
            counter_last[enc_mode][row_i][enc_i] = counter[enc_mode][row_i][enc_i];
            draw_enc_ptr_sat(row_i, enc_i, map_u16_to_0_24(counter[enc_mode][row_i][enc_i], MAX_VALUE), 0, enc_i);
            enc_dirty = true;
          }
        }
      }
      //hell
      if (row_i == 1) {
        for (byte enc_i = 0; enc_i < 8; enc_i++) {
          if (counter[enc_mode][row_i][enc_i] != counter_last[enc_mode][row_i][enc_i]) {
            counter_last[enc_mode][row_i][enc_i] = counter[enc_mode][row_i][enc_i];
            draw_enc_ptr_hell(row_i, enc_i, map_u16_to_0_24(counter[enc_mode][row_i][enc_i], MAX_VALUE), cl_wh_dimm, enc_i);
            enc_dirty = true;
          }
        }
      }
    }
  }
  void draw_enc_licht() {
    for (byte row_i = 1; row_i <= 3; row_i++) {
      for (byte enc_i = 0; enc_i < 8; enc_i++) {
        if ((counter[enc_mode][row_i][enc_i] != counter_last[enc_mode][row_i][enc_i]) && enc_used_licht[row_i - 1][enc_i] == 1) {
          counter_last[enc_mode][row_i][enc_i] = counter[enc_mode][row_i][enc_i];
          uint32_t col_ye = CRGB_to_u32(CHSV(35, 255, capToMax(255, max_power_back)));
          uint32_t col_re = CRGB_to_u32(CHSV(0, 255, capToMax(255, max_power_back)));
          uint32_t col_bl = CRGB_to_u32(CHSV(160, 255, capToMax(255, max_power_back)));
          uint32_t col_gr = CRGB_to_u32(CHSV(96, 255, capToMax(200, max_power_back)));
          uint32_t col_pu = CRGB_to_u32(CHSV(198, 255, capToMax(255, max_power_back)));
          uint32_t col_wh_dimm = CRGB_to_u32(CHSV(0, 0, capToMax(180, max_power_back)));
          //Serial.printf("counter_enc: %d,  counter/4: %d   map: %d \n",counter[row_i][enc_i],  counter[row_i][enc_i]/4,map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24));
          enc_dirty = true;
          //neuzeichnen
          if (row_i == 1) {
            if (enc_i == 0) {
              ring_2_halves(1, 0, col_pu, col_gr);
            } else if (enc_i == 1) {
              draw_enc_dyn(enc_led[0][1]);
            } else if (enc_i == 2) {

            } else if (enc_i == 3) {
              draw_upper_half(enc_led[1 - 1][3], col_wh_dimm, 0);
            } else if (enc_i == 4) {
              draw_enc_switching(1, 4, col_wh_dimm, col_ye);
            } else if (enc_i == 5) {

            } else if (enc_i == 6) {
              draw_upper_half(enc_led[1 - 1][6], col_wh_dimm, 0);
            } else {
              draw_enc_switching(1, 7, col_wh_dimm, col_bl);
            }
          } else if (row_i == 2) {
            if (enc_i == 0) {

            } else if (enc_i == 1) {
              draw_enc_saet(enc_led[2][1]);
            } else if (enc_i == 2) {

            } else if (enc_i == 3) {
              draw_enc_hell(2, 3, col_wh_dimm);
            } else if (enc_i == 4) {
              draw_enc_switching(2, 4, col_wh_dimm, 0);
            } else if (enc_i == 5) {

            } else if (enc_i == 6) {
              draw_mid_half(enc_led[2 - 1][6], col_wh_dimm, 0);
            } else {
              draw_enc_switching(2, 7, col_wh_dimm, col_gr);
            }
          } else {
            if (enc_i == 0) {
              ring_2_halves(3, 0, col_ye, col_bl);
            } else if (enc_i == 1) {
              draw_enc_saet(enc_led[2][1]);
            } else if (enc_i == 2) {

            } else if (enc_i == 3) {
              draw_lower_half(enc_led[3 - 1][3], col_wh_dimm, 0);
            } else if (enc_i == 4) {
              draw_enc_switching(3, 4, col_wh_dimm, col_bl);
            } else if (enc_i == 5) {

            } else if (enc_i == 6) {
              draw_lower_half(enc_led[3 - 1][6], col_wh_dimm, 0);
            } else {
              draw_enc_switching(3, 7, col_wh_dimm, col_ye);
            }
          }
          //pointer
          led_enc.setPixel(enc_led[row_i - 1][enc_i] + led_rev[map(counter[enc_mode][row_i][enc_i], 0, MAX_VALUE, 0, 23)], col_re);
        }
      }
    }
  }
  void draw_enc_details() {
    for (byte row_i = 1; row_i <= 3; row_i++) {
      for (byte enc_i = 0; enc_i < 8; enc_i++) {
        if ((counter[enc_mode][row_i][enc_i] != counter_last[enc_mode][row_i][enc_i]) && enc_used_details[row_i - 1][enc_i] == 1) {
          counter_last[enc_mode][row_i][enc_i] = counter[enc_mode][row_i][enc_i];
          fill_1_enc(row_i, enc_i, CRGB_to_u32(CHSV(0, 255, capToMax(18, max_power_back))));
          uint32_t col_gr = CRGB_to_u32(CHSV(96, 255, capToMax(200, max_power_back)));
          uint32_t col_wh_dimm = CRGB_to_u32(CHSV(0, 0, capToMax(40, max_power_back)));
          //neuzeichnen
          if (row_i == 1) {
            if (enc_i == 0) {
              draw_enc_switching_hell(1, 0, 0, 0, 0, 255);
            } else if (enc_i == 1) {

            } else if (enc_i == 2) {
              draw_rnd_k(enc_led[1 - 1][2], cl_wh, 0, 12);
            } else if (enc_i == 3) {
              draw_enc_switching_rgb_rnd(1, 3, col_wh_dimm);
            } else if (enc_i == 4) {

            } else if (enc_i == 5) {
              draw_enc_switching(1, 5, cl_wh, 0);
            } else if (enc_i == 6) {

            } else {
              draw_enc_switching_hell(1, 7, 0, 0, 0, 255);
            }
          } 
          else if (row_i == 2) {
            if (enc_i == 0) {
              draw_enc_switching_hell(2, 0, 0, 0, 0, 255);
            } else if (enc_i == 1) {

            } else if (enc_i == 2) {
              draw_rnd_k(enc_led[2 - 1][2], cl_wh, 0, 12);
            } else if (enc_i == 3) {
              draw_enc_switching_rgb_rnd(2, 3, col_wh_dimm);
            } else if (enc_i == 4) {

            } else if (enc_i == 5) {
              draw_mid_korn(enc_led[2 - 1][5], cl_wh, 0);
            } else if (enc_i == 6) {

            } else {
              draw_corner(enc_led[2 - 1][7], cl_wh, 0);
            }
          } 
          else {
            if (enc_i == 0) {
              draw_enc_switching_hell(3, 0, 0, 0, 0, 255);
            } else if (enc_i == 1) {

            } else if (enc_i == 2) {
              draw_rnd_k(enc_led[3 - 1][2], cl_wh, 0, 12);
            } else if (enc_i == 3) {
              draw_enc_switching_rgb_rnd(3, 3, col_wh_dimm);
            } else if (enc_i == 4) {

            } else if (enc_i == 5) {
              draw_unrg_arr(enc_led[3 - 1][5], cl_wh, 0);
            } else if (enc_i == 6) {

            } else {
              draw_corner(enc_led[3 - 1][7], cl_wh, 0);
            }
          }
          enc_dirty = true;
          //pointer
          led_enc.setPixel(enc_led[row_i - 1][enc_i] + led_rev[map(counter[enc_mode][row_i][enc_i], 0, MAX_VALUE, 0, 23)], col_gr);
        }
      }
    }
  }
  void draw_enc_cg() {
    uint32_t col_wh_dimm = CRGB_to_u32(CHSV(0, 0, capToMax(180, max_power_back)));
    enc_dirty = true;
    for (byte row_i = 1; row_i <= 3; row_i++) {
      for (byte enc_i = 0; enc_i < 8; enc_i++) {
        ring_3_halves(3, 7, col_l, col_m, col_r);
        draw_mid_korn(enc_led[0][7], col_wh_dimm, color_rgb);
        if ((counter[enc_mode][row_i][enc_i] != counter_last[enc_mode][row_i][enc_i]) && enc_used_cg[row_i - 1][enc_i] == 1) {
          counter_last[enc_mode][row_i][enc_i] = counter[enc_mode][row_i][enc_i];
          if(row_i != 3 && enc_i != 7) {
            fill_1_enc(row_i, enc_i, CRGB_to_u32(CHSV(0, 255, capToMax(17, max_power_back))));
          }
          col_l = CRGB_to_u32(CHSV(map(counter[enc_mode][1][0], 0, MAX_VALUE, 0, 255), 255, capToMax(255, max_power_back)));
          col_m = CRGB_to_u32(CHSV(map(counter[enc_mode][1][2], 0, MAX_VALUE, 0, 255), 255, capToMax(255, max_power_back)));
          col_r = CRGB_to_u32(CHSV(map(counter[enc_mode][1][4], 0, MAX_VALUE, 0, 255), 255, capToMax(255, max_power_back)));
          //neuzeichnen
          if (row_i == 1) {
            if (enc_i == 0) {
              draw_enc_dyn(enc_led[0][0]);
              draw_enc_saet_var(enc_led[1][0], map(counter[enc_mode][1][0], 0, MAX_VALUE, 0, 255));
              draw_enc_hell_var(enc_led[2][0], map(counter[enc_mode][1][0], 0, MAX_VALUE, 0, 255));
            } else if (enc_i == 1) {

            } else if (enc_i == 2) {
              draw_enc_dyn(enc_led[0][2]);
              draw_enc_saet_var(enc_led[1][2], map(counter[enc_mode][1][2], 0, MAX_VALUE, 0, 255));
              draw_enc_hell_var(enc_led[2][2], map(counter[enc_mode][1][2], 0, MAX_VALUE, 0, 255));
            } else if (enc_i == 3) {

            } else if (enc_i == 4) {
              draw_enc_dyn(enc_led[0][4]);
              draw_enc_saet_var(enc_led[1][4], map(counter[enc_mode][1][4], 0, MAX_VALUE, 0, 255));
              draw_enc_hell_var(enc_led[2][4], map(counter[enc_mode][1][4], 0, MAX_VALUE, 0, 255));
            } else if (enc_i == 5) {

            } else if (enc_i == 6) {
              draw_enc_dyn(enc_led[0][6]);
              draw_enc_saet_var(enc_led[1][6], map(counter[enc_mode][1][6], 0, MAX_VALUE, 0, 255));
              draw_enc_hell_var(enc_led[2][6], map(counter[enc_mode][1][6], 0, MAX_VALUE, 0, 255));
            } else {

            }
          } 
          else if (row_i == 2) {
            if (enc_i == 0) {
              draw_enc_saet_var(enc_led[1][0], map(counter[enc_mode][1][0], 0, MAX_VALUE, 0, 255));
            } else if (enc_i == 1) {

            } else if (enc_i == 2) {  
              draw_enc_saet_var(enc_led[1][2], map(counter[enc_mode][1][2], 0, MAX_VALUE, 0, 255));
            } else if (enc_i == 3) {

            } else if (enc_i == 4) {  
              draw_enc_saet_var(enc_led[1][4], map(counter[enc_mode][1][4], 0, MAX_VALUE, 0, 255));
            } else if (enc_i == 5) {

            } else if (enc_i == 6) {
              draw_enc_saet_var(enc_led[1][6], map(counter[enc_mode][1][6], 0, MAX_VALUE, 0, 255));
            } else {
              draw_enc_hell(2, 7, col_wh_dimm);
            }
          } 
          else {
            if (enc_i == 0) {
              draw_enc_hell_var(enc_led[2][0], map(counter[enc_mode][1][0], 0, MAX_VALUE, 0, 255));
            } else if (enc_i == 1) {

            } else if (enc_i == 2) {
              draw_enc_hell_var(enc_led[2][2], map(counter[enc_mode][1][2], 0, MAX_VALUE, 0, 255));
            } else if (enc_i == 3) {

            } else if (enc_i == 4) {
              draw_enc_hell_var(enc_led[2][4], map(counter[enc_mode][1][4], 0, MAX_VALUE, 0, 255));
            } else if (enc_i == 5) {

            } else if (enc_i == 6) {
              draw_enc_hell_var(enc_led[2][6], map(counter[enc_mode][1][6], 0, MAX_VALUE, 0, 255));
            } else {
                
            }
          }
          //pointer
          led_enc.setPixel(enc_led[row_i - 1][enc_i] + led_rev[map(counter[enc_mode][row_i][enc_i], 0, MAX_VALUE, 0, 23)], 0);
        }        
      }
    }
  }
  void draw_enc_setup(uint8_t modus, uint32_t color, uint32_t color2) {
    uint32_t col_re = CRGB_to_u32(CHSV(0, 255, capToMax(255, max_power_back)));
    switch (modus) {
      case 0:{  //LICHT
          for (byte row_i = 1; row_i <= 3; row_i++) {
            for (byte enc_i = 0; enc_i < 8; enc_i++) {
              if (enc_used_licht[row_i - 1][enc_i] == 0) {
                fill_1_enc(row_i, enc_i, CRGB_to_u32(CHSV(0, 255, capToMax(17, max_power_back))));
              }
            }
          }
          uint32_t col_ye = CRGB_to_u32(CHSV(35, 255, capToMax(255, max_power_back)));
          uint32_t col_bl = CRGB_to_u32(CHSV(160, 255, capToMax(255, max_power_back)));
          uint32_t col_gr = CRGB_to_u32(CHSV(96, 255, capToMax(200, max_power_back)));
          uint32_t col_pu = CRGB_to_u32(CHSV(200, 255, capToMax(255, max_power_back)));
          uint32_t col_wh_dimm = CRGB_to_u32(CHSV(0, 0, capToMax(180, max_power_back)));
          //TEMP, TON
          ring_2_halves(1, 0, col_pu, col_gr);
          ring_2_halves(3, 0, col_ye, col_bl);
          //DYN, SÄT
          draw_enc_dyn(enc_led[0][1]);
          draw_enc_saet(enc_led[2][1]);
          //lichter
          draw_upper_half(enc_led[1 - 1][3], col_wh_dimm, 0);
          draw_enc_switching(1, 4, col_wh_dimm, col_ye);
          draw_enc_hell(2, 3, col_wh_dimm);
          draw_enc_switching(2, 4, col_wh_dimm, 0);
          draw_lower_half(enc_led[3 - 1][3], col_wh_dimm, 0);
          draw_enc_switching(3, 4, col_wh_dimm, col_bl);
          //GRAD-KURVE
          draw_upper_half(enc_led[1 - 1][6], col_wh_dimm, 0);
          draw_mid_half(enc_led[2 - 1][6], col_wh_dimm, 0);
          draw_lower_half(enc_led[3 - 1][6], col_wh_dimm, 0);
          //PRÄSENZ
          draw_enc_switching(1, 7, col_wh_dimm, col_bl);
          draw_enc_switching(2, 7, col_wh_dimm, col_gr);
          draw_enc_switching(3, 7, col_wh_dimm, col_ye);
          //pointer
          for (byte row_i = 1; row_i <= 3; row_i++) {
            for (byte enc_i = 0; enc_i < 8; enc_i++) {
              if (enc_used_licht[row_i - 1][enc_i] == 1) {
                led_enc.setPixel(enc_led[row_i - 1][enc_i] + led_rev[map(counter[enc_mode][row_i][enc_i], 0, MAX_VALUE, 0, 23)], col_re);
              }              
            }
          }          
          break;
        }
      case 1:{  //HSV
          uint32_t col_bl = CRGB_to_u32(CHSV(160, 255, capToMax(255, max_power_back)));
          for (byte row_i = 1; row_i <= 3; row_i++) {
            //hue
            if (row_i == 3) {
              for (byte enc_i = 0; enc_i < 8; enc_i++) {
                //Serial.printf("counter_enc: %d,  counter/4: %d   map: %d \n",counter[row_i][enc_i],  counter[row_i][enc_i]/4,map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24));
                uint32_t color_enc = CRGB_to_u32(CHSV(HUES_enc[enc_i], 255, capToMax(255, max_power_back)));
                draw_enc_ptr(row_i, enc_i, map_u16_to_0_24(counter[enc_mode][row_i][enc_i], MAX_VALUE), col_bl, color_enc);
              }
            }
            //sat
            if (row_i == 2) {
              for (byte enc_i = 0; enc_i < 8; enc_i++) {
                draw_enc_ptr_sat(row_i, enc_i, map_u16_to_0_24(counter[enc_mode][row_i][enc_i], MAX_VALUE), col_bl, enc_i);
              }
            }
            //hell
            if (row_i == 1) {
              for (byte enc_i = 0; enc_i < 8; enc_i++) {
                draw_enc_ptr_hell(row_i, enc_i, map_u16_to_0_24(counter[enc_mode][row_i][enc_i], MAX_VALUE), col_bl, enc_i);
              }
            }
          }
          //pointer
          for (byte row_i = 1; row_i <= 3; row_i++) {
            for (byte enc_i = 0; enc_i < 8; enc_i++) {
                led_enc.setPixel(enc_led[row_i - 1][enc_i] + led_rev[map(counter[enc_mode][row_i][enc_i], 0, MAX_VALUE, 0, 23)], 0);
            }
          }    
          break;
        }
      case 2:{  //DETAILS
          for (byte row_i = 1; row_i <= 3; row_i++) {
            for (byte enc_i = 0; enc_i < 8; enc_i++) {
              if (enc_used_details[row_i - 1][enc_i] == 0) {
                fill_1_enc(row_i, enc_i, CRGB_to_u32(CHSV(0, 255, capToMax(17, max_power_back))));
              }
            }
          }
          uint32_t col_wh_dimm = CRGB_to_u32(CHSV(0, 0, capToMax(40, max_power_back)));
          //Schärfe
          draw_enc_switching_hell(1, 0, 0, 0, 0, 255);
          draw_enc_switching_hell(2, 0, 0, 0, 0, 255);
          draw_enc_switching_hell(3, 0, 0, 0, 0, 255);
          //Rausch 1
          draw_rnd_k(enc_led[1 - 1][2], cl_wh, 0, 12);
          draw_rnd_k(enc_led[2 - 1][2], cl_wh, 0, 12);
          draw_rnd_k(enc_led[3 - 1][2], cl_wh, 0, 12);
          //Rausch 2
          draw_enc_switching_rgb_rnd(1, 3, col_wh_dimm);
          draw_enc_switching_rgb_rnd(2, 3, col_wh_dimm);
          draw_enc_switching_rgb_rnd(3, 3, col_wh_dimm);
          //Körnung
          draw_enc_switching(1, 5, cl_wh, 0);
          draw_mid_korn(enc_led[2 - 1][5], cl_wh, 0);
          draw_unrg_arr(enc_led[3 - 1][5], cl_wh, 0);
          //Vignette
          draw_enc_switching_hell(1, 7, 0, 0, 0, 255);
          draw_corner(enc_led[2 - 1][7], cl_wh, 0);
          draw_corner(enc_led[3 - 1][7], cl_wh, 0);
          //pointer
          for (byte row_i = 1; row_i <= 3; row_i++) {
            for (byte enc_i = 0; enc_i < 8; enc_i++) {
              if (enc_used_details[row_i - 1][enc_i] == 1) {
                led_enc.setPixel(enc_led[row_i - 1][enc_i] + led_rev[map(counter[enc_mode][row_i][enc_i], 0, MAX_VALUE, 0, 23)], col_re);
              }              
            }
          }   
          break;
        }
      case 3:{  //COLOR GRADING
          for (byte row_i = 1; row_i <= 3; row_i++) {
            for (byte enc_i = 0; enc_i < 8; enc_i++) {
              if (enc_used_cg[row_i - 1][enc_i] == 0) {
                fill_1_enc(row_i, enc_i, CRGB_to_u32(CHSV(0, 255, capToMax(17, max_power_back+50))));
              }
            }
          }
          col_l = CRGB_to_u32(CHSV(0, 255, capToMax(255, max_power_back)));
          col_m = CRGB_to_u32(CHSV(96, 255, capToMax(255, max_power_back)));
          col_r = CRGB_to_u32(CHSV(160, 255, capToMax(255, max_power_back)));
          uint32_t col_wh_dimm = CRGB_to_u32(CHSV(0, 0, capToMax(180, max_power_back)));
          //HIGH
          draw_enc_dyn(enc_led[0][0]);
          draw_enc_saet_var(enc_led[1][0], map(counter[enc_mode][1][0], 0, MAX_VALUE, 0, 255));
          draw_enc_hell_var(enc_led[2][0], map(counter[enc_mode][1][0], 0, MAX_VALUE, 0, 255));
          //MID
          draw_enc_dyn(enc_led[0][2]);
          draw_enc_saet_var(enc_led[1][2], map(counter[enc_mode][1][2], 0, MAX_VALUE, 0, 255));
          draw_enc_hell_var(enc_led[2][2], map(counter[enc_mode][1][2], 0, MAX_VALUE, 0, 255));
          //LOW
          draw_enc_dyn(enc_led[0][4]);
          draw_enc_saet_var(enc_led[1][4], map(counter[enc_mode][1][4], 0, MAX_VALUE, 0, 255));
          draw_enc_hell_var(enc_led[2][4], map(counter[enc_mode][1][4], 0, MAX_VALUE, 0, 255));
          //GLOBAL
          draw_enc_dyn(enc_led[0][6]);
          draw_enc_saet_var(enc_led[1][6], map(counter[enc_mode][1][6], 0, MAX_VALUE, 0, 255));
          draw_enc_hell_var(enc_led[2][6], map(counter[enc_mode][1][6], 0, MAX_VALUE, 0, 255));
          //ABGLEICH(was ist stärler)/ ÜBERBLENDEN, AN/AUS
          draw_mid_korn(enc_led[1 - 1][7], col_wh_dimm, color_rgb);
          ring_3_halves(3, 7, col_l, col_m, col_r);
          draw_enc_hell(2, 7, col_wh_dimm);
          //pointer
          for (byte row_i = 1; row_i <= 3; row_i++) {
            for (byte enc_i = 0; enc_i < 8; enc_i++) {
              if (enc_used_cg[row_i - 1][enc_i] == 1) {
                led_enc.setPixel(enc_led[row_i - 1][enc_i] + led_rev[map(counter[enc_mode][row_i][enc_i], 0, MAX_VALUE, 0, 23)], 0);
              }              
            }
          }   
          break;
        }
      default:{  //LEER/ ROT
          for (byte row_i = 1; row_i <= 3; row_i++) {
            for (byte enc_i = 0; enc_i < 8; enc_i++) {
              draw_enc_ptr(row_i, enc_i, map(counter[enc_mode][row_i][enc_i], 0, MAX_VALUE, 0, 24), color, color2);
            }
          }
          break;
        }
    }
    enc_dirty = true;
  }
  void draw_pcb_leds(uint8_t pcb) {
    const uint8_t *k2l = K2L_u8[pcb];
    for (uint8_t key = 0; key < KEY_COUNT[pcb]; key++) {
      uint8_t ledIdx = k2l[key];  // Index auf led_main
      ledIdx += LED_shift[pcb];
      // Sicherheit: nicht außerhalb vom Strip
      if (ledIdx >= NUM_LEDS_main) continue;
      led_rest.setPixel(ledIdx, button_state[pcb][key] ? color_main : color_back_gr);
    }
    if (pcb == 0) {
      //Modus_Button: licht
      led_rest.setPixel(k2l[0] + LED_shift[pcb], button_state[pcb][0] ? color_main : cl_wh);
      //Modus_Button: HSV
      led_rest.setPixel(k2l[1] + LED_shift[pcb], button_state[pcb][1] ? color_main : color_rgb);
      //Modus_Button: Details
      led_rest.setPixel(k2l[2] + LED_shift[pcb], button_state[pcb][2] ? color_main : cl_wh);
      //Modus_Button: Color_Grading
      led_rest.setPixel(k2l[3] + LED_shift[pcb], button_state[pcb][3] ? color_main : color_rgb2);
      //FEIN UP
      led_rest.setPixel(k2l[4] + LED_shift[pcb], button_state[pcb][4] ? c_blu : color_back);
      //FEIN OFF
      led_rest.setPixel(k2l[5] + LED_shift[pcb], button_state[pcb][5] ? c_blu : color_back_bright);
      //FEIN DOWW
      led_rest.setPixel(k2l[6] + LED_shift[pcb], button_state[pcb][6] ? c_blu : color_back);
      //Helligkeitsbutton
      led_rest.setPixel(k2l[7] + LED_shift[pcb], button_state[pcb][7] ? c_blu : color_var_hell);
      //CLIPPING  
      uint32_t color_back_clipping = CRGB_to_u32(CHSV(160, 255, capToMax(255, max_power_back)));
      led_rest.setPixel(k2l[8] + LED_shift[pcb], button_state[pcb][8] ? cl_wh : color_back_clipping);
      //AUTO
      uint32_t col_ye = CRGB_to_u32(CHSV(35, 255, capToMax(255, max_power_back)));
      led_rest.setPixel(k2l[9] + LED_shift[pcb], button_state[pcb][9] ? cl_wh : col_ye);
      //copy
      uint32_t col_lil = CRGB_to_u32(CHSV(210, 255, capToMax(255, max_power_back)));
      led_rest.setPixel(k2l[10] + LED_shift[pcb], button_state[pcb][10] ? cl_wh : col_lil);
      //paste
      led_rest.setPixel(k2l[11] + LED_shift[pcb], button_state[pcb][11] ? cl_wh : col_lil);
      //lupe
      led_rest.setPixel(k2l[12] + LED_shift[pcb], button_state[pcb][12] ? cl_wh : color_back_gr);
      //zurück
      uint32_t col_aqua = CRGB_to_u32(CHSV(135, 255, capToMax(255, max_power_back)));
      led_rest.setPixel(k2l[13] + LED_shift[pcb], button_state[pcb][13] ? cl_wh : col_aqua);
      //widerholen
      led_rest.setPixel(k2l[14] + LED_shift[pcb], button_state[pcb][14] ? cl_wh : col_aqua);
      //reset
      uint32_t col_reset = CRGB_to_u32(CHSV(0, 255, capToMax(hell_up_down, max_power_back)));
      led_rest.setPixel(k2l[15] + LED_shift[pcb], button_state[pcb][15] ? cl_wh : col_reset);
    }
    if(pcb == 2){
      uint32_t col_ye = CRGB_to_u32(CHSV(35, 255, capToMax(255, max_power_back)));
      led_rest.setPixel(k2l[5] + LED_shift[pcb], button_state[pcb][5] ? cl_wh : col_ye);
      led_rest.setPixel(k2l[6] + LED_shift[pcb], button_state[pcb][6] ? cl_wh : col_ye);
      led_rest.setPixel(k2l[4] + LED_shift[pcb], button_state[pcb][4] ? cl_wh : color_back);
      led_rest.setPixel(k2l[7] + LED_shift[pcb], button_state[pcb][7] ? cl_wh : color_back);
    }
  }
  void power_save(uint16_t cap_hell) {
    if (digitalReadFast(5) == HIGH) {
      max_power_back = capToMax(max_hell_power, cap_hell);
      max_power_back_dimm = max_power_back - 50;
      if (max_power_back_dimm < 0) max_power_back_dimm = 0;
      cl_wh = c_wh_bright;
    } 
    else {
      max_power_back = capToMax(max_hell_usb, cap_hell);
      max_power_back_dimm = max_power_back - 20;
      if (max_power_back_dimm < 0) max_power_back_dimm = 0;
      cl_wh = c_wh_dimm;
    }
    //Serial.printf("LED MODE: %d \n", digitalReadFast(7));
  }
  void color_calc() {
    color_back = CRGB_to_u32(CHSV(0, 255, capToMax(80, max_power_back)));
    color_back_bright = CRGB_to_u32(CHSV(0, 255, capToMax(255, max_power_back)));
    color_back_gr = CRGB_to_u32(CHSV(100, 255, capToMax(100, max_power_back)));
    color_back_bright_gr = CRGB_to_u32(CHSV(100, 255, capToMax(200, max_power_back)));
    color_main = CRGB_to_u32(CHSV(160, 255, capToMax(255, max_power_back)));
    color_main_bright = CRGB_to_u32(CHSV(160, 255, 255));
    //color_rgb = CRGB_to_u32(CHSV(100, 225, capToMax(255, max_power_back)));
    cl_wh = CRGB_to_u32(CHSV(0, 0, capToMax(255, max_power_back)));
    cl_wh_dimm = CRGB_to_u32(CHSV(0, 0, capToMax(150, max_power_back)));
  }
  void pride() {
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    uint8_t sat8 = beatsin88(87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, 25 * 256, 40 * 256);
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for (uint16_t i = 0; i < NUM_LEDS_enc; i++) {
      hue16 += hueinc16;
      uint8_t hue8 = hue16 >> 8;

      brightnesstheta16 += brightnessthetainc16;
      uint16_t b16 = sin16(brightnesstheta16) + 32768;
      uint32_t bri16 = ((uint32_t)b16 * (uint32_t)b16) >> 16;
      uint8_t bri8 = (uint32_t)bri16 * brightdepth >> 16;
      bri8 += (255 - brightdepth);

      CRGB newcolor = CHSV(hue8, sat8, bri8);

      // optional: invertierte Laufrichtung wie in deinem Code
      uint16_t pixelnumber = (NUM_LEDS_enc - 1) - i;

      // Aktuelle Farbe aus dem WS2812Serial-Puffer holen, blenden, zurückschreiben
      CRGB cur = getPixelColor(drawingMemory1, pixelnumber);
      nblend(cur, newcolor, 64);
      led_enc.setPixel(pixelnumber, CRGB_to_u32(cur));
    }
    led_enc.show();
  }
  // ---- Idle / Starlight-Wedge --------------------------------------------------
    elapsedMillis since_idle;
    const uint16_t IDLE_FRAME_MS = 30;   // Geschwindigkeit (kleiner = schneller)
    const uint8_t  WEDGE_WIDTH   = 5;    // Breite des Keils (in LEDs, 1..24)
    const uint8_t  SLOPE_PER_RING= 3;    // Versatz pro Ring (0..23) -> bestimmt den "Winkel"
    const uint8_t  FADE_FACTOR   = 250;  // 0..255 (höher = weniger Nachleuchten)
    static uint8_t wedgeBasePos  = 0;    // globale Keilposition 0..23
    // Optional: wenn deine Ringe „anders herum“ laufen, auf true stellen
    #define USE_LED_REV true
    inline uint16_t ringLocalToGlobal(uint8_t row, uint8_t enc, uint8_t local) {
      #if USE_LED_REV
        local = led_rev[local];
      #endif
      return enc_led[row - 1][enc] + local;
    }
    // schnelles globales Fade (alle Encoder-LEDs leicht abdunkeln)
    void fadeAllEnc(uint8_t fade /*0..255*/) {
      for (int i = 0; i < NUM_LEDS_enc; ++i) {
        uint32_t c = getPixelColor(drawingMemory1, i);
        uint8_t r, g, b; u32_to_rgb(c, r, g, b);
        r = (uint16_t)r * fade / 255;
        g = (uint16_t)g * fade / 255;
        b = (uint16_t)b * fade / 255;
        led_enc.setPixel(i, (uint32_t)r << 16 | (uint32_t)g << 8 | b);
      }
    }
    void idle_starlight_wedge_step() {
      // weiches Nachleuchten
      fadeAllEnc(FADE_FACTOR);
      // Farb-/Helligkeitsbasis (du kannst hier auch farbig arbeiten)
      // Tipp: für "kaltes Sternenlicht" nimm leicht bläulich: CHSV(160, 20, ...)
      for (uint8_t row = 1; row <= 3; ++row) {
        for (uint8_t e = 0; e < 8; ++e) {
          // pro Ring einen Positionsversatz -> ergibt den diagonalen "Winkel"
          uint8_t ringIndex = (row - 1) * 8 + e;                 // 0..23
          uint8_t pos = (uint8_t)(wedgeBasePos + ringIndex * SLOPE_PER_RING) % 24;

          // Keilbreite mit weicher Helligkeitsverteilung (Dreieck)
          int halfW = WEDGE_WIDTH / 2;
          for (int dx = -halfW; dx <= halfW; ++dx) {
            int p = (int)pos + dx;
            if (p < 0) p += 24;
            if (p >= 24) p -= 24;

            // Spitzen-LED am hellsten, Rand dunkler
            uint8_t v = 255 - (uint8_t)(abs(dx) * (255 / (halfW ? (halfW + 1) : 1)));
            v = capToMax(v, max_power_back); // respektiert dein Power-Limit

            //uint32_t col = CRGB_to_u32(CHSV(0, 0, v)); // weißes Sternlicht
            // sanft bläulich
            //uint32_t col = CRGB_to_u32(CHSV(160, 30, v));
            // langsam changierend:
            uint8_t hue = color_shift_rain;  // hast du ohnehin
            uint32_t col = CRGB_to_u32(CHSV(hue, 40, v));
            led_enc.setPixel(ringLocalToGlobal(row, e, (uint8_t)p), col);
          }
        }
      }

      led_enc.show();
      wedgeBasePos = (wedgeBasePos + 1) % 24;
    }
    void runIdleStarlight() {
      if (since_idle >= IDLE_FRAME_MS) {
        since_idle -= IDLE_FRAME_MS;
        idle_starlight_wedge_step();
      }
    }
  // ---- Idle: Sparse Twinkles (einzelne LEDs fahren weich hoch/runter) ----------------
    elapsedMillis since_idle_tw;
    const uint16_t TW_FRAME_MS    = 12;   // Framezeit ~33 ms -> ~30 FPS
    const uint8_t  MAX_SPARKS     = 150;   // gleichzeitige Funken (klein halten)
    const uint8_t  SPAWN_CHANCE   = 40;    // 0..255 pro Frame -> ~0.35 Funken/Sekunde bei 30 FPS
    const uint8_t  WHITE_BIAS     = 180;  // 0..255 (höher = häufiger weiß als farbig)
    struct Spark {
      uint16_t idx;      // LED 0..NUM_LEDS_enc-1
      uint8_t  b;        // aktuelle Helligkeit 0..255
      uint8_t  peak;     // Zielhelligkeit  (wird an max_power_back gekappt)
      uint8_t  up;       // Schritt pro Frame beim Auffaden
      uint8_t  down;     // Schritt pro Frame beim Abfaden
      uint8_t  hue;      // Farbe (bei Sättigung=0 egal)
      uint8_t  sat;      // 0..255 (0 = weiß)
      uint8_t  state;    // 0=inactive, 1=rising, 2=falling
    };
    static Spark sparks[MAX_SPARKS];
    // optional: einmalig alles löschen
    void idleTwinklesReset() {
      for (uint8_t i = 0; i < MAX_SPARKS; ++i) sparks[i].state = 0;
    }
    void spawnSpark() {
      for (uint8_t i = 0; i < MAX_SPARKS; ++i) {
        if (sparks[i].state == 0) {
          Spark &s = sparks[i];
          s.idx  = (uint16_t)random(NUM_LEDS_enc);
          // seltene, aber sichtbare Peaks – respektieren Power-Limit
          uint8_t maxV = capToMax(255, max_power_back);
          maxV=220;
          s.peak = (uint8_t)random(maxV * 3 / 5, maxV);   // ~60..100% von max
          s.b    = 0;
          s.up   = (uint8_t)random(3, 10);                 // schneller hoch
          s.down = (uint8_t)random(1, 3);                 // langsam runter
          if ((uint8_t)random(255) < WHITE_BIAS) {        // meist weiß
            s.hue = 0;  s.sat = 0;
          } else {                                        // selten dezent farbig
            s.hue = (uint8_t)random(0, 255);
            s.sat = (uint8_t)random(15, 80);
          }
          s.state = 1;
          return;
        }
      }
    }
    void idleTwinklesStep() {
      // selten neuen Funken erzeugen
      if ((uint8_t)random(255) < SPAWN_CHANCE) spawnSpark();

      // alle Funken updaten & zeichnen
      for (uint8_t i = 0; i < MAX_SPARKS; ++i) {
        Spark &s = sparks[i];
        if (s.state == 0) continue;

        if (s.state == 1) {                  // rising
          uint16_t nb = s.b + s.up;
          s.b = (nb >= s.peak) ? s.peak : (uint8_t)nb;
          if (s.b >= s.peak) s.state = 2;
        } else {                              // falling
          s.b = (s.b > s.down) ? (s.b - s.down) : 0;
          if (s.b == 0) {
            // zum Schluss sicher schwarz schreiben, dann deaktivieren
            led_enc.setPixel(s.idx, 0);
            s.state = 0;
            continue;
          }
        }
        uint8_t v = capToMax(s.b, max_power_back);
        uint32_t col = CRGB_to_u32(CHSV(s.hue, s.sat, v));
        led_enc.setPixel(s.idx, col);
        if(s.idx <= NUM_LEDS_main){
          led_rest.setPixel(s.idx, col);
          led_rest.show();
        }
        if(s.idx/2 <= NUM_LEDS_main){
          led_rest.setPixel(s.idx/2, col);
          led_rest.show();
        }
      }
      //led_enc.show();
    }
    void runIdleTwinkles() {
      if (since_idle_tw >= TW_FRAME_MS) {
        since_idle_tw -= TW_FRAME_MS;
        idleTwinklesStep();
      }
    }
  // ------------------------------------------------------------------------------

// -------------------- UPDATE_all ------------------------------------------------------------------------------------------

  void update_my() {
    //kritischer update stuff
    if (since_last_counter >= interval_col_btn) {
      since_last_counter = since_last_counter - interval_col_btn;
      hell_up_down += delta_var;

      if(hell_up_down >= 250 || hell_up_down < 10){ 
        delta_var = delta_var * -1; 
      }
    }        
    if (since_last_led >= interval_show) {
      since_last_led = since_last_led - interval_show;
      color_shift_my += 1;
      color_shift_rain++;
      rainbow();

      color_rgb = CRGB_to_u32(CHSV(color_shift_my, 255, capToMax(255, max_power_back)));
      color_rgb2 = CRGB_to_u32(CHSV(color_shift_my+color_shift_rain, 255, capToMax(255, max_power_back)));
      color_var_hell = CRGB_to_u32(CHSV(0, 0, capToMax(hell_up_down, max_power_back)));

      //Modus der gerendert wird
      if (enc_mode == 0) {
        draw_enc_licht();
      } 
      else if (enc_mode == 1) {
        draw_enc_hsv();
      } 
      else if (enc_mode == 2) {
        draw_enc_details();
      } 
      else if (enc_mode == 3) {
        draw_enc_cg();
      } 
      else {
        draw_enc_1_col(color_main, color_back);
      }

      //es wurden knöpfe gedrückt
      if (true) {
        sth_changed = false;
        draw_pcb_leds(0);  // main
        draw_pcb_leds(1);  // key
        draw_pcb_leds(2);  // nav
        //nav counter
        led_nav_draw(18, map_u16_to_0_24(counter_nav, MAX_VALUE_nav), color_main_bright, color_back);
        draw_nav_butt(); 

        if (button_state[0][0]) {  //licht
          enc_mode = 0;
          draw_enc_setup(enc_mode, color_main, color_back);
          CHANNEL_switch = cs::Channel{ enc_mode };
          haptic_play(86);
        }
        if (button_state[0][1]) {  //HSV
          enc_mode = 1;
          draw_enc_setup(enc_mode, color_main, color_back);
          CHANNEL_switch = cs::Channel{ enc_mode };
          haptic_play(86);
        }
        if (button_state[0][2]) {  //details
          enc_mode = 2;
          draw_enc_setup(enc_mode, color_main, color_back);
          CHANNEL_switch = cs::Channel{ enc_mode };
          haptic_play(86);
        }
        if (button_state[0][3]) {  //color grading
          enc_mode = 3;
          draw_enc_setup(enc_mode, color_main, color_back);
          CHANNEL_switch = cs::Channel{ enc_mode };
          haptic_play(86);
        }
        if (button_state[0][7]) {  //hell anpassen
          power_save(map(counter_nav, 0, MAX_VALUE_nav, 25, 255));
          color_calc();
          haptic_play(73);
          draw_enc_setup(enc_mode, color_main, color_back);
          enc_dirty = true;
        }
        /*
        //haptic fx test
            if(button_state[0][6]){ //hell anpassen
              haptic_play(fx_fx);
              Serial.println(fx_fx);
            }
            if(button_state[0][4] && last_button1){ //hell anpassen
              last_button1 = false;
              fx_fx++;
              Serial.println(fx_fx);
              
            }
            if(button_state[0][5] && last_button2){ //hell anpassen
              last_button2 = false;
              fx_fx--;
              Serial.println(fx_fx);
              
            }
            if(!button_state[0][4] && !last_button1){ //hell anpassen
              last_button1 = true;
            }
            if(!button_state[0][5] && !last_button2){ //hell anpassen
              last_button2 = true;
            }*/

        led_rest.show();
      }

      if (enc_dirty) {
        led_enc.show();
        enc_dirty = false;
      }
    }
  }

// -------------------- INTERRUPT-SR ----------------------------------------------------------------------------------------
  // ----------------- A-ENCODER ----------------------------------
    void onEncoderInterrupt_A1() {
      encoderChanged[1][0] = true;
    }
    void onEncoderInterrupt_A2() {
      encoderChanged[2][0] = true;
    }
    void onEncoderInterrupt_A3() {
      encoderChanged[3][0] = true;
    }
  // ----------------- B-ENCODER ----------------------------------
    void onEncoderInterrupt_B1() {
      encoderChanged[1][1] = true;
    }
    void onEncoderInterrupt_B2() {
      encoderChanged[2][1] = true;
    }
    void onEncoderInterrupt_B3() {
      encoderChanged[3][1] = true;
    }

  // ----------------- BUTTONS ------------------------------------
    void onButtonInterrupt() {
      button_changed[1][0] = true;
    }
    void onButtonInterrupt2a() {
      button_changed[3][0] = true;
    }
    void onButtonInterrupt2b() {
      button_changed[2][0] = true;
    }

  // ----------------- BUTTONS-KEYPAD -----------------------------
    void onButtonInterrupt_key_a() {
      rubber_button_changed[1][0] = true;
    }
    void onButtonInterrupt_key_b() {
      rubber_button_changed[1][1] = true;
    }
  // ----------------- BUTTONS-NAV --------------------------------
    void onButtonInterrupt_nav_a() {
      nav_changed = true;
    }
    void onButtonInterrupt_nav_b() {
      nav_changed = true;
    }
  // ----------------- BUTTONS-MAIN -------------------------------
  void onButtonInterrupt_main_a() {
    rubber_button_changed[0][0] = true;
  }
  void onButtonInterrupt_main_b() {
    rubber_button_changed[0][1] = true;
  }


// -------------------- MIDI-STUFF ------------------------------------------------------------------------------------------

  static inline uint8_t channelToMode(cs::Channel ch) {
    // In neueren Control_Surface-Versionen:
    uint8_t raw = ch.getRaw();   // ergibt 1..16
    // Falls getRaw() bei deiner Version nicht existiert,
    // nimm stattdessen:  uint8_t raw = static_cast<uint8_t>(ch);
    return raw; //? (raw - 1) : 0;  // 1→0, 2→1, ..., 16→15
  }

  struct MyMIDI_Callbacks : MIDI_Callbacks {
    // Callback for channel messages (notes, control change, pitch bend, etc.).... https://tttapa.github.io/Control-Surface/Doxygen/d7/d3f/structChannelMessage.html#a35187e10c578f1ff079af83ffb2aba0a
    void onChannelMessage(MIDI_Interface &, ChannelMessage msg) override {
      auto type = msg.getMessageType();
      if (type == msg.ControlChange || type == msg.NoteOn)
        onNoteMessage(msg);
    }

    // Our own callback specifically for Note On and Note Off messages
    void onNoteMessage(ChannelMessage msg) {
      auto type = msg.getMessageType();
      auto note = msg.getData1();
      auto velocity = msg.getData2();
      auto chanel = msg.getChannel();

      uint8_t row, enc;
      decodeCC(note, row, enc);
      uint8_t enc_channel = channelToMode(chanel);
      if ((row >= 1 && row <= 3) && (enc >= 0 && enc <= 7)) {
        if (enc_channel < MAX_MODES) {
          counter[enc_channel][row][enc] = 4 * velocity;         // jetzt klar "welcher" counter
        }
      }

      /*
        if (note == 60) {//alt
        counter_nav = velocity;
        sth_changed = true;
      }*/
      //Serial << type << ": " << chanel << ", Note " << note << ", Velocity " << velocity << ", counter[row][enc]: " << counter[enc_channel][row][enc] << ", mode: " << enc_channel << endl;
    }
  } callback;  // Instantiate a callback


// -------------------- SETUP -----------------------------------------------------------------------------------------------

  void setup() {
    // put your setup code here, to run once:
    delay(500);
    pinMode(5, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    initRamps();
    power_save(100);
    delay(200);
    seedRNG();
    delta_var=1;
    enc_mode = 0;
    color_calc();
    clearAll();
    led_enc.begin();
    led_eb.begin();
    led_rest.begin();
    led_enc.show();
    led_eb.show();
    led_rest.show();

    Serial.begin(115200);
    dump_last_crash();
    print_startup_info();
    delay(500);

    init_I2C_Peripherals();

    Serial.println("Encoder mit Interrupts bereit.");
    initPCIInterruptForTinyReceiver();
    //drvEnableOverdrive(/*rated*/0x8F, /*clamp*/0x9B);
    drv.selectLibrary(2);  // ERM-Library
    delay(150);
    drv.setMode(DRV2605_MODE_DIAGNOS);
    drv.go();
    uint8_t status = drv.readRegister8(0x00);  // STATUS
    Serial.println(status, BIN);
    drv.setMode(DRV2605_MODE_INTTRIG);
    //drv.writeRegister8(0x16, ratedVoltage);  // RATED_VOLTAGE
    //drv.writeRegister8(0x17, odClamp);       // OVERDRIVE_CLAMP
    delay(50);
    drv.setWaveform(0, 84);  // ramp up medium 1, see datasheet part 11.2
    drv.setWaveform(1, 1);   // strong click 100%, see datasheet part 11.2
    drv.setWaveform(2, 73);
    drv.setWaveform(3, 0);  // end of waveforms
    drv.go();               // play the effect!
    Serial.println("DRV ready");

    midi.begin();
    midi.setCallbacks(callback);  // Attach the custom callback
    Serial.println("MIDI ready");
    welcome_led();
    Serial.println("Start Loop");

    update_my();
    main_mode = 3;
    since_last_touch = millis();
    idle_b = false;
    idle_b_prev = true;
    touched_b = false;
    idleTwinklesReset();
    haptic_play(86);
  }


// -------------------- LOOP ------------------------------------------------------------------------------------------------

  void jump_back(){
    encoder_handle();
    encoder_button_handle();
    handle_rest();
    if(touched_b){
      touched_b = false;
      main_mode = 0;
      since_last_led      = 0;
      since_last_counter  = 0;
      since_idle          = 0;
      since_idle_tw       = 0;
      haptic_play(52);
      haptic_play(52);
      draw_enc_setup(enc_mode, color_main, color_back);
    }
  } 

  void loop() {
    // put your main code here, to run repeatedly:
    led_blink();

    if (main_mode == 0) {
      if(millis()-since_last_touch > interval_touched){
        idle_b = true;
        idle_b_prev = true;
      }  
      encoder_handle();
      encoder_button_handle();
      handle_rest();
      if(idle_b){
        main_mode = 2;
      }
      else{
        update_my();    
      }
      midi.update();
    } 
    else if (main_mode == 1) {
      //led show
      pride();
      color_shift_rain++; 
      rainbow();
      led_eb.show();
      jump_back();
    }
    else if (main_mode == 2) {
      //led show
      color_shift_rain++; 
      runIdleTwinkles();
      rainbow();
      led_eb.show();
      led_enc.show();
      jump_back();
    }
    else if (main_mode == 3)  {
      //led show
      color_shift_rain++; 
      runIdleStarlight();
      rainbow();
      led_eb.show();
      jump_back();
    }
  }





// --------------------------------------------------------------------------------------------------------------------------





























