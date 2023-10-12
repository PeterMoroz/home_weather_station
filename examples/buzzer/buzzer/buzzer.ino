void setup() {
  // put your setup code here, to run once:
  pinMode(D5, OUTPUT);
}

void tone(unsigned d) {
  digitalWrite(D5, HIGH);
  delay(d);
  digitalWrite(D5, LOW);
  delay(d);  
}

void loop() {
  // put your main code here, to run repeatedly:
  for (int i = 0; i < 10; i++) {
    tone(500);
  }

  for (int i = 0; i < 10; i++) {
    tone(1000);
  } 

}
