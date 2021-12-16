#ifndef BUTTON_H
#define BUTTON_H

class Button {
public:

  Button(int p): buttonPin(p) {
    pinMode(buttonPin, INPUT_PULLUP);
    lastDebounceTime = millis();
    buttonState = 1; // pullup so idle is 1
    lastButtonReading = 1;
  }

  bool getState() {
    updateState();
    // make the switch so 1 is pressed
    return !buttonState;
  }

private:

  static const int debounceDelay = 50; // ms
  
  const int buttonPin;
  unsigned long lastDebounceTime;
  bool lastButtonReading, buttonState;

  void updateState() {
    int reading = digitalRead(buttonPin);
    if (reading != lastButtonReading) {
      lastDebounceTime = millis();
    }
    lastButtonReading = reading;
    
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (reading != buttonState) {
        buttonState = reading;
      }
    }
  }
};

#endif