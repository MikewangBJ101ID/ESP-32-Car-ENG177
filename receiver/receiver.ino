
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
      /*
      msg.id can be 0, 1, 2, 3, 4:
      0 - No buttons pressed on controller 1
      1 - Button 1 on controller 1 pressed
      2 - Button 2 on controller 1 pressed
      3 - No buttons pressed on controller 2
      4 - Button 1 on controller 2 pressed
      5 - Button 2 on controller 2 pressed
      */
    }
  }
  else{

    //SAFETY precaution: No packets received in fixed period of time, controller may be down
    //Shutdown the car or stop the car

  }
}
