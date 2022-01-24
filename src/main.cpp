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

// Receiver baud rate
#define BAUD 115200

// Latency testing, this will emulate different refresh rates and/or add latency between input and output.
// Values are milliseconds.
#define LATENCY 0  // increase to add latency, 0 means no latency and 255 is the maximum.
#define INTERVAL 1 // increase to change the refresh interval in milliseconds, USB won't go faster than 1ms anyway!

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
const uint8_t rebootcmd[] = {0xEC, 0x04, 0x32, 0x62, 0x6c, 0x0A};
const uint8_t crsfbatt[CRSF_FRAME_BATTERY_SENSOR_PAYLOAD_SIZE] = {0, 50, 0, 50, 0, 0, 0, 100}; // fake full 5v battery
const uint16_t hats[3] = {293, 338, 0};
uint16_t ch_latency[LATENCY + 1][CHANNELS];
uint32_t timing[3] = {0, 0, 0};
bool sbusStatus[2];

struct crsfSerialState
{
  char serialInBuff[64];
  uint8_t serialInBuffLen;
  bool serialEcho;
} crsfState;

void setSticks(uint16_t _min = 1000, uint16_t _max = 2000)
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

void setButton(uint8_t _button, uint16_t _min = 1000, uint16_t _max = 2000)
{
  for (uint8_t _position = 0; _position < 3; _position++)
  {
    Joystick.button(_button * 3 + _position + 1, (uint8_t)map(ch_latency[0][4 + _button], _min, _max, 0, 2) == _position ? true : false);
  }
}

void setButtons(uint16_t _min = 1000, uint16_t _max = 2000)
{
  for (uint8_t _button = 0; _button < (CHANNELS - 4); _button++)
  {
    setButton(_button, _min, _max);
  }
}

void packetChannels()
{
  for (uint8_t _channel = 0; _channel < CHANNELS; _channel++)
  {
    ch_latency[LATENCY][_channel] = crsf.getChannel(_channel + 1);
  }

  setSticks(US_MIN, US_MAX);
  setButtons(US_MIN, US_MAX);

  crsf.queuePacket(CRSF_SYNC_BYTE, CRSF_FRAMETYPE_BATTERY_SENSOR, &crsfbatt, sizeof(crsfbatt));
}

void induceLatency()
{
  for (uint8_t _bufs = 0; _bufs < LATENCY; _bufs++)
  {
    memcpy(ch_latency[_bufs], ch_latency[_bufs + 1], sizeof(uint16_t) * CHANNELS);
  }
}

void linkUp()
{
  digitalWrite(LED_BUILTIN, HIGH);
}

void linkDown()
{
  digitalWrite(LED_BUILTIN, LOW);
}

void crsfShiftyByte(uint8_t _byte)
{
  // Serial data returned from the receiver, status messages from ELRS and the ESP in passthrough
  if (crsf.getPassthroughMode())
    Serial.write(_byte);
}

static bool handleSerialCommand(char *cmd)
{
  // Fake a CRSF RX on UART6
  bool prompt = true;
  if (strcmp(cmd, "#") == 0)
  {
    Serial.println("Fake CLI Mode, type 'exit' or 'help' to do nothing\r\n");
    crsfState.serialEcho = true;
  }

  else if (strcmp(cmd, "serial") == 0)
    Serial.println("serial 5 64 0 0 0 0\r\n");

  else if (strcmp(cmd, "get serialrx_provider") == 0)
    Serial.println("serialrx_provider = CRSF\r\n");

  else if (strcmp(cmd, "get serialrx_inverted") == 0)
    Serial.println("serialrx_inverted = OFF\r\n");

  else if (strcmp(cmd, "get serialrx_halfduplex") == 0)
    Serial.println("serialrx_halfduplex = OFF\r\n");

  else if (strncmp(cmd, "serialpassthrough 5 ", 20) == 0)
  {
    Serial.println("Passthrough serial 5");
    // Force a reboot command since we want to send the reboot
    // at 420000 then switch to what the user wanted
    crsf.write(rebootcmd, sizeof(rebootcmd));

    unsigned int baud = atoi(cmd + 20);
    crsf.setPassthroughMode(true, baud);
    crsfState.serialEcho = false;
    return false;
  }

  else
    prompt = false;

  if (prompt)
    Serial.print("# ");

  return true;
}

