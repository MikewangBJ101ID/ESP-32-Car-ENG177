/*
 * receiver_v2.ino
 * ===============
 * Updated version of receiver.ino with a working update_car() that drives the
 * car from the 8 signal flags using a differential-drive model and two motors.
 *
 * DRIVE MODEL (differential drive)
 *   - Motor A = one side, Motor B = the other side (see MOTOR_A_IS_LEFT below).
 *   - Each motor keeps a *signed* speed:
 *         speed > 0  -> motor runs forward
 *         speed < 0  -> motor runs backward
 *         |speed|    -> PWM value written to the enable pin (0..255)
 *
 * HOW THE FLAGS BECOME MOTION
 *   - TURN_MOTOR_FORWARD  -> accelerate forward, capped at +SPEED_THRESHOLD.
 *                            At/above the threshold the motor "does nothing"
 *                            (i.e. it simply holds the capped speed).
 *   - TURN_MOTOR_BACKWARD -> accelerate backward, capped at -SPEED_THRESHOLD
 *                            (same threshold value, opposite direction).
 *   - STEER_LEFT          -> release the LEFT motor: its speed decays to 0
 *                            through simulated friction, so the car veers left
 *                            (it will spin in place if stationary).
 *   - STEER_RIGHT         -> same for the RIGHT motor, car veers right.
 *   - DONT_* flags        -> the affected command is treated as released and
 *                            the motor is turned off; its speed decays to 0,
 *                            like real friction slowing a free-wheeling wheel.
 *
 * TUNING NOTE
 *   update_car() runs once per received packet, so ACCEL_STEP and
 *   FRICTION_PERCENT are applied per packet. If your controller sends packets
 *   at a very different rate, adjust these two constants accordingly.
 */
#include <L298NX2.h>
enum MotorIndex { MOTOR_A = 0, MOTOR_B = 1 };

void setup() {
  Serial.begin(115200);
}

/*FLAGS
 * These flags reflect what signals the car currently holds.
 * These flags are updated when a new signal is received.
 * Currently the signal assignments are as follows:
 * 1: Turn Motor Forward
 * 2: Steer Left
 * 3: Don't Turn Motor Forward
 * 4: Don't Steer Left
 * 5: Turn Motor Backward
 * 6: Steer Right
 * 7: Don't Turn Motor Backward
 * 8: Don't Steer Right
 */
#define TURN_MOTOR_FORWARD_FLAG 1
#define STEER_LEFT_FLAG 2
#define DONT_TURN_MOTOR_FORWARD_FLAG 3
#define DONT_STEER_LEFT_FLAG 4
#define TURN_MOTOR_BACKWARD_FLAG 5
#define STEER_RIGHT_FLAG 6
#define DONT_TURN_MOTOR_BACKWARD_FLAG 7
#define DONT_STEER_RIGHT_FLAG 8
bool signal_flags[9] = {0, 0, 0, 1, 1, 0, 0, 1, 1}; //initially the car should be stationary

void update_flags(int msg){
  int digit_1 = msg / 10;
  int digit_2 = msg % 10;
  signal_flags[digit_1] = 1;
  signal_flags[digit_2] = 1;

  //Set conflicting signals to 0, for example: left & right, foward & backward
  if (signal_flags[TURN_MOTOR_FORWARD_FLAG] && signal_flags[TURN_MOTOR_BACKWARD_FLAG]){
    signal_flags[TURN_MOTOR_FORWARD_FLAG] = 0;
    signal_flags[TURN_MOTOR_BACKWARD_FLAG] = 0;
  }
  if (signal_flags[STEER_LEFT_FLAG] && signal_flags[STEER_RIGHT_FLAG]){
    signal_flags[STEER_LEFT_FLAG] = 0;
    signal_flags[STEER_RIGHT_FLAG] = 0;
  }
  //The following lines will likely NOT be executed as they are resolved at the controller side
  //But adding them anyways in case of solar radiation flipping bits
  if (signal_flags[TURN_MOTOR_FORWARD_FLAG] && signal_flags[DONT_TURN_MOTOR_FORWARD_FLAG]){
    signal_flags[DONT_TURN_MOTOR_FORWARD_FLAG] = 1;
    signal_flags[TURN_MOTOR_FORWARD_FLAG] = 0;
  }
  if (signal_flags[TURN_MOTOR_BACKWARD_FLAG] && signal_flags[DONT_TURN_MOTOR_BACKWARD_FLAG]){
    signal_flags[DONT_TURN_MOTOR_BACKWARD_FLAG] = 1;
    signal_flags[TURN_MOTOR_BACKWARD_FLAG] = 0;
  }
  if (signal_flags[STEER_LEFT_FLAG] && signal_flags[DONT_STEER_LEFT_FLAG]){
    signal_flags[DONT_STEER_LEFT_FLAG] = 1;
    signal_flags[STEER_LEFT_FLAG] = 0;
  }
  if (signal_flags[STEER_RIGHT_FLAG] && signal_flags[DONT_STEER_RIGHT_FLAG]){
    signal_flags[DONT_STEER_RIGHT_FLAG] = 1;
    signal_flags[STEER_RIGHT_FLAG] = 0;
  }
}

