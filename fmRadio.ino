#include <TEA5767N.h> // current version at dannbuckley/TEA5767
#include <LiquidCrystal.h>

/* Definitions */
#define SEEK_UP 1
#define SEEK_DOWN 0

/* Physical Components */
LiquidCrystal screen = LiquidCrystal(13, 12, 5, 4, 3, 2);
TEA5767N radio = TEA5767N();

/* Button Pins */
const int muteBtn = A0;
const int indexUpBtn = A1;
const int indexDownBtn = A2;
const int menuBtn = A3;

const unsigned long debounceDelay = 100;

/* Variables */
int recp = 0;
byte strength[51];
byte signalLev = 0;

// Current FCC channel / frequency
int channel = 200;
float frequency = 0.0;

// Radio state values
bool seekMode = true;
bool changed = false;
bool globalMute = false;

// Button states
int muteBtnState = 0;
int indexUpBtnState = 0;
int indexDownBtnState = 0;
int menuBtnState = 0;

// Time of last button press (for debouncing)
unsigned long muteTime = 0;
unsigned long indexUpTime = 0;
unsigned long indexDownTime = 0;
unsigned long menuTime = 0;

// Convert FCC channel ID to frequency in MHz
float calculateFrequencyFromChannel(int chn)
{
  return 87.9 + ((chn - 200) * 0.2);
}

// Get the original signal level from the last time this channel was visited
byte getStoredSignalLevel(int chn)
{
  return ((chn % 2) == 1) ? ((strength[(chn - 200) / 2] & 0xF0) >> 4) : (strength[(chn - 200) / 2] & 0x0F);
}

void setup()
{
  // Initialize screen
  screen.begin(16, 2);

  // Show start screen
  screen.print("FMRadio (c) 2019");
  screen.setCursor(0, 1);
  screen.print("Daniel Buckley");
  delay(200);
  screen.clear();

  // Test channels for signal level
  radio.toggleMute();
  byte sig = 0;
  for (int i = 0; i <= 100; i++)
  {
    screen.print("Testing CH");
    screen.print(i + 200);
    screen.setCursor(0, 1);
    screen.print(i + 1);
    screen.print("/");
    screen.print(101);
    radio.selectChannel(i + 200, false);
    sig = radio.getSignalLevel();
    strength[i / 2] |= ((i % 2) == 1) ? (sig << 4) : sig;
    screen.clear();
  }

  // Seek UP from channel 200 (87.9 MHz)
  radio.toggleStereoNoiseCancelling();
  seekChannel(SEEK_UP);
  radio.selectChannel(channel, false);
  radio.toggleMute();

  // Print station data
  setStationData();
  printStationData();
}

void loop()
{
  // Get current button press states
  muteBtnState = analogRead(muteBtn);
  indexUpBtnState = analogRead(indexUpBtn);
  indexDownBtnState = analogRead(indexDownBtn);
  menuBtnState = analogRead(menuBtn);

  // Get time since boot
  unsigned long readTime = millis();

  // Mute button press: toggle mute
  if (muteBtnState == 1023)
  {
    if ((readTime - muteTime) >= debounceDelay)
    {
      radio.toggleMute();
      globalMute = radio.isMuted();

      delay(debounceDelay);
    }

    muteTime = readTime;
  }

  // Menu button press: toggle seek mode
  if (menuBtnState == 1023)
  {
    if ((readTime - menuTime) >= debounceDelay)
    {
      seekMode = !seekMode;
      delay(debounceDelay);
    }

    menuTime = readTime;
  }

  // Index Up button press
  if (indexUpBtnState == 1023)
  {
    if ((readTime - indexUpTime) >= debounceDelay)
    {
      if (!globalMute)
      {
        radio.toggleMute();
      }

      // Go to next channel
      channel = (channel == 300) ? 200 : (channel + 1);
      if (seekMode)
      {
        seekChannel(SEEK_UP);
      }

      radio.selectChannel(channel, false);
      changed = true;

      if (!globalMute)
      {
        radio.toggleMute();
      }

      delay(debounceDelay);
    }

    indexUpTime = readTime;
  }

  // Index Down button press
  if (indexDownBtnState == 1023)
  {
    if ((readTime - indexDownTime) >= debounceDelay)
    {
      if (!globalMute)
      {
        radio.toggleMute();
      }

      // Go to previous channel
      channel = (channel == 200) ? 300 : (channel - 1);
      if (seekMode)
      {
        seekChannel(SEEK_DOWN);
      }

      radio.selectChannel(channel, false);
      changed = true;

      if (!globalMute)
      {
        radio.toggleMute();
      }

      delay(debounceDelay);
    }

    indexDownTime = readTime;
  }

  // Update data if station has been changed
  if (changed)
  {
    setStationData();
    changed = false;
  }

  // Redraw screen text
  screen.clear();
  printStationData();

  if (globalMute)
  {
    printMuted();
  }

  delay(50);
}

// Use the signal level array to find a new channel
void seekChannel(int direction)
{
  bool found = false;

  screen.clear();
  screen.print("Seeking...");

  while (!found)
  {
    if (getStoredSignalLevel(channel) >= 7)
    {
      found = true;
    }
    else
    {
      if (direction)
      {
        // SEEK_UP
        channel = (channel == 300) ? 200 : (channel + 1);
      }
      else
      {
        // SEEK_DOWN
        channel = (channel == 200) ? 300 : (channel - 1);
      }
    }
  }
}

void setStationData()
{
  frequency = calculateFrequencyFromChannel(channel);
  recp = radio.isStereo();
  signalLev = radio.getSignalLevel();

  // Update signal level
  byte oldSig = strength[(channel - 200) / 2];
  strength[(channel - 200) / 2] = ((channel % 2) == 1) ? ((oldSig & 0x0F) | (signalLev << 4)) : ((oldSig & 0xF0) | signalLev);
}

void printStationData()
{
  if (frequency < 100)
  {
    screen.print(" ");
  }

  screen.print(frequency, 1);
  screen.print(recp ? "-ST" : "-MN");

  if (seekMode)
  {
    screen.print(" SK");
  }
}

void printMuted()
{
  screen.setCursor(0, 1);
  screen.print("Muted...");
}
