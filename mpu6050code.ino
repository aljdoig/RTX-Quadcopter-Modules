
void handleMarkerLed(unsigned long now) {
  if (markerFlashing && now - markerFlashStart >= markerFlashDuration) {
    digitalWrite(breakLedPin, LOW);
    markerFlashing = false;
  }
}

float measureDistanceCm() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(3);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, 25000UL);
  if (duration == 0) return -1.0;
  return duration * 0.0343f / 2.0f;
}

void updateDistanceMode(unsigned long now) {
  if (activeMode != DISTANCE_MODE) {
    digitalWrite(distanceLedPin, LOW);
    distanceLedOn = false;
    return;
  }

  if (now - lastDistanceRead >= distanceReadInterval) {
    lastDistanceRead = now;
    distanceCm = measureDistanceCm();
    Serial.print(F("Distance: "));
    if (distanceCm < 0) {
      Serial.println(F("no echo"));
    } else {
      Serial.print(distanceCm, 1);
      Serial.println(F(" cm"));
    }
  }

  float bounded = distanceCm < 0 ? maxDistanceCm : distanceCm;
  if (bounded > maxDistanceCm) bounded = maxDistanceCm;
  if (bounded < 2.0f) bounded = 2.0f;

  unsigned long halfPeriod = map((long)(bounded * 10), 20,
                                  maxDistanceCm * 10, 60, 1000);
  halfPeriod = constrain(halfPeriod, 60, 1000);
  if (now - lastDistanceFlash >= halfPeriod) {
    lastDistanceFlash = now;
    distanceLedOn = !distanceLedOn;
    digitalWrite(distanceLedPin, distanceLedOn ? HIGH : LOW);
  }
}

void readMPU6050() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) {
    Serial.println(F("MPU6050 READ ERROR"));
    return;
  }

  byte received = Wire.requestFrom(MPU_ADDR, (byte)14, (byte)true);
  if (received != 14) {
    Serial.println(F("MPU6050 READ ERROR"));
    while (Wire.available()) Wire.read();
    return;
  }

  int16_t ax = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t ay = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t az = ((int16_t)Wire.read() << 8) | Wire.read();
  Wire.read(); Wire.read(); // Skip temperature bytes
  int16_t gx = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t gy = ((int16_t)Wire.read() << 8) | Wire.read();
  int16_t gz = ((int16_t)Wire.read() << 8) | Wire.read();

  Serial.print(millis()); Serial.print(',');
  Serial.print(ax / 16384.0f, 3); Serial.print(',');
  Serial.print(ay / 16384.0f, 3); Serial.print(',');
  Serial.print(az / 16384.0f, 3); Serial.print(',');
  Serial.print(gx / 131.0f, 3); Serial.print(',');
  Serial.print(gy / 131.0f, 3); Serial.print(',');
  Serial.println(gz / 131.0f, 3);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(recordButtonPin, INPUT_PULLUP);
  pinMode(modeButtonPin, INPUT_PULLUP);
  pinMode(breakButtonPin, INPUT_PULLUP);
  pinMode(recordLedPin, OUTPUT);
  pinMode(breakLedPin, OUTPUT);
  pinMode(distanceLedPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  digitalWrite(recordLedPin, LOW);
  digitalWrite(breakLedPin, LOW);
  digitalWrite(distanceLedPin, LOW);
  digitalWrite(trigPin, LOW);

  // Wake MPU6050 from sleep.
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  byte mpuStatus = Wire.endTransmission();

  if (mpuStatus == 0) Serial.println(F("MPU6050 ready (SDA=A4, SCL=A5)."));
  else Serial.println(F("MPU6050 not responding; check power and I2C wiring."));

  Serial.println(F("RECORD mode active. D11 starts/stops recording; D10 adds a numbered break."));
  Serial.println(F("When stopped, press D12 to switch RECORD/DISTANCE modes."));
}

void loop() {
  unsigned long now = millis();

  handleRecordButton(now);
  handleModeButton(now);
  handleBreakButton(now);
  handleMarkerLed(now);
  updateDistanceMode(now);

  if (activeMode == RECORD_MODE && recording && now - lastSample >= sampleInterval) {
    lastSample = now;
    readMPU6050();