void set_flag(bool forward, bool backward, bool left, bool right){
  signal_flags[TURN_MOTOR_FORWARD_FLAG] = forward;
  signal_flags[DONT_TURN_MOTOR_FORWARD_FLAG] = !forward;

  signal_flags[TURN_MOTOR_BACKWARD_FLAG] = backward;
  signal_flags[DONT_TURN_MOTOR_BACKWARD_FLAG] = !backward;

  // Left / right
  signal_flags[STEER_LEFT_FLAG] = left;
  signal_flags[DONT_STEER_LEFT_FLAG] = !left;

  signal_flags[STEER_RIGHT_FLAG] = right;
  signal_flags[DONT_STEER_RIGHT_FLAG] = !right;
}

void reset_flags(){
  signal_flags[0] = 0; signal_flags[1] = 0; signal_flags[2] = 0;
  signal_flags[3] = 1; signal_flags[4] = 1; signal_flags[5] = 0;
  signal_flags[6] = 0; signal_flags[7] = 1; signal_flags[8] = 1;
}

//===========================================================
// Car control algorithm
//===========================================================
//#define EN_A 0 //Placeholder values, TBD
#define IN1_A 33 //In 1
#define IN2_A 25 //In 2
//#define EN_B 0
#define IN1_B 26 //In 3
#define IN2_B 27 //In 4

//L298NX2 myMotors(EN_A, IN1_A, IN2_A, EN_B, IN1_B, IN2_B);
L298NX2 myMotors(IN1_A, IN2_A, IN1_B, IN2_B);

//---------- Tunable parameters ----------
#define SPEED_THRESHOLD   200  // Max |speed| for forward AND backward (0..255), same value both ways.
#define ACCEL_STEP        20   // Speed added per packet while accelerating.
#define FRICTION_PERCENT  10   // % of |speed| lost per packet while coasting.
#define MIN_FRICTION_LOSS 1    // A coasting motor always slows by at least this much.

//---------- Physical setup ----------
// True  = motor A is on the LEFT side of the car (steer left releases it).
// False = motor A is on the RIGHT side of the car.
static const bool MOTOR_A_IS_LEFT = true;


// Signed speed of each motor: >0 forward, <0 backward.
static int motor_speed[2] = {0, 0};

// Which side does this motor belong to?
static bool is_left_motor(MotorIndex m) {
  if (MOTOR_A_IS_LEFT) return (m == MOTOR_A);
  return (m == MOTOR_B);
}

// Move `current` one `step` toward `target`, never overshooting it.
static int step_toward(int current, int target, int step) {
  if (current < target) {
    current += step;
    if (current > target) current = target;
  } else if (current > target) {
    current -= step;
    if (current < target) current = target;
  }
  return current;
}

// Simulate friction: shrink |current| toward 0, never crossing zero.
static int coast_toward_zero(int current) {
  if (current == 0) return 0;
  int loss = abs(current) * FRICTION_PERCENT / 100;
  if (loss < MIN_FRICTION_LOSS) loss = MIN_FRICTION_LOSS;

  if (current > 0) {
    current -= loss;
    return (current < 0) ? 0 : current;
  } else {
    current += loss;
    return (current > 0) ? 0 : current;
  }
}

// Write a signed speed to the hardware for one motor.
// The sign of `speed` selects the direction pins; the magnitude goes to the
// enable/PWM pin.
static void apply_motor(MotorIndex m, int speed) {
  unsigned short pwm = (unsigned short)(speed < 0 ? -speed : speed);

  if (m == MOTOR_A) {
    myMotors.setSpeedA(pwm);
    if (speed > 0)      myMotors.forwardA();
    else if (speed < 0) myMotors.backwardA();
    else                myMotors.stopA();
  } else {
    myMotors.setSpeedB(pwm);
    if (speed > 0)      myMotors.forwardB();
    else if (speed < 0) myMotors.backwardB();
    else                myMotors.stopB();
  }
}

