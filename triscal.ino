/*
 * Tetris game on a 16x2 LCD display
 * https://github.com/dzimboum/triscalino
 * Implemented on the great Deuligne shield, by Snootlab.
 *
 * Please file issues and send contributions on Github.
 *
 * Copyright 2012 dzimboum
 * Released under the WTFPL 2.0 license
 * http://sam.zoy.org/wtfpl/COPYING
 */

#include <Wire.h>
#include <Deuligne.h>

#include <inttypes.h>
#include <string.h>

#define DEBUG_TRISCAL 1
#undef DEBUG_TRISCAL

/*
 * A Tetris game on a 16x2 display is not very funny.
 * In order to make it a little bit more exciting, we
 * will split rows into 2, in order to emulate a 16x4
 * display.  Yeah, this is crazy!
 */
Deuligne lcd;

// Upper square
byte upperHalfRow[] = {
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
byte lowerHalfRow[] = {
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
byte fullRow[] = {
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
 * will split rows into 2, in order to emulate a 16x4
 * display.  Yeah, this is crazy!
 * Board looks like:
 *             1111111
 *   01234567890123456
 * 0 #################
 * 1 #################
 * 2              X X#
 * 3     +++      XXX#
 * 4               XX#
 * 5               X #
 * 6 #################
 * 7 #################
 *
 * where # is a virtual wall, X are frozen pieces,
 * and + is the moving one.
 * Both # and X are stored in the board array, and
 * + is stored in the currentPiece object.
 */

// Global variables
boolean board[8][17];
int score;
int level;

// Geometric shape
class Shape
{
public:
  // Local shape, it contains a 3x3 grid, and there are 4 orientations
  boolean grid_[4][3][3];

public:
  Shape(byte row0, byte row1, byte row2);
  boolean pixel(int orientation, int i, int j);
#ifdef DEBUG_TRISCAL
  void print();
#endif
};

Shape::Shape(byte row0, byte row1, byte row2)
{
  grid_[0][0][0] = (row0 >> 2) & 1; grid_[0][0][1] = (row0 >> 1) & 1; grid_[0][0][2] = row0 & 1;
  grid_[0][1][0] = (row1 >> 2) & 1; grid_[0][1][1] = (row1 >> 1) & 1; grid_[0][1][2] = row1 & 1;
  grid_[0][2][0] = (row2 >> 2) & 1; grid_[0][2][1] = (row2 >> 1) & 1; grid_[0][2][2] = row2 & 1;
  // Build grid_ for other orientations
  for (int o = 1; o < 4; ++o) {
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        grid_[o][i][j] = grid_[o-1][2-j][i];
      }
    }
  }
  // In order to avoid some visual discrepancies, we modify grid_ array.
  // For instance, user assumes that the 2x2 square is invariant by rotation.
  //
  // Shift piece to the left in order to prevent pieces from
  // moving to the left when rotating.  There may be one or two
  // empty columns, so these instructions have to be run twice
  for (int o = 0; o < 4; /* */) {
    if (!grid_[o][0][0] && !grid_[o][1][0] && !grid_[o][2][0]) {
      for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
          grid_[o][i][j] = grid_[o][i][j+1];
        }
        grid_[o][i][2] = false;
      }
    } else {
      ++o;
    }
  }
  // Shift piece to the bottom
  for (int o = 0; o < 4; ++o) {
    if (!grid_[o][2][0] && !grid_[o][2][1] && !grid_[o][2][2]) {
      for (int i = 2; i > 0; --i) {
        for (int j = 0; j < 3; ++j) {
          grid_[o][i][j] = grid_[o][i-1][j];
        }
      }
      for (int j = 0; j < 3; ++j) {
        grid_[o][0][j] = false;
      }
    }
  }
  // Shift piece to the top, unless this is a middle line
  for (int o = 0; o < 4; ++o) {
    if (!grid_[o][0][0] && !grid_[o][0][1] && !grid_[o][0][2]) {
      if (grid_[o][2][0] || grid_[o][2][1] || grid_[o][2][2]) {
        for (int i = 0; i < 2; ++i) {
          for (int j = 0; j < 3; ++j) {
            grid_[o][i][j] = grid_[o][i+1][j];
          }
        }
        for (int j = 0; j < 3; ++j) {
          grid_[o][2][j] = false;
        }
      }
    }
  }
}

