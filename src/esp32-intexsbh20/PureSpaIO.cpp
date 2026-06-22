/*
 * project:  Intex PureSpa WiFi Controller
 *
 * file:     PureSpaIO.cpp
 *
 * encoding: UTF-8
 * created:  14th March 2021
 *
 * Copyright (C) 2021 Jens B.
 *
 * Enhanced and maintained by Petr Kašpar (27 March 2026).
 * https://github.com/caspercze
 *
 * Receive data handling based on code from:
 *
 * DIYSCIP <https://github.com/yorffoeg/diyscip> (c) by Geoffroy HUBERT - yorffoeg@gmail.com
 *
 * DIYSCIP is licensed under a
 * Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 * You should have received a copy of the license along with this
 * work. If not, see <https://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 * DIYSCIP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY.
 *
 * SPDX-License-Identifier: CC-BY-NC-SA-4.0
 *
 */

#include "PureSpaIO.h"
#include "driver/gpio.h"

#ifdef CUSTOM_MODEL_NAME
const char* const MODEL_NAME = CUSTOM_MODEL_NAME;
#else
const char* const MODEL_NAME = CONFIG::DEFAULT_MODEL_NAME;
#endif

namespace FRAME_LED_SB
{
  const uint16 POWER          = 0x0001;
  const uint16 HEATER_ON      = 0x0080;  // max. 72 h, will start filter, will not stop filter
  const uint16 NO_BEEP        = 0x0100;
  const uint16 HEATER_STANDBY = 0x0200;
  const uint16 BUBBLE         = 0x0400;  // max. 30 min
  const uint16 FILTER         = 0x1000;  // max. 24 h
}

namespace FRAME_LED_SJBHS
{
  const uint16 POWER          = 0x0001;
  const uint16 BUBBLE         = 0x0002;  // max. 30 min
  const uint16 HEATER_ON      = 0x0080;  // max. 72 h, will start filter, will not stop filter
  const uint16 NO_BEEP        = 0x0100;
  const uint16 HEATER_STANDBY = 0x0200;
  const uint16 JET            = 0x0400;
  const uint16 FILTER         = 0x1000;  // max. 24 h
  const uint16 DISINFECTION   = 0x2000;  // max. 8 h
}

// bit mask of button frames
namespace FRAME_BUTTON_SB
{
  const uint16 FILTER    = 0x0002;
  const uint16 BUBBLE    = 0x0008;
  const uint16 TEMP_DOWN = 0x0080;
  const uint16 POWER     = 0x0400;
  const uint16 TEMP_UP   = 0x1000;
  const uint16 TEMP_UNIT = 0x2000;
  const uint16 HEATER    = 0x8000;
}

namespace FRAME_BUTTON_SJBHS
{
  const uint16 DISINFECTION = 0x0001;
  const uint16 BUBBLE       = 0x0002;
  const uint16 JET          = 0x0008;
  const uint16 FILTER       = 0x0080;
  const uint16 TEMP_DOWN    = 0x0200;
  const uint16 POWER        = 0x0400;
  const uint16 TEMP_UP      = 0x1000;
  const uint16 TEMP_UNIT    = 0x2000;
  const uint16 HEATER       = 0x8000;
}

namespace FRAME_DIGIT
{
  // bit mask of 7-segment display selector
  const uint16 POS_1 = 0x0040;
  const uint16 POS_2 = 0x0020;
  const uint16 POS_3 = 0x0800;
  const uint16 POS_4 = 0x0004;

  // bit mask of 7-segment display element
  const uint16 SEGMENT_A  = 0x2000;
  const uint16 SEGMENT_B  = 0x1000;
  const uint16 SEGMENT_C  = 0x0200;
  const uint16 SEGMENT_D  = 0x0400;
  const uint16 SEGMENT_E  = 0x0080;
  const uint16 SEGMENT_F  = 0x0008;
  const uint16 SEGMENT_G  = 0x0010;
  const uint16 SEGMENT_DP = 0x8000;
  const uint16 SEGMENTS   = SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_E | SEGMENT_F | SEGMENT_G;

  // bit mask of human readable value on 7-segment display
  const uint16 OFF   = 0x0000;
  const uint16 NUM_0 = SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_E | SEGMENT_F;
  const uint16 NUM_1 = SEGMENT_B | SEGMENT_C;
  const uint16 NUM_2 = SEGMENT_A | SEGMENT_B | SEGMENT_G | SEGMENT_E | SEGMENT_D;
  const uint16 NUM_3 = SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_G;
  const uint16 NUM_4 = SEGMENT_F | SEGMENT_G | SEGMENT_B | SEGMENT_C;
  const uint16 NUM_5 = SEGMENT_A | SEGMENT_F | SEGMENT_G | SEGMENT_C | SEGMENT_D;
  const uint16 NUM_6 = SEGMENT_A | SEGMENT_F | SEGMENT_E | SEGMENT_D | SEGMENT_C | SEGMENT_G;
  const uint16 NUM_7 = SEGMENT_A | SEGMENT_B | SEGMENT_C;
  const uint16 NUM_8 = SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_E | SEGMENT_F | SEGMENT_G;
  const uint16 NUM_9 = SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_F | SEGMENT_G;
  const uint16 LET_A = SEGMENT_E | SEGMENT_F | SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_G;
  const uint16 LET_C = SEGMENT_A | SEGMENT_F | SEGMENT_E | SEGMENT_D;
  const uint16 LET_D = SEGMENT_B | SEGMENT_C | SEGMENT_D | SEGMENT_E | SEGMENT_G;
  const uint16 LET_E = SEGMENT_A | SEGMENT_F | SEGMENT_E | SEGMENT_D | SEGMENT_G;
  const uint16 LET_F = SEGMENT_E | SEGMENT_F | SEGMENT_A | SEGMENT_G;
  const uint16 LET_H = SEGMENT_B | SEGMENT_C | SEGMENT_E | SEGMENT_F | SEGMENT_G;
  const uint16 LET_N = SEGMENT_A | SEGMENT_B | SEGMENT_C | SEGMENT_E | SEGMENT_F;
}

// frame type markers
namespace FRAME_TYPE
{
  const uint16 CUE    = 0x0100;
  const uint16 LED    = 0x4000;
  const uint16 DIGIT  = FRAME_DIGIT::POS_1 | FRAME_DIGIT::POS_2 | FRAME_DIGIT::POS_3 | FRAME_DIGIT::POS_4;

  const uint16 BUTTON =
    CUE |
    FRAME_BUTTON_SB::POWER |
    FRAME_BUTTON_SB::FILTER |
    FRAME_BUTTON_SB::HEATER |
    FRAME_BUTTON_SB::BUBBLE |
    FRAME_BUTTON_SB::TEMP_UP |
    FRAME_BUTTON_SB::TEMP_DOWN |
    FRAME_BUTTON_SB::TEMP_UNIT |
    FRAME_BUTTON_SJBHS::FILTER |
    FRAME_BUTTON_SJBHS::BUBBLE |
    FRAME_BUTTON_SJBHS::TEMP_DOWN |
    FRAME_BUTTON_SJBHS::DISINFECTION |
    FRAME_BUTTON_SJBHS::JET;
}

namespace DIGIT
{
  // 7-segment display update control
  const uint8 POS_1     = 0x8;
  const uint8 POS_2     = 0x4;
  const uint8 POS_3     = 0x2;
  const uint8 POS_4     = 0x1;
  const uint8 POS_1_2   = POS_1 | POS_2;
  const uint8 POS_1_2_3 = POS_1 | POS_2 | POS_3;
  const uint8 POS_ALL   = POS_1 | POS_2 | POS_3 | POS_4;

  // ASCII values used to map non-numeric states of the 7-segment display
  const char OFF = ' ';
};

namespace ERROR
{
  // human readable error on display
  const char CODE_90[]    PROGMEM = "E90";
  const char CODE_91[]    PROGMEM = "E91";
  const char CODE_92[]    PROGMEM = "E92";
  const char CODE_94[]    PROGMEM = "E94";
  const char CODE_95[]    PROGMEM = "E95";
  const char CODE_96[]    PROGMEM = "E96";
  const char CODE_97[]    PROGMEM = "E97";
  const char CODE_99[]    PROGMEM = "E99";
  const char CODE_END[]   PROGMEM = "END";
  const char CODE_OTHER[] PROGMEM = "EXX";

  const unsigned int COUNT = 9;

