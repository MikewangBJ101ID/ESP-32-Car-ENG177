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

#include <WiFi.h>
#include <esp_now.h>
#include <L298NX2.h>

#define WIFI_CHANNEL 6 //The same as controller
#define FORCE_SHUTDOWN_TIME 5000 //If no data packets have been received for 5000 ms, shut down the car

typedef struct {
  uint8_t id;
  uint8_t mac[6];
} ReceivedMessage;

QueueHandle_t messageQueue; //Queue for handling messages
int last_packet_receive_time = 0;//last time receiving a packet

void onDataRecv(const esp_now_recv_info_t *info,
                const uint8_t *data,
                int len) {
  // We expect exactly one byte: the sender's ID.
  if (len != sizeof(uint8_t)) {
    Serial.println("Message length does not match");
    return;
  }

  ReceivedMessage msg;
  msg.id = data[0];
  memcpy(msg.mac, info->src_addr, 6); //Copy message into formatted form

  // Callback-safe: queue the message for processing in loop().
  // Non-blocking, so the callback returns quickly.
  if (messageQueue != nullptr) {
    xQueueSend(messageQueue, &msg, 0);
  }
  last_packet_receive_time = millis();
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.setChannel(WIFI_CHANNEL);

  messageQueue = xQueueCreate(200, sizeof(ReceivedMessage)); //Maximum 200 messages

  if (messageQueue == nullptr) {
    Serial.println("Queue creation failed");
    return;
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);

  Serial.println("ESP-NOW receiver ready");
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

void reset_flags(){
  signal_flags[0] = 0; signal_flags[1] = 0; signal_flags[2] = 0;
  signal_flags[3] = 1; signal_flags[4] = 1; signal_flags[5] = 0;
  signal_flags[6] = 0; signal_flags[7] = 1; signal_flags[8] = 1;
}

//===========================================================
// Car control algorithm
//===========================================================
#define EN_A 0 //Placeholder values, TBD
#define IN1_A 0
#define IN2_A 0
#define EN_B 0
#define IN1_B 0
#define IN2_B 0

L298NX2 myMotors(EN_A, IN1_A, IN2_A, EN_B, IN1_B, IN2_B);

//---------- Tunable parameters ----------
#define SPEED_THRESHOLD   200  // Max |speed| for forward AND backward (0..255), same value both ways.
#define ACCEL_STEP        20   // Speed added per packet while accelerating.
#define FRICTION_PERCENT  10   // % of |speed| lost per packet while coasting.
#define MIN_FRICTION_LOSS 1    // A coasting motor always slows by at least this much.

//---------- Physical setup ----------
// True  = motor A is on the LEFT side of the car (steer left releases it).
// False = motor A is on the RIGHT side of the car.
static const bool MOTOR_A_IS_LEFT = true;

enum MotorIndex { MOTOR_A = 0, MOTOR_B = 1 };
static bool is_left_motor(MotorIndex m);
static void apply_motor(MotorIndex m, int speed);
//Some mysterious errors. Somehow removing the above two lines will
//throw a MotorIndex not declared error

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

  MotorIndex motors[2] = {MOTOR_A, MOTOR_B};

  for (int i = 0; i < 2; i++) {
    MotorIndex m  = motors[i];
    int speed     = motor_speed[m];
    bool cutMotor = is_left_motor(m) ? steer_left : steer_right; // steering releases this side's motor

    if (cutMotor) {
      // Steering: let this motor spin down; the car turns toward this side.
      speed = coast_toward_zero(speed);
    } else if (drive_forward) {
      // Accelerate forward unless already at/above the (positive) threshold.
      speed = step_toward(speed,  SPEED_THRESHOLD, ACCEL_STEP);
    } else if (drive_backward) {
      // Accelerate backward unless already at/below the (negative) threshold.
      speed = step_toward(speed, -SPEED_THRESHOLD, ACCEL_STEP);
    } else {
      // No input for this motor: turn it off, friction slows it to 0.
      speed = coast_toward_zero(speed);
    }

    motor_speed[m] = speed;
    apply_motor(m, speed);
  }
}

void loop() {
  ReceivedMessage msg;
  if(millis() - last_packet_receive_time <= FORCE_SHUTDOWN_TIME){
    while (xQueueReceive(messageQueue, &msg, 0) == pdTRUE) {
      Serial.printf(
        "Received ID: %u from %02X:%02X:%02X:%02X:%02X:%02X\n",
        msg.id,
        msg.mac[0], msg.mac[1], msg.mac[2],
        msg.mac[3], msg.mac[4], msg.mac[5]
      );

      /*CAR CONTROLLER LOGIC HERE*/
      update_flags(msg.id);
      update_car();
    }
  }
  else{

    //SAFETY precaution: No packets received in fixed period of time, controller may be down
    //Shutdown the car or stop the car
    reset_flags();
    update_car();
  }
}