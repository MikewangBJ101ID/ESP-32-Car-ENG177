
#include <WiFi.h>
#include <esp_now.h>

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
  ** This is not equivalent to acceleration. When signal 2 is received, the car
  ** will initially accelerate but its speed will be capped at a fixed value.
  ** When signal 2 is no longer received, 
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
      //TBD: control the car
    }
  }
  else{

    //SAFETY precaution: No packets received in fixed period of time, controller may be down
    //Shutdown the car or stop the car
    reset_flags();
  }
}