  // English error messages
  const char EN_90[]    PROGMEM = "no water flow";
  const char EN_91[]    PROGMEM = "salt level too low";
  const char EN_92[]    PROGMEM = "salt level too high";
  const char EN_94[]    PROGMEM = "water temp too low";
  const char EN_95[]    PROGMEM = "water temp too high";
  const char EN_96[]    PROGMEM = "system error";
  const char EN_97[]    PROGMEM = "dry fire protection";
  const char EN_99[]    PROGMEM = "water temp sensor error";
  const char EN_END[]   PROGMEM = "heating aborted after 72h";
  const char EN_OTHER[] PROGMEM = "error";

  // German error messages
  const char DE_90[]    PROGMEM = "kein Wasserdurchfluss";
  const char DE_91[]    PROGMEM = "niedriges Salzniveau";
  const char DE_92[]    PROGMEM = "hohes Salzniveau";
  const char DE_94[]    PROGMEM = "Wassertemperatur zu niedrig";
  const char DE_95[]    PROGMEM = "Wassertemperatur zu hoch";
  const char DE_96[]    PROGMEM = "Systemfehler";
  const char DE_97[]    PROGMEM = "Trocken-Brandschutz";
  const char DE_99[]    PROGMEM = "Wassertemperatursensor defekt";
  const char DE_END[]   PROGMEM = "Heizbetrieb nach 72 h deaktiviert";
  const char DE_OTHER[] PROGMEM = "Störung";

  // Czech error messages
  const char CZ_90[]    PROGMEM = "žádný průtok vody";
  const char CZ_91[]    PROGMEM = "nízká hladina soli";
  const char CZ_92[]    PROGMEM = "vysoká hladina soli";
  const char CZ_94[]    PROGMEM = "teplota vody příliš nízká";
  const char CZ_95[]    PROGMEM = "teplota vody příliš vysoká";
  const char CZ_96[]    PROGMEM = "systémová chyba";
  const char CZ_97[]    PROGMEM = "ochrana proti chodu nasucho";
  const char CZ_99[]    PROGMEM = "chyba čidla teploty vody";
  const char CZ_END[]   PROGMEM = "vytápění ukončeno po 72 h";
  const char CZ_OTHER[] PROGMEM = "chyba";

  const char* const TEXT[4][COUNT + 1] PROGMEM = {
    { CODE_90, CODE_91, CODE_92, CODE_94, CODE_95, CODE_96, CODE_97, CODE_99, CODE_END, CODE_OTHER },
    { EN_90,   EN_91,   EN_92,   EN_94,   EN_95,   EN_96,   EN_97,   EN_99,   EN_END,   EN_OTHER },
    { DE_90,   DE_91,   DE_92,   DE_94,   DE_95,   DE_96,   DE_97,   DE_99,   DE_END,   DE_OTHER },
    { CZ_90,   CZ_91,   CZ_92,   CZ_94,   CZ_95,   CZ_96,   CZ_97,   CZ_99,   CZ_END,   CZ_OTHER }
  };
}

// special display values
inline char display2LastDigit(uint32 v) { return (v >> 24) & 0xFFU; }
inline uint16 display2Num(uint32 v)     { return (((v & 0xFFU) - '0')*100) + ((((v >> 8) & 0xFFU) - '0')*10) + (((v >> 16) & 0xFFU) - '0'); }
inline uint32 display2Error(uint32 v)   { return v & 0x00FFFFFFU; }
inline bool displayIsTemp(uint32 v)     { return display2LastDigit(v) == 'C' || display2LastDigit(v) == 'F'; }
inline bool displayIsTime(uint32 v)     { return display2LastDigit(v) == 'H'; }
inline bool displayIsError(uint32 v)    { return (v & 0xFFU) == 'E'; }
inline bool displayIsBlank(uint32 v)    { return (v & 0x00FFFFFFU) == (' ' << 16) + (' ' << 8) + ' '; }

static int g_lastKnownSetTemp = PureSpaIO::UNDEF::INT;
static int g_lastAcceptedDesiredFromIsrC = PureSpaIO::UNDEF::INT;
static unsigned long g_lastTempUiActionTime = 0;
static int g_lastTempUiActionDirection = 0; // +1 = up, -1 = down, 0 = unknown

PureSpaDebugStats g_dbgStats;
bool g_dbgStatsReady = false;

static constexpr unsigned int spaBusFramesForWallMs(unsigned long ms)
{
  return (unsigned int)((ms * 34ULL) / 21ULL);
}

static constexpr unsigned long TEMP_UI_WATER_SUPPRESS_MS = 3000;

static constexpr unsigned long POST_BLINK_WATER_SUPPRESS_MS = 1000;

static constexpr unsigned long ERROR_CLEAR_HOLD_MS = 10000;

// How long after a temp up/down button press the blinking display is still
// considered to be showing the new SETPOINT, not the actual water temp (see
// recentTempUiAction usage in decodeDisplay() and shouldRejectBlinkAsSetpoint()).
static constexpr unsigned long RECENT_TEMP_UI_ACTION_MS = 12000;

static volatile unsigned int g_errorClearOkStartFrame = 0;
static volatile unsigned int g_lastTempUiActionFrame = 0;
static volatile unsigned int g_lastBlinkEndedFrame = 0;

static volatile unsigned int g_lastDesiredBusRawChangeFrame = 0;

static unsigned long g_lastGenericCommandMs = 0;
static unsigned long g_lastPowerCommandMs   = 0;

static void waitCommandCooldown(unsigned long& stamp, unsigned long cooldownMs)
{
  unsigned long now = millis();
  if (stamp != 0)
  {
    unsigned long elapsed = timeDiff(now, stamp);
    if (elapsed < cooldownMs)
    {
      delay(cooldownMs - elapsed);
      yield();
    }
  }
}

static void markCommandTime(unsigned long& stamp)
{
  stamp = millis();
}

static int displayTempToCelsiusRaw(uint32 value)
{
  int celsiusValue = display2Num(value);
  char tempUnit = display2LastDigit(value);

  if (tempUnit == 'F')
  {
    float fValue = (float)celsiusValue;
    celsiusValue = (int)round(((fValue - 32) * 5) / 9);
  }
  else if (tempUnit != 'C')
  {
    celsiusValue = PureSpaIO::UNDEF::INT;
  }

  return (celsiusValue >= 0) && (celsiusValue <= 60) ? celsiusValue : PureSpaIO::UNDEF::INT;
}

// Blinking-display path sometimes shows the same value as actual water temperature.
// Treating that as a setpoint update makes HA climate jump target to current temp.
static bool shouldRejectBlinkAsSetpoint(uint32 blinkRaw, int prevDesiredC, int waterC,
                                        unsigned int frameNow)
{
  const int b = displayTempToCelsiusRaw(blinkRaw);
  if (b == PureSpaIO::UNDEF::INT)
  {
    return false;
  }

  const bool recentUiAction =
    (g_lastTempUiActionFrame != 0)
    && diff(frameNow, g_lastTempUiActionFrame) <= spaBusFramesForWallMs(RECENT_TEMP_UI_ACTION_MS);

  const bool inPostBlink =
    (g_lastBlinkEndedFrame != 0)
    && diff(frameNow, g_lastBlinkEndedFrame) <= spaBusFramesForWallMs(4000);

  if (waterC != PureSpaIO::UNDEF::INT && b == waterC)
  {
    if (g_lastAcceptedDesiredFromIsrC != PureSpaIO::UNDEF::INT
        && g_lastAcceptedDesiredFromIsrC != waterC
        && (recentUiAction || inPostBlink))
    {
      return true;
    }

    if (prevDesiredC != PureSpaIO::UNDEF::INT
        && prevDesiredC != waterC
        && (recentUiAction || inPostBlink))
    {
      return true;
    }
  }

  if (recentUiAction)
  {
    if (prevDesiredC != PureSpaIO::UNDEF::INT)
    {
      if (abs(prevDesiredC - b) > 4)
      {
        return true;
      }

      // Reject if the value does not move in the direction of the last button press.
      // Prevents the old setpoint from oscillating with the new one when the SB-H20
      // display briefly re-shows the previous value during a transition.
      if (g_lastTempUiActionDirection > 0 && b <= prevDesiredC)
      {
        return true;
      }
      if (g_lastTempUiActionDirection < 0 && b >= prevDesiredC)
      {
        return true;
      }
    }

    return false;
  }

  if (waterC != PureSpaIO::UNDEF::INT && b == waterC)
  {
    return true;
  }

  if (waterC != PureSpaIO::UNDEF::INT && b != waterC)
  {
    return false;
  }

  const int maxFineStepC = inPostBlink ? 3 : 6;

  int refForGap = prevDesiredC;
  if (prevDesiredC != PureSpaIO::UNDEF::INT && g_lastAcceptedDesiredFromIsrC != PureSpaIO::UNDEF::INT)
  {
    if (waterC != PureSpaIO::UNDEF::INT && prevDesiredC == waterC
        && abs(g_lastAcceptedDesiredFromIsrC - waterC) > 3)
    {
      refForGap = g_lastAcceptedDesiredFromIsrC;
    }
    else if (waterC == PureSpaIO::UNDEF::INT && prevDesiredC == b
             && abs(g_lastAcceptedDesiredFromIsrC - b) > 3)
    {
      refForGap = g_lastAcceptedDesiredFromIsrC;
    }
  }

  if (prevDesiredC == PureSpaIO::UNDEF::INT)
  {
    return false;
  }

  return abs(refForGap - b) > maxFineStepC;
}

