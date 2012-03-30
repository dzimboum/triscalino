/*
 * Tetris game on a Deuligne 16x2 LCD display
 * 
 * https://github.com/dzimboum/triscalino
 * Copyright 2012 dzimboum
 * Released under the WTFPL 2.0 license
 * http://sam.zoy.org/wtfpl/COPYING
 *
 * This is a work in progress, please be patient!
 */

#include <Wire.h>
#include <Deuligne.h>

Deuligne lcd;

/*
 * A Tetris game on a 16x2 display is not very fun.
 * In order to make it a little bit funnier, we will
 * split rows into 2, in order to emulate a 16x4
 * display.  This is crazy!
 */

// Upper square
byte uppperSquare[] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B0,
  B0,
  B0,
  B0
};

// Lower square
byte lowerSquare[] = {
  B0,
  B0,
  B0,
  B0,
  B11111,
  B11111,
  B11111,
  B11111 
};

int key=-1;
int oldkey=-1;
int ligneCurseur = 0;
int colonneCurseur = 0;

void setup()
{
  Wire.begin();
  lcd.init();

  lcd.createChar(0, uppperSquare);
  lcd.createChar(1, lowerSquare);

  lcd.backLight(true); // Backlight ON

  lcd.setCursor(5,0); // Place cursor row 6, 1st line (counting from 0)
  lcd.print("Setup");
  lcd.setCursor(7,1); // Place cursor row 8, 2nd line (counting from 0)
  lcd.print("ok");
  delay(300);
  lcd.clear();
}

void loop() {
  key = lcd.get_key();  // read the value from the sensor & convert into key press

  if (key != oldkey)    // if keypress is detected
  {
    delay(50);          // wait for debounce time
    key = lcd.get_key();// read the value from the sensor & convert into key press
    if (key != oldkey)				
    {
      oldkey = key;
      lcd.setCursor(colonneCurseur, ligneCurseur / 2);
      lcd.print(' ');      
      if (key == 0) {
        ++colonneCurseur;
      } else if (key == 1) {
        --ligneCurseur;
      } else if (key == 2) {
        ++ligneCurseur;
      } else if (key == 3) {
        --colonneCurseur;
      }
 
      if (ligneCurseur < 0) {
        ligneCurseur = 0;
      }
      if (ligneCurseur > 3) {
        ligneCurseur = 3;
      }
      if (colonneCurseur < 0) {
        colonneCurseur = 0;
      }
      if (colonneCurseur > 15) {
        colonneCurseur = 15;
      }
      lcd.setCursor(colonneCurseur, ligneCurseur / 2);
      lcd.write(ligneCurseur % 2);
    }
  }
}

