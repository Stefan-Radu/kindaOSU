#ifndef GAME_H
#define GAME_H

#include <Wire.h>
#include "Joystick.h"
#include "ButtonGroup.h"
#include "Globals.h"
#include "Symbols.h"


class Game {
public:
  Game(int brightness = 2) {
    Wire.begin();
    randomSeed(analogRead(0));

    lc.shutdown(0, false); // turn off power saving, enables display
    lc.setIntensity(0, brightness); // sets brightness (0~15 possible values)
    lc.clearDisplay(0); // clear screen    
    
    startAnimation();
    joystick = Joystick::getInstance();
    buttonGroup = ButtonGroup::getInstance();
  }

  void updateBrightness(int value) {
    if (0 <= value && value <= 15) {
      for (int i = 0; i < lc.getDeviceCount(); ++i) {
        lc.setIntensity(0, value); // sets brightness (0~15 possible values)    
      }
    }
  }

  void updateDifficulty(float d) {
    difficulty = d;
  }

  // fill all the matrix with the given byte
  void setMatrix(byte b) {
    for (int i = 0; i < MATRIX_HEIGHT; ++i) {
      lc.setRow(0, i, b);
    }
  }

  /*
   * modify score and lives based on how well you
   * hit the procedurally generated tiles and sliders
   * based on which buttons are pressed at a time a 
   * distinctive sound will be played
   * display friendly messaged and start / end animations
   */
  int playSurvival() {
    unsigned long startTime = millis();
    clearMatrix();
    
    currentRow = 0, lastStateChange = 0, score = 0;
    int gameDelay = 250, scoreIncrement = difficulty;

    float lives = MAX_LIVES, coeff = difficulty, 
          adder = 0.003, invCoeff = 3 - difficulty;

    updateInGameDisplayedStats(score, lives);
    
    byte sliderLength[MATRIX_WIDTH / 2] = {0, 0, 0, 0};

    while (true) {
      unsigned long timeNow = millis();
      
      if (timeNow - lastStateChange > gameDelay) {
        bool change = false;
        int lastRow = currentRow - MATRIX_HEIGHT + 1;
        if (lastRow < 0) {
          lastRow += MAP_HEIGHT;
        }

        int toneCoeff = 0;
        for (int j = 0; j < MATRIX_WIDTH; j += 2) {
          bool onMatrix = (matrixMap[lastRow] & (1 << j)) != 0;
          if (onMatrix != buttonStates[3 - (j / 2)]) {
            lives = max(0, lives - 1.0 * coeff);
            change = true;
          }
          else if (onMatrix == true) {
            score += scoreIncrement;
            lives = min(MAX_LIVES, lives + 2.0 * invCoeff);
            change = true;
          }
        }

        if (change) {
          updateInGameDisplayedStats(score, lives);
        }

        if (lives <= 0) {
          break;
        }

        currentRow += 1;
        if (currentRow == MAP_HEIGHT) {
          currentRow = 0;
        }
        displayMatrix();
        generateNewLine(coeff, sliderLength);
        
        lastStateChange = timeNow;
        coeff += adder;
        invCoeff = max(0.0, invCoeff - adder);
      }

      buttonGroup->updateAllStates(buttonStates);
      int toneCoeff = 0;
      for (int i = 0; i < ButtonGroup::buttonCount; ++i) {
          toneCoeff <<= 1;
          toneCoeff ^= buttonStates[i];
      }     
      if (toneCoeff) {
        tone(speakerPin, BASE_TONE + TONE_MULTIPLYER * toneCoeff,
            TONE_DURATION);
      }
    }

    endGameAnimation(gameDelay / 4, sliderLength);
    displaySurvivalEndGameStats(score, startTime);
    
    return score;
  }

