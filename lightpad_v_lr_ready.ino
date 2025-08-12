
// -------------------- Konfiguration -------------------------------------------------------------------------------------
  #include <Wire.h>
  #include <Adafruit_MCP23X17.h>
  #include <WS2812Serial.h>
  #define FASTLED_OVERCLOCK 1
  #define FASTLED_ALLOW_INTERRUPTS 1
  #define FASTLED_INTERRUPT_RETRY_COUNT 4
  #include <FastLED.h>  
  #include <WS2812Serial.h>
  #include "Adafruit_DRV2605.h"
  #include <Control_Surface.h>
  #define IR_INPUT_PIN    33
  #include "TinyIRReceiver.hpp"

  Adafruit_MCP23X17 mcp[8];
  #define MCP_COUNT 8
  USBMIDI_Interface midi;
  Adafruit_DRV2605 drv;



  
// -------------------- MIDI-CONFIG -----------------------------------------------------------------------------------------

  //const MIDIAddress note = MIDI_Notes::C[4]; // C4 is middle C
  const uint8_t velocity = 127;              // 127 is maximum velocity -> value
  constexpr uint8_t BASE_NOTE = 60;    // C4
  cs::Channel CHANNEL_switch = cs::Channel{0};

  // Hilfsfunktion: Note-Adresse für einen Encoder berechnen
  static inline cs::MIDIAddress encoderAddr(uint8_t row, uint8_t enc_i, uint8_t base, auto channel_x) {
    // row: 1..3, enc_i: 0..7 -> 8 Noten je Reihe
    uint8_t note = base + (row - 1) * 8 + enc_i; // achte auf 0..127
    return cs::MIDIAddress{note, channel_x};
  }
  static inline cs::MIDIAddress encoderAddr_2(uint8_t row, uint8_t enc_i, uint8_t base, auto channel_x) {
    // row: 1..3, enc_i: 0..7 -> 8 Noten je Reihe
    uint8_t note = base + (row - 1) * 16 + enc_i; // achte auf 0..127
    return cs::MIDIAddress{note, channel_x};
  }
  inline void decodeCC(uint8_t cc, uint8_t &row, uint8_t &enc_i) {
    uint8_t offset = cc - BASE_NOTE; // 0..23
    row   = (offset / 8) + 1;      // 1..3
    enc_i = offset % 8;            // 0..7
  }

// -------------------- LED-CONFIG -------------------------------------------------------------------------------------------

  #define NUM_LEDS_main  64   
  #define NUM_LEDS_enc  672  
  #define DATA_PIN_main  1    
  #define DATA_PIN_enc  8
  const int colorOrder = WS2812_GRB;
  byte   drawingMemory1[NUM_LEDS_enc * 3]= {0};
  DMAMEM byte displayMemory1[NUM_LEDS_enc * 12]= {0};
  byte   drawingMemory2[NUM_LEDS_main * 3]= {0};
  DMAMEM byte displayMemory2[NUM_LEDS_main * 12]= {0};
  WS2812Serial leds1(NUM_LEDS_enc, displayMemory1, drawingMemory1, DATA_PIN_enc, colorOrder);
  WS2812Serial leds2(NUM_LEDS_main, displayMemory2, drawingMemory2, DATA_PIN_main, colorOrder);
  uint32_t color_back, color_main, color_rgb, cl_wh, cl_wh_dimm, color_main_bright, color_back_bright, color_rain;
  constexpr uint32_t c_wh_dimm = 0x303030; 
  constexpr uint32_t c_wh_bright = 0xFFFFFF;  
  constexpr uint32_t c_blu = 0x0000FF;
  int  max_power_back_dimm = 50; //init wert
  int  max_power_back = 15; //init wert
  uint8_t max_hell_power = 160;
  uint8_t max_hell_usb = 60;
  uint8_t  color_shift_my = 0;
  uint8_t  color_shift_rain = 0;
  constexpr uint8_t HUES_enc[8] = { 0, 12, 30, 96, 134, 160, 194, 222 };  // Farben: Rot, Orange, Gelb, Grün, Aqua, Blau, Lila, Magenta
  constexpr bool enc_upper[24] = {1,1,1, 1,0,0,0,0,0, 0,0,0,0,0,0, 0,0,0,0,0,0, 1,1,1};
  constexpr bool enc_lower[24] = {0,0,0, 0,0,0,0,0,0, 1,1,1,1,1,1, 1,0,0,0,0,0, 0,0,0};
  constexpr bool enc_left[24] = {0,0,0, 1,1,1,1,1,1, 0,0,0,0,0,0, 0,0,0,0,0,0, 0,0,0};
  constexpr bool enc_right[24] = {0,0,0, 0,0,0,0,0,0, 0,0,0,0,0,0, 1,1,1,1,1,1, 0,0,0};
  constexpr bool enc_mid[24] = {0,0,0, 0,1,1,1,1,1, 0,0,0,0,0,0, 0,1,1,1,1,1, 0,0,0};


// -------------------- VARIABLEN-REST --------------------------------------------------------------------------------------

  elapsedMillis since_last_led_change;
  elapsedMillis since_last_led;
  constexpr int interval_blink = 2000;
  constexpr int interval_show = 35;
  bool ledState = LOW; 
  int loop_counter=0;
  long loop_timer=0;
  volatile bool rubber_button_changed[3][2] = {false}; //INT
  bool button_state[3][16] = {false};
  constexpr uint8_t UNUSED = 255;
  constexpr const uint8_t CHIP_OF_PCB[3]   = {6, 7, 0}; //main, key, nav
  constexpr const uint8_t KEY_COUNT[3]     = {16, 16, 8};                     // NAV hat 8 normale Keys + 5 Sonderkeys (9..13)
  constexpr const uint8_t LED_shift[3]     = {0, 48, 16};
  uint8_t sth_changed = true;   //damit dfirekt gezeichnet wird
  uint16_t IR_aAddress;
  uint8_t IR_aCommand; 
  bool IR_isRepeat;
  uint8_t main_mode = 0;


  constexpr uint8_t led_key[16] =  {0, 1, 2, 3, 
                                    7, 6, 5, 4, 
                                    8, 9, 10 ,11, 
                                    15, 14, 13, 12};

  constexpr uint8_t led_nav[8] =    {0,          1, 
                                    31,         26,  //24 leds für weiteren encoder dazwischen
                                    30, 29, 28, 27,};

  constexpr uint8_t led_nav_enc[4] ={2, 8, 14, 20};                                  

  constexpr uint8_t led_main[16] = {0, 1, 2, 3, 4, 5, 6, 7,       //von 0..16 beim durchgehen, welcher pin 
                                    8, 9, 10 , 11, 12, 13, 14, 15};



  constexpr uint8_t but_key[16] =  {8, 9, 10, 11, //von 0..16 beim durchgehen, welcher pin 
                                    12, 13, 14, 15, 
                                    0, 1, 2 ,3, 
                                    4, 5, 6, 7};

  constexpr uint8_t but_nav[16] =  {UNUSED, 6, 5, 4, 2, 0, 1, UNUSED, UNUSED, 9, 10, 11, 12, 13, 3, 7};      //links oben nach rechts unten    //navigation encoder->  9mid  10up  12down  11left  13right  //enca 3(0)  encb 8(8)

  constexpr uint8_t but_main[16] = {4, 5, 6, 7, 15, 14, 13, 12, 11, 10, 9, 8, 0, 1, 2, 3}; //links oben nach rechts unten

  constexpr const uint8_t* B2K[3]          = {but_main, but_key, but_nav}; 
  constexpr const uint8_t* K2L_u8[3]       = {led_main, led_key, led_nav};                                  

  const int MIN_VALUE = 0;
  const int MAX_VALUE = 508; //anpassen led min max

  const int MIN_VALUE_nav = 0;
  const int MAX_VALUE_nav = 255; //anpassen led min max
  int speed_nav = 1;
  int counter_nav = {0};
  int counter_last_nav = {0};
  int counter_last_nav_midi = 0;
  uint8_t lastAB_nav = {0};
  volatile bool nav_changed = {false};
  constexpr uint8_t DEBOUNCE_MS = 10;   // 5–10 ms passt meist
  static bool     debounce_pending[3] = {false,false,false};
  static uint32_t debounce_due[3]     = {0,0,0};
  static uint16_t debounce_snap1[3]   = {0xFFFF,0xFFFF,0xFFFF};