volatile PureSpaIO::State PureSpaIO::state;
volatile PureSpaIO::IsrState PureSpaIO::isrState;
volatile PureSpaIO::Buttons PureSpaIO::buttons;
volatile PureSpaIO::MODEL PureSpaIO::model = PureSpaIO::MODEL::SBH20;

// @TODO detect when latch signal stays low
// @TODO detect act temp change during error
// @TODO improve reliability of water temp change (counter auto repeat and too short press)
void PureSpaIO::setup(LANG language)
{
  this->language = language;

  state.error = ERROR_NONE;
  g_errorClearOkStartFrame = 0;

  pinMode(PIN::CLOCK, INPUT);
  pinMode(PIN::DATA,  INPUT);
  pinMode(PIN::LATCH, INPUT);

  attachInterruptArg(digitalPinToInterrupt(PIN::CLOCK), PureSpaIO::clockRisingISR, this, RISING);
}

PureSpaIO::MODEL PureSpaIO::getModel() const
{
  return model;
}

const char* PureSpaIO::getModelName() const
{
  return MODEL_NAME;
}

void PureSpaIO::loop()
{
  unsigned long now = millis();
  if (state.stateUpdated)
  {
    lastStateUpdateTime = now;
    state.online = true;
    state.stateUpdated = false;
  }
  else if (timeDiff(now, lastStateUpdateTime) > CYCLE::RECEIVE_TIMEOUT)
  {
    state.online = false;
  }
}

bool PureSpaIO::isOnline() const
{
  return state.online;
}

unsigned int PureSpaIO::getTotalFrames() const
{
  return state.frameCounter;
}

unsigned int PureSpaIO::getDroppedFrames() const
{
  return state.frameDropped;
}

int PureSpaIO::getActWaterTempCelsius() const
{
  return (state.waterTemp != UNDEF::UINT) ? convertDisplayToCelsius(state.waterTemp) : UNDEF::INT;
}

int PureSpaIO::getDesiredWaterTempCelsius() const
{
  int v = (state.desiredTemp != UNDEF::UINT) ? convertDisplayToCelsius(state.desiredTemp) : UNDEF::INT;
  if (v != UNDEF::INT)
  {
    g_lastKnownSetTemp = v;
  }
  return v;
}

int PureSpaIO::getDisinfectionTime() const
{
  return isDisinfectionOn() ? (state.disinfectionTime != UNDEF::UINT ? display2Num(state.disinfectionTime) : UNDEF::INT) : 0;
}

void PureSpaIO::markTempDisplayDisturbance()
{
  const unsigned long t = millis();
  lastTempUiActionTime = t;
  g_lastTempUiActionTime = t;
  g_lastTempUiActionFrame = state.frameCounter;
}

void PureSpaIO::setAuthoritativeDesiredCelsius(int celsius)
{
  if (celsius >= WATER_TEMP::SET_MIN && celsius <= WATER_TEMP::SET_MAX)
  {
    g_lastAcceptedDesiredFromIsrC = celsius;
    g_lastKnownSetTemp = celsius;
    g_lastDesiredBusRawChangeFrame = state.frameCounter;
  }
}

int PureSpaIO::getAuthoritativeDesiredWaterTempCelsius() const
{
  return (g_lastAcceptedDesiredFromIsrC != UNDEF::INT) ? g_lastAcceptedDesiredFromIsrC : UNDEF::INT;
}

bool PureSpaIO::isActWaterTempStable() const
{
  return true;
}

String PureSpaIO::getErrorCode() const
{
  memcpy((void*)errorBuffer, (void*)&state.error, 4);
  return errorBuffer;
}

String PureSpaIO::getErrorMessage(const String& errorCode) const
{
  if (errorCode.length())
  {
    unsigned int errorIndex = UINT_MAX;
    for (unsigned int i=0; i<ERROR::COUNT; i++)
    {
      String ec = FPSTR(ERROR::TEXT[(unsigned int)LANG::CODE][i]);
      if (errorCode == ec)
      {
        errorIndex = i;
        break;
      }
    }

    if (errorIndex != UINT_MAX)
    {
      return FPSTR(ERROR::TEXT[(unsigned int)language][errorIndex]);
    }
    else
    {
      return errorCode;
    }
  }
  else
  {
    return errorCode;
  }
}

unsigned int PureSpaIO::getRawLedValue() const
{
  return (state.ledStatus != PureSpaIO::UNDEF::USHORT) ? state.ledStatus : UNDEF::USHORT;
}

uint8 PureSpaIO::isPowerOn() const
{
  const uint16 mask = (model == MODEL::SBH20) ? FRAME_LED_SB::POWER : FRAME_LED_SJBHS::POWER;
  return (state.ledStatus != PureSpaIO::UNDEF::USHORT) ? ((state.ledStatus & mask) != 0) : UNDEF::BOOL;
}

uint8 PureSpaIO::isFilterOn() const
{
  const uint16 mask = (model == MODEL::SBH20) ? FRAME_LED_SB::FILTER : FRAME_LED_SJBHS::FILTER;
  return (state.ledStatus != PureSpaIO::UNDEF::USHORT) ? ((state.ledStatus & mask) != 0) : UNDEF::BOOL;
}

uint8 PureSpaIO::isBubbleOn() const
{
  const uint16 mask = (model == MODEL::SBH20) ? FRAME_LED_SB::BUBBLE : FRAME_LED_SJBHS::BUBBLE;
  return (state.ledStatus != PureSpaIO::UNDEF::USHORT) ? ((state.ledStatus & mask) != 0) : UNDEF::BOOL;
}

uint8 PureSpaIO::isHeaterOn() const
{
  const uint16 mask = (model == MODEL::SBH20)
    ? (FRAME_LED_SB::HEATER_ON | FRAME_LED_SB::HEATER_STANDBY)
    : (FRAME_LED_SJBHS::HEATER_ON | FRAME_LED_SJBHS::HEATER_STANDBY);
  return (state.ledStatus != PureSpaIO::UNDEF::USHORT) ? ((state.ledStatus & mask) != 0) : UNDEF::BOOL;
}

uint8 PureSpaIO::isHeaterStandby() const
{
  const uint16 mask = (model == MODEL::SBH20) ? FRAME_LED_SB::HEATER_STANDBY : FRAME_LED_SJBHS::HEATER_STANDBY;
  return (state.ledStatus != PureSpaIO::UNDEF::USHORT) ? ((state.ledStatus & mask) != 0) : UNDEF::BOOL;
}

uint8 PureSpaIO::isBuzzerOn() const
{
  const uint16 mask = (model == MODEL::SBH20) ? FRAME_LED_SB::NO_BEEP : FRAME_LED_SJBHS::NO_BEEP;
  return (state.ledStatus != PureSpaIO::UNDEF::USHORT) ? ((state.ledStatus & mask) == 0) : UNDEF::BOOL;
}

uint8 PureSpaIO::isDisinfectionOn() const
{
  if (model != MODEL::SJBHS) return false;
  const uint16 mask = FRAME_LED_SJBHS::DISINFECTION;
  return (state.ledStatus != PureSpaIO::UNDEF::USHORT) ? ((state.ledStatus & mask) != 0) : UNDEF::BOOL;
}