  /*
   * use data of a song (notes and duration) to display a 
   * guide using vertial bars. hitting the bars plays the
   * corresponding notes
   * use tempo based on difficulty
   * score increment & health drain / heal based on difficulty
   *  and missed hits in on a single bar
   * display appropriate messages & animations at the beggining
   * and end of the game
   * 
   * each update to the game is made at an interval based on tempo & difficulty
   */
  int playSong(byte song, const char *name) {
    clearMatrix();
    int totalBadHits = 0;
    bool finished = false;

    selectSongTransmission(song); {
      currentRow = 0, lastStateChange = 0;
      score = 0, lives = MAX_LIVES;
      
      updateInGameDisplayedStats(score, lives);
  
      byte sliderLength = 0, sliderColumn = 0, 
          melodyDisplayIndex = -1, melodyRefillIndex = 0,
          melodyNoteIndex = -1, lastLitColumnIndex = -1;
  
      bool canRefill = refillMelodyBuffer(melodyRefillIndex);
  
      int badHits = 0, goodHits = 0;
  
      const int tempo = BASE_TEMPO + (difficulty - 2) * TEMPO_MULTIPLYER;
      const int wholeNoteDuration = 60000 / tempo * 4; // 60 s / tempo * 4 beats
      const int melodyBarDuration = wholeNoteDuration / WHOLE_NOTE_BAR_COUNT;
  
      delay(MELODY_INNITIAL_DELAY);
           
      while (true) {
        unsigned long timeNow = millis(); 
        if (timeNow - lastStateChange > melodyBarDuration) {
          int lastRow = currentRow - MATRIX_HEIGHT + 1;
          if (lastRow < 0) {
            lastRow += MAP_HEIGHT;
          }
  
          bool anyLine = false;
          for (int j = 0; j < MATRIX_WIDTH; j += 2) {
            bool onMatrix = (matrixMap[lastRow] & (3 << j)) != 0;
            int columnIndex = 3 - (j / 2);
            // update sound to be played
            if (onMatrix == true) {
              anyLine = true;
              if (columnIndex != lastLitColumnIndex) {
                lastLitColumnIndex = columnIndex;
                melodyNoteIndex += 1;
                if (melodyNoteIndex == MELODY_BUFFER_LENGTH) {
                  melodyNoteIndex = 0;
                }
                updateStats(badHits, goodHits);
                updateInGameDisplayedStats(score, lives);
                totalBadHits += badHits;
                badHits = 0, goodHits = 0;
              }
            }
            // update stats & play sounds
            if (onMatrix != buttonStates[columnIndex]) {
              noTone(speakerPin);
              badHits += 1;
            } else if (onMatrix == true) {
              goodHits += 1;
              // for sound pauses
              if (melodyBuffer[melodyNoteIndex].note != 0) {
                tone(speakerPin, melodyBuffer[melodyNoteIndex].note);
              } else {
                noTone(speakerPin);
              }
            }
          }
  
          if (!anyLine && finished) {
            break;
          }
  
          if (lives <= 0) {
            break;
          }
  
          currentRow += 1;
          if (currentRow == MAP_HEIGHT) {
            currentRow = 0;
          }
          displayMatrix();
          
          if (!finished) {
            finished = generateNewLine(sliderLength, sliderColumn, 
                melodyDisplayIndex, melodyRefillIndex, canRefill);
          } else {
            getAndResetUpdateRow();
          }
          
          lastStateChange = timeNow;
        }
  
        buttonGroup->updateAllStates(buttonStates);
      }
    }

    noTone(speakerPin);
    endGameAnimation(finished);
    songEndMessage(name, finished);
    displaySongEndGameStats(score, totalBadHits);
    clearMatrix();
    
    return score;
  }
  
private:
  
  Joystick* joystick = nullptr;
  ButtonGroup *buttonGroup = nullptr;
  
  float difficulty, score;

  byte matrixMap[MAP_HEIGHT] {
    B00000000,
    B00000000,
    B00000000,
    B00000000,
    B00000000,
    B00000000,
    B00000000,
    B00000000,
    B00000000,
  };

  int currentRow, lives;
  unsigned long lastStateChange;

  struct {
    int note, divider;
  } melodyBuffer[MELODY_BUFFER_LENGTH];
  
  bool buttonStates[ButtonGroup::buttonCount];
/*
 * 
 * ================= Melody Transfer =================
 * 
 */

  /*
   * asks slave to prepare to transmit the next song
   */
  void selectSongTransmission(int song) {
    Wire.beginTransmission(SLAVE_NUMBER);
    Wire.write(song);
    Wire.endTransmission();
  }

