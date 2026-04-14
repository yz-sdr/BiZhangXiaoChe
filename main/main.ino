#include <NewPing.h>

// =========================
// 超声波引脚
// =========================
const uint8_t L_TRIG_PIN = 8;
const uint8_t L_ECHO_PIN = 9;
const uint8_t R_TRIG_PIN = 10;
const uint8_t R_ECHO_PIN = 11;

const uint16_t MAX_CM = 200;

NewPing sonarL(L_TRIG_PIN, L_ECHO_PIN, MAX_CM);
NewPing sonarR(R_TRIG_PIN, R_ECHO_PIN, MAX_CM);

// =========================
// 板载电机驱动引脚（UNO-C）
// A组=左轮，B组=右轮
// =========================
const uint8_t A_DIR_PIN = 7;
const uint8_t A_PWM_PIN = 6;
const uint8_t B_DIR_PIN = 4;
const uint8_t B_PWM_PIN = 5;

// 如果方向不对，只改这里
const int8_t LEFT_DIR_SIGN  = 1;
const int8_t RIGHT_DIR_SIGN = -1;

// =========================
// 参数
// =========================
const int16_t FORWARD_SPEED    = 255;   // 原来255，先降一点更稳
const int16_t SOFT_TURN_DELTA  = 110;
const int16_t BOTH_TURN_DELTA  = 150;
const int16_t HARD_TURN_INNER  = 0;     // 不再倒车，急转时内侧轮停转

const uint16_t HARD_TURN_CM = 15;       // 原来15，提前一点开始急转
const uint16_t CAUTION_CM   = 26;       // 原来23，提前一点开始修正
const uint16_t DIFF_CM      = 8;

const uint16_t SENSOR_MS       = 20;    // 更快刷新
const uint16_t HARD_TURN_HOLD  = 140;   // 急转锁定时间，防止墙角抖动

// =========================
// 全局变量
// =========================
uint16_t leftCm  = MAX_CM;
uint16_t rightCm = MAX_CM;
uint32_t lastSenseMs = 0;
int8_t lastTurnDir = 1;   // 1=右转, -1=左转


bool cornerLocked = false;   // 这次拐弯方向是否已锁定
int8_t cornerDir = 0;        // -1=左转, 1=右转

bool hardTurning = false;
int8_t hardTurnDir = 1;   // 1=右转, -1=左转
uint32_t hardTurnUntil = 0;

// =========================
// 超声波函数
// =========================
uint16_t pingOnceCm(NewPing &sonar) {
  uint16_t d = sonar.ping_cm();
  return (d == 0) ? MAX_CM : d;
}

void readSensors() {
  leftCm = pingOnceCm(sonarL);
  delay(6);
  rightCm = pingOnceCm(sonarR);
}
/*
void updatePreferredTurnDir() {
  // 用一个很小的门槛，尽量早点记住开口方向
  if (leftCm > rightCm + 2) {
    preferredTurnDir = -1;   // 左边更空 -> 以后优先左转
  } else if (rightCm > leftCm + 2) {
    preferredTurnDir = 1;    // 右边更空 -> 以后优先右转
  }
}
*/

void updateCornerLock() {
  // 只有还没锁定时，才允许决定这次拐弯方向
  if (!cornerLocked && !hardTurning && (leftCm <= CAUTION_CM || rightCm <= CAUTION_CM)) {
    if (leftCm > rightCm + DIFF_CM) {
      cornerDir = -1;      // 左边更空 -> 锁定左转
      cornerLocked = true;
    } 
    else if (rightCm > leftCm + DIFF_CM) {
      cornerDir = 1;       // 右边更空 -> 锁定右转
      cornerLocked = true;
    }
  }

  // 回到比较开阔时，才允许为下一个弯重新判断
  if (!hardTurning && leftCm > CAUTION_CM + 8 && rightCm > CAUTION_CM + 8) {
    cornerLocked = false;
    cornerDir = 0;
  }
}



// =========================
// 电机控制
// speedVal: -255 ~ 255
// 正数前进，负数后退
// =========================
void driveMotor(uint8_t dirPin, uint8_t pwmPin, int16_t speedVal) {
  speedVal = constrain(speedVal, -255, 255);

  if (speedVal > 0) {
    digitalWrite(dirPin, HIGH);
    analogWrite(pwmPin, speedVal);
  } else if (speedVal < 0) {
    digitalWrite(dirPin, LOW);
    analogWrite(pwmPin, -speedVal);
  } else {
    analogWrite(pwmPin, 0);
  }
}

void drive(int16_t left, int16_t right) {
  left  *= LEFT_DIR_SIGN;
  right *= RIGHT_DIR_SIGN;

  driveMotor(A_DIR_PIN, A_PWM_PIN, left);
  driveMotor(B_DIR_PIN, B_PWM_PIN, right);
}

void stopCar() {
  drive(0, 0);
}