// -------------------- VARIABLEN-ENCODER -----------------------------------------------------------------------------------

  // MCP-Pins für Encoder-Kanal A/B 
  constexpr uint8_t enc_rot_pins_A[8] = {6, 4, 2, 0, 15, 13, 11, 9};
  constexpr uint8_t enc_rot_pins_B[8] = {7, 5, 3, 1, 14, 12, 10, 8};
  // MCP-Pins für Buttons der encoder
  constexpr uint8_t enc_but_pins_4[8] = {7, 6, 5, 4, 3, 2, 1, 0};
  constexpr uint8_t enc_but_pins_5[16] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5 ,4, 3, 2, 1, 0};
  // Teensy-INT-Pins 
  constexpr uint8_t mcpIntA[8] = {21, 23, 41, 36, 38, 32, 28, 30};
  constexpr uint8_t mcpIntB[8] = {20, 22, 40, 35, 37, 31, 27, 29};

  constexpr uint16_t enc_led[3][8] = {{120,96, 264,240, 408,384, 552,528},
                                      {168,144, 312,288, 456,432, 600,576},
                                      {216,192, 360,336, 504,480, 648,624}};

  constexpr uint8_t enc_used_licht[3][8] = {{1,1,   0,1,  1,0,  1,1},
                                            {0,0,   0,1,  1,0,  1,1},
                                            {1,1,   0,1,  1,0,  1,1}};             

  constexpr uint8_t enc_used_details[3][8] = {{1,0,   1,0,  0,1,  0,1},
                                             {1,0,   1,0,  0,1,  0,1},
                                             {1,0,   1,0,  0,1,  0,1}}; 

  constexpr uint8_t enc_used_cg[3][8] =     {{1,0,   1,0,  0,1,  0,1},
                                             {1,0,   1,0,  0,1,  0,1},
                                             {1,0,   1,0,  0,1,  0,1}};                                           

  constexpr uint8_t NUM_ENCODER_CHIPS = 3+1;
  constexpr uint8_t ENC_PER_CHIP = 8;
  bool enc_dirty = false;
  uint8_t enc_mode = 0;
  volatile bool encoderChanged[NUM_ENCODER_CHIPS][2] = {false};
  volatile bool button_changed[NUM_ENCODER_CHIPS][2] = {false};
  int counter[NUM_ENCODER_CHIPS][ENC_PER_CHIP] = {0};
  int counter_last[NUM_ENCODER_CHIPS][ENC_PER_CHIP] = {0};
  bool button_enc_last[NUM_ENCODER_CHIPS][ENC_PER_CHIP*2] = {false};
  int counter_last_midi[NUM_ENCODER_CHIPS][ENC_PER_CHIP] = {0};
  uint8_t lastAB[NUM_ENCODER_CHIPS][ENC_PER_CHIP] = {0};
  // pro Encoder die Zeit der letzten Flanke merken
  static uint32_t lastStepUs[NUM_ENCODER_CHIPS][ENC_PER_CHIP] = {{0}};
  // simple Wheel-Acceleration (Grenzen gerne feinjustieren)  
  // Quadratur-Übergangstabelle (prev<<2 | curr) -> delta
  // +1: 00->01->11->10->00,  -1: umgekehrt, 0: ungültig/Prell
  static constexpr int8_t quadLUT[16] = {
    0,  -1,  +1,   0,
    +1,  0,   0,  -1,
    -1,  0,   0,  +1,
    0, +1,  -1,   0
  };


// -------------------- ENCODER-HANDLE ---------------------------------------------------------------------------------------
  static inline uint8_t accelMultiplier(uint32_t dt_us) {
    if (dt_us < 1800)  return 8;  // <1.0 ms pro Flanke -> sehr schnell
    if (dt_us < 2400)  return 4;  // <2.5 ms
    if (dt_us < 3000)  return 2;  // <6.0 ms
    return 1;                      // sonst normal
  }
  
  void encoder_handle(){
    for (uint8_t row = 1; row <= 3; ++row) {
      if (encoderChanged[row][0] || encoderChanged[row][1]) { //eine der beiden bänke hat int getriggert
        encoderChanged[row][0] = false;
        encoderChanged[row][1] = false;
        uint16_t gpio = mcp[row].readGPIOAB();  //alle pins vom mcp auslesen

        for (uint8_t enc_i = 0; enc_i < ENC_PER_CHIP; ++enc_i) {
          uint8_t a = bitRead(gpio, enc_rot_pins_A[enc_i]);
          uint8_t b = bitRead(gpio, enc_rot_pins_B[enc_i]);
          uint8_t curr = (a << 1) | b;
          uint8_t prev = lastAB[row][enc_i];

          int8_t delta = quadLUT[(prev << 2) | curr];  // -1,0,+1 pro Flanke

          if(delta){
            // Encoder 4..7 haben invertierte Richtung
            int8_t dir = (enc_i > 3) ? +1 : -1;
            // Zeit seit letzter Flanke messen
            uint32_t now = micros();
            uint32_t dt  = lastStepUs[row][enc_i] ? (now - lastStepUs[row][enc_i]) : 0xFFFFFFFF;
            lastStepUs[row][enc_i] = now;
            uint8_t mult = accelMultiplier(dt);
            
            counter[row][enc_i] += dir * delta * mult;

            // Begrenzen
            if (counter[row][enc_i] > MAX_VALUE) counter[row][enc_i] = MAX_VALUE;
            if (counter[row][enc_i] < MIN_VALUE) counter[row][enc_i] = MIN_VALUE;            

            //midi senden wenn änderung groß genug -> eine rastung
            uint8_t v = counter[row][enc_i] / 4;
            if (v != counter_last_midi[row][enc_i]) {
              counter_last_midi[row][enc_i] = v;
              auto addr = encoderAddr(row, enc_i, BASE_NOTE, CHANNEL_switch);
              midi.sendControlChange(addr, v);
              //Serial.printf("row=%u enc=%u cc=%u val=%u\n", row, enc_i, addr.getAddress(), v);
              //Serial.printf("Counter: %d,   Counter/4: %d,  mult: %d \n", counter[row][enc_i], counter[row][enc_i]/4, mult);
            }
          }
          lastAB[row][enc_i] = curr;
        }
        mcp[row].clearInterrupts();
      }
    }
  }


