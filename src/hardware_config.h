#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Bounce2.h>
#include <ESP32Encoder.h>
#include <MIDI.h>
#include <SPI.h>
#include <Wire.h>
#include <driver/dac.h>
#include <driver/uart.h>

const int ADC_0 = 35;
const int ADC_1 = 39;
const int ADC_2 = 34;
const int ADC_3 = 36;
#include "adc_arduino.h"
#include "dma_serial.h"

void setupWifi();

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 32;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

DmaSerial midiSerial;
midi::MidiInterface<DmaSerial, midi::DefaultSettings> MIDI(midiSerial);

ESP32Encoder encoder;

// Reset pin # (or -1 if sharing Arduino reset pin)
const int OLED_RESET = -1;
// See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
const int SCREEN_ADDRESS = 0x3C;

const int DIP_0 = 4;
const int DIP_1 = 5;
const int DIP_2 = 18;
const int DIP_3 = 19;
const int BUTTON_A = 23;
const int BUTTON_B = 32;
const int ENCODER_SW = 13;
const int PWM = 27;
const int DAC_1 = 26;
const int DAC_2 = 25;
const int SYNC_IN = 15;
const int SYNC_OUT = 2;

struct Button {
  // A wrapper for Bounce2 library to handle if we already checked a press
  Bounce debouncer;
  bool thePressWasChecked{false};
  bool serverClicked{false};
  Button(int pin, int interval) : debouncer(pin, interval) {}

  void update() {
    debouncer.update();
    if (!isPressedNow()) {
      thePressWasChecked = false;
    }
  }

  bool isNewPress() {
    if (thePressWasChecked) {
      return false;
    }
    if (isPressedNow() | serverClicked) {
      thePressWasChecked = true;
      serverClicked = false;
      return true;
    }
    return false;
  }

  bool isPressedNow() { return !debouncer.read(); }

  // button click emulation from the web
  void serverClick() { serverClicked = true; }
};

Button buttonA{BUTTON_A, 5};
Button buttonB{BUTTON_B, 5};
Button encoderButton{ENCODER_SW, 5};

void setupHardware() {
  encoder.attachFullQuad(12, 14);
  encoder.setCount(0);
  encoder.setFilter(1023);

  pinMode(DIP_0, INPUT_PULLUP);
  pinMode(DIP_1, INPUT_PULLUP);
  pinMode(DIP_2, INPUT_PULLUP);
  pinMode(DIP_3, INPUT_PULLUP);
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(SYNC_IN, INPUT);
  pinMode(SYNC_OUT, OUTPUT);

  // randomSeed(analogRead(0));

  Serial.begin(9600);
  Serial.println("Starting...");

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
    // We should proceed actually, as many functions can be used without the display
  }

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.cp437(true);

  encoder.clearCount();

  int pwmBits = 12;
  int pwmRange = 1 << pwmBits;
  ledcSetup(0, 80000000 / pwmRange, pwmBits);
  ledcAttachPin(PWM, 0);

  adc_setup();

  // test Serial2 for loopback

  Serial2.begin(31250, SERIAL_8N1, RX2, TX2);
  Serial.println("Serial2 for MIDI loopback test initialized.");
  delay(1000);
  // Send some bytes
  Serial2.write(0x90);  // Note On
  Serial2.write(60);    // Note Number - Middle C
  Serial2.write(127);   // Velocity - Maximum
  Serial2.write(0x90);  // Note On
  Serial2.write(60);    // Note Number - Middle C
  Serial2.write(127);   // Velocity - Maximum

  delay(1000);

  // Attempt to receive the sent bytes
  if (Serial2.available() > 0) {
    Serial.println("Bytes available for reading.");
    uint8_t byte1 = Serial2.read();
    uint8_t byte2 = Serial2.read();
    uint8_t byte3 = Serial2.read();
    uint8_t byte4 = Serial2.read();
    uint8_t byte5 = Serial2.read();
    uint8_t byte6 = Serial2.read();
    // print bytes left
    Serial.printf("Bytes left: %d\n", Serial2.available());

    // Debug print received bytes
    Serial.print("Received bytes: ");
    Serial.print(byte1, HEX);
    Serial.print(" ");
    Serial.print(byte2, HEX);
    Serial.print(" ");
    Serial.println(byte3, HEX);
    Serial.print(" ");
    Serial.print(byte4, HEX);
    Serial.print(" ");
    Serial.print(byte5, HEX);
    Serial.print(" ");
    Serial.println(byte6, HEX);

    // If received bytes match the sent bytes, go into an infinite loop
    if (byte1 == 0x90 && byte2 == 60 && byte3 == 127) {
      Serial.println("Loopback successful.");
    } else {
      Serial.println("Loopback failed.");
    }
  } else {
    Serial.println("No bytes available for reading.");
  }
  Serial2.end();
}

#define EX_UART_NUM UART_NUM_1
#define BUF_SIZE    (1024)
