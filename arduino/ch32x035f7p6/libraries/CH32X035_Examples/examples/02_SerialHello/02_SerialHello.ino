// Print a counter over the board's native USB CDC serial port.
// Open Tools > Serial Monitor at any baud rate after uploading.

unsigned long count = 0;

void setup() {
  Serial.begin(115200);  // USB CDC ignores the baud rate.
  Serial.println("CH32X035 USB Serial is ready");
}

void loop() {
  Serial.print("count = ");
  Serial.println(count++);
  delay(1000);
}
