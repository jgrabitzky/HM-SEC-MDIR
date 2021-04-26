//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// ci-test=yes board=328p aes=no
//- -----------------------------------------------------------------------------------------------------------------------

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

// define this if you have a TSL2561 connected at address 0x29
// #define USE_TSL2561

// define this if you have a BH1750 connected at address 0x23
#define USE_BH1750

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <AskSinPP.h>
#include <LowPower.h>

#if defined(USE_TSL2561)
#include <sensors/Tsl2561.h>
#elif defined(USE_BH1750)
#include <sensors/Bh1750.h>
#endif
#include <Wire.h>
#include <MultiChannelDevice.h>
#include <Motion.h>

// we use a Pro Mini
// Arduino pin for the LED
// D4 == PIN 4 on Pro Mini
#define LED_PIN 4
// Arduino pin for the config button
// B0 == PIN 8 on Pro Mini
#define CONFIG_BUTTON_PIN 8
// Arduino pin for the PIR
// A0 == PIN 14 on Pro Mini
#define PIR_PIN 14

// === Battery measurement ===
#define BAT_VOLT_LOW        21  // 2.1V low voltage threshold
#define BAT_VOLT_CRITICAL   19  // 1.9V critical voltage threshold, puts AVR into sleep-forever mode
// Internal measuring: AVR voltage
#define BAT_SENSOR BatterySensor
// External measuring: Potential devider on GPIO; required if a StepUp converter is used
// one can consider lower thresholds (low=20; cri=13)
//#define BAT_SENSOR BatterySensorUni<A3,7,3000> // <SensPIN, ActivationPIN, RefVcc>

// number of available peers per channel
#define PEERS_PER_CHANNEL 6

// all library classes are placed in the namespace 'as'
using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
    {0x56,0x78,0x90},       // Device ID
    "JG-MDIR001",           // Device Serial
    {0x00,0x4a},            // Device Model
    0x16,                   // Firmware Version
    as::DeviceType::MotionDetector, // Device Type
    {0x01,0x00}             // Info Bytes
};

/**
 * Configure the used hardware
 */
typedef AvrSPI<10,11,12,13> SPIType;
typedef Radio<SPIType,2> RadioType;
typedef StatusLed<LED_PIN> LedType;
typedef AskSin<LedType,BAT_SENSOR,RadioType> Hal;

#if defined(USE_TSL2561)
typedef MotionChannel<Hal,PEERS_PER_CHANNEL,List0,Tsl2561<TSL2561_ADDR_LOW> > MChannel;
#elif defined(USE_BH1750)
typedef MotionChannel<Hal,PEERS_PER_CHANNEL,List0,Bh1750<0x23> > MChannel;
#else
typedef MotionChannel<Hal,PEERS_PER_CHANNEL,List0> MChannel;
#endif

typedef MultiChannelDevice<Hal,MChannel,1> MotionType;

Hal hal;
MotionType sdev(devinfo,0x20);
ConfigButton<MotionType> cfgBtn(sdev);

void setup () {
  DINIT(57600,ASKSIN_PLUS_PLUS_IDENTIFIER);
  sdev.init(hal);
  buttonISR(cfgBtn,CONFIG_BUTTON_PIN);
  motionISR(sdev,1,PIR_PIN);
  hal.initBattery(60UL*60,BAT_VOLT_LOW,BAT_VOLT_CRITICAL); // Measure Battery every 1h
       // Set frequency for CC1101
  hal.radio.initReg(CC1101_FREQ2, 0x21);
  hal.radio.initReg(CC1101_FREQ1, 0x65);
  hal.radio.initReg(CC1101_FREQ0, 0xCA);
  sdev.initDone();
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if( worked == false && poll == false ) {
    // deep discharge protection
    // if we drop below critical battery level - switch off all and sleep forever
    if( hal.battery.critical() ) {
      // this call will never return
      hal.activity.sleepForever(hal);
    }
    // if nothing to do - go sleep
    hal.activity.savePower<Sleep<>>(hal);
  }
}
