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

extern "C" {
  #include <inttypes.h>
}

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
byte upperSquare[] = {
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

// Upper and lower squares
byte bothSquares[] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

/*
 * In order to draw pieces, we have to keep track of pieces
 * which have already been inserted.  The simplest solution
 * is to use a 4x16 grid.
 * But we will add two virtual rows at the top and bottom,
 * and one virtual column at the right.  Eventually we
 * need an 8x17 array.
 */
boolean board[8][17] = {
};

// Piece number currently processed
uint8_t currentNumber = -1;

// Geometric shape
class Shape
{
public:
  // Geometric shape, it contains a 3x3 grid
  boolean grid_[3][3];

  /*
   * In order to speed up processing, we use 3 auxiliary tables:
   * top_, bottom and right_.  For instance with this shape:
   *   xoo
   *   xxo
   *   oxo
   * top_    = { 0, 1, 3 }
   * bottom_ = { 1, 2, 3 }
   * right_  = { 0, 1, 1 }
   */
  unsigned char top_[3];
  unsigned char bottom_[3];
  unsigned char right_[3];

public:
  Shape(unsigned int row0, unsigned int row1, unsigned int row2);
  boolean pixel(int i, int j);
};

Shape::Shape(unsigned int row0, unsigned int row1, unsigned int row2)
{
  grid_[0][0] = (row0 >> 2) & 1; grid_[0][1] = (row0 >> 1) & 1; grid_[0][2] = row0 & 1;
  grid_[1][0] = (row1 >> 2) & 1; grid_[1][1] = (row1 >> 1) & 1; grid_[1][2] = row1 & 1;
  grid_[2][0] = (row2 >> 2) & 1; grid_[2][1] = (row2 >> 1) & 1; grid_[2][2] = row2 & 1;

  for (int i = 0; i < 3; ++i) {
    top_[i] = bottom_[i] = right_[i] = 3;
  }
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (grid_[i][j]) {
        bottom_[j] = i;
        right_[i] = j;
      }
      if (grid_[2-i][j]) 
        top_[j] = 2-i;
    }
  }
}

boolean
Shape::pixel(int i, int j)
{
  return grid_[i][j];

}

Shape allPieces[] = {
  Shape(
     B111,
     B000,
     B000),
  Shape(
     B010,
     B111,
     B010),
  Shape(
     B110,
     B110,
     B000),
  Shape(
     B010,
     B010,
     B011),
};

class Piece
{
private:
public:
  uint8_t row_;
  uint8_t column_;
  uint8_t number_;
public:
  Piece(uint8_t row, uint8_t colonne, uint8_t number);
  void display(uint8_t oldRow, uint8_t oldColumn);
  boolean moveUp();
  boolean moveDown();
  boolean moveRight();
  void freeze();
  static Piece newPiece();
};

Piece::Piece(uint8_t row, uint8_t colonne, uint8_t number) :
    row_(row),
    column_(colonne),
    number_(number)
{
}

Piece
Piece::newPiece()
{
  ++currentNumber;
  if (currentNumber >= sizeof(allPieces)/sizeof(Shape))
    currentNumber = 0;
  return Piece(3, 0, currentNumber);
}

void
Piece::display(uint8_t oldRow, uint8_t oldColumn)
{
  if (oldRow == row_ && oldColumn == column_) return;

  for (int j = 0; j < 3; ++j) {
    for (int i = -1; i < 3; ++i) {
      if ((oldRow + i) % 2 == 0) {
          lcd.setCursor(oldColumn+j, (oldRow+i) / 2 - 1);
          if (board[oldRow + i][oldColumn + j] && board[oldRow + i + 1][oldColumn + j]) {
            lcd.write(2);
          } else if (board[oldRow + i][oldColumn + j]) {
            lcd.write(0);
          } else if (board[oldRow + i + 1][oldColumn + j]) {
            lcd.write(1);
          } else {
            lcd.print(' ');
          }
          ++i;
      }
    }
  }
  for (int j = 0; j < 3; ++j) {
    for (int i = 0; i < 3; ++i) {
      if (allPieces[number_].pixel(i, j)) {
        lcd.setCursor(column_+j, (row_+i) / 2 - 1);
        if ((row_ + i) % 2) {
          if (board[row_ + i - 1][column_ + j])
            lcd.write(2);
          else
            lcd.write(1);
        } else {
          if (i < 2 && allPieces[number_].pixel(i+1, j)) {
            lcd.write(2);
          } else if (board[row_ + i + 1][column_ + j]) {
            lcd.write(2);
          } else {
            lcd.write(0);
          }
          ++i;
        }
      }
    }
  }
}