static void checkSerialInPassthrough()
{
  static uint32_t lastData = 0;
  // static bool LED = false;
  bool gotData = false;

  // Simple data passthrough from in to crsf
  unsigned int avail;
  while ((avail = Serial.available()) != 0)
  {
    uint8_t buf[16];
    avail = Serial.readBytes((char *)buf, min(sizeof(buf), avail));
    crsf.write(buf, avail);
    // digitalWrite(DPIN_LED, LED);
    // LED = !LED;
    gotData = true;
  }

  // If longer than X seconds since last data, switch out of passthrough
  if (gotData || !lastData)
    lastData = millis();
  else if (millis() - lastData > 10000)
  {
    lastData = 0;
    // digitalWrite(DPIN_LED, HIGH ^ LED_INVERTED);
    // delay(250);
    // digitalWrite(DPIN_LED, LOW ^ LED_INVERTED);
    //crsf.write(rebootcmd, sizeof(rebootcmd));
    crsf.setPassthroughMode(false);
    //crsf.setPassthroughMode(false, 115200);
  }
}

static void checkSerialInNormal()
{
  while (Serial.available())
  {
    char c = Serial.read();
    if (crsfState.serialEcho && c != '\n')
      Serial.write(c);

    if (c == '\r' || c == '\n')
    {
      if (crsfState.serialInBuffLen != 0)
      {
        Serial.write('\n');
        Serial.flush();
        crsfState.serialInBuff[crsfState.serialInBuffLen] = '\0';
        handleSerialCommand(crsfState.serialInBuff);
        crsfState.serialInBuffLen = 0;
      }
    }
    else
    {
      crsfState.serialInBuff[crsfState.serialInBuffLen++] = c;
      // if the buffer fills without getting a newline, just reset
      if (crsfState.serialInBuffLen >= sizeof(crsfState.serialInBuff))
        crsfState.serialInBuffLen = 0;
    }
  } /* while Serial */
}

void checkSerialIn()
{
  if (crsf.getPassthroughMode())
  {
    checkSerialInPassthrough();
  }
  else
  {
    checkSerialInNormal();
  }
}

void setup()
{
  for (uint16_t _x = 0; _x <= LATENCY; _x++)
  {
    for (uint8_t _y = 0; _y < CHANNELS; _y++)
    {
      ch_latency[_x][_y] = 0; // pre-allocate the array with zeros
    }
  }

  pinMode(LED_BUILTIN, OUTPUT); // LED to show if CRSF is active

  Serial.begin(115200);

  sbus.begin();

  crsf.onLinkUp = &linkUp;
  crsf.onLinkDown = &linkDown;
  crsf.onShiftyByte = &crsfShiftyByte;
  crsf.onPacketChannels = &packetChannels;

  // crsf.write(rebootcmd, sizeof(rebootcmd));
  // crsf.setPassthroughMode(false);

  Joystick.useManualSend(true);
}

void loop()
{
  crsf.loop();

  if (crsf.getPassthroughMode())
  {
    if (millis() - timing[0] >= 1000)
    {
      timing[0] = millis();
    }

    digitalWrite(LED_BUILTIN, millis() - timing[0] < 500 ? HIGH : LOW);
  }
  else
  {
    if (!crsf.isLinkUp()) // fallback to SBUS
    {
      digitalWrite(LED_BUILTIN, LOW);

      if (sbus.read(&ch_latency[LATENCY][0], &sbusStatus[0], &sbusStatus[1]))
      {
        setSticks(STARTPOINT, ENDPOINT);
        setButtons(STARTPOINT, ENDPOINT);
      }
    }

    if (LATENCY > 0 && micros() - timing[1] >= 1000)
    {
      timing[1] = micros();

      induceLatency();
    }

    if (micros() - timing[2] >= (INTERVAL == 0 ? 1 : INTERVAL) * 1000)
    {
      timing[2] = micros();

      Joystick.send_now();
    }
  }

  checkSerialIn();
}