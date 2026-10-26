#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define NODE_ID 1 //The ID of this board
#define BUTTON_PIN 27 //The pin to read from
#define WIFI_CHANNEL 6
#define DATA_SEND_INTERVAL 15 //how many milliseconds in between each message

// Broadcast address: send to all nearby ESP-NOW devices
uint8_t broadcastAddress[] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

uint32_t nextSend = 0; //next timestamp to send message

void setup() {
  Serial.begin(115200);

  // Enable the Wi-Fi radio without connecting to a router
  //Registrating this board
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(WIFI_CHANNEL);

  Serial.print("Board ID: ");
  Serial.println(NODE_ID);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Start ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed");
    return;
  }

  // Register the broadcast destination
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastAddress, 6);
  peer.channel = WIFI_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Failed to add broadcast peer");
    return;
  }

  Serial.println("ESP-NOW ready");
}

void send_message(int id){
  // Send a signal to the other ESP32 device
  // This will need to be adjusted based on your specific ESP32 devices and ESP-NOW setup
  /*
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  */
  // Send message
  uint8_t msg = id;
  esp_err_t result = esp_now_send(broadcastAddress, &msg, sizeof(msg));
  
  if (result == ESP_OK) {
    Serial.println(id);
  } else {
    Serial.println("Error sending the data");
  }
}

void loop() {
  // Send one message every main loop
  int button_states = digitalRead(BUTTON_PIN);
  if(millis() - nextSend >= 0){
    Serial.println(button_states);
    if (button_states){
      send_message(0);
    }
    else{
      send_message(1);
    }
    nextSend = millis() + DATA_SEND_INTERVAL;
  }
}