boolean
Piece::moveUp()
{
  --row_;
  if ((allPieces[number_].top_[0] != 3 && board[row_ + allPieces[number_].top_[0]][column_]) ||
      (allPieces[number_].top_[1] != 3 && board[row_ + allPieces[number_].top_[1]][column_]) ||
      (allPieces[number_].top_[2] != 3 && board[row_ + allPieces[number_].top_[2]][column_])) {
    ++row_;
    return false;
  }
  display(row_ + 1, column_);
  return true;
}

boolean
Piece::moveDown()
{
  ++row_;
  if ((allPieces[number_].bottom_[0] != 3 && board[row_ + allPieces[number_].bottom_[0]][column_]) ||
      (allPieces[number_].bottom_[1] != 3 && board[row_ + allPieces[number_].bottom_[1]][column_]) ||
      (allPieces[number_].bottom_[2] != 3 && board[row_ + allPieces[number_].bottom_[2]][column_])) {
    --row_;
    return false;
  }
  display(row_ - 1, column_);
  return true;
}

boolean
Piece::moveRight()
{
  ++column_;
  if ((allPieces[number_].right_[0] != 3 && board[row_  ][column_ + allPieces[number_].right_[0]]) ||
      (allPieces[number_].right_[1] != 3 && board[row_+1][column_ + allPieces[number_].right_[1]]) ||
      (allPieces[number_].right_[2] != 3 && board[row_+2][column_ + allPieces[number_].right_[2]])) {
    --column_;
    return false;
  }
  display(row_, column_ - 1);
  return true;
}

void
Piece::freeze()
{
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (allPieces[number_].pixel(i, j)) {
        board[row_ + i][column_ + j] = true;
      }
    }
  }
  for (int j = 0; j < 3; ++j) {
    boolean complet = true;
    for (int i = 2; i < 6; ++i) {
      if (!board[i][column_ + j]) {
        complet = false;
        break;
      }
    }
    if (complet) {
    }
  }
}

/* Current piece */
Piece currentPiece = Piece::newPiece();

int levelUp = 10000;
int counter = 0;

int key=-1;
int oldkey=-1;

void setup()
{
  Wire.begin();
  lcd.init();

  lcd.createChar(0, upperSquare);
  lcd.createChar(1, lowerSquare);
  lcd.createChar(2, bothSquares);

  lcd.backLight(true); // Backlight ON

  lcd.setCursor(5,0); // Place cursor row 6, 1st line (counting from 0)
  lcd.print("Setup");
  lcd.setCursor(7,1); // Place cursor row 8, 2nd line (counting from 0)
  lcd.print("ok");
  delay(300);
  lcd.clear();

  currentPiece.display(0, 0);

  for (int j = 0; j < 17; ++j) {
    board[0][j] = true;
    board[1][j] = true;
    board[6][j] = true;
    board[7][j] = true;
  }
  for (int i = 0; i < 8; ++i) {
    board[i][16] = true;
  }
}

void loop() {
  ++counter;
  key = lcd.get_key();  // read the value from the sensor & convert into key press

  if (key != oldkey)    // if keypress is detected
  {
    delay(50);          // wait for debounce time
    key = lcd.get_key();// read the value from the sensor & convert into key press
    if (key != oldkey)				
    {
      oldkey = key;
      if (key == 0) {
        currentPiece.moveRight();
      } else if (key == 1) {
        currentPiece.moveUp();
      } else if (key == 2) {
        currentPiece.moveDown();
      } else if (key == 3) {
        /* */
      }
    }
  }
  if (counter == levelUp) {
    counter = 0;
    if (!currentPiece.moveRight()) {
      currentPiece.freeze();
      currentPiece = Piece::newPiece();
    }
  }
}