void startHardTurn(int8_t dir, uint32_t now) {
  hardTurning = true;
  hardTurnDir = dir;
  hardTurnUntil = now + HARD_TURN_HOLD;
  lastTurnDir = dir;

  cornerLocked = false;
  cornerDir = 0;
}


void applyHardTurn() {
  if (hardTurnDir > 0) {
    // 右转：左轮前进，右轮停
    drive(FORWARD_SPEED, HARD_TURN_INNER);
  } else {
    // 左转：左轮停，右轮前进
    drive(HARD_TURN_INNER, FORWARD_SPEED);
  }
}

// =========================
// 主程序
// =========================
void setup() {
  pinMode(A_DIR_PIN, OUTPUT);
  pinMode(A_PWM_PIN, OUTPUT);
  pinMode(B_DIR_PIN, OUTPUT);
  pinMode(B_PWM_PIN, OUTPUT);

  stopCar();

  Serial.begin(9600);
  delay(200);

  readSensors();
  lastSenseMs = millis();
}

void loop() {
  uint32_t now = millis();

  if (now - lastSenseMs < SENSOR_MS) return;
  lastSenseMs = now;

  readSensors();
  // updatePreferredTurnDir();

  updateCornerLock();

  Serial.print("L=");
  Serial.print(leftCm);
  Serial.print("cm, R=");
  Serial.print(rightCm);
  Serial.println("cm");

  int diff = (int)leftCm - (int)rightCm;

  // 如果正在急转，先保持一小段时间，不要立刻改判
  if (hardTurning) {
    // 正在右转，但左边突然明显更空 -> 说明转错了，取消锁定
    if (hardTurnDir > 0 && leftCm > rightCm + DIFF_CM + 2) {
      hardTurning = false;
    }
    // 正在左转，但右边突然明显更空 -> 说明转错了，取消锁定
    else if (hardTurnDir < 0 && rightCm > leftCm + DIFF_CM + 2) {
      hardTurning = false;
    }
    else if (now < hardTurnUntil) {
      applyHardTurn();
      return;
    } else {
      hardTurning = false;
    }
  }


  // 1. 两边都很近：前方大概率是墙，强制选一个方向持续急转
  if (leftCm <= HARD_TURN_CM && rightCm <= HARD_TURN_CM) {
    if (cornerLocked && cornerDir != 0) {
      startHardTurn(cornerDir, now);    // 优先使用提前锁定的方向
    } 
    else if (diff < -DIFF_CM) {
      startHardTurn(1, now);            // 右转
    } 
    else if (diff > DIFF_CM) {
      startHardTurn(-1, now);           // 左转
    } 
    else {
      startHardTurn(lastTurnDir, now);  // 最后兜底
    }
    applyHardTurn();
  }


  // 2. 左边非常近：强力向右
  else if (leftCm <= HARD_TURN_CM) {
    if (cornerLocked && cornerDir != 0) {
      startHardTurn(cornerDir, now);
    } else {
      startHardTurn(1, now);
    }
    applyHardTurn();
  }

  else if (rightCm <= HARD_TURN_CM) {
    if (cornerLocked && cornerDir != 0) {
      startHardTurn(cornerDir, now);
    } else {
      startHardTurn(-1, now);
    }
    applyHardTurn();
  }

  // 4. 左边较近：轻微向右修正
  else if (leftCm <= CAUTION_CM && rightCm > CAUTION_CM) {
    drive(FORWARD_SPEED, FORWARD_SPEED - SOFT_TURN_DELTA);
    lastTurnDir = 1;
  }

  // 5. 右边较近：轻微向左修正
  else if (rightCm <= CAUTION_CM && leftCm > CAUTION_CM) {
    drive(FORWARD_SPEED - SOFT_TURN_DELTA, FORWARD_SPEED);
    lastTurnDir = -1;
  }

  // 6. 两边都较近：按更空的一边修正，但不倒车
  else if (leftCm <= CAUTION_CM && rightCm <= CAUTION_CM) {
    if (cornerLocked && cornerDir != 0) {
      if (cornerDir < 0) {
        drive(FORWARD_SPEED - BOTH_TURN_DELTA, FORWARD_SPEED); // 左转预备
        lastTurnDir = -1;
      } else {
        drive(FORWARD_SPEED, FORWARD_SPEED - BOTH_TURN_DELTA); // 右转预备
        lastTurnDir = 1;
      }
    } 
    else if (diff < 0) {
      drive(FORWARD_SPEED, FORWARD_SPEED - BOTH_TURN_DELTA);
      lastTurnDir = 1;
    } 
    else {
      drive(FORWARD_SPEED - BOTH_TURN_DELTA, FORWARD_SPEED);
      lastTurnDir = -1;
    }
  }

  // 7. 通畅：直行
  else {
    drive(FORWARD_SPEED, FORWARD_SPEED);
  }
}