boolean
Shape::pixel(int orientation, int i, int j)
{
  return grid_[orientation][i][j];
}

#ifdef DEBUG_TRISCAL
void
Shape::print()
{
  Serial.println();
  Serial.println(" 0: ##### 1: ##### 2: ##### 3: #####");
  for (int i = 0; i < 3; ++i) {
    for (int orientation = 0; orientation < 4; ++orientation) {
      Serial.print("    #");
      for (int j = 0; j < 3; ++j) {
        if (pixel(orientation, i, j)) {
          Serial.print("X");
        } else {
          Serial.print(" ");
        }
      }
      Serial.print("#");
    }
    Serial.println();
  }
  Serial.println("    #####    #####    #####    #####");
}
#endif

Shape allPieces[] = {
  Shape(
     B111,
     B000,
     B000),
  Shape(
     B110,
     B110,
     B000),
  Shape(
     B010,
     B010,
     B011),
  Shape(
     B010,
     B010,
     B110),
  Shape(
     B011,
     B010,
     B000),
  Shape(
     B111,
     B010,
     B000),
};

class Piece
{
public:
  uint8_t row_;
  uint8_t column_;
  uint8_t number_;
  uint8_t orientation_;

  Piece(uint8_t row, uint8_t column, uint8_t number);
  void display(uint8_t oldOrientation = 0, uint8_t oldRow = 0, uint8_t oldColumn = 0);
  boolean moveUp();
  boolean moveDown();
  boolean moveRight();
  boolean rotateClockwise(boolean force = false);
  boolean rotateCounterClockwise(boolean force = false);
  void freeze();
  boolean findMatchingPlace();
  boolean hasRoom();
  static Piece newPiece();
};

Piece::Piece(uint8_t row, uint8_t column, uint8_t number) :
    row_(row),
    column_(column),
    number_(number),
    orientation_(0)
{
}

Piece
Piece::newPiece()
{
  uint8_t n = random(sizeof(allPieces)/sizeof(Shape));
#ifdef DEBUG_TRISCAL
  Serial.print("New piece: ");
  Serial.print(n);
  Serial.print(" out of ");
  Serial.print(sizeof(allPieces)/sizeof(Shape));
  Serial.println(" pieces");
#endif
  return Piece(3, 0, n);
}