  /*
   * Requests melody parts from slave.
   * returns true if there is more to receive
   * and false otherwise
   */
  bool refillMelodyBuffer(byte &melodyBufferIndex) {
    Wire.requestFrom(SLAVE_NUMBER, MELODY_BYTES_TO_RECEIVE);
    while (true) {
      int value;
      Wire.readBytes((byte*)&value, sizeof(value));
      
      if (value == MELODY_SECTION_END) {
        return true;
      } else if (value == MELODY_END) {
        return false;
      }
      
      int note = value, divider;
      Wire.readBytes((byte*)&divider, sizeof(divider));

      melodyBuffer[melodyBufferIndex++] = {
        note, divider
      };
      if (melodyBufferIndex == MELODY_BUFFER_LENGTH) {
        melodyBufferIndex = 0;
      }
    }
  }

/*
 * 
 * ================= Game Logic =================
 * 
 */

  /*
   * used when playing a song
   * some math I found fitting after some minutes
   * of thinking and scribbling on the paper
   */
  void updateStats(float bad, float good) {
    float perc = min(1, bad / (bad + good + 1));
    score += difficulty * (1 - perc);
    lives = min(MAX_LIVES, lives + sqrt((4 - difficulty)) * (2 - perc));
    lives = max(0, lives - difficulty * perc * 5);
  }
  
  /*
   * sets the row to be updated to 0
   * and returns its index
   */
  int getAndResetUpdateRow() {
    int updateRow = currentRow + 1;
    if (updateRow == MAP_HEIGHT) {
      updateRow = 0;
    }
    matrixMap[updateRow] = B00000000;
    return updateRow;
  }

  /*
   * used in survival
   * randomly generates sliders and hit points
   */
  void generateNewLine(float coeff, byte* sliderLength) {
    int updateRow = getAndResetUpdateRow();

    const int threshold = 80;
    for (int i = 0; i < MATRIX_WIDTH; i += 2) {
      if (sliderLength[i / 2] != 0) {
        matrixMap[updateRow] ^= (3 << i);
        --sliderLength[i / 2];
        continue;
      }
      if (random(1000) < 2 * coeff) {
        sliderLength[i / 2] = 2 + random(6);
        continue; 
      }

      if (matrixMap[currentRow] & (3 << i) != 0) {
        continue;
      }
      
      if (random(100) < min(threshold, 7 * coeff)) {
        matrixMap[updateRow] ^= (3 << i);
      }
    };
  }

  /*
   * used when playing a song
   * over multiple iteration displays a bar corresponding to
   * the length of the note that should be played
   * 
   * if the note is a pause, display a dotted bar
   * the length of the bar is based on a predefined length
   * for a wholeNote and the duration divider of the current note
   * 
   * data for the bars is retrieved from the slave which reads
   * it from an sd card. I use a small buffer and hold only partial data
   * the logic for navigating and refiling the buffer is here as well
   */
  bool generateNewLine(byte &sliderLength, byte &sliderColumn, 
      byte &melodyDisplayIndex, byte &melodyRefillIndex, bool &canRefill) {
        
    if (sliderLength == 0) {
      melodyDisplayIndex += 1;
      if (melodyDisplayIndex == MELODY_BUFFER_LENGTH) {
        melodyDisplayIndex = 0;
      }
      if (melodyDisplayIndex == melodyRefillIndex) {
        if (canRefill) {
          canRefill = refillMelodyBuffer(melodyRefillIndex);
        } else {
          return true;
        }
      }
      if (melodyDisplayIndex != melodyRefillIndex) {
        sliderLength = WHOLE_NOTE_BAR_COUNT / 
            abs(melodyBuffer[melodyDisplayIndex].divider);
        if (melodyBuffer[melodyDisplayIndex].divider < 0) {
          sliderLength += sliderLength / 2;
        }
        int newColumn = melodyBuffer[melodyDisplayIndex].note % 4;
        if (newColumn == sliderColumn) {
          newColumn = (newColumn + 1) % 4;
        }
        sliderColumn = newColumn;
      } else {
        return true;
      }
    }
    
    --sliderLength;
    byte litBits = 0;
    int updateRow = getAndResetUpdateRow();
    if (melodyBuffer[melodyDisplayIndex].note == 0) {
      if (updateRow & 1) {
        litBits = 1;
      } else {
        litBits = 2;
      }
    } else {
      litBits = 3;
    } 
    matrixMap[updateRow] ^= (litBits << ((3 - sliderColumn) * 2));
    
    return false;
  }

/*
 * 
 * ================= Animations & Display =================
 * 
 */

