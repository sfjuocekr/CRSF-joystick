/*
 * CRSF/SBUS USB Joystick by Sjoer van der Ploeg
 *
 * SBUS = Serial1 (pin 0)
 * CRSF = Serial2 (rx pin 9, tx pin 10)
 *
 * Channels 1, 2, 3 and 4 are axis; the rest is assumed to be three position switches.
 * Having separate buttons makes setting up simulator functions a breeze!
 *
 * SBUS from bolderflight: https://github.com/bolderflight/SBUS
 * CrsfSerial from CapnBry: https://github.com/CapnBry/CRServoF/tree/master/lib/CrsfSerial
 */

#include <Arduino.h>
#include "SBUS.h"
#include <CrsfSerial.h>

// Latency testing, this will emulate different refresh rates and/or add latency between input and output.
// Values are milliseconds.
#define LATENCY 1  // increase to add latency, 1 means no latency and 255 is the maximum.
#define INTERVAL 1 // increase to change the refresh interval in milliseconds, 1 means every millisecond... USB won't go faster anyway!

// Number of channels
#define CHANNELS 16

// Endpoints for SBUS, you might need to find your own values!
#define STARTPOINT 221 // 172 = FrSky, 221 = FlySky
#define ENDPOINT 1824  // 1811 = FrSky, 1824 = FlySky

// Endpoints for CrossFire
#define US_MIN 988
#define US_MAX 2011

SBUS sbus(Serial1);
CrsfSerial crsf(Serial2, 115200);
IntervalTimer hidTimer, latencyTimer, tlmTimer;
uint16_t hats[3] = {293, 338, 360};
uint16_t ch_latency[LATENCY + 1][CHANNELS];
bool failSafe, lostFrame;

void setSticks(int _min = 1000, int _max = 2000)
{
  // Use 0-1024 for min and mix instad of 0-65535 if you use the normal layout in usb_desc.h
  Joystick.X(map(ch_latency[0][0], _min, _max, 0, 65535));       // ROLL
  Joystick.Y(map(ch_latency[0][1], _min, _max, 0, 65535));       // PITCH
  Joystick.Z(map(ch_latency[0][2], _min, _max, 0, 65535));       // THROTTLE
  Joystick.Xrotate(map(ch_latency[0][3], _min, _max, 0, 65535)); // YAW

  // These are hacks to make different simulators work that do not support buttons!
  Joystick.Yrotate(map(ch_latency[0][4], _min, _max, 0, 65535));   // AUX1 for TWGO
  Joystick.Zrotate(map(ch_latency[0][5], _min, _max, 0, 65535));   // AUX2 for TWGO
  Joystick.slider(1, map(ch_latency[0][6], _min, _max, 0, 65535)); // FPV.SkyDive only sees one slider
  Joystick.hat(1, hats[map(ch_latency[0][7], _min, _max, 0, 2)]);  // FPV.SkyDive knows about the hat!
}

void setButton(unsigned _button, int _min = 1000, int _max = 2000)
{
  for (unsigned _position = 0; _position < 3; _position++)
  {
    Joystick.button(_button * 3 + _position + 1, (unsigned)map(ch_latency[0][4 + _button], _min, _max, 0, 2) == _position ? 1 : 0);
  }
}

void setButtons(int _min = 1000, int _max = 2000)
{
  for (unsigned _button = 0; _button < (CHANNELS - 4); _button++)
  {
    setButton(_button, _min, _max);
  }
}

void packetChannels()
{
  for (unsigned _channel = 0; _channel < CHANNELS; _channel++)
  {
    ch_latency[LATENCY > 1 ? LATENCY - 1 : 0][_channel] = crsf.getChannel(_channel + 1);
  }

  setSticks(US_MIN, US_MAX);
  setButtons(US_MIN, US_MAX);
}

void onInterval()
{
  Joystick.send_now();
}

void induceLatency()
{
  if (LATENCY > 1)
  {
    for (uint8_t _bufs = 0; _bufs < LATENCY - 1; _bufs++)
    {
      memcpy(ch_latency[_bufs], ch_latency[_bufs + 1], sizeof(uint16_t) * CHANNELS);
    }
  }
}

void onTelemetry()
{
  uint8_t crsfbatt[CRSF_FRAME_BATTERY_SENSOR_PAYLOAD_SIZE];

  crsfbatt[0] = 0;
  crsfbatt[1] = 50; // Fake 5v voltage
  crsfbatt[2] = 0;
  crsfbatt[3] = 50; // current
  crsfbatt[4] = 0;
  crsfbatt[5] = 0; // capacity
  crsfbatt[6] = 0;
  crsfbatt[7] = 100; // Bat%

  crsf.queuePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_BATTERY_SENSOR, &crsfbatt, sizeof(crsfbatt));
}

void linkUp()
{
  digitalWrite(13, HIGH);

  tlmTimer.begin(&onTelemetry, 1000 * 500); // Send telemetry only twice per second
}

void linkDown()
{
  digitalWrite(13, LOW);

  tlmTimer.end();
}

void setup()
{
  pinMode(13, OUTPUT); // LED to show if CRSF is active

  sbus.begin();

  crsf.onLinkUp = &linkUp;
  crsf.onLinkDown = &linkDown;
  crsf.onPacketChannels = &packetChannels;

  Joystick.useManualSend(true);

  hidTimer.begin(&onInterval, (INTERVAL == 0 ? 1 : INTERVAL) * 1000);
  latencyTimer.begin(&induceLatency, 1000); // latency step is one millisecond
}

void loop()
{
  crsf.loop();

  if (!crsf.isLinkUp()) // fallback to SBUS
  {
    if (sbus.read(&ch_latency[LATENCY > 1 ? LATENCY - 1 : 0][0], &failSafe, &lostFrame))
    {
      setSticks(STARTPOINT, ENDPOINT);
      setButtons(STARTPOINT, ENDPOINT);
    }
  }
}