void
Piece::display(uint8_t oldOrientation, uint8_t oldRow, uint8_t oldColumn)
{
  if (oldOrientation == orientation_ && oldRow == row_ && oldColumn == column_) return;

  if (oldRow != 0 || oldColumn != 0) {
    for (int j = 0; j < 3; ++j) {
      for (int i = -1; i < 3; ++i) {
        if ((oldRow + i > 0) && ((oldRow + i) % 2 == 0)) {
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
  }
  for (int j = 0; j < 3; ++j) {
    for (int i = 0; i < 3; ++i) {
      if (allPieces[number_].pixel(orientation_, i, j)) {
        lcd.setCursor(column_+j, (row_+i) / 2 - 1);
        if ((row_ + i) % 2) {
          if (board[row_ + i - 1][column_ + j])
            lcd.write(2);
          else
            lcd.write(1);
        } else {
          if (i < 2 && allPieces[number_].pixel(orientation_, i+1, j)) {
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
  if (!hasRoom()) {
    ++row_;
    return false;
  }
  display(orientation_, row_ + 1, column_);
  return true;
}

boolean
Piece::moveDown()
{
  ++row_;
  if (!hasRoom()) {
    --row_;
    return false;
  }
  display(orientation_, row_ - 1, column_);
  return true;
}

boolean
Piece::moveRight()
{
  ++column_;
  if (!hasRoom()) {
    --column_;
    return false;
  }
  display(orientation_, row_, column_ - 1);
  return true;
}

boolean
Piece::findMatchingPlace()
{
  int oldRow = row_;
  for (int delta = 1; delta < 3; ++delta) {
    row_ = oldRow + delta;
    if (row_ < 6) {
      if (hasRoom()) {
        return true;
      }
    }
    if (oldRow >= delta) {
      row_ = oldRow - delta;
      if (hasRoom()) {
        return true;
      }
    }
  }
  row_ = oldRow;
  return false;
}

boolean
Piece::rotateClockwise(boolean force)
{
  uint8_t oldOrientation = orientation_;
  ++orientation_;
  orientation_ = (orientation_ & 3);
  if (force)
    return true;

  if (!hasRoom()) {
    if (!findMatchingPlace()) {
      orientation_ = oldOrientation;
      return false;
    }
  }
  display(oldOrientation, row_, column_);
  return true;
}

boolean
Piece::rotateCounterClockwise(boolean force)
{
  uint8_t oldOrientation = orientation_;
  --orientation_;
  orientation_ = (orientation_ & 3);
  if (force)
    return true;

  if (!hasRoom()) {
    if (!findMatchingPlace()) {
      orientation_ = oldOrientation;
      return false;
    }
  }
  display(oldOrientation, row_, column_);
  return true;
}

void
Piece::freeze()
{
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (allPieces[number_].pixel(orientation_, i, j)) {
        board[row_ + i][column_ + j] = true;
      }
    }
  }
}

boolean
Piece::hasRoom()
{
  for (int j = 0; j < 3; ++j) {
    for (int i = 0; i < 3; ++i) {
      if (board[row_ + i][column_ + j] && allPieces[number_].pixel(orientation_, i, j)) {
        return false;
      }
    }
  }
  return true;
}

// For debugging purpose
#ifdef DEBUG_TRISCAL
void dumpBoard(const char *msg) {
  Serial.println(msg);
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 17; ++j) {
      if (i < 2 || i > 5 || j == 16) {
        if (board[i][j])
          Serial.print("#");
        else
          Serial.print("!");
      }
      else if (board[i][j])
        Serial.print("X");
      else
        Serial.print(" ");
    }
    Serial.println("");
  }
}
#endif

/*
 * Redraw the screen based on data found in the board array.
 */
void redrawScreen() {
  for (int i = 0; i < 2; ++i) {
    lcd.setCursor(0, i);
    for (int j = 0; j < 17; ++j) {
      if (board[2*i+2][j] && board[2*i+3][j])
        lcd.write(2);
      else if (board[2*i+2][j])
        lcd.write(0);
      else if (board[2*i+3][j])
        lcd.write(1);
      else
        lcd.print(" ");
    }
  }
}

/*
 * Check for full columns and remove them.
 * It checks only [column,column+2] columns.
 */
void checkFullColumns(uint8_t column) {
  int fullColumns = 0;
  static boolean toRemove[3];
  for (int j = 0; j < 3; ++j) {
    if (j+column >= 16) {
      toRemove[j] = false;
      continue;
    }
    toRemove[j] = true;
    for (int i = 2; i < 6; ++i) {
      if (!board[i][column+j]) {
        toRemove[j] = false;
        break;
      }
    }
    if (toRemove[j]) {
      ++fullColumns;
    }
  }
  if (fullColumns > 0) {
#ifdef DEBUG_TRISCAL
    Serial.print("Finished columns: ");
    Serial.println(fullColumns);
    Serial.print("found starting from column ");
    Serial.println(column);
#endif
    for (int b = 0; b < 6; ++b) {
      for (int j = 0; j < 3; ++j) {
        if (toRemove[j]) {
          lcd.setCursor(column+j, 0);
          if (b % 2)
            lcd.write(2);
          else
            lcd.print(" ");
          lcd.setCursor(column+j, 1);
          if (b % 2)
            lcd.write(2);
          else
            lcd.print(" ");
        }
        delay(30);
      }
    }
    for (int j = 0; j < 3; ++j) {
      if (!toRemove[j])
        continue;
      for (int i = 2; i < 6; ++i) {
        for (int b = column + j; b > 0; --b) {
          board[i][b] = board[i][b-1];
        }
        board[i][0] = false;
      }
    }
#ifdef DEBUG_TRISCAL
    dumpBoard("");
    Serial.print("Add to score: ");
    Serial.println(fullColumns * (40 - column) * level);
#endif
    redrawScreen();
  }
  score += fullColumns * (40 - column) * level;
}

/* Current piece */
Piece currentPiece = Piece::newPiece();

int levelUp;
int counter, maxCounter;
int key, oldkey;

void gameInit()
{
  lcd.clear();

  maxCounter = 10000;
  counter = 0;

  levelUp = 0;
  level = 1;
  score = 0;

  key=-1;
  oldkey=-1;

  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 17; ++j) {
      board[i][j] = false;
    }
  }
  for (int j = 0; j < 17; ++j) {
    board[0][j] = true;
    board[1][j] = true;
    board[6][j] = true;
    board[7][j] = true;
  }
  for (int i = 0; i < 8; ++i) {
    board[i][16] = true;
  }

  currentPiece = Piece::newPiece();
  currentPiece.display();
}

void gameOver() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Score:");
  lcd.setCursor(0, 1);
  lcd.print(score);
  delay(10000);
}

void setup()
{
#ifdef DEBUG_TRISCAL
  Serial.begin(9600);
  Serial.println("Hello world");
  for (int i = 0; i < sizeof(allPieces)/sizeof(Shape); ++i) {
    allPieces[i].print();
  }
#endif

  Wire.begin();
  lcd.init();

  lcd.createChar(0, upperHalfRow);
  lcd.createChar(1, lowerHalfRow);
  lcd.createChar(2, fullRow);

  lcd.backLight(true); // Backlight ON

  lcd.setCursor(5,0); // Place cursor row 6, 1st line (counting from 0)
  lcd.print("Setup");
  lcd.setCursor(7,1); // Place cursor row 8, 2nd line (counting from 0)
  lcd.print("ok");

  int seed = analogRead(1);
#ifdef DEBUG_TRISCAL
  Serial.print("random seed: ");
  Serial.println(seed);
#endif
  randomSeed(seed);

  gameInit();
}

void loop()
{
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
        currentPiece.rotateClockwise();
      } else if (key == 1) {
        currentPiece.moveUp();
      } else if (key == 2) {
        currentPiece.moveDown();
      } else if (key == 3) {
        currentPiece.rotateCounterClockwise();
      } else if (key == 4) {
        while(currentPiece.moveRight()) {
          delay(100);
        }
      }
    }
  }
  if (counter >= maxCounter) {
    ++levelUp;
    if (levelUp >= 10) {
      levelUp = 0;
      ++level;
      maxCounter *= 0.8;
      if (maxCounter < 40) {
        maxCounter = 40;
      }
#ifdef DEBUG_TRISCAL
      Serial.println("Level up");
#endif
    }
    counter = 0;
    if (!currentPiece.moveRight()) {
      currentPiece.freeze();
#ifdef DEBUG_TRISCAL
      dumpBoard("Board before removing columns");
#endif
      checkFullColumns(currentPiece.column_);
#ifdef DEBUG_TRISCAL
      dumpBoard("Board after removing columns");
#endif
      currentPiece = Piece::newPiece();
      currentPiece.display();
      if (!currentPiece.hasRoom()) {
        gameOver();
        gameInit();
      }
    }
  }
}

