// Echo data from Serial Monitor back over native USB CDC.

void setup() {
  Serial.begin(115200);
  Serial.println("Type something and press Send:");
}

void loop() {
  while (Serial.available() > 0) {
    Serial.write(Serial.read());
  }
}
