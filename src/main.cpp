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
bool failSafe, lostFrame;

void setSticks(int _min = 1000, int _max = 2000)
{
  // Use Xrotate and 0-1024 if you use the normal layout in usb_desc.h
  Joystick.X(map(channels[0], _min, _max, 0, 65535));
  Joystick.Y(map(channels[1], _min, _max, 0, 65535));
  Joystick.Z(map(channels[2], _min, _max, 0, 65535));
  Joystick.Xrotate(map(channels[3], _min, _max, 0, 65535));

  Joystick.send_now();
}

void setButton(unsigned _button, int _min = 1000, int _max = 2000)
{
  for (unsigned _position = 0; _position < 3; _position++)
  {
    Joystick.button(_button * 3 + _position + 1, (unsigned)map(channels[4 + _button], _min, _max, 0, 2) == _position ? 1 : 0);
  }
}

static void packetChannels()
{
  for (unsigned _axis = 0; _axis <= 4; _axis++)
  {
    channels[_axis] = crsf.getChannel(_axis + 1);
  }

  setSticks(US_MIN, US_MAX);

  for (unsigned _button = 0; _button < (CHANNELS - 4); _button++)
  {
    channels[4 + _button] = crsf.getChannel(5 + _button);
    
    setButton(_button, US_MIN, US_MAX);
  }
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
      setSticks(STARTPOINT, ENDPOINT);

      for (unsigned _button = 0; _button < CHANNELS - 4; _button++)
      {
        setButton(_button, STARTPOINT, ENDPOINT);
      }
    }
  }
}