uint8 PureSpaIO::isJetOn() const
{
  if (model != MODEL::SJBHS) return false;
  const uint16 mask = FRAME_LED_SJBHS::JET;
  return (state.ledStatus != PureSpaIO::UNDEF::USHORT) ? ((state.ledStatus & mask) != 0) : UNDEF::BOOL;
}

void PureSpaIO::setDesiredWaterTempCelsius(int temp)
{
  if (temp < WATER_TEMP::SET_MIN || temp > WATER_TEMP::SET_MAX)
  {
    return;
  }

  if (!isPowerOn() || state.error != ERROR_NONE)
  {
    return;
  }

  markTempDisplayDisturbance();

#ifdef FORCE_WIFI_SLEEP
  wifiForceSleepBeginCompat();
#endif

  int setTemp = getDesiredWaterTempCelsius();

  // Prefer desired setpoint if available.
  // If not, use last known setpoint, then fall back to actual water temp.
  if (setTemp == UNDEF::INT && g_lastKnownSetTemp != UNDEF::INT)
  {
    setTemp = g_lastKnownSetTemp;
  }
  if (setTemp == UNDEF::INT)
  {
    setTemp = getActWaterTempCelsius();
  }

  // If still unknown, do a safe mechanical "wake" step up once,
  // then try to read the setpoint again.
  if (setTemp == UNDEF::INT)
  {
    // 1) First, try to enter the setpoint display mode without changing it.
    // requestSetTempDisplay() pushes the UI into the setpoint view.
    (void)requestSetTempDisplay();

    const int INIT_CONFIRM_DELAY_MS = 150;
    const int INIT_CONFIRM_TRIES = 20;
    int candidate = UNDEF::INT;
    for (int tries = 0; tries < INIT_CONFIRM_TRIES && candidate == UNDEF::INT; ++tries)
    {
      delay(INIT_CONFIRM_DELAY_MS);
      yield();
      candidate = getDesiredWaterTempCelsius();
    }

    if (candidate == UNDEF::INT)
    {
      // 2) Still unknown: choose a direction using actual water temp when possible.
      // This avoids always doing "up" (which could overshoot when user wants to lower).
      int act = getActWaterTempCelsius();
      const int direction = (act != UNDEF::INT) ? (temp >= act ? 1 : -1) : 1;

      const bool clickOk = changeWaterTemp(direction);
      if (!clickOk)
      {
#ifdef FORCE_WIFI_SLEEP
        wifiForceSleepWakeCompat();
#endif
        DEBUG_MSG("set temp wake click failed\n");
        return;
      }

      for (int tries = 0; tries < INIT_CONFIRM_TRIES && candidate == UNDEF::INT; ++tries)
      {
        delay(INIT_CONFIRM_DELAY_MS);
        yield();
        candidate = getDesiredWaterTempCelsius();
      }
    }

    if (candidate == UNDEF::INT)
    {
#ifdef FORCE_WIFI_SLEEP
      wifiForceSleepWakeCompat();
#endif
      DEBUG_MSG("set temp unavailable\n");
      return;
    }

    setTemp = candidate;
    g_lastKnownSetTemp = setTemp;
  }

  int deltaTemp = temp - setTemp;

  while (deltaTemp != 0)
  {
    feedWatchdog();

    const int direction = (deltaTemp > 0) ? 1 : -1;
    const int previousSetTemp = setTemp;

    const unsigned int CONFIRM_DELAY_MS = 150;
    const int CONFIRM_TRIES = 20;

    auto confirmSetpointChange = [&](int& outSetTemp) -> bool
    {
      outSetTemp = UNDEF::INT;
      for (int tries = 0; tries < CONFIRM_TRIES; ++tries)
      {
        delay(CONFIRM_DELAY_MS);
        yield();

        int candidate = getDesiredWaterTempCelsius();
        g_dbgStats.pollsDone = tries + 1;
        g_dbgStats.lastCandidate = candidate;
        if (isrState.isDisplayBlinking) g_dbgStats.blinkingEver = true;
        if ((int)isrState.stableBlinkingWaterTempCount > g_dbgStats.maxBlinkCnt)
          g_dbgStats.maxBlinkCnt = (int)isrState.stableBlinkingWaterTempCount;
        g_dbgStats.latestBlink = isrState.latestBlinkingTemp;

        DEBUG_MSG("cSPC t=%d cand=%d prev=%d blink=%d stBlnk=%u latBlink=%08X stBlinkCnt=%u\n",
          tries, candidate, previousSetTemp,
          (int)isrState.isDisplayBlinking,
          isrState.stableDisplayBlankCount,
          (unsigned)isrState.latestBlinkingTemp,
          isrState.stableBlinkingWaterTempCount);

        if (candidate == UNDEF::INT)
        {
          continue;
        }

        if (candidate != previousSetTemp)
        {
          outSetTemp = candidate;
          return true;
        }
      }

      return false;
    };

    int newSetTemp = UNDEF::INT;

    g_dbgStats = {};
    g_dbgStats.prevSetTemp  = previousSetTemp;
    g_dbgStats.direction    = direction;
    g_dbgStatsReady         = false;

    DEBUG_MSG("sDWT step prev=%d dir=%d blink=%d stBlnk=%u latBlink=%08X actFrm=%u uiActFrm=%u\n",
      previousSetTemp, direction,
      (int)isrState.isDisplayBlinking,
      isrState.stableDisplayBlankCount,
      (unsigned)isrState.latestBlinkingTemp,
      state.frameCounter,
      g_lastTempUiActionFrame);

    bool clickOk = changeWaterTemp(direction);
    g_dbgStats.clickOk    = clickOk;
    g_dbgStats.latestBlink= isrState.latestBlinkingTemp;
    DEBUG_MSG("cWT click=%d blink=%d latBlink=%08X\n",
      clickOk,
      (int)isrState.isDisplayBlinking,
      (unsigned)isrState.latestBlinkingTemp);

    bool confirmed = clickOk ? confirmSetpointChange(newSetTemp) : false;
    if (!confirmed)
    {
      DEBUG_MSG("retry cWT\n");
      clickOk = changeWaterTemp(direction);
      DEBUG_MSG("cWT retry click=%d\n", clickOk);
      confirmed = clickOk ? confirmSetpointChange(newSetTemp) : false;
    }

    g_dbgStats.confirmed = confirmed;
    g_dbgStatsReady = true;

    if (!confirmed)
    {
      DEBUG_MSG("set temp confirm failed, aborting\n");
      break;
    }

    if (newSetTemp < WATER_TEMP::SET_MIN)
    {
      newSetTemp = WATER_TEMP::SET_MIN;
    }
    else if (newSetTemp > WATER_TEMP::SET_MAX)
    {
      newSetTemp = WATER_TEMP::SET_MAX;
    }

    setTemp = newSetTemp;
    g_lastKnownSetTemp = setTemp;
    deltaTemp = temp - setTemp;
  }

  if (setTemp != UNDEF::INT)
  {
    g_lastKnownSetTemp = setTemp;
    g_lastAcceptedDesiredFromIsrC = setTemp;
  }

#ifdef FORCE_WIFI_SLEEP
  wifiForceSleepWakeCompat();
#endif
}

void PureSpaIO::setDisinfectionTime(int hours)
{
  if (hours > 5)      hours = 8;
  else if (hours > 3) hours = 5;
  else if (hours > 0) hours = 3;
  else                hours = 0;

  if (isPowerOn() && state.error == ERROR_NONE)
  {
#ifdef FORCE_WIFI_SLEEP
    wifiForceSleepBeginCompat();
#endif

    int tries = 8;
    do
    {
      int actHours = getDisinfectionTime();
      if (actHours == UNDEF::INT)
      {
        DEBUG_MSG("\naborted\n");
        break;
      }
      else if (actHours == hours)
      {
        break;
      }

      pressButton(buttons.toggleDisinfection);
      tries--;
    } while (tries);

#ifdef FORCE_WIFI_SLEEP
    wifiForceSleepWakeCompat();
#endif
  }
}

bool PureSpaIO::verifyBoolState(uint8 actualState, bool desiredState) const
{
  return actualState != UNDEF::BOOL && ((actualState == true) == desiredState);
}