// -------------------- BUTTON-HANDLE ---------------------------------------------------------------------------------------
  bool button_used(uint8_t row, uint8_t enc){
    if(enc_mode == 0 && enc_used_licht[row][enc] == 1){
      return true;
    }   
    if(enc_mode == 1){
      return true;
    }
    if(enc_mode == 2 && enc_used_details[row][enc] == 1){
      return true;
    } 
    if(enc_mode == 3 && enc_used_cg[row][enc] == 1){
      return true;
    } 
    return false;
  }

  void encoder_button_handle(){
    for (uint8_t row_i = 1; row_i <= 3; row_i++) { // --- Buttons verarbeiten: je Reihe ein Flag, einmal Ports lesen ---
        if (row_i == 1) {
          if (button_changed[1][0]) {
            button_changed[1][0] = false;
            // Einmal den ganzen 16-Bit Port lesen, dann pro Taste auswerten
            uint16_t gpio = mcp[4].readGPIOAB();
            for (int enc_i = 0; enc_i < 8; enc_i++) {
              bool currentbtn = bitRead(gpio, enc_but_pins_4[enc_i]); // Pullup: LOW = gedrückt              
              if (!currentbtn != button_enc_last[row_i][enc_i]){
                button_enc_last[row_i][enc_i] = !currentbtn;
                auto addr = encoderAddr(row_i, enc_i, BASE_NOTE+25+16, CHANNEL_switch);
                if (!currentbtn){
                  if(button_used(row_i-1, enc_i)){
                    fill_1_enc(row_i, enc_i, color_main);                  
                    leds1.show();
                    counter_last[row_i][enc_i] = -1;
                  }                  
                  midi.sendControlChange(addr, 127);
                }
                else{
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
            uint16_t gpio = mcp[5].readGPIOAB();
            for (int enc_i = 0; enc_i < 8; enc_i++) {
              bool currentbtn = bitRead(gpio, enc_but_pins_5[enc_i]); // 0..7
              if (!currentbtn != button_enc_last[row_i][enc_i]){
                button_enc_last[row_i][enc_i] = !currentbtn;
                auto addr = encoderAddr(row_i, enc_i, BASE_NOTE+25+16, CHANNEL_switch);
                if (!currentbtn){
                  if(button_used(row_i-1, enc_i)){
                    fill_1_enc(row_i, enc_i, color_main);                  
                    leds1.show();
                    counter_last[row_i][enc_i] = -1;
                  }                  
                  midi.sendControlChange(addr, 127);
                }
                else{
                  midi.sendControlChange(addr, 0);
                }
                //Serial.printf("row= %u    enc= %u     cc=%u  btn: %d \n" , row_i, enc_i, addr.getAddress(), !currentbtn);
              }
            }
            mcp[5].clearInterrupts();
          }
        }
        else if (row_i == 3) {
          if (button_changed[3][0]) {
            button_changed[3][0] = false;
            uint16_t gpio = mcp[5].readGPIOAB();
            for (uint8_t enc_i = 8; enc_i < 16; enc_i++) {
              bool currentbtn = bitRead(gpio, enc_but_pins_5[enc_i]); // 8..15
              if (!currentbtn != button_enc_last[row_i][enc_i]){
                button_enc_last[row_i][enc_i] = !currentbtn;
                auto addr = encoderAddr(row_i, enc_i-8, BASE_NOTE+25+16, CHANNEL_switch);
                if (!currentbtn){
                  if(button_used(row_i-1, enc_i-8)){
                    fill_1_enc(row_i, enc_i-8, color_main);                  
                    leds1.show();
                    counter_last[row_i][enc_i-8] = -1;
                  }                  
                  midi.sendControlChange(addr, 127);                      
                }
                else{
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

      debounce_snap1[pcb] = mcp[chip].readGPIOAB();   // erste Probe
      debounce_due[pcb]   = millis() + DEBOUNCE_MS;   // Zeitpunkt für Recheck
      debounce_pending[pcb] = true;                   // Recheck aktivieren
      mcp[chip].clearInterrupts();
      return;                                    // noch NICHT übernehmen!
    }

    // Phase 2: Recheck fällig?
    if (debounce_pending[pcb] && (int32_t)(millis() - debounce_due[pcb]) >= 0) {
      uint16_t readout = mcp[chip].readGPIOAB();   // zweite Probe
      if (readout == debounce_snap1[pcb]) {       // stabil -> jetzt übernehmen        
        const uint8_t* b2k = B2K[pcb];
        for (uint8_t bit = 0; bit < 16; ++bit) {
          uint8_t key = b2k[bit];
          if (key == UNUSED) continue;
          bool pressed = !bitRead(readout, bit);   // Pullup: LOW=pressed
          if (button_state[pcb][key] != pressed) {
            button_state[pcb][key] = pressed;
            sth_changed = true;                  // LEDs später updaten
            //if (pressed) Serial.printf("pcb:%u chip:%u key:%u bit:%u gpio:%u\n", pcb, chip, key, bit, readout);
            auto addr = encoderAddr_2(pcb+1, key, 0, CHANNEL_switch);
            if(!pressed){
              //counter[row_i][enc_i - 8] = 0;                 
              midi.sendControlChange(addr, 127);
            }
            else{
              midi.sendControlChange(addr, 0);
            }
          }
        }
        debounce_pending[pcb] = false;                // fertig entprellt
      } 
      else {
        // noch prellend -> neue Probe merken, erneuter Recheck
        debounce_snap1[pcb] = readout;
        debounce_due[pcb]   = millis() + DEBOUNCE_MS;
      }
      mcp[chip].clearInterrupts();
    }
  }
  void handle_nav(){
    int pcb = 2;  //nav
    uint8_t  chip = CHIP_OF_PCB[pcb];
    if(nav_changed){
      nav_changed = false;     
      //uint16_t gpio = mcp[chip].readGPIOAB();  //alle pins vom mcp auslesen    
      debounce_snap1[pcb] = mcp[chip].readGPIOAB();   // erste Probe  
      debounce_due[pcb]   = millis() + DEBOUNCE_MS;   // Zeitpunkt für Recheck
      debounce_pending[pcb] = true;                   // Recheck aktivieren
      //encoder      
      uint8_t a = bitRead(debounce_snap1[pcb], 0);
      uint8_t b = bitRead(debounce_snap1[pcb], 8);
      uint8_t curr = (a << 1) | b;
      uint8_t prev = lastAB_nav;
      int8_t delta = quadLUT[(prev << 2) | curr];  // -1,0,+1 pro Flanke
      if (delta) {
        sth_changed = true;
        counter_nav -= delta*speed_nav;
        //Serial.println(counter_nav);
        if (counter_nav > MAX_VALUE_nav) counter_nav = MAX_VALUE_nav;
        if (counter_nav < MIN_VALUE_nav) counter_nav = MIN_VALUE_nav;
        //midi senden wenn änderung groß genug -> eine rastung
        uint8_t v = counter_nav / 4;
        if (v != counter_last_nav_midi) {
          counter_last_nav_midi = v;
          auto addr = cs::MIDIAddress{120, CHANNEL_switch};
          midi.sendControlChange(addr, counter_nav);
        }          
      }
      lastAB_nav = curr;
      mcp[chip].clearInterrupts();
    }  

    if (debounce_pending[pcb] && (int32_t)(millis() - debounce_due[pcb]) >= 0) {
      //buttons
      uint16_t readout = mcp[chip].readGPIOAB();   // zweite Probe
      if (readout == debounce_snap1[pcb]) {       // stabil -> jetzt übernehmen      
        const uint8_t* b2k = B2K[pcb];
        for (uint8_t bit = 0; bit < 16; ++bit) {
          uint8_t key = b2k[bit];
          if (key == UNUSED) continue;
          bool pressed = !bitRead(readout, bit);      // Pullup → LOW = gedrückt
          if (button_state[pcb][key] != pressed) {
            button_state[pcb][key] = pressed;
            sth_changed = true;     
            auto addr = encoderAddr_2(pcb+1, key, 0, CHANNEL_switch);
            if(!pressed){
              //counter[row_i][enc_i - 8] = 0;                 
              midi.sendControlChange(addr, 127);
            }
            else{
              midi.sendControlChange(addr, 0);
            }       
          }
        }
      }  
    }
  }
  void handle_rest(){
    handle_pcb(0);
    handle_pcb(1);
    handle_nav();       
  }
  

// -------------------- HILFSZEUG -------------------------------------------------------------------------------------------
  void handleReceivedTinyIRData(uint16_t aAddress, uint8_t aCommand, bool isRepeat){
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
      if(IR_aCommand == 0x1 && !isRepeat && digitalReadFast(5)){
        main_mode=1;
        haptic_play(86);
      }
      else if(IR_aCommand == 0x2 && !isRepeat){
        main_mode=0;
        enc_mode = 0;
        draw_enc_setup(enc_mode,color_main,color_back);
        CHANNEL_switch = cs::Channel{enc_mode};
        haptic_play(86);
      }
  }
  void haptic_play(uint8_t fx) {
    drv.setWaveform(0, fx);  // Slot 0: Effekt
    drv.setWaveform(1, 0);   // Slot 1: Ende
    drv.go();                // abspielen
  }
  void led_blink(){
    if (since_last_led_change >= interval_blink){
      since_last_led_change = since_last_led_change - interval_blink;
      if (ledState == LOW) {
        ledState = HIGH;
      } else {
        ledState = LOW;
      }
      digitalWrite(LED_BUILTIN, ledState);
      //Serial.print("Wert_1: "); Serial.print(counter);Serial.print("Wert_2: "); Serial.println(counter[2][0]);
      Serial.printf("CPU speed: %d MHz   Temp: %.1f C  \n",  F_CPU_ACTUAL / 1000000, tempmonGetTemp());
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
  bool fastRead(uint8_t pin) {return digitalReadFast(pin);} // Teensy-Core, extrem schnell
  uint16_t capToMax(uint32_t v, uint32_t max) {
    return (v > max) ? max : v;
  }
  void init_I2C_Peripherals() {
    Wire.begin();
    Wire.setClock(400000);
    Wire2.begin();
    delay(200);
    // MCP23017 von 0x20 bis (0x20 + MCP_COUNT - 1) initialisieren
    for (uint8_t addr = 0x20; addr <= (0x20 + MCP_COUNT - 1); addr++) {
      if (!mcp[addr - 0x20].begin_I2C(addr, &Wire)) {
        Serial.print("MCP23017 0x");Serial.print(addr, HEX);Serial.println(" nicht gefunden!");
        while (1);  // abbrechen
      }
      else {
        Serial.print("MCP23017 0x");Serial.print(addr, HEX);Serial.println(" gestartet.");
      }
      delay(50);
    }
    // DRV2605 initialisieren
    if (!drv.begin(&Wire2)) {
      Serial.println("DRV2605 nicht gefunden!");
      while (1) delay(10);
    } 
    else {
      Serial.println("DRV2605 gestartet.");
    }
    delay(50);

    for (uint8_t i = 0; i < MCP_COUNT; i++) {
      init_MCP_pins(mcp[i]);
    }
    attachTeensyInterrupts();

    delay(50);
    //pins/ int resetten
    for (uint8_t i = 0; i < MCP_COUNT; i++){ 
      mcp[i].readGPIOAB();
      mcp[i].clearInterrupts(); 
    }

    for (uint8_t row = 1; row <= 3; ++row) {
      uint16_t gpio = mcp[row].readGPIOAB();
      for (uint8_t enc = 0; enc < ENC_PER_CHIP; ++enc) {
        uint8_t a = bitRead(gpio, enc_rot_pins_A[enc]);
        uint8_t b = bitRead(gpio, enc_rot_pins_B[enc]);
        lastAB[row][enc] = (a << 1) | b;   // AB als 2-Bit Zustand
      }
    }
  }
  void init_MCP_pins(Adafruit_MCP23X17& ref_mcp){
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
    attachInterrupt(digitalPinToInterrupt(mcpIntA[7]), onButtonInterrupt_nav_a, FALLING);

    attachInterrupt(digitalPinToInterrupt(mcpIntB[5]), onButtonInterrupt_main_b, FALLING);
    attachInterrupt(digitalPinToInterrupt(mcpIntB[6]), onButtonInterrupt_key_b, FALLING);
    attachInterrupt(digitalPinToInterrupt(mcpIntB[7]), onButtonInterrupt_nav_b, FALLING);
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
    g = (c >>  8) & 0xFF;
    b = (c      ) & 0xFF;
  }
  void all_Led_set (uint32_t color){
    for (int i = 0; i < leds1.numPixels(); i++) leds1.setPixel(i, color);
    for (int i = 0; i < leds2.numPixels(); i++) leds2.setPixel(i, color);
  }
  void welcome_led(){
    all_Led_set(0);
    leds1.show();    
    for (byte i = 0; i < 24; i++) {
      for (byte row_i = 1; row_i <= 3; row_i++) {  
        for (byte enc_i = 0; enc_i < 8; enc_i++) {
          leds1.setPixel(enc_led[row_i-1][enc_i]+i, color_back);
        }
      }
      delay(10);
      leds1.show();
    }
    for (byte row_i = 1; row_i <= 3; row_i++) {
      for (byte enc_i = 0; enc_i < 8; enc_i++) {
          draw_enc_ptr(enc_led[row_i-1][enc_i],map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24),color_main,color_back); 
      }
    }
    leds1.show();
  }
  void clearAll() {
    for (int i = 0; i < leds1.numPixels(); i++) leds1.setPixel(i, 0);
    for (int i = 0; i < leds2.numPixels(); i++) leds2.setPixel(i, 0);
  }
  void fill_1_enc(uint8_t row, uint8_t enc, uint32_t color){
    for (byte i = 0; i < 24; i++) {
          leds1.setPixel((enc_led[row-1][enc])+i, color);
    }    
  }
  void ring_gradient24_rgb(WS2812Serial &strip, int start, uint32_t c1, uint32_t c2) {
    constexpr int N = 24;
    uint8_t r1,g1,b1, r2,g2,b2;
    u32_to_rgb(c1, r1,g1,b1);
    u32_to_rgb(c2, r2,g2,b2);

    for (int i = 0; i < N; ++i) {
      // gewichtete Mischung: ((a*(N-1-i) + b*i) / (N-1))
      uint16_t w1 = (N - 1 - i);
      uint16_t w2 = i;
      uint8_t r = (uint16_t(r1)*w1 + uint16_t(r2)*w2) / (N - 1);
      uint8_t g = (uint16_t(g1)*w1 + uint16_t(g2)*w2) / (N - 1);
      uint8_t b = (uint16_t(b1)*w1 + uint16_t(b2)*w2) / (N - 1);
      uint32_t c = (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
      strip.setPixel(start + i, c);
    }
  }
  void ring_2_halves(uint8_t row, uint8_t enc, uint32_t color, uint32_t color2){
    for (byte i = 0; i < 13; i++) {
          leds1.setPixel((enc_led[row-1][enc])+i, color);
    } 
    for (byte i = 13; i < 24; i++) {
          leds1.setPixel((enc_led[row-1][enc])+i, color2);
    } 
  }
  void draw_enc_ptr(int start_pos, int val, uint32_t color, uint32_t color2){
    int value = val < 0 ? 0 : (val > 23 ? 23 : val); //begrenzen auf min, max
    for (int i = start_pos; i < start_pos+24; i++) leds1.setPixel(i, color2);
    //counter 
    leds1.setPixel(start_pos+value, color);
  }
  void draw_enc_ptr_sat(int start_pos, int val, uint32_t color_ptr, uint8_t enc_i){
    int value = val < 0 ? 0 : (val > 23 ? 23 : val); //begrenzen auf min, max
    uint32_t color_verlauf;
    for (int i = start_pos; i < start_pos+24; i++){
      uint8_t var_hell = capToMax(map(i, start_pos, start_pos+24, 150, 200), max_power_back);
      uint8_t var_sat = map(i, start_pos, start_pos+24, 30, 255);
      color_verlauf = CHSV_to_u32(CHSV(HUES_enc[enc_i], var_sat, var_hell));
      leds1.setPixel(i, color_verlauf);
    } 
    //counter beleuchten
    leds1.setPixel(start_pos+value, color_ptr);
  }
  void draw_enc_ptr_hell(int start_pos, int val, uint32_t color_ptr, uint8_t enc_i){
    int value = val < 0 ? 0 : (val > 23 ? 23 : val); //begrenzen auf min, max
    uint32_t color_verlauf;
    for (int i = start_pos; i < start_pos+24; i++){
      uint8_t var_hell = capToMax(map(i, start_pos, start_pos+24, 0, 200), max_power_back);
      color_verlauf = CHSV_to_u32(CHSV(HUES_enc[enc_i], 255, var_hell));
      leds1.setPixel(i, color_verlauf);
    } 
    //counter beleuchten
    leds1.setPixel(start_pos+value, color_ptr);

  }
  void draw_enc_saet(int start_pos){
    uint32_t color_verlauf;
    for (int i = start_pos; i < start_pos+24; i++){
      uint8_t var_hell = capToMax(map(i, start_pos, start_pos+24, 80, 255), max_power_back);;
      uint8_t var_sat = map(i, start_pos, start_pos+24, 0, 255);
      color_verlauf = CHSV_to_u32(CHSV((i-start_pos)*8+160, var_sat, var_hell));
      leds1.setPixel(i, color_verlauf);
    } 
  }
  void draw_enc_dyn(int start_pos){
    uint32_t color_verlauf;
    for (int i = start_pos; i < start_pos+24; i++){
      uint8_t var_hell = capToMax(255, max_power_back);
      color_verlauf = CHSV_to_u32(CHSV(HUES_enc[(i-start_pos)/3], 255, var_hell));
      leds1.setPixel(i, color_verlauf);
    } 
  }
  void draw_enc_switching(uint8_t row, uint8_t enc, uint32_t color, uint32_t color2){
    for (byte i = 0; i < 24; i++) {
      leds1.setPixel((enc_led[row-1][enc])+i, color);
    } 
    for (byte i = 0; i < 24; i+=2) {
      leds1.setPixel((enc_led[row-1][enc])+i, color2);
    } 
  }  
  void draw_enc_hell(int start_pos, uint32_t color){
    uint32_t color_verlauf;
    for (int i = start_pos; i < start_pos+24; i++){
      uint8_t var_hell = capToMax(map(i, start_pos, start_pos+24, 10, 200), max_power_back);
      color_verlauf = CHSV_to_u32(CHSV(0, 0, var_hell));
      leds1.setPixel(i, color_verlauf);
    } 
  }
  void draw_upper_half(int start_pos, uint32_t color, uint32_t color2){
    for (int i = start_pos; i < start_pos+24; i++){
      if(enc_upper[i-start_pos]){
        leds1.setPixel(i, color);
      }
      else{
        leds1.setPixel(i, color2);
      }
    } 
  }  
  void draw_lower_half(int start_pos, uint32_t color, uint32_t color2){
    for (int i = start_pos; i < start_pos+24; i++){
      if(enc_lower[i-start_pos] == 1){
        leds1.setPixel(i, color);
      }
      else{
        leds1.setPixel(i, color2);
      }
    } 
  }  
  void draw_left_half(int start_pos, uint32_t color, uint32_t color2){
    for (int i = start_pos; i < start_pos+24; i++){
      if(enc_left[i-start_pos]){
        leds1.setPixel(i, color);
      }
      else{
        leds1.setPixel(i, color2);
      }
    } 
  } 
  void draw_right_half(int start_pos, uint32_t color, uint32_t color2){
    for (int i = start_pos; i < start_pos+24; i++){
      if(enc_right[i-start_pos]){
        leds1.setPixel(i, color);
      }
      else{
        leds1.setPixel(i, color2);
      }
    } 
  } 
  void draw_mid_half(int start_pos, uint32_t color, uint32_t color2){
    for (int i = start_pos; i < start_pos+24; i++){
      if(enc_mid[i-start_pos]){
        leds1.setPixel(i, color);
      }
      else{
        leds1.setPixel(i, color2);
      }
    } 
  } 
  void led_nav_draw(int start_pos, int val, uint32_t color, uint32_t color2){
    int value = val < 0 ? 0 : (val > 23 ? 23 : val); //begrenzen auf min, max
    //alle schwarz zeichnen
    //fill_solid(&big[start_pos], 24, color2);
    for (int i = start_pos; i < start_pos+24; i++) leds2.setPixel(i, color2);
    //nur count beleuchten
    //big[start_pos+value] = color;
    leds2.setPixel(start_pos+value, color);
  }
  void draw_pcb_leds(uint8_t pcb) {
    const uint8_t* k2l = K2L_u8[pcb];
    for (uint8_t key = 0; key < KEY_COUNT[pcb]; key++) {
      uint8_t ledIdx = k2l[key];               // Index auf leds2
      ledIdx += LED_shift[pcb];
      // Sicherheit: nicht außerhalb vom Strip
      if (ledIdx >= NUM_LEDS_main) continue;
      leds2.setPixel(ledIdx, button_state[pcb][key] ? color_main : color_back_bright);
    }
    if(pcb == 0){
      //Modus_Button: licht
      leds2.setPixel(k2l[0]+LED_shift[pcb], button_state[pcb][0] ? color_main : cl_wh);  
      //Modus_Button: HSV
      leds2.setPixel(k2l[1]+LED_shift[pcb], button_state[pcb][1] ? color_main : color_rgb); 
      //Modus_Button: Details
      leds2.setPixel(k2l[2]+LED_shift[pcb], button_state[pcb][2] ? color_main : cl_wh); 
      //Modus_Button: Color_Grading
      leds2.setPixel(k2l[3]+LED_shift[pcb], button_state[pcb][3] ? color_main : color_rgb);
      //Helligkeitsbutton
      leds2.setPixel(k2l[7]+LED_shift[pcb], button_state[pcb][7] ? c_blu : cl_wh);
    }     
  }
  void draw_nav_butt() {
    uint32_t color_read = 0; 
    for (uint8_t key = 10; key <= 13; key++) {
      uint8_t ledIdx = led_nav_enc[key-10];               // Index auf leds2
      ledIdx += LED_shift[2];
      if (ledIdx >= NUM_LEDS_main) continue;
      color_read = getPixelColor(drawingMemory2, ledIdx);
      leds2.setPixel(ledIdx, button_state[2][key] ? color_rgb : color_read);
    }
    if(button_state[2][9]){      
      for(int i = (2+LED_shift[2]); i < (24+2+LED_shift[2]); i++){
        leds2.setPixel(i, color_rgb);        
      }
    }
  }
  void draw_enc_hsv(){
    for (byte row_i = 1; row_i <= 3; row_i++){
      //hue
      if(row_i == 3){
        for (byte enc_i = 0; enc_i < 8; enc_i++) {
          if(counter[row_i][enc_i] != counter_last[row_i][enc_i]){
            counter_last[row_i][enc_i]=counter[row_i][enc_i];
            //Serial.printf("counter_enc: %d,  counter/4: %d   map: %d \n",counter[row_i][enc_i],  counter[row_i][enc_i]/4,map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24));
            uint32_t color_enc = CRGB_to_u32(CHSV(HUES_enc[enc_i], 255, capToMax(255, max_power_back)));
            draw_enc_ptr(enc_led[row_i-1][enc_i],map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24),cl_wh_dimm,color_enc);
            enc_dirty = true;
          } 
        }
      }
      //sat
      if(row_i == 2){
        for (byte enc_i = 0; enc_i < 8; enc_i++) {
          if(counter[row_i][enc_i] != counter_last[row_i][enc_i]){
            counter_last[row_i][enc_i]=counter[row_i][enc_i]; 
            draw_enc_ptr_sat(enc_led[row_i-1][enc_i],map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24), 0, enc_i);
            enc_dirty = true;
          } 
        }
      }
      //hell
      if(row_i == 1){
        for (byte enc_i = 0; enc_i < 8; enc_i++) {
          if(counter[row_i][enc_i] != counter_last[row_i][enc_i]){
            counter_last[row_i][enc_i]=counter[row_i][enc_i]; 
            draw_enc_ptr_hell(enc_led[row_i-1][enc_i],map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24), cl_wh_dimm, enc_i);
            enc_dirty = true;
          } 
        }
      }
    }
  }
  void draw_enc_licht(){
    //background/ unused keys
    for (byte row_i = 1; row_i <= 3; row_i++){
      for (byte enc_i = 0; enc_i < 8; enc_i++){
        if(enc_used_licht[row_i-1][enc_i] == 0){
          fill_1_enc(row_i, enc_i, CRGB_to_u32(CHSV(0, 255, capToMax(20, max_power_back))));    
        }
      }
    }
    uint32_t col_ye = CRGB_to_u32(CHSV(35, 255, capToMax(255, max_power_back)));
    uint32_t col_bl = CRGB_to_u32(CHSV(160, 255, capToMax(255, max_power_back)));
    uint32_t col_gr = CRGB_to_u32(CHSV(96, 255, capToMax(200, max_power_back)));
    uint32_t col_pu = CRGB_to_u32(CHSV(198, 255, capToMax(255, max_power_back)));
    uint32_t col_wh_dimm = CRGB_to_u32(CHSV(0, 0, capToMax(180, max_power_back)));
    //TEMP, TON
    ring_2_halves(1,0,col_gr, col_pu);
    ring_2_halves(3,0,col_bl, col_ye);
    //DYN, SÄT
    draw_enc_dyn(enc_led[0][1]);
    draw_enc_saet(enc_led[2][1]);
    //lichter
    draw_upper_half(enc_led[1-1][3],col_wh_dimm,0);
    draw_enc_switching(1,4, col_wh_dimm,col_ye);
    draw_enc_hell(enc_led[2-1][3], col_wh_dimm);
    draw_enc_switching(2,4, col_wh_dimm,0);
    draw_lower_half(enc_led[3-1][3],col_wh_dimm,0);
    draw_enc_switching(3,4, col_wh_dimm,col_bl);
    //GRAD-KURVE
    draw_upper_half(enc_led[1-1][6],col_wh_dimm,0);
    draw_mid_half(enc_led[2-1][6],col_wh_dimm,0);
    draw_lower_half(enc_led[3-1][6],col_wh_dimm,0);
    //PRÄSENZ
    draw_enc_switching(1,7, col_wh_dimm,col_bl);
    draw_enc_switching(2,7, col_wh_dimm,col_gr);
    draw_enc_switching(3,7, col_wh_dimm,col_ye);
    enc_dirty=true; 
  }
  void draw_enc_setup(uint8_t modus,uint32_t color, uint32_t color2){
    switch (modus) {
      case 0:{
        for (byte row_i = 1; row_i <= 3; row_i++){
          for (byte enc_i = 0; enc_i < 8; enc_i++){
            if(enc_used_licht[row_i-1][enc_i] == 0){
              fill_1_enc(row_i, enc_i, CRGB_to_u32(CHSV(0, 255, capToMax(20, max_power_back))));    
            }
          }
        }
        uint32_t col_ye = CRGB_to_u32(CHSV(35, 255, capToMax(255, max_power_back)));
        uint32_t col_bl = CRGB_to_u32(CHSV(160, 255, capToMax(255, max_power_back)));
        uint32_t col_gr = CRGB_to_u32(CHSV(96, 255, capToMax(200, max_power_back)));
        uint32_t col_pu = CRGB_to_u32(CHSV(198, 255, capToMax(255, max_power_back)));
        uint32_t col_wh_dimm = CRGB_to_u32(CHSV(0, 0, capToMax(180, max_power_back)));
        //TEMP, TON
        ring_2_halves(1,0,col_gr, col_pu);
        ring_2_halves(3,0,col_bl, col_ye);
        //DYN, SÄT
        draw_enc_dyn(enc_led[0][1]);
        draw_enc_saet(enc_led[2][1]);
        //lichter
        draw_upper_half(enc_led[1-1][3],col_wh_dimm,0);
        draw_enc_switching(1,4, col_wh_dimm,col_ye);
        draw_enc_hell(enc_led[2-1][3], col_wh_dimm);
        draw_enc_switching(2,4, col_wh_dimm,0);
        draw_lower_half(enc_led[3-1][3],col_wh_dimm,0);
        draw_enc_switching(3,4, col_wh_dimm,col_bl);
        //GRAD-KURVE
        draw_upper_half(enc_led[1-1][6],col_wh_dimm,0);
        draw_mid_half(enc_led[2-1][6],col_wh_dimm,0);
        draw_lower_half(enc_led[3-1][6],col_wh_dimm,0);
        //PRÄSENZ
        draw_enc_switching(1,7, col_wh_dimm,col_bl);
        draw_enc_switching(2,7, col_wh_dimm,col_gr);
        draw_enc_switching(3,7, col_wh_dimm,col_ye);
        break;
      }  
      case 1:{
        uint32_t col_bl = CRGB_to_u32(CHSV(160, 255, capToMax(255, max_power_back)));
        for (byte row_i = 1; row_i <= 3; row_i++){
          //hue
          if(row_i == 3){
            for (byte enc_i = 0; enc_i < 8; enc_i++) {
                //Serial.printf("counter_enc: %d,  counter/4: %d   map: %d \n",counter[row_i][enc_i],  counter[row_i][enc_i]/4,map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24));
                uint32_t color_enc = CRGB_to_u32(CHSV(HUES_enc[enc_i], 255, capToMax(255, max_power_back)));
                draw_enc_ptr(enc_led[row_i-1][enc_i],map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24),col_bl,color_enc);
            }
          }
          //sat
          if(row_i == 2){
            for (byte enc_i = 0; enc_i < 8; enc_i++) {
                draw_enc_ptr_sat(enc_led[row_i-1][enc_i],map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24), col_bl, enc_i);
            }
          }
          //hell
          if(row_i == 1){
            for (byte enc_i = 0; enc_i < 8; enc_i++) {
                draw_enc_ptr_hell(enc_led[row_i-1][enc_i],map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24), col_bl, enc_i);
            }
          }
        }
        break;
      }
      default:{
        for (byte row_i = 1; row_i <= 3; row_i++) {
          for (byte enc_i = 0; enc_i < 8; enc_i++) {
              draw_enc_ptr(enc_led[row_i-1][enc_i],map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24),color,color2); 
          }
        }
      break;
      }
    }
    enc_dirty=true; 
  }
  void draw_enc_1_col(uint32_t color, uint32_t color2){
    for (byte row_i = 1; row_i <= 3; row_i++) {
      for (byte enc_i = 0; enc_i < 8; enc_i++) {
        if(counter[row_i][enc_i] != counter_last[row_i][enc_i]){
          counter_last[row_i][enc_i]=counter[row_i][enc_i];
          draw_enc_ptr(enc_led[row_i-1][enc_i],map(counter[row_i][enc_i], 0, MAX_VALUE, 0, 24),color,color2);      
          enc_dirty=true; 
        }   
      }
    }
  }
  void rainbow(){
    for(int i = 0;i < 96;i++){
      color_rain = CRGB_to_u32(CHSV(color_shift_rain, 255, capToMax(255, max_power_back))); 
      leds1.setPixel(i, color_rain);
    }   
  }
  void power_save(uint16_t cap_hell){
    if(digitalReadFast(5) == HIGH){
      max_power_back = capToMax(max_hell_power, cap_hell);
      max_power_back_dimm = max_power_back-50;
      if (max_power_back_dimm < 0) max_power_back_dimm = 0;
      cl_wh=c_wh_bright;
    }
    else{
      //color_rgb = CRGB_to_u32(CHSV(120, 255, 15));
      max_power_back = capToMax(max_hell_usb, cap_hell);;
      max_power_back_dimm = max_power_back-20;
      if (max_power_back_dimm < 0) max_power_back_dimm = 0;
      cl_wh=c_wh_dimm;
    }
    //Serial.printf("LED MODE: %d \n", digitalReadFast(7));
  }
  void color_calc(){
    color_back = CRGB_to_u32(CHSV(0, 255, capToMax(50, max_power_back)));
    color_back_bright = CRGB_to_u32(CHSV(0, 255, capToMax(200, max_power_back)));
    color_main = CRGB_to_u32(CHSV(160, 255, capToMax(255, max_power_back))); 
    color_main_bright = CRGB_to_u32(CHSV(160, 255, 255)); 
    color_rgb = CRGB_to_u32(CHSV(100, 225, capToMax(255, max_power_back)));  
    cl_wh = CRGB_to_u32(CHSV(0, 0, capToMax(255, max_power_back)));   
    cl_wh_dimm = CRGB_to_u32(CHSV(0, 0, capToMax(150, max_power_back))); 
  }


// -------------------- UPDATE_all ------------------------------------------------------------------------------------------  

  void update_my(){
    //kritischer update stuff
    if (since_last_led >= interval_show){
      since_last_led = since_last_led - interval_show;
      color_shift_my += 2; 
      color_shift_rain ++;
      rainbow();
      color_rgb = CRGB_to_u32(CHSV(color_shift_my, 255, capToMax(255, max_power_back))); 

      //Modus der gerendert wird
      if(enc_mode == 0){
        draw_enc_licht();
      }  
      else if(enc_mode == 1){
        draw_enc_hsv();
      }
      else if(enc_mode == 2){
        draw_enc_1_col(color_main,color_back);
      }    
      else{
        draw_enc_1_col(color_main,color_back);
      }      


      //es wurden knöpfe gedrückt
      if(true){
        sth_changed = false;
        draw_pcb_leds(0); // main
        draw_pcb_leds(1); // key
        draw_pcb_leds(2); // nav        
        //Serial.printf("counter: %d,     speed: %d,    map: %d, max_hell_dimm: %d, max_power_back: %d \n",counter_nav,speed_nav,map(counter_nav, 0, MAX_VALUE_nav, 0, 255), max_power_back_dimm, max_power_back);
        led_nav_draw(18,map(counter_nav, 0, MAX_VALUE_nav, 0, 24),color_main_bright,color_back);
        draw_nav_butt();        

        if(button_state[0][0]){ //licht
          enc_mode = 0;
          draw_enc_setup(enc_mode,color_main,color_back);
          CHANNEL_switch = cs::Channel{enc_mode};
          haptic_play(86);
        } 
        if(button_state[0][1]){ //HSV
          enc_mode = 1;
          draw_enc_setup(enc_mode,color_main,color_back);
          CHANNEL_switch = cs::Channel{enc_mode};
          haptic_play(86);
        }
        if(button_state[0][2]){ //details
          enc_mode = 2;
          draw_enc_setup(enc_mode,color_main,color_back);
          CHANNEL_switch = cs::Channel{enc_mode};
          haptic_play(86);
        }   
        if(button_state[0][3]){ //color grading
          enc_mode = 3;
          draw_enc_setup(enc_mode,color_main,color_back);
          CHANNEL_switch = cs::Channel{enc_mode};
          haptic_play(86);
        } 
        if(button_state[0][7]){ //hell anpassen
          power_save(map(counter_nav,0,MAX_VALUE_nav,25,255));
          color_calc();
          haptic_play(13);
          draw_enc_setup(enc_mode,color_main,color_back);
          enc_dirty=true; 
        }
        leds2.show();
      }

      if(enc_dirty) {
        leds1.show();     
        enc_dirty=false;    
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


  struct MyMIDI_Callbacks : MIDI_Callbacks { 
      // Callback for channel messages (notes, control change, pitch bend, etc.).... https://tttapa.github.io/Control-Surface/Doxygen/d7/d3f/structChannelMessage.html#a35187e10c578f1ff079af83ffb2aba0a
      void onChannelMessage(MIDI_Interface &, ChannelMessage msg) override {
          auto type = msg.getMessageType();
          if (type == msg.ControlChange || type == msg.NoteOn)
              onNoteMessage(msg);
      } 

      // Our own callback specifically for Note On and Note Off messages
      void onNoteMessage(ChannelMessage msg) {
          auto type     = msg.getMessageType(); 
          auto channel  = msg.getChannel();
          auto note     = msg.getData1();
          auto velocity = msg.getData2();

          uint8_t row, enc;
          decodeCC(note, row, enc);
          if((row >= 1 && row <=3) && (enc >= 0 && enc <= 7)){
            counter[row][enc] = 4 * velocity;     
          }            

          if(note == 60){
            counter_nav=velocity; 
            sth_changed = true;
          }

          Serial << type << ": "<< channel << ", Note " << note << ", Velocity " << velocity << ", row " << row << ", enc " << enc << ", counter[row][enc]: " << counter[row][enc] << endl;
      } 
  } callback; // Instantiate a callback


// -------------------- SETUP -----------------------------------------------------------------------------------------------      

  void setup() {
    // put your setup code here, to run once:    
    delay(500);
    pinMode(5, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    power_save(255);
    delay(200);

    color_calc();             
    clearAll();
    leds1.begin();
    leds2.begin();
    leds1.show();
    leds2.show();

    Serial.begin(115200);    
    dump_last_crash();
    print_startup_info();
    delay(500);

    init_I2C_Peripherals();    
    
    Serial.println("Encoder mit Interrupts bereit.");
    initPCIInterruptForTinyReceiver();
    
    drv.setMode(DRV2605_MODE_INTTRIG); 
    drv.selectLibrary(1);                // ERM-Library
    // Wenn du einen LRA benutzt:
    // drv.useLRA(); drv.selectLibrary(6); // oder 7
    drv.setMode(DRV2605_MODE_AUTOCAL);
    drv.go();
    delay(120);                          // kurz warten
    drv.setMode(DRV2605_MODE_INTTRIG);
    delay(50);
    drv.setWaveform(0, 84);  // ramp up medium 1, see datasheet part 11.2
    drv.setWaveform(1, 1);  // strong click 100%, see datasheet part 11.2
    drv.setWaveform(2, 73);
    drv.setWaveform(3, 0);  // end of waveforms
    drv.go();// play the effect!    
    Serial.println("DRV ready");     

    midi.begin();     
    midi.setCallbacks(callback); // Attach the custom callback
    Serial.println("MIDI ready");

    welcome_led();

    Serial.println("Start Loop");
  }



// -------------------- LOOP ------------------------------------------------------------------------------------------------


  void loop() {
    // put your main code here, to run repeatedly:
    led_blink();
    
    if(main_mode == 0){
      encoder_handle();
      encoder_button_handle();
      handle_rest();
      update_my();
      midi.update();
    }
    else{
      //led show
      pride();
    }
  }





// --------------------------------------------------------------------------------------------------------------------------





void pride() {
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t  sat8                 = beatsin88(87, 220, 250);
  uint8_t  brightdepth          = beatsin88(341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88(203, 25 * 256, 40 * 256);
  uint8_t  msmultiplier         = beatsin88(147, 23, 60);

  uint16_t hue16    = sHue16;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  uint16_t ms      = millis();
  uint16_t deltams = ms - sLastMillis;
  sLastMillis      = ms;
  sPseudotime     += deltams * msmultiplier;
  sHue16          += deltams * beatsin88(400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for (uint16_t i = 0; i < NUM_LEDS_enc; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 >> 8;

    brightnesstheta16 += brightnessthetainc16;
    uint16_t b16  = sin16(brightnesstheta16) + 32768;
    uint32_t bri16 = ((uint32_t)b16 * (uint32_t)b16) >> 16;
    uint8_t bri8 = (uint32_t)bri16 * brightdepth >> 16;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV(hue8, sat8, bri8);

    // optional: invertierte Laufrichtung wie in deinem Code
    uint16_t pixelnumber = (NUM_LEDS_enc - 1) - i;

    // Aktuelle Farbe aus dem WS2812Serial-Puffer holen, blenden, zurückschreiben
    CRGB cur = getPixelColor(drawingMemory1, pixelnumber);
    nblend(cur, newcolor, 64);
    leds1.setPixel(pixelnumber, CRGB_to_u32(cur));
  }
  leds1.show();
}





























