#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>



/*

WIRING: CONNECT 27 -> BUTTON 1 IN -> BUTTON 1 OUT -> GND and set 27 to INPUT_PULLUP
                26 -> BUTTON 2 IN -> BUTTON 1 OUT -> GND and set 26 to INPUT_PULLUP
When button 1 (27) is pressed, a signal of 1 will be continually sent by the controller. The raw message will contain the data "1" stored in uint8_t form
When button 2 (26) is pressed, a signal of 2 will be continually sent, the same as above.
****Button 1 overwrites button 2****.
When no button is pressed, a signal of 0 will be continually sent.

For controller 2, the wiring should be the same but the signal will be changed to 3, 4, and 5.
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

void loop() {
  // Send one message every main loop
  int button_states_1 = digitalRead(BUTTON_PIN_1);
  int button_states_2 = digitalRead(BUTTON_PIN_2);
  if(millis() >= nextSend){{//Fixing: ensure that no underflow happens
    //Serial.print("Button State: ");
    //Serial.println(button_states);
    int offset = (NODE_ID - 1) * 3;//this will ensure that each node has a unique set of signal values
    if (!button_states_1){
      send_message(1 + offset);
    }
    else if (!button_states_2){
      send_message(2+offset);
    }
    else {
      send_message(0+offset);
    }
    nextSend = millis() + DATA_SEND_INTERVAL;
  }
  }
}