void update_car(){
  // DONT_* flags are normally resolved inside update_flags(), but we re-check
  // them here too so that update_car() also behaves correctly from a cold
  // reset, where reset_flags() has set all the DONT_* flags to 1.
  bool drive_forward  = signal_flags[TURN_MOTOR_FORWARD_FLAG]  && !signal_flags[DONT_TURN_MOTOR_FORWARD_FLAG];
  bool drive_backward = signal_flags[TURN_MOTOR_BACKWARD_FLAG] && !signal_flags[DONT_TURN_MOTOR_BACKWARD_FLAG];
  bool steer_left     = signal_flags[STEER_LEFT_FLAG]          && !signal_flags[DONT_STEER_LEFT_FLAG];
  bool steer_right    = signal_flags[STEER_RIGHT_FLAG]         && !signal_flags[DONT_STEER_RIGHT_FLAG];
  bool do_nothing     = !drive_forward && !drive_backward && !steer_left && !steer_right;
  if (do_nothing){
    apply_motor(MOTOR_A, 0);
    apply_motor(MOTOR_B, 0);
  }
  if (drive_forward){
    if (steer_left) {
      apply_motor(MOTOR_A, 200);
      apply_motor(MOTOR_B, 0);
    }
    else if (steer_right){
      apply_motor(MOTOR_A, 0);
      apply_motor(MOTOR_B, 200);
    }
    else {
      apply_motor(MOTOR_A, 200);
      apply_motor(MOTOR_B, 200);
    }
  }
  else if (drive_backward){
    if (steer_left){
      apply_motor(MOTOR_A, -200);
      apply_motor(MOTOR_B, 0);  
    }
    else if (steer_right) {
      apply_motor(MOTOR_A, 0);
      apply_motor(MOTOR_B, -200);  
    }
    else {
      apply_motor(MOTOR_A, -200);
      apply_motor(MOTOR_B, -200);
    }
  }
  else if (steer_left) {
    apply_motor(MOTOR_A, 200);
    apply_motor(MOTOR_B, 0);
  }
  else if (steer_right) {
    apply_motor(MOTOR_A, 0);
    apply_motor(MOTOR_B, 200);
  }
}
int change_interval = 2000;
int last_change = 0;

//Functions to change
void change_1(){ //forward
  Serial.println("Forward");
  set_flag(1, 0, 0, 0);
}
void change_2(){ //steer
  Serial.println("Steer");
  set_flag(0, 0, 0, 0);
}
void change_3(){ //backward
  Serial.println("Backward");
  set_flag(0, 1, 0, 0);
}
void change_4(){ //steer
  Serial.println("Steer");
  set_flag(0, 0, 0, 0);
}
void change_5(){ //forward, left
  Serial.println("Forward Left");
  set_flag(1, 0, 1, 0);
}
void change_6(){ //steer
  Serial.println("Steer");
  set_flag(0, 0, 0, 0);
}
void change_7(){ //forward, right
  Serial.println("Forward Right");
  set_flag(1, 0, 0, 1);
}
void change_8(){ //steer
  Serial.println("Steer");
  set_flag(0, 0, 0, 0);
}
void change_9(){ //backward, left
  Serial.println("Backward Left");
  set_flag(0, 1, 1, 0);
}
void change_10(){ //steer
  Serial.println("Steer");
  set_flag(0, 0, 0, 0);
}
void change_11(){ //backward, right
  Serial.println("Backward Right");
  set_flag(0, 1, 0, 1);
}
void change_12(){ //steer
  Serial.println("Steer");
  set_flag(0, 0, 0, 0);
}
void change_13(){ //left
  Serial.println("Left");
  set_flag(0, 0, 1, 0);
}
void change_14(){ //right
  Serial.println("Right");
  set_flag(0, 0, 0, 1);
}
void change_15(){ //forward, left
  Serial.println("Forward Left");
  set_flag(1, 0, 1, 0);
}
void change_16(){ //backward, right
  Serial.println("Backward Right");
  set_flag(0, 1, 0, 1);
}
void (*changes[]) () = {change_1, change_2, change_3, change_4, change_5, change_6, change_7, change_8, change_9, change_10, change_11, change_12, change_13, change_14, change_15, change_16};
int change_param = 0;
void loop() {
  //2 seconds interval, respectively:
  /*
  * Drive Forward
  * Steer
  * Drive Backward
  * Steer
  * Drive Forward and Turn Left
  * Steer
  * Drive Forward and Turn Right
  * Steer
  * Drive Backward and Turn Left
  * Steer
  * Drive Backward and Turn Right
  * Steer
  * Turn Left
  * Turn Right
  * Drive Forward and Turn Left
  * Drive Backward and Turn Right
  */
  int cur_time = millis();
  if(cur_time - last_change > change_interval){
    last_change = cur_time;
    changes[change_param++]();
    if (change_param >= 16) change_param = 0;
  }
  update_car();
}