#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "Globals.h"
#include "Button.h"

class Joystick {
public:
  Joystick(Joystick &other) = delete;
  Joystick operator=(const Joystick &) = delete;

  static Joystick* getInstance();
  
  // for holds 
  int getStateX() { return getState(joystickXPin); }
  int getStateY() { return getState(joystickYPin); }
  // for moves / flicks
  int detectMoveX() { return detectMove(joystickXPin, joyMovedX); }
  int detectMoveY() { return detectMove(joystickYPin, joyMovedY); }

  // for presses
  bool getButton() {
    return button.getPress();
  }

  // do nothing until a button press
  void waitForPress() {
    while (!getButton());
  }
  
private:

  Joystick(): button(joystickSWPin) {
    pinMode(joystickXPin, INPUT);
    pinMode(joystickYPin, INPUT);
    joyMovedX = false;
    joyMovedY = false;
  }

  static Joystick *instance;
  
  Button button;
  bool joyMovedX, joyMovedY;
  const int minThreshold = 200, maxThreshold = 800;

  /*
   * detect if the joystick has moved on one of the
   * axes. this will return 0 (false) after a move
   * and until the joystick is reset. IT's NOT A HOLD
   */
  int detectMove(int pin, bool &joyMoved) {
    int state = getState(pin);
    
    if (state == 0) {
      joyMoved = false;
      return 0;
    }

    if (!joyMoved && state != 0) {
      joyMoved = true;
      return state;
    }
    return 0;
  }

  /*
   * detects hold direction on X and Y axis
   */
  int getState(int pin) {
    int val = analogRead(pin);
    if (val >= maxThreshold) {
      return 1; 
    } else if (val <= minThreshold) {
      return -1;
    }
    return 0;
  }
};

Joystick* Joystick::instance = nullptr;

Joystick* Joystick::getInstance() {
  if (instance == nullptr) {
    instance = new Joystick();
  }
  return instance;
}

#endif
