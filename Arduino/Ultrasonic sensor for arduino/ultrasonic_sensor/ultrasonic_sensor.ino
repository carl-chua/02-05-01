#include <NewPing.h>
 
#define TRIGGER_PIN 13
#define ECHO_PIN 12
#define MAX_DISTANCE 200

// NewPing setup of pins and maximum distance
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); 
 
void setup() {
   Serial.begin(9600);
}
 
void loop() {
   delay(50);
   unsigned int distance = 0;
   for (int i = 0; i < 50; i++) {
    delay(10);
    distance += sonar.ping_cm();
   }
   distance = distance/50;
   Serial.print(distance);
   Serial.println("cm");
}
