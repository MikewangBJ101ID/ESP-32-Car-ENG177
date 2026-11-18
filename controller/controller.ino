#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>


/*

WIRING: CONNECT 27 -> BUTTON 1 IN -> BUTTON 1 OUT -> GND and set 27 to INPUT_PULLUP
                26 -> BUTTON 2 IN -> BUTTON 2 OUT -> GND and set 26 to INPUT_PULLUP
When button 1 (27) is pressed, a signal of 1 will be continually sent by the controller. The raw message will contain the data "1" stored in uint8_t form
When button 2 (26) is pressed, a signal of 2 will be continually sent, the same as above.
When button 1 (27) is NOT pressed, a signal of 3 will be continually sent.
When button 2 (26) is NOT pressed, a signal of 4 will be continually sent.
**ONE PACKET CONTAINING TWO DIGITS WILL BE SENT IN A LOOP**
**The 1st and 0th digits can be exchanged - 1 - 8 are distinct from each other.**
*/

#define NODE_ID 1 //The ID of this board
#define BUTTON_PIN_1 27 //BUTTON 1
#define BUTTON_PIN_2 26 //BUTTON 2
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

  pinMode(BUTTON_PIN_1, INPUT_PULLUP);
  pinMode(BUTTON_PIN_2, INPUT_PULLUP);

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
  // Send message
  uint8_t msg = id;
  esp_err_t result = esp_now_send(broadcastAddress, &msg, sizeof(msg));
  
  if (result == ESP_OK) {
    Serial.print("Sent Message: ");
    Serial.println(id);
  } else {
    Serial.println("Error sending the data");
  }
}
//Generalizations
#if NODE_ID == 1
  #define FIRST_BUTTON_SIGNAL 1 //The signal to be broadcasted when button 1 is pressed.
  #define SECOND_BUTTON_SIGNAL 2 //The signal to be broadcasted when button 2 is pressed.
  #define FIRST_BUTTON_NOT_SIGNAL 3 //The signal to be broadcasted when button 1 is NOT pressed.
  #define SECOND_BUTTON_NOT_SIGNAL 4 //The signal to be broadcasted when button 2 is NOT pressed.
#else
  #define FIRST_BUTTON_SIGNAL 5 //the same.
  #define SECOND_BUTTON_SIGNAL 6
  #define FIRST_BUTTON_NOT_SIGNAL 7
  #define SECOND_BUTTON_NOT_SIGNAL 8
#endif
//Change for controller 2


void loop() {
  // Send one message every main loop
  int button_states_1 = digitalRead(BUTTON_PIN_1);
  int button_states_2 = digitalRead(BUTTON_PIN_2);
  if(millis() - nextSend >= DATA_SEND_INTERVAL){
    //Serial.print("Button State: ");
    //Serial.println(button_states);
    int this_msg = 0;
    if ((!button_states_1) && (!button_states_2)){
      this_msg = FIRST_BUTTON_SIGNAL * 10 + SECOND_BUTTON_SIGNAL; //encoding
    }
    else if (!button_states_1 || !button_states_2){
      if (!button_states_1){
        this_msg = FIRST_BUTTON_SIGNAL * 10 + SECOND_BUTTON_NOT_SIGNAL;
      }
      if (!button_states_2){
        this_msg = FIRST_BUTTON_NOT_SIGNAL * 10 + SECOND_BUTTON_SIGNAL;
      }
    }
    else {
      this_msg = FIRST_BUTTON_NOT_SIGNAL * 10 + SECOND_BUTTON_NOT_SIGNAL;
    }
    send_message(this_msg);
    nextSend = millis() + DATA_SEND_INTERVAL;
  }
}
