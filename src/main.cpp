/*
 * CRSF USB Joystick by Sjoer van der Ploeg
 * 
 * CrsfSerial from CapnBry: https://github.com/CapnBry/CRServoF/tree/master/lib/CrsfSerial
 */

#include <Arduino.h>
#include <CrsfSerial.h>

// Number of channels
#define CHANNELS 16

// Endpoints
#define US_MIN 988
#define US_MAX 2011

static CrsfSerial crsf(Serial2, 115200);

static void packetChannels()
{
  // Use Xrotate and 0-1024 if you use the normal layout in usb_desc.h
  Joystick.X(map(crsf.getChannel(1), US_MIN, US_MAX, 0, 65535));
  Joystick.Y(map(crsf.getChannel(2), US_MIN, US_MAX, 0, 65535));
  Joystick.Z(map(crsf.getChannel(3), US_MIN, US_MAX, 0, 65535));
  Joystick.Zrotate(map(crsf.getChannel(4), US_MIN, US_MAX, 0, 65535));

  for (unsigned int _button = 0; _button <= (CHANNELS - 4) / 3; _button++)
  {
    Joystick.button(_button * 3 + 1, map(crsf.getChannel(5 + _button), US_MIN, US_MAX, 0, 2) == 0 ? 1 : 0);
    Joystick.button(_button * 3 + 2, map(crsf.getChannel(5 + _button), US_MIN, US_MAX, 0, 2) == 1 ? 1 : 0);
    Joystick.button(_button * 3 + 3, map(crsf.getChannel(5 + _button), US_MIN, US_MAX, 0, 2) == 2 ? 1 : 0);
  }

  Joystick.send_now();
}

static void checkVbatt()
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

void setup()
{
  crsf.onPacketChannels = &packetChannels;
  Joystick.useManualSend(true);
}

void loop()
{
  crsf.loop();
  checkVbatt();
}