bool PureSpaIO::setOutputState(volatile unsigned int& buttonPressCount, bool desiredState, uint8 actualState, uint8 (*refreshState)())
{
  if (verifyBoolState(actualState, desiredState))
  {
    return true;
  }

  for (unsigned int attempt = 0; attempt < BUTTON::MAX_RETRIES; ++attempt)
  {
    if (!pressButton(buttonPressCount))
    {
      delay(120);
      continue;
    }

    delay(BUTTON::VERIFY_DELAY + (attempt * 150));
    yield();
    if (verifyBoolState(refreshState(), desiredState))
    {
      return true;
    }
  }

  return false;
}

bool PureSpaIO::pressButton(volatile unsigned int& buttonPressCount)
{
  waitBuzzerOff();
  waitCommandCooldown(g_lastGenericCommandMs, 350);

  unsigned int tries = BUTTON::ACK_TIMEOUT/BUTTON::ACK_CHECK_PERIOD;
  wifiLightSleepOn();
  buttonPressCount = BUTTON::PRESS_COUNT;
  while (buttonPressCount && tries)
  {
    delay(BUTTON::ACK_CHECK_PERIOD);
    tries--;
  }
  bool success = state.buzzer;
  wifiSleepOff();

  if (success)
  {
    markCommandTime(g_lastGenericCommandMs);
    delay(120);
  }

  return success;
}

void PureSpaIO::setBubbleOn(bool on)
{
  setOutputState(buttons.toggleBubble, on, isBubbleOn(), []() -> uint8 {
    const uint16 mask = (PureSpaIO::model == PureSpaIO::MODEL::SBH20) ? FRAME_LED_SB::BUBBLE : FRAME_LED_SJBHS::BUBBLE;
    return PureSpaIO::state.ledStatus != PureSpaIO::UNDEF::USHORT ? ((PureSpaIO::state.ledStatus & mask) != 0) : PureSpaIO::UNDEF::BOOL;
  });
}

void PureSpaIO::setFilterOn(bool on)
{
  setOutputState(buttons.toggleFilter, on, isFilterOn(), []() -> uint8 {
    const uint16 mask = (PureSpaIO::model == PureSpaIO::MODEL::SBH20) ? FRAME_LED_SB::FILTER : FRAME_LED_SJBHS::FILTER;
    return PureSpaIO::state.ledStatus != PureSpaIO::UNDEF::USHORT ? ((PureSpaIO::state.ledStatus & mask) != 0) : PureSpaIO::UNDEF::BOOL;
  });
}

void PureSpaIO::setHeaterOn(bool on)
{
  setOutputState(buttons.toggleHeater, on,
                 (isHeaterOn() == true || isHeaterStandby() == true) ? 1 :
                 (isHeaterOn() == PureSpaIO::UNDEF::BOOL ? PureSpaIO::UNDEF::BOOL : 0),
                 []() -> uint8 {
    const uint16 mask = (PureSpaIO::model == PureSpaIO::MODEL::SBH20)
      ? (FRAME_LED_SB::HEATER_ON | FRAME_LED_SB::HEATER_STANDBY)
      : (FRAME_LED_SJBHS::HEATER_ON | FRAME_LED_SJBHS::HEATER_STANDBY);
    return PureSpaIO::state.ledStatus != PureSpaIO::UNDEF::USHORT ? ((PureSpaIO::state.ledStatus & mask) != 0) : PureSpaIO::UNDEF::BOOL;
  });
}

void PureSpaIO::setJetOn(bool on)
{
  if (PureSpaIO::model != PureSpaIO::MODEL::SJBHS) return;
  setOutputState(buttons.toggleJet, on, isJetOn(), []() -> uint8 {
    return PureSpaIO::state.ledStatus != PureSpaIO::UNDEF::USHORT ? ((PureSpaIO::state.ledStatus & FRAME_LED_SJBHS::JET) != 0) : PureSpaIO::UNDEF::BOOL;
  });
}

void PureSpaIO::setPowerOn(bool on)
{
  if (on)
  {
    uint8 current = isPowerOn();
    if (verifyBoolState(current, true))
    {
      return;
    }

    waitCommandCooldown(g_lastPowerCommandMs, 1500);

    for (unsigned int attempt = 0; attempt < 3; ++attempt)
    {
      pressButton(buttons.togglePower);
      markCommandTime(g_lastPowerCommandMs);

      delay(700 + (attempt * 250));
      yield();

      if (verifyBoolState(isPowerOn(), true))
      {
        return;
      }
    }

    pressButton(buttons.togglePower);
    markCommandTime(g_lastPowerCommandMs);
    delay(500);
    yield();
    return;
  }

  // OFF — never trust a quick "already off" while setpoint is visible on the bus (blink/multiplex).
  // The old 5× sample skip caused false "off" → no button press while the spa was still ON.
  {
    const bool setpointOnBus = (getDesiredWaterTempCelsius() != UNDEF::INT);
    if (!setpointOnBus)
    {
      uint8 c0 = isPowerOn();
      if (c0 == false)
      {
        delay(400);
        yield();
        uint8 c1 = isPowerOn();
        if (c1 == false)
        {
          delay(400);
          yield();
          if (isPowerOn() == false)
          {
            return;
          }
        }
      }
    }
  }

  // Repeat until LED confirms OFF (or give up after many tries).
  static const unsigned int MAX_POWER_OFF_ATTEMPTS = 20;
  for (unsigned int attempt = 0; attempt < MAX_POWER_OFF_ATTEMPTS; ++attempt)
  {
    waitCommandCooldown(g_lastPowerCommandMs, attempt == 0 ? 1500U : 1000U);

    (void)pressButton(buttons.togglePower);
    markCommandTime(g_lastPowerCommandMs);

    delay(850U + (attempt % 5U) * 120U);
    yield();
    feedWatchdog();

    uint8 p = isPowerOn();
    if (verifyBoolState(p, false))
    {
      delay(100);
      yield();
      if (verifyBoolState(isPowerOn(), false))
      {
        return;
      }
    }
  }
}

bool PureSpaIO::requestSetTempDisplay()
{
  if (!isPowerOn() || state.error != ERROR_NONE)
  {
    return false;
  }

  waitBuzzerOff();
  waitCommandCooldown(g_lastGenericCommandMs, 350);

#ifndef FORCE_WIFI_SLEEP
  wifiLightSleepOn();
#endif

  int tries = BUTTON::PRESS_SHORT_COUNT * CYCLE::PERIOD / BUTTON::ACK_CHECK_PERIOD;

  buttons.toggleTempUp = BUTTON::PRESS_SHORT_COUNT;
  while (buttons.toggleTempUp && tries)
  {
    delay(BUTTON::ACK_CHECK_PERIOD);
    yield();
    tries--;
  }
  buttons.toggleTempUp = 0;

  tries = (BUTTON::PRESS_COUNT - BUTTON::PRESS_SHORT_COUNT) * CYCLE::PERIOD / BUTTON::ACK_CHECK_PERIOD;
  while (!state.buzzer && tries)
  {
    delay(BUTTON::ACK_CHECK_PERIOD);
    yield();
    tries--;
  }

  bool success = state.buzzer;

#ifndef FORCE_WIFI_SLEEP
  wifiSleepOff();
#endif

  if (!success)
  {
    DEBUG_MSG("request set temp display failed blink=%d\n", (int)isrState.isDisplayBlinking);
    return false;
  }

  lastTempUiActionTime = millis();
  g_lastTempUiActionTime = lastTempUiActionTime;
  g_lastTempUiActionFrame = state.frameCounter;
  markCommandTime(g_lastGenericCommandMs);
  DEBUG_MSG("rSTD ok frm=%u blink=%d\n", state.frameCounter, (int)isrState.isDisplayBlinking);
  delay(250);
  yield();
  return true;
}

bool PureSpaIO::waitBuzzerOff() const
{
  int tries = BUTTON::ACK_TIMEOUT/BUTTON::ACK_CHECK_PERIOD;
  while (state.buzzer && tries)
  {
    delay(BUTTON::ACK_CHECK_PERIOD);
    tries--;
  }

  if (tries)
  {
    delay(2*CYCLE::PERIOD);
    return true;
  }
  else
  {
    DEBUG_MSG("\nwBO fail");
    return false;
  }
}

