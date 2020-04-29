/*
 * Board: Arduino NANO Every
 *
 * MIDI Ch. 1 to 8: CV/GATE/NRPN MODE
 *   - When receive NOTE ON/OFF for Ch. N, then update CV port N
 *     and update GATE port N ON/OFF
 *   - When receibe NRPB MSB 0x40, then build 16bit data
 *     by LSB lower 2bit + DATA MSB 7bit + LSB 7bit,
 *     and use the data to update CV port N.
 * ex) use Ch. 1 NOTE ON/OFF to control CV1 by pitch,
 *     GATE1 by NOTE ON/OFF
 * 
 * MIDI Ch. 13 to 16: CV/GATE/CV MODE
 *   - When receive NOTE ON/OFF for Ch. N, then update CV port (N - 12)
 *     and update GATE port (N - 12) ON/OFF
 *   - When receive Modulation CC, then update CV port (N - 8)
 * ex) use Ch. 13 NOTE ON/OFF to control CV1 by pitch,
 *     GATE1 by NOTE ON/OFF, CV5 by modulation wheel
 * 
 * MIDI Ch. 9: GATE MODE
 *   - When Receive NOTE ON/FF with note number N, 
 *     GATE port N+1 ON/OFF.
 *
 */

//#define DEBUG 1

/*
 * MIDI settings
 */
#define NRPN_CV_MSB 0x40 // 64
#define GATE_CH 9
#define GATE_NOTENUM_BASE 0
#define CV_NOTENUM_BASE 36
#define DEFAULT_PBEND_RANGE 2.0

/*
 * configurations
 */
#define INITIAL_VOLTAGE 0x00

// current status register
#define NRPN_UNDEF 0xFF // undefined flag
byte note_number[8];
byte note_on_count[8];
byte nrpn_lsb[8];
byte nrpn_msb[8];
byte data_lsb[8];
byte data_msb[8];
byte mod_msb[4];

/*
 * Pin assings: GATE OUTPUT (inv)
 */
#define GATE_1 A1
#define GATE_2 A0
#define GATE_3 3
#define GATE_4 2
#define GATE_5 6
#define GATE_6 7
#define GATE_7 4
#define GATE_8 5
const byte GATE_PORTS[] = { GATE_1, GATE_2, GATE_3, GATE_4,
                           GATE_5, GATE_6, GATE_7, GATE_8 };

/*
 * DAC port assign
 * 0:A -> 2
 * 1:B -> 5
 * 2:C -> 1
 * 3:D -> 6
 * 4:E -> 4
 * 5:F -> 7
 * 6:G -> 3
 * 7:H -> 8
 */ 
#define CV1 2
#define CV2 0
#define CV3 6
#define CV4 4
#define CV5 1
#define CV6 3
#define CV7 5
#define CV8 7
const byte DAC_PORTS[] = { CV1, CV2, CV3, CV4,
                          CV5, CV6, CV7, CV8 };

/*
 * Pin assings: EXTENDS PORT
 */
#define EXT_1 A2
#define EXT_2 A3
#define EXT_3 A4
#define EXT_4 A5
#define EXT_5 A6
//#define EXT_6 A7

/*
 * Pin assings: CONNECT TO DA CHIP
 */
#define SCK 13
#define SS 10
#define MOSI 11
#define LDAC 8
#define CLR 9
#define VREF A7

/*
 * Main codes
 */

#define gate_on(x)  digitalWrite(GATE_PORTS[x], LOW)
#define gate_off(x) digitalWrite(GATE_PORTS[x], HIGH)

/* use Arduino MIDI Library */
#include <MIDI.h>

/*
 * use AD5668-Library
 * https://github.com/bobhart/AD5668-Library
 */
#include "AD5668.h"
#include <SPI.h>

// Created and binds the MIDI interface to the default hardware Serial port
// Arduino NANO every's serial port is 'Serial1'
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1,  MIDI);

struct MySettings : public midi::DefaultSettings {
  static const long BaudRate = 9600;
};

MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial, MIDI2, MySettings);

// Create DAC instance
AD5668 DAC = AD5668(SS, CLR, LDAC);

void setup() {
  // initialize GATE PIN
  byte n;
  for (n = 0; n < 8; n++) {
    pinMode(GATE_PORTS[n], OUTPUT);
    gate_off(n);
    note_on_count[n] = 0;
  }

  // initialize NRPN handler
  memset(nrpn_lsb, NRPN_UNDEF, sizeof(byte) * 16);
  memset(nrpn_msb, NRPN_UNDEF, sizeof(byte) * 16);
  memset(data_lsb, NRPN_UNDEF, sizeof(byte) * 16);
  memset(data_msb, NRPN_UNDEF, sizeof(byte) * 16);

  // initialize MIDI
  MIDI.begin(MIDI_CHANNEL_OMNI);  // Listen to all incoming messages
  MIDI.setHandleNoteOn(onNoteOn);
  MIDI.setHandleNoteOff(onNoteOff);
  MIDI.setHandleControlChange(onControlChange);
  MIDI.setHandlePitchBend(onPitchBend);
  MIDI.turnThruOff();

  // initialize MIDI2
  MIDI2.begin(MIDI_CHANNEL_OMNI);  // Listen to all incoming messages
  MIDI2.setHandleNoteOn(onNoteOn);
  MIDI2.setHandleNoteOff(onNoteOff);
  MIDI2.setHandleControlChange(onControlChange);
  MIDI2.setHandlePitchBend(onPitchBend);
  MIDI2.turnThruOff();

  /* initialize SPI pins */
  pinMode(SCK, OUTPUT);
  pinMode(SS, OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(LDAC, OUTPUT);
  pinMode(CLR, OUTPUT);
  digitalWrite(SCK, LOW);
  digitalWrite(SS, HIGH);
  digitalWrite(MOSI, LOW);
  digitalWrite(LDAC, LOW);
  digitalWrite(CLR, HIGH);

  /* initialize DAC */
  unsigned int refVolt = 0;
  while(refVolt < 256) {
    initDAC();
    delay(10);
    refVolt = analogRead(A7);
    delay(10);
#ifdef DEBUG
    digitalWrite(2, HIGH);
    delay(500);
    digitalWrite(2, LOW);
    delay(500);
#endif
  }

  // set initial voltage
  byte i = 0;
  for (i = 0; i < 8; i++) {
    updateDacVoltage(i, INITIAL_VOLTAGE);
  }

  //Serial.begin(9600);
  Serial.write(128);

#ifdef DEBUG  
  delay(1000);
  for (n = 0; n < 8; n++) {
    gate_on(n);
  }
#endif

}

void initDAC() {
  // initialize the DAC
  DAC.init();
  DAC.enableInternalRef(); // turn on the internal reference
  DAC.powerDAC_Normal(B11111111); // Power up all channels normal
}

unsigned int convertNoteNumberToVoltage(byte number) {
  // DAC resolusion is 16 bits: 0 to 0xFF (65535)
  // max voltage is 5V
  //   => 13107 / V
  // Center "C" is #60
  //
  if (number <= CV_NOTENUM_BASE) {
    return 0;
  }
  if (number >= CV_NOTENUM_BASE + 60) {
    return 0xFF;
 }
  return (unsigned int)((unsigned long)(number - CV_NOTENUM_BASE) * 13107 / 12);
}

void onPitchBend(byte channel, int bend) {
  byte ch = channel;
  if (channel < 9) {
    ch = channel - 1;
  }
  else if (12 < channel) {
    ch = channel - 13;
  }
  else {
    return;
  }
  // bend is -8192 to 8191 (14 bit)
  double delta = (double)(note_number[ch] - CV_NOTENUM_BASE)
    + (double)bend * DEFAULT_PBEND_RANGE / 8192;

  unsigned int volOut;
  if (delta < 0) {
    volOut = 0;
  }
  else if (delta > 60.0) {
    volOut = 0xFF;
  }
  else {
    volOut = (unsigned int)(delta * 13107 / 12);
  }
  updateDacVoltage(ch, volOut);
}

void onNoteOn(byte channel, byte number, byte velocity) {
  if (velocity == 0) {
    onNoteOff(channel, number, velocity);
    return;
  }

  Serial.write(1);

  if (1 <= channel && channel <= 8) {
    // CV and GATE mode
    byte ch = channel - 1;
    note_number[ch] = number;
    updateDacVoltage(ch, convertNoteNumberToVoltage(number));
    gate_on(ch);
    note_on_count[ch]++;
    return;
  }
  
  if (channel == GATE_CH) {
    // GATE mode
    if (number < GATE_NOTENUM_BASE
        || number > GATE_NOTENUM_BASE + 7) {
      return;
    }
    gate_on(number - GATE_NOTENUM_BASE);
    return;
  }

  if (13 <= channel && channel <= 16) {
    // CV and GATE and Mod mode
    byte ch = channel - 13;
    note_number[ch] = number;
    updateDacVoltage(ch, convertNoteNumberToVoltage(number));
    gate_on(ch);
    note_on_count[ch]++;
    return;
  }
}

void onNoteOff(byte channel, byte number, byte velocity) {

  Serial.write(0);

  if (1 <= channel && channel <= 8) {
    // CV and GATE mode
    byte ch = channel - 1;
    if (note_on_count[ch] == 0) {
      gate_off(ch);
      return;
    }
    note_on_count[ch]--;
    if (note_on_count[ch] == 0) {
      gate_off(ch);
    }
    return;
  }
  
  if (channel == GATE_CH) {
    // GATE mode
    if (number < GATE_NOTENUM_BASE
        || number > GATE_NOTENUM_BASE + 7) {
      return;
    }
    gate_off(number - GATE_NOTENUM_BASE);
    return;
  }

  if (13 <= channel && channel <= 16) {
    // CV and GATE and Mod mode
    byte ch = channel - 13;
    if (note_on_count[ch] == 0) {
      gate_off(ch);
      return;
    }
    note_on_count[ch]--;
    if (note_on_count[ch] == 0) {
      gate_off(ch);
    }
    return;
  }
}

void onControlChange(byte channel, byte number, byte value) {
  // channel is 1 to 16
  if (number == 0x7b) { // All note off
    byte i;
    for (i = 0; i < 8; i++) {
      gate_off(i);
      note_on_count[i] = 0;
    }
    return;
  }
  else if (number == 0x01) { // 1: modulation MSB
    // modulation is acceptable only 13 - 16 ch.
    if (channel < 13) {
      return;
    }
    mod_msb[channel - 13] = value;
    // 0xFF (65535) / 127 = 516.0236...
    unsigned int volOut = (unsigned int)value * 516;
    updateDacVoltage(channel - 9, volOut);
  }
  else if (number == 0x21) { // 33: modulation LSB
    // modulation is acceptable only 13 - 16 ch.
    if (channel < 13) {
      return;
    }
    unsigned int msb = mod_msb[channel - 13];
    // 0xFF (65535) / 127 = 516.0236...
    // 0xFF (65535) / (127 * 127) = 4.063178...
    unsigned int volOut = (unsigned int)msb * 512 + (unsigned int)value * 4;
    updateDacVoltage(channel - 9, volOut);
  }
  else if (number == 0x62) { // 98: NRPN LSB
    // NRPN is acceptable only 1 - 8 ch.
    if (8 < channel) {
      return;
    }
    nrpn_lsb[channel - 1] = value;
    return;
  }
  else if (number == 0x63) { // 99: NRPN MSB
    // NRPN is acceptable only 1 - 8 ch.
    if (8 < channel) {
      return;
    }
    nrpn_msb[channel - 1] = value;
    return;
  }
  else if (number == 0x06) { // 6: data entry MSB
    // NRPN is acceptable only 1 - 8 ch.
    if (8 < channel) {
      return;
    }
    byte ch = channel - 1;
    if (nrpn_msb[ch] == NRPN_UNDEF || nrpn_lsb[ch] == NRPN_UNDEF) {
    // invalid message squence
      nrpn_msb[ch] = NRPN_UNDEF;
      nrpn_lsb[ch] = NRPN_UNDEF;
      return;
    }
    data_msb[ch] = value;
    if (data_lsb[ch] != NRPN_UNDEF) {
      onNrpn(channel);
    }
    return;
  }
  else if (number == 0x26) { // 38: data entry LSB
    // NRPN is acceptable only 1 - 8 ch.
    if (8 < channel) {
      return;
    }
    byte ch = channel - 1;
    if (nrpn_msb[ch] == NRPN_UNDEF || nrpn_lsb[ch] == NRPN_UNDEF) {
      // invalid message squence
      nrpn_msb[ch] = NRPN_UNDEF;
      nrpn_lsb[ch] = NRPN_UNDEF;
      return;
    }
    data_lsb[ch] = value;
    if (data_msb[ch] != NRPN_UNDEF) {
      onNrpn(channel);
    }
    return;
  }
}

void onNrpn(byte channel) {
  byte ch = channel - 1;
  unsigned int dac_level = 0;
  if (nrpn_msb[ch] == NRPN_CV_MSB) {
    if (ch < 8) {
      dac_level = (unsigned int)nrpn_lsb[ch] << 14;
      dac_level += (unsigned int)data_msb[ch] << 7;
      dac_level += (unsigned int)data_lsb[ch];
      updateDacVoltage(ch, dac_level);
    }
  }
  nrpn_msb[ch] = NRPN_UNDEF;
  nrpn_lsb[ch] = NRPN_UNDEF;
  data_msb[ch] = NRPN_UNDEF;
  data_lsb[ch] = NRPN_UNDEF;

#ifdef DEBUG
  if (dac_level > 0) {
    gate_on(0);
  } else {
    gate_off(1);
  }
#endif
}

void updateDacVoltage(byte ch, unsigned int value) {
  // ch is 0 to 7
  if (ch >= 8) {
    return;
  }
  DAC.writeUpdateCh(DAC_PORTS[ch], value);
}
void loop() {
  MIDI.read();
  MIDI2.read();
}