  /*
   * matrixMap works in reverse, so I fill each
   * row going forwards in the matrix and
   * backwards in the map
   */
  void displayMatrix() {
    int index = currentRow;
    for (int i = 0; i < MATRIX_HEIGHT; ++i) {
      lc.setRow(0, i, matrixMap[index]);
      --index;
      if (index == -1) {
        index = MAP_HEIGHT - 1;
      }
    }
  }

  void clearMatrix() {
    for (int i = 0; i < MAP_HEIGHT; ++i) {
      matrixMap[i] = B00000000;
    }
    lc.clearDisplay(0);
  }

  void displayIndentedMessage(int row, String prefix, int &value, String suffix = "") {
    lcd.setCursor(2, row);
    lcd.print(prefix);
    lcd.print(value);
    lcd.print(suffix);
  }
 
  void updateInGameDisplayedStats(int s, int l) {
    lcd.clear();
    displayIndentedMessage(1, F("Lives: "), l);
    displayIndentedMessage(0, F("Score: "), s);
  }

  void displaySurvivalEndGameStats(int score, unsigned long &startTime) {
    lcd.clear();
    displayIndentedMessage(0, F("Score: "), score);
    int duration = (millis() - startTime) / 1000;
    displayIndentedMessage(1, F("Duration: "), duration, "s");
    joystick->waitForPress();
  }

  void displaySongEndGameStats(int score, int badHits) {
    lcd.clear();
    displayIndentedMessage(0, F("Score: "), score);
    displayIndentedMessage(1, F("Bad hits: "), badHits);
    joystick->waitForPress();
  }

  /*
   * used for Survival
   * go through a few of the future positions at a
   * faster rate
   * then clear the matrix
   */
  void endGameAnimation(int d, byte *sliderLength) {
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print(F("Game Over!"));
    lcd.setCursor(2, 1);
    lcd.print(F("Not TOO bad!"));
    
    for (int i = 0; i < MAP_HEIGHT * 20; ++i) {
      currentRow += 1;
      if (currentRow == MAP_HEIGHT) {
        currentRow = 0;
      }
      generateNewLine(8, sliderLength);
      displayMatrix();
      delay(d);
    }
    for (int i = 0; i < MAP_HEIGHT; ++i) {
      currentRow += 1;
      if (currentRow == MAP_HEIGHT) {
        currentRow = 0;
      }
      int updateRow = currentRow + 1;
      if (updateRow == MAP_HEIGHT) {
        updateRow = 0;
      }
      matrixMap[updateRow] = B00000000;
      displayMatrix();
      delay(d);
    }
    joystick->waitForPress();
  }

  /*
   * used for SONG
   * falling horizontal bars
   * clear
   * then show smiley for win and 
   * sad face for lose
   */
  void endGameAnimation(bool won) {
    for (int i = 0; i < MATRIX_HEIGHT * 3; ++i) {
      currentRow += 1;
      if (currentRow == MAP_HEIGHT) {
        currentRow = 0;
      }
      int updateRow = currentRow + 1;
      if (updateRow == MAP_HEIGHT) {
        updateRow = 0;
      }
      if (i % 2 == 0 || i >= MATRIX_HEIGHT) {
        matrixMap[updateRow] = B00000000;
      } else {
        matrixMap[updateRow] = B11111111;
      }
      displayMatrix();
      delay(ANIMATION_DELAY);
    }

    Symbols symbols;
    symbols.smiley(won);
  }

  void songEndMessage(const char *name, bool win) {
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print(F("Game Over!"));
    if (win) {
      lcd.setCursor(1, 1);
      lcd.print(F("Congratz "));
      lcd.print(name);
    }
    else {
      lcd.setCursor(0, 1);
      lcd.print(F("Quite bad "));
      lcd.print(name);
    }
    joystick->waitForPress();
  }

  /*
   * fill every cell in order
   * shut every cell in order
   * small delay
   */
  void startAnimation() {
    for (int i = 0; i < MATRIX_HEIGHT; ++i) {
      for (int j = 0; j < MATRIX_WIDTH; ++ j) {
        lc.setLed(0, i, j, true);
        delay(ANIMATION_DELAY_SMALL);
      }
    }
  
    for (int i = 0; i < MATRIX_HEIGHT; ++i) {
      for (int j = 0; j < MATRIX_WIDTH; ++ j) {
        lc.setLed(0, i, j, false);
        delay(ANIMATION_DELAY_SMALL);
      }
    }
  }
};

#endif
