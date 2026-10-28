
#include <WiFi.h>
#include <esp_now.h>

#define WIFI_CHANNEL 6 //The same as controller

typedef struct {
  uint8_t id;
  uint8_t mac[6];
} ReceivedMessage;

QueueHandle_t messageQueue; //Queue for handling messages

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
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.setChannel(WIFI_CHANNEL);

  messageQueue = xQueueCreate(20, sizeof(ReceivedMessage)); //Maximum 20 messages

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
    0 - No buttons pressed
    1 - Button 1 on controller 1 pressed
    2 - Button 2 on controller 2 pressed
    3 - Button 1 on controller 2 pressed
    4 - Button 2 on controller 2 pressed
    Note that controllers 1 and 2 can both send 0, or one of them could send 0 and one of them could 
    send another signal
    */
  }
}
