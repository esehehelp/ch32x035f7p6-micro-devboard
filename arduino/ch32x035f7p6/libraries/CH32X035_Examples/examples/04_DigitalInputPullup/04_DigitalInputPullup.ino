// Connect D0 (PA0) to GND. The on-board LED turns on while D0 is low.

const uint8_t inputPin = PA0;  // PA0 and D0 are the same Arduino pin.

void setup() {
  pinMode(inputPin, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, digitalRead(inputPin) == LOW ? HIGH : LOW);
}
