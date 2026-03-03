#include <NewPing>
#include <Servo>

// sonar
const uint8_t TRIG_PIN = ;
const uint8_t ECHO_PIN = ;
const uint8_t MAX_CM = ;
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_CM);

inline uint16_t pingOnceCm() {
  uint16_t d = sonar.ping_cm();
  return d == 0 ? MAX_CM : d;
}

// servo
Servo servo;
const uint8_t SERVO_PIN = ;
const uint8_t L_ANG = 150;
const uint8_t M_ANG = 90;
const uint8_t R_ANG = 30;

void drive(int16_t left, int16_t right) {
  TODO:
}

void stopCar() {
  drive(0, 0);
}

// state machine
enum State : uint8_t {
  ST_FORWARD,
  ST_BACK,
  ST_SCAN_TO_LEFT,
  ST_SCAN_MEASURE_LEFT,
  ST_SCAN_TO_RIGHT,
  ST_SCAN_MEASURE_RIGHT,
  ST_RETURN_CENTER,
  ST_TURN
}

State state = ST_FORWARD;

uint16_t frontCm = MAX_CM;
uint16_t leftCm = MAX_CM;
uint16_t rightCm = MAX_CM;

uint16_t scanAverageCm(uint8_t samples) {
  uint32_t sum = 0;

  for(int i = 0; i < samples; i++) {
    sum += pingOnceCm();
    delay(15);
  }

  return (uint16_t)(sum / samples);
}

void setup() {
  servo.attach(SERVO_PIN);
  servo.write(M_ANG);
  frontCm = MAX_CM;
}

void loop() {
  uint32_t time = millis();
  // if(state == ST_FORWARD) 

  switch (state) {
    case ST_FORWARD: {
      
    }
    // case ...
  }
}
