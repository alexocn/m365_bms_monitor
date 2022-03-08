//for Xiaomi M365 BMS activation

byte buf_30[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x30, 0x0C, 0x9D, 0xFF};
void setup() {
  Serial.begin(115200);
  delay(100);
}
void loop() {
  Serial.write(buf_30,9);
  delay(10);
  while(Serial.available() > 0) Serial.read();
  delay(200);
}
