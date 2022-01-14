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

// Number of channels
#define CHANNELS 16

// Endpoints for SBUS, you might need to find your own values!
#define STARTPOINT 221 // 172 = FrSky, 221 = FlySky
#define ENDPOINT 1824  // 1811 = FrSky, 1824 = FlySky

// Endpoints for CrossFire
#define US_MIN 988
#define US_MAX 2011

SBUS sbus(Serial1);
static CrsfSerial crsf(Serial2, 115200);

uint16_t channels[CHANNELS];
uint16_t values[CHANNELS];

bool failSafe, lostFrame;

struct
{
  unsigned roll = 1500;
  unsigned pitch = 1500;
  unsigned yaw = 1500;
  unsigned throttle = 1500;
} sticks;

void setSticks(int _min, int _max)
{
  // Use Xrotate and 0-1024 if you use the normal layout in usb_desc.h
  Joystick.X(map(sticks.roll, _min, _max, 0, 65535));
  Joystick.Y(map(sticks.pitch, _min, _max, 0, 65535));
  Joystick.Z(map(sticks.throttle, _min, _max, 0, 65535));
  Joystick.Zrotate(map(sticks.yaw, _min, _max, 0, 65535));
}

void setButton(unsigned _button, unsigned _value)
{
  Joystick.button(_button * 3 + 1, _value == 0 ? 1 : 0);
  Joystick.button(_button * 3 + 2, _value == 1 ? 1 : 0);
  Joystick.button(_button * 3 + 3, _value == 2 ? 1 : 0);
}

static void packetChannels()
{
  sticks.roll = crsf.getChannel(1);
  sticks.pitch = crsf.getChannel(2);
  sticks.yaw = crsf.getChannel(3);
  sticks.throttle = crsf.getChannel(4);

  setSticks(US_MIN, US_MAX);

  for (unsigned _button = 0; _button <= (CHANNELS - 4) / 3; _button++)
  {
    setButton(_button, map(crsf.getChannel(5 + _button), US_MIN, US_MAX, 0, 2));
  }

  Joystick.send_now();
}

static void fakeVbatt()
{
  uint8_t crsfbatt[CRSF_FRAME_BATTERY_SENSOR_PAYLOAD_SIZE];

  crsfbatt[0] = 0;
  crsfbatt[1] = 50; // Fake 5v voltage
  crsfbatt[2] = 0;
  crsfbatt[3] = 0; // current
  crsfbatt[4] = 0;
  crsfbatt[5] = 0; // capacity
  crsfbatt[6] = 0;
  crsfbatt[7] = 100; // Bat%

  crsf.queuePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_BATTERY_SENSOR, &crsfbatt, sizeof(crsfbatt));
}

void linkUp()
{
  digitalWrite(13, HIGH);
}

void linkDown()
{
  digitalWrite(13, LOW);
}

void setup()
{
  pinMode(13, OUTPUT);

  sbus.begin();

  crsf.onLinkUp = &linkUp;
  crsf.onLinkDown = &linkDown;
  crsf.onPacketChannels = &packetChannels;

  Joystick.useManualSend(true);
}

void loop()
{
  crsf.loop();

  if (crsf.isLinkUp())
  {
    fakeVbatt();
  }
  else
  {
    if (sbus.read(&channels[0], &failSafe, &lostFrame))
    {
      sticks.roll = channels[0];
      sticks.pitch = channels[1];
      sticks.yaw = channels[2];
      sticks.throttle = channels[3];

      setSticks(STARTPOINT, ENDPOINT);

      for (unsigned _button = 0; _button <= (CHANNELS - 4) / 3; _button++)
      {
        setButton(_button, map(channels[4 + _button], STARTPOINT, ENDPOINT, 0, 2));
      }

      Joystick.send_now();
    }
  }
}
