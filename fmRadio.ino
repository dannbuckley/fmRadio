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

const byte signal_bitmaps[4] = {0xFC, 0xF3, 0xCF, 0x3F};

/* Variables */
int recp = 0;
byte strength[26];
byte currentStddev = 0;

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
float calculateFrequencyFromChannel(int chn) {
  return 87.9 + ((chn - 200) * 0.2);
}

// Get the original signal level from the last time this channel was visited
byte getStoredSignalLevel(int chn) {
  int chn_reduc = chn - 200;
  return (strength[chn_reduc / 4] >> (2 * (chn_reduc % 4))) & 3;
}

// Get the distance from the sample mean for a given sample level
byte signal_to_stddev(byte signal_lev) {
  // Results of sample standard deviation for 16 active FM channel signals
  // Sample mean: 8.25
  // Sample standard deviation: 3.40
  if (signal_lev >= 12) {
    // +2 standard deviations: [12, 15]
    return 3;
  } else if (signal_lev >= 8) {
    // +1 standard deviation: [8, 12)
    return 2;
  } else if (signal_lev >= 5) {
    // -1 standard deviation: [5, 8)
    return 1;
  } else {
    // -2 standard deviations: [0, 5)
    return 0;
  }
}

void setup() {
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
  byte sig = 0, stddev = 0, old = 0;

  for (int i = 0; i <= 100; i++) {
    screen.print("Testing CH");
    screen.print(i + 200);
    screen.setCursor(0, 1);
    screen.print(i + 1);
    screen.print("/");
    screen.print(101);
    radio.selectChannel(i + 200, false);
    sig = radio.getSignalLevel();
    stddev = signal_to_stddev(sig);
    old = strength[i / 4] & signal_bitmaps[i % 4];
    strength[i / 4] = old | (stddev << (2 * (i % 4)));
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

void loop() {
  // Get current button press states
  muteBtnState = analogRead(muteBtn);
  indexUpBtnState = analogRead(indexUpBtn);
  indexDownBtnState = analogRead(indexDownBtn);
  menuBtnState = analogRead(menuBtn);

  // Get time since boot
  unsigned long readTime = millis();

  // Mute button press: toggle mute
  if (muteBtnState == 1023) {
    if ((readTime - muteTime) >= debounceDelay) {
      radio.toggleMute();
      globalMute = radio.isMuted();

      delay(debounceDelay);
    }

    muteTime = readTime;
  }

  // Menu button press: toggle seek mode
  if (menuBtnState == 1023) {
    if ((readTime - menuTime) >= debounceDelay) {
      seekMode = !seekMode;
      delay(debounceDelay);
    }

    menuTime = readTime;
  }

  // Index Up button press
  if (indexUpBtnState == 1023) {
    if ((readTime - indexUpTime) >= debounceDelay) {
      if (!globalMute) {
        radio.toggleMute();
      }

      // Go to next channel
      channel = (channel == 300) ? 200 : (channel + 1);

      if (seekMode) {
        seekChannel(SEEK_UP);
      }

      radio.selectChannel(channel, false);
      changed = true;

      if (!globalMute) {
        radio.toggleMute();
      }

      delay(debounceDelay);
    }

    indexUpTime = readTime;
  }

  // Index Down button press
  if (indexDownBtnState == 1023) {
    if ((readTime - indexDownTime) >= debounceDelay) {
      if (!globalMute) {
        radio.toggleMute();
      }

      // Go to previous channel
      channel = (channel == 200) ? 300 : (channel - 1);

      if (seekMode) {
        seekChannel(SEEK_DOWN);
      }

      radio.selectChannel(channel, false);
      changed = true;

      if (!globalMute) {
        radio.toggleMute();
      }

      delay(debounceDelay);
    }

    indexDownTime = readTime;
  }

  // Update data if station has been changed
  if (changed) {
    setStationData();
    changed = false;
  }

  // Redraw screen text
  screen.clear();
  printStationData();

  if (globalMute) {
    printMuted();
  }

  delay(50);
}

// Use the signal level array to find a new channel
void seekChannel(int direction) {
  bool found = false;
  
  // Save the starting channel to prevent an infinite loop
  int start = channel;

  screen.clear();
  screen.print("Seeking...");

  while (!found) {
    if (getStoredSignalLevel(channel) & 2) {
      // Stored station signal level is above the sample mean
      found = true;
    } else {
      // Stored station signal level is below the sample mean
      if (direction) {
        // SEEK_UP
        channel = (channel == 300) ? 200 : (channel + 1);
      } else {
        // SEEK_DOWN
        channel = (channel == 200) ? 300 : (channel - 1);
      }
      if (start == channel) {
        // If there are no other available stations on the spectrum,
        // use the channel set at the beginning of the seek operation
        found = true;
      }
    }
  }
}

void setStationData() {
  frequency = calculateFrequencyFromChannel(channel);
  recp = radio.isStereo();
  currentStddev = signal_to_stddev(radio.getSignalLevel());

  // Update signal level
  int chn_reduc = channel - 200;
  byte oldSig = strength[chn_reduc / 4] & signal_bitmaps[chn_reduc % 4];
  strength[chn_reduc / 4] = oldSig | (currentStddev << (2 * (chn_reduc % 4)));
}

void printStationData() {
  if (frequency < 100) {
    screen.print(" ");
  }

  screen.print(frequency, 1);
  screen.print(recp ? "-ST:" : "-MN:");
  screen.print(currentStddev);

  if (seekMode) {
    screen.print(" SK");
  }
}

void printMuted() {
  screen.setCursor(0, 1);
  screen.print("Muted...");
}
