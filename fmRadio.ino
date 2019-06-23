#include <TEA5767N.h> // current version at dannbuckley/TEA5767
#include <LiquidCrystal.h>

LiquidCrystal screen = LiquidCrystal(13, 12, 5, 4, 3, 2);
TEA5767N radio = TEA5767N();

void setup() {
  screen.begin(16, 2);
}

void loop() {
  // loop code here
}