bool PureSpaIO::changeWaterTemp(int up)
{
  bool success = false;

  if (isPowerOn() && state.error == ERROR_NONE)
  {
    waitBuzzerOff();
    waitCommandCooldown(g_lastGenericCommandMs, 350);

#ifndef FORCE_WIFI_SLEEP
    wifiLightSleepOn();
#endif

    int tries = BUTTON::PRESS_SHORT_COUNT*CYCLE::PERIOD/BUTTON::ACK_CHECK_PERIOD;
    if (up > 0)
    {
      buttons.toggleTempUp = BUTTON::PRESS_SHORT_COUNT;
      while (buttons.toggleTempUp && tries)
      {
        delay(BUTTON::ACK_CHECK_PERIOD);
        yield();
        tries--;
      }
      buttons.toggleTempUp = 0;
    }
    else if (up < 0)
    {
      buttons.toggleTempDown = BUTTON::PRESS_SHORT_COUNT;
      while (buttons.toggleTempDown && tries)
      {
        delay(BUTTON::ACK_CHECK_PERIOD);
        yield();
        tries--;
      }
      buttons.toggleTempDown = 0;
    }

    tries = (BUTTON::PRESS_COUNT - BUTTON::PRESS_SHORT_COUNT)*CYCLE::PERIOD/BUTTON::ACK_CHECK_PERIOD;
    while (!state.buzzer && tries)
    {
      delay(BUTTON::ACK_CHECK_PERIOD);
      yield();
      tries--;
    }

    success = state.buzzer;

#ifndef FORCE_WIFI_SLEEP
    wifiSleepOff();
#endif

    if (success)
    {
      lastTempUiActionTime = millis();
      g_lastTempUiActionTime = lastTempUiActionTime;
      g_lastTempUiActionFrame = state.frameCounter;
      g_lastTempUiActionDirection = (up > 0) ? 1 : -1;
      g_lastDesiredBusRawChangeFrame = state.frameCounter;
      markCommandTime(g_lastGenericCommandMs);

      // The buzzer is the spa's acknowledgment that the setpoint changed by one step.
      // Write the expected new desiredTemp directly — no display-parsing required.
      // This makes confirmSetpointChange() succeed on the very first poll regardless
      // of how the spa encodes its setpoint blink/display transition.
      const int currentDC = displayTempToCelsiusRaw(state.desiredTemp);
      if (currentDC != UNDEF::INT)
      {
        const int newDC = currentDC + (up > 0 ? 1 : -1);
        if (newDC >= WATER_TEMP::SET_MIN && newDC <= WATER_TEMP::SET_MAX)
        {
          // Re-encode keeping the unit character from the existing raw value
          const uint32 unitByte = state.desiredTemp & 0x00FF0000U;
          const uint32 newRaw   = unitByte
                                | ((uint32)('0' + newDC / 10) << 8)
                                | ((uint32)('0' + newDC % 10));
          state.desiredTemp            = newRaw;
          g_lastKnownSetTemp           = newDC;
          g_lastAcceptedDesiredFromIsrC = newDC;
        }
      }
    }
    else
    {
      DEBUG_MSG("\ncWT fail");
    }
  }

  return success;
}

int PureSpaIO::convertDisplayToCelsius(uint32 value) const
{
  int celsiusValue = display2Num(value);
  char tempUnit = display2LastDigit(value);
  if (tempUnit == 'F')
  {
    float fValue = (float)celsiusValue;
    celsiusValue = (int)round(((fValue - 32) * 5) / 9);
  }
  else if (tempUnit != 'C')
  {
    celsiusValue = UNDEF::INT;
  }

  return (celsiusValue >= 0) && (celsiusValue <= 60) ? celsiusValue : UNDEF::INT;
}

IRAM_ATTR void PureSpaIO::clockRisingISR(void* arg)
{
  bool data = !digitalRead(PIN::DATA);
  bool enabled = !digitalRead(PIN::LATCH);

  if (enabled || isrState.receivedBits == (FRAME::BITS - 1))
  {
    isrState.frameValue = (isrState.frameValue << 1) + data;
    isrState.receivedBits++;

    if (isrState.receivedBits == FRAME::BITS)
    {
      state.frameCounter++;

      if (isrState.frameValue == FRAME_TYPE::CUE)
      {
      }
      else if (isrState.frameValue & FRAME_TYPE::DIGIT)
      {
        decodeDisplay();
      }
      else if (isrState.frameValue & FRAME_TYPE::LED)
      {
        decodeLED();
      }
      else if (isrState.frameValue & FRAME_TYPE::BUTTON)
      {
        decodeButton();
      }

      isrState.receivedBits = 0;
    }
  }
  else
  {
    isrState.receivedBits = 0;
    state.frameCounter++;
  }
}

inline void PureSpaIO::decodeDisplay()
{
  char digit;
  switch (isrState.frameValue & FRAME_DIGIT::SEGMENTS)
  {
    case FRAME_DIGIT::OFF:   digit = DIGIT::OFF; break;
    case FRAME_DIGIT::NUM_0: digit = '0'; break;
    case FRAME_DIGIT::NUM_1: digit = '1'; break;
    case FRAME_DIGIT::NUM_2: digit = '2'; break;
    case FRAME_DIGIT::NUM_3: digit = '3'; break;
    case FRAME_DIGIT::NUM_4: digit = '4'; break;
    case FRAME_DIGIT::NUM_5: digit = '5'; break;
    case FRAME_DIGIT::NUM_6: digit = '6'; break;
    case FRAME_DIGIT::NUM_7: digit = '7'; break;
    case FRAME_DIGIT::NUM_8: digit = '8'; break;
    case FRAME_DIGIT::NUM_9: digit = '9'; break;
    case FRAME_DIGIT::LET_C: digit = 'C'; break;
    case FRAME_DIGIT::LET_D: digit = 'D'; break;
    case FRAME_DIGIT::LET_E: digit = 'E'; break;
    case FRAME_DIGIT::LET_F: digit = 'F'; break;
    case FRAME_DIGIT::LET_H: digit = 'H'; break;
    case FRAME_DIGIT::LET_N: digit = 'N'; break;
    default:
      return;
  }

  switch (isrState.frameValue & FRAME_TYPE::DIGIT)
  {
    case FRAME_DIGIT::POS_1:
      isrState.displayValue = (isrState.displayValue & 0xFFFFFF00U) + digit;
      isrState.receivedDigits = DIGIT::POS_1;
      break;

    case FRAME_DIGIT::POS_2:
      if (isrState.receivedDigits == DIGIT::POS_1)
      {
        isrState.displayValue = (isrState.displayValue & 0xFFFF00FFU) + (digit << 8);
        isrState.receivedDigits |= DIGIT::POS_2;
      }
      break;

    case FRAME_DIGIT::POS_3:
      if (isrState.receivedDigits == DIGIT::POS_1_2)
      {
        isrState.displayValue = (isrState.displayValue & 0xFF00FFFFU) + (digit << 16);
        isrState.receivedDigits |= DIGIT::POS_3;
      }
      break;

    case FRAME_DIGIT::POS_4:
      if (isrState.receivedDigits == DIGIT::POS_1_2_3)
      {
        isrState.displayValue = (isrState.displayValue & 0x00FFFFFFU) + (digit << 24);
        isrState.receivedDigits = DIGIT::POS_ALL;
      }
      break;
  }

  if (isrState.receivedDigits == DIGIT::POS_ALL)
  {
    if (isrState.displayValue == isrState.latestDisplayValue)
    {
      isrState.stableDisplayValueCount--;
      if (isrState.stableDisplayValueCount == 0)
      {
        isrState.stableDisplayValueCount = CONFIRM_FRAMES::REGULAR;
        if (isrState.isDisplayBlinking)
        {
          if (diff(state.frameCounter, isrState.lastBlankDisplayFrameCounter)
              > spaBusFramesForWallMs(500))
          {
            isrState.isDisplayBlinking = false;
            g_lastBlinkEndedFrame = state.frameCounter;
            isrState.latestBlinkingTemp = UNDEF::UINT;
          }
        }

        if (!displayIsError(isrState.displayValue))
        {
          if (state.error != ERROR_NONE)
          {
            if (g_errorClearOkStartFrame == 0)
            {
              g_errorClearOkStartFrame = state.frameCounter;
            }
            else if (diff(state.frameCounter, g_errorClearOkStartFrame)
                     >= spaBusFramesForWallMs(ERROR_CLEAR_HOLD_MS))
            {
              state.error = ERROR_NONE;
              g_errorClearOkStartFrame = 0;
            }
          }
          else
          {
            g_errorClearOkStartFrame = 0;
          }

          if (model == MODEL::SJBHS && displayIsTime(isrState.displayValue))
          {
            if (isrState.displayValue == isrState.latestDisinfectionTime)
            {
              isrState.stableDisinfectionTimeCount--;
              if (isrState.stableDisinfectionTimeCount == 0)
              {
                if (state.disinfectionTime != isrState.displayValue)
                {
                  state.disinfectionTime = isrState.displayValue;
                }

                isrState.stableDisinfectionTimeCount = CONFIRM_FRAMES::REGULAR;
              }
            }
            else
            {
              isrState.latestDisinfectionTime = isrState.displayValue;
              isrState.stableDisinfectionTimeCount = CONFIRM_FRAMES::REGULAR;
            }
          }
          else
          {
            if (displayIsTemp(isrState.displayValue))
            {
              if (isrState.isDisplayBlinking)
              {
                if (isrState.displayValue == isrState.latestBlinkingTemp)
                {
                  isrState.stableBlinkingWaterTempCount++;

                  if (isrState.stableBlinkingWaterTempCount >= CONFIRM_FRAMES::REGULAR)
                  {
                    const int waterC = (state.waterTemp != UNDEF::UINT)
                      ? displayTempToCelsiusRaw(state.waterTemp)
                      : UNDEF::INT;
                    const int prevDC = (state.desiredTemp != UNDEF::UINT)
                      ? displayTempToCelsiusRaw(state.desiredTemp)
                      : UNDEF::INT;
                    const int blinkC = displayTempToCelsiusRaw(isrState.latestBlinkingTemp);

                    // Bug fix: a blinking value within +/-1 degC of the actual water temp is
                    // not automatically the panel's own "blink the actual temp" quirk. During
                    // an active temp button sequence (recent press, see g_lastTempUiActionFrame)
                    // the blink IS the new setpoint and it commonly lands close to the actual
                    // temp - that's the normal case when raising/lowering the target by a few
                    // degrees. Misrouting it into state.waterTemp instead of state.desiredTemp
                    // made setDesiredWaterTempCelsius()'s per-step confirm poll see a stale
                    // value for the full 3s timeout, trigger a retry (a second real button
                    // press), and eventually abort the whole multi-degree request after two
                    // failures - while the panel's real setpoint kept advancing on every button
                    // ACK regardless of this readback, leaving HA's reported target behind the
                    // panel's actual one. Only apply the "it's the actual temp" shortcut when
                    // there has been no recent temp button action.
                    const bool recentTempUiAction =
                      (g_lastTempUiActionFrame != 0)
                      && diff(state.frameCounter, g_lastTempUiActionFrame)
                             <= spaBusFramesForWallMs(RECENT_TEMP_UI_ACTION_MS);

                    if (state.error == ERROR_NONE && blinkC != PureSpaIO::UNDEF::INT
                        && blinkC < PureSpaIO::WATER_TEMP::SET_MIN)
                    {
                      if (state.waterTemp != isrState.latestBlinkingTemp)
                      {
                        state.waterTemp = isrState.latestBlinkingTemp;
                      }
                    }
                    else if (state.error == ERROR_NONE && blinkC != PureSpaIO::UNDEF::INT
                             && waterC != PureSpaIO::UNDEF::INT
                             && abs(blinkC - waterC) <= 1
                             && !recentTempUiAction)
                    {
                      if (state.waterTemp != isrState.latestBlinkingTemp)
                      {
                        state.waterTemp = isrState.latestBlinkingTemp;
                      }
                    }
                    else if (state.error == ERROR_NONE
                             && !shouldRejectBlinkAsSetpoint(isrState.latestBlinkingTemp, prevDC, waterC,
                                                             state.frameCounter))
                    {
                      const int t = displayTempToCelsiusRaw(isrState.latestBlinkingTemp);
                      if (t != PureSpaIO::UNDEF::INT
                          && waterC != PureSpaIO::UNDEF::INT
                          && t == waterC)
                      {
                        // ignore
                      }
                      else
                      {
                        if (state.desiredTemp != isrState.latestBlinkingTemp)
                        {
                          g_lastDesiredBusRawChangeFrame = state.frameCounter;
                        }
                        state.desiredTemp = isrState.latestBlinkingTemp;
                        if (t != PureSpaIO::UNDEF::INT)
                        {
                          g_lastKnownSetTemp = t;
                          g_lastAcceptedDesiredFromIsrC = t;
                        }
                      }
                    }
                  }
                }
                else if (diff(state.frameCounter, isrState.lastBlankDisplayFrameCounter)
                         < spaBusFramesForWallMs(BLINK::PERIOD / 4))
                {
                  isrState.latestBlinkingTemp = isrState.displayValue;
                  isrState.stableBlinkingWaterTempCount = 0;
                }
              }
              else
              {
                if (g_lastTempUiActionFrame != 0
                    && diff(state.frameCounter, g_lastTempUiActionFrame)
                           <= spaBusFramesForWallMs(TEMP_UI_WATER_SUPPRESS_MS))
                {
                  // Water temp suppressed during button-press window.
                  // Spa models that show the new setpoint stably (no blank phase)
                  // are detected here: treat a stable temp value as the new setpoint
                  // when the display changed AND the value is in the setpoint range.
                  isrState.latestWaterTemp = UNDEF::UINT;
                  isrState.stableWaterTempCount = CONFIRM_FRAMES::NOT_BLINKING;

                  const int stableC = displayTempToCelsiusRaw(isrState.displayValue);
                  const int waterC  = (state.waterTemp != UNDEF::UINT)
                    ? displayTempToCelsiusRaw(state.waterTemp) : UNDEF::INT;
                  const int prevDC  = (state.desiredTemp != UNDEF::UINT)
                    ? displayTempToCelsiusRaw(state.desiredTemp) : UNDEF::INT;

                  // Only accept values that move in the direction of the last button press.
                  // This prevents the old setpoint value (shown briefly on the SB-H20 display
                  // while transitioning) from overwriting a newly accepted higher/lower value.
                  const bool directionOk =
                    (g_lastTempUiActionDirection > 0 && (prevDC == UNDEF::INT || stableC > prevDC))
                    || (g_lastTempUiActionDirection < 0 && (prevDC == UNDEF::INT || stableC < prevDC))
                    || (g_lastTempUiActionDirection == 0);

                  if (state.error == ERROR_NONE
                      && stableC != UNDEF::INT
                      && stableC >= WATER_TEMP::SET_MIN
                      && stableC <= WATER_TEMP::SET_MAX
                      && directionOk
                      && !shouldRejectBlinkAsSetpoint(isrState.displayValue, prevDC, waterC,
                                                      state.frameCounter))
                  {
                    if (state.desiredTemp != isrState.displayValue)
                    {
                      g_lastDesiredBusRawChangeFrame = state.frameCounter;
                    }
                    state.desiredTemp = isrState.displayValue;
                    if (stableC != UNDEF::INT)
                    {
                      g_lastKnownSetTemp = stableC;
                      g_lastAcceptedDesiredFromIsrC = stableC;
                    }
                  }
                }
                else if (g_lastBlinkEndedFrame != 0
                         && diff(state.frameCounter, g_lastBlinkEndedFrame)
                                <= spaBusFramesForWallMs(POST_BLINK_WATER_SUPPRESS_MS))
                {
                  isrState.latestWaterTemp = UNDEF::UINT;
                  isrState.stableWaterTempCount = CONFIRM_FRAMES::NOT_BLINKING;
                }
                else if (diff(state.frameCounter, isrState.lastBlankDisplayFrameCounter)
                         > spaBusFramesForWallMs(BLINK::PERIOD / 4))
                {
                  if (isrState.displayValue == isrState.latestWaterTemp)
                  {
                    isrState.stableWaterTempCount--;
                    if (isrState.stableWaterTempCount == 0)
                    {
                      if (state.waterTemp != isrState.displayValue)
                      {
                        state.waterTemp = isrState.displayValue;
                      }

                      isrState.stableWaterTempCount = CONFIRM_FRAMES::NOT_BLINKING;
                    }
                  }
                  else
                  {
                    isrState.latestWaterTemp = isrState.displayValue;
                    isrState.stableWaterTempCount = CONFIRM_FRAMES::NOT_BLINKING;
                  }
                }
              }
            }
          }
        }
        else
        {
          state.error = display2Error(isrState.displayValue);
          g_errorClearOkStartFrame = 0;
        }
      }
    }
    else if (displayIsBlank(isrState.displayValue))
    {
      if (isrState.stableDisplayBlankCount)
      {
        isrState.stableDisplayBlankCount--;
      }
      else
      {
        if (isrState.isDisplayBlinking)
        {
          if (isrState.latestBlinkingTemp != UNDEF::UINT)
          {
            isrState.blankCounter++;
          }

          if (state.error == ERROR_NONE && isrState.blankCounter > 2
              && isrState.stableBlinkingWaterTempCount >= CONFIRM_FRAMES::REGULAR
              && (state.desiredTemp != isrState.latestBlinkingTemp
                  || state.waterTemp == UNDEF::UINT))
          {
            const int waterC = (state.waterTemp != UNDEF::UINT)
              ? displayTempToCelsiusRaw(state.waterTemp)
              : UNDEF::INT;
            const int prevDC = (state.desiredTemp != UNDEF::UINT)
              ? displayTempToCelsiusRaw(state.desiredTemp)
              : UNDEF::INT;
            if (!shouldRejectBlinkAsSetpoint(isrState.latestBlinkingTemp, prevDC, waterC,
                                             state.frameCounter))
            {
              const int t = displayTempToCelsiusRaw(isrState.latestBlinkingTemp);
              if (t != PureSpaIO::UNDEF::INT
                  && waterC != PureSpaIO::UNDEF::INT
                  && t == waterC)
              {
                // ignore
              }
              else
              {
                if (state.desiredTemp != isrState.latestBlinkingTemp)
                {
                  g_lastDesiredBusRawChangeFrame = state.frameCounter;
                }
                state.desiredTemp = isrState.latestBlinkingTemp;
                if (t != PureSpaIO::UNDEF::INT)
                {
                  g_lastKnownSetTemp = t;
                  g_lastAcceptedDesiredFromIsrC = t;
                }
              }
            }
          }

          isrState.latestBlinkingTemp = UNDEF::UINT;
          isrState.stableBlinkingWaterTempCount = 0;
        }
        else
        {
          isrState.isDisplayBlinking = true;
          isrState.blankCounter = 0;
        }
        isrState.lastBlankDisplayFrameCounter = state.frameCounter;
      }
    }
    else
    {
      // When a recent temp button press is active, ignore display values that move
      // in the wrong direction. On spa models that do not show a proper blank phase
      // between setpoint values (e.g. SB-H20 shows a garbage/partial frame instead),
      // such frames would reset stableDisplayValueCount for the correct new value,
      // preventing the stable setpoint detection from ever reaching its count of 3.
      // By not resetting the counter for off-direction values, the correct new setpoint
      // accumulates its 3 consecutive valid frames uninterrupted.
      bool resetCount = true;
      if (g_lastTempUiActionDirection != 0
          && g_lastTempUiActionFrame != 0
          && diff(state.frameCounter, g_lastTempUiActionFrame)
                 <= spaBusFramesForWallMs(TEMP_UI_WATER_SUPPRESS_MS))
      {
        if (!displayIsTemp(isrState.displayValue))
        {
          // Non-temperature value during an active button press is a garbage/blank
          // frame — ignore it so it does not reset the stable count for the real value.
          resetCount = false;
        }
        else
        {
          const int newC   = displayTempToCelsiusRaw(isrState.displayValue);
          const int prevDC = (state.desiredTemp != UNDEF::UINT)
                           ? displayTempToCelsiusRaw(state.desiredTemp) : UNDEF::INT;
          if (newC != UNDEF::INT && prevDC != UNDEF::INT)
          {
            const bool dirOk =
              (g_lastTempUiActionDirection > 0 && newC > prevDC)
              || (g_lastTempUiActionDirection < 0 && newC < prevDC);
            if (!dirOk)
            {
              resetCount = false;
            }
          }
        }
      }

      if (resetCount)
      {
        isrState.latestDisplayValue = isrState.displayValue;
        isrState.stableDisplayValueCount = CONFIRM_FRAMES::REGULAR;
        isrState.stableDisplayBlankCount = CONFIRM_FRAMES::SIGNIFICANT_BLANK_STABLE;
      }
    }
  }
}

inline void PureSpaIO::decodeLED()
{
  if (isrState.frameValue == isrState.latestLedStatus)
  {
    isrState.stableLedStatusCount--;
    if (isrState.stableLedStatusCount == 0)
    {
      state.ledStatus = isrState.frameValue;
      state.buzzer = !(state.ledStatus & FRAME_LED_SB::NO_BEEP);
      state.stateUpdated = true;
      isrState.stableLedStatusCount = CONFIRM_FRAMES::REGULAR;

      if (state.buzzer)
      {
        buttons.toggleBubble = 0;
        buttons.toggleDisinfection = 0;
        buttons.toggleFilter = 0;
        buttons.toggleHeater = 0;
        buttons.toggleJet = 0;
        buttons.togglePower = 0;
        buttons.toggleTempUp = 0;
        buttons.toggleTempDown = 0;
      }
    }
  }
  else
  {
    isrState.latestLedStatus = isrState.frameValue;
    isrState.stableLedStatusCount = CONFIRM_FRAMES::REGULAR;
  }
}

inline void PureSpaIO::updateButtonState(volatile unsigned int& buttonPressCount)
{
  if (buttonPressCount)
  {
    if (state.buzzer)
    {
      buttonPressCount = 0;
    }
    else
    {
      isrState.reply = true;
      buttonPressCount--;
    }
  }
}

inline void PureSpaIO::decodeButton()
{
  if (model == MODEL::SBH20)
  {
    if (isrState.frameValue & FRAME_BUTTON_SB::FILTER)
    {
      updateButtonState(buttons.toggleFilter);
    }
    else if (isrState.frameValue & FRAME_BUTTON_SB::HEATER)
    {
      updateButtonState(buttons.toggleHeater);
    }
    else if (isrState.frameValue & FRAME_BUTTON_SB::BUBBLE)
    {
      updateButtonState(buttons.toggleBubble);
    }
    else if (isrState.frameValue & FRAME_BUTTON_SB::POWER)
    {
      updateButtonState(buttons.togglePower);
    }
    else if (isrState.frameValue & FRAME_BUTTON_SB::TEMP_UP)
    {
      updateButtonState(buttons.toggleTempUp);
    }
    else if (isrState.frameValue & FRAME_BUTTON_SB::TEMP_DOWN)
    {
      updateButtonState(buttons.toggleTempDown);
    }
  }
  else
  {
    if (isrState.frameValue & FRAME_BUTTON_SJBHS::FILTER)
    {
      updateButtonState(buttons.toggleFilter);
    }
    else if (isrState.frameValue & FRAME_BUTTON_SJBHS::HEATER)
    {
      updateButtonState(buttons.toggleHeater);
    }
    else if (isrState.frameValue & FRAME_BUTTON_SJBHS::BUBBLE)
    {
      updateButtonState(buttons.toggleBubble);
    }
    else if (isrState.frameValue & FRAME_BUTTON_SJBHS::POWER)
    {
      updateButtonState(buttons.togglePower);
    }
    else if (isrState.frameValue & FRAME_BUTTON_SJBHS::TEMP_UP)
    {
      updateButtonState(buttons.toggleTempUp);
    }
    else if (isrState.frameValue & FRAME_BUTTON_SJBHS::TEMP_DOWN)
    {
      updateButtonState(buttons.toggleTempDown);
    }
    else if (isrState.frameValue & FRAME_BUTTON_SJBHS::DISINFECTION)
    {
      updateButtonState(buttons.toggleDisinfection);
    }
    else if (isrState.frameValue & FRAME_BUTTON_SJBHS::JET)
    {
      updateButtonState(buttons.toggleJet);
    }
  }

  if (isrState.reply)
  {
    esp_rom_delay_us(1);
    gpio_set_direction((gpio_num_t)PIN::DATA, GPIO_MODE_OUTPUT_OD);
    gpio_set_level((gpio_num_t)PIN::DATA, 0);
    esp_rom_delay_us(3);
    gpio_set_direction((gpio_num_t)PIN::DATA, GPIO_MODE_INPUT);
    isrState.reply = false;
  }
}

void PureSpaIO::setModelFromString(const char* modelName)
{
  if (!modelName) return;
  String s(modelName);
  s.toUpperCase();
  if (s.indexOf("SJB") >= 0 || s.indexOf("HS") >= 0) {
    model = MODEL::SJBHS;
  } else {
    model = MODEL::SBH20;
  }
}

void PureSpaIO::setForceWifiSleep(bool enabled)
{
  (void)enabled;
}