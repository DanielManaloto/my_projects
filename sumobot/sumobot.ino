#include <Ultrasonic.h>
#define IN1 8
#define IN2 7
#define IN3 6
#define IN4 5
Ultrasonic ultrasonic(13, 12);
int distance;

int irPin1 = 10;     // IR sensor 1 pin (front sensor)
int irPin2 = 11;    // IR sensor 2 pin (back sensor)
int irValue1 = 0;   // Value read from IR sensor 1
int irValue2 = 0;   // Value read from IR sensor 2


void setup() {
  Serial.begin(9600);
  delay (5000); // as per sumo compat roles
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(irPin1, INPUT);  // Set IR sensor 1 pin as input
  pinMode(irPin2, INPUT);  // Set IR sensor 2 pin as input

}

void loop() {
  distance = ultrasonic.read();
  irValue1 = digitalRead(irPin1); // Read the value from IR sensor 1
  irValue2 = digitalRead(irPin2); // Read the value from IR sensor 2
  Serial.print("IR1: ");
  Serial.print(irValue1);
  Serial.print(" IR2: ");
  Serial.print(irValue2);
  Serial.print(" Dist1: ");
  Serial.print(distance);
  
  // Distance Sensor Logic
  while (distance <= 40) { // Move Forward if distance less than 40
    distance = ultrasonic.read();
    irValue1 = digitalRead(irPin1); // Read the value from IR sensor 1
    irValue2 = digitalRead(irPin2); // Read the value from IR sensor 2
    // Edge detection logic
    if (irValue1 == 0 && irValue2 == 1) {
      Stop();
      BACKWARD(255);
      delay(750);
    } 
    else if (irValue2 == 0 && irValue1 == 1) {
      Stop();
      FORWARD(255);
      delay(750);
    } 
    else {
      FORWARD(255);
      delay(250);
    }
  }
  ROTATE_LEFT(255);
  delay(1);
}

void FORWARD (int Speed){
  analogWrite(IN1,Speed);
  analogWrite(IN2,0);
  analogWrite(IN3,Speed);
  analogWrite(IN4,0);
}

void BACKWARD (int Speed){
  analogWrite(IN1,0);
  analogWrite(IN2,Speed);
  analogWrite(IN3,0);
  analogWrite(IN4,Speed);
}

void ROTATE_LEFT (int Speed){

  analogWrite(IN1,0);
  analogWrite(IN2,Speed);
  analogWrite(IN3,Speed);
  analogWrite(IN4,0);

}
void ROTATE_RIGHT (int Speed){
  analogWrite(IN1,Speed);
  analogWrite(IN2,0);
  analogWrite(IN3,0);
  analogWrite(IN4,Speed);
}

void Stop(){
  analogWrite(IN1,0);
  analogWrite(IN2,0);
  analogWrite(IN3,0);
  analogWrite(IN4,0);
}