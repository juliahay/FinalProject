/*
  ESP-NOW Multi Unit Demo
  esp-now-multi.ino
  Broadcasts control messages to all devices in network
  Load script on multiple devices

  DroneBot Workshop 2022
  https://dronebotworkshop.com
*/

// Include Libraries
#include <WiFi.h>
#include <esp_now.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>

//include images
#include "../images/punch1.h"
#include "../images/punch2.h"
#include "../images/block.h"

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

bool gameStart = false;
int readyCounter = 0;

volatile bool punch = false;
volatile bool block = false;
volatile bool kick = false;

String cmdRecvd = "";
bool redrawCmdRecvd = false;

bool drawEndGame = false;

int health = 100;
bool redrawHealth = true;

//we could also use xSemaphoreGiveFromISR and its associated fxns, but this is fine
volatile bool scheduleCmdAsk = true;

#define ARRAY_SIZE 100
String player[ARRAY_SIZE] = {};
String opp[ARRAY_SIZE] = {};

int playerMoves = 0;
int oppMoves = 0;
int lineHeight = 30;

// Define LED and pushbutton pins
#define BUTTON_LEFT 0
#define BUTTON_RIGHT 35


void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength)
// Formats MAC Address
{
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}


void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen)
/* Called when data is received
   You can receive 3 types of messages
   1) a "PUNCH" message
   2) a "BLOCK" message
   3) a "KICK" message
   4) a "GAMEOVER" message, indicating the other player has run out of health
*/

{
  // Only allow a maximum of 250 characters in the message + a null terminating byte
  char buffer[ESP_NOW_MAX_DATA_LEN + 1];
  int msgLen = min(ESP_NOW_MAX_DATA_LEN, dataLen);
  strncpy(buffer, (const char *)data, msgLen);

  // Make sure we are null terminated
  buffer[msgLen] = 0;
  String recvd = String(buffer);
  Serial.println(recvd);
  // Format the MAC address
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);

  // Send Debug log message to the serial port
  Serial.printf("Received message from: %s \n%s\n", macStr, buffer);
  if(recvd[0] == 'R')
  {
    gameStart = true;
    if (readyCounter < 5) {
      broadcast("READY");
      readyCounter++;
    }
    
  }
  else if (recvd[0] == 'P') //only take an ask if you don't have an ask already and only take it XX% of the time
  {
    opp[oppMoves] = "P";
    oppMoves++;

    if (player[playerMoves-1] == "B") {
      health = health;
    } else {
      health = health - 5;
    }
    cmdRecvd = recvd;
    
    redrawCmdRecvd = true;
    redrawHealth = true;
  }
  else if (recvd[0] == 'B')
  {
    opp[oppMoves] = "B";
    oppMoves++;
    cmdRecvd = recvd;
    redrawCmdRecvd = true;
    redrawHealth = true;
    
  }
  else if (recvd[0] == 'K')
  {
    opp[oppMoves] = "K";
    oppMoves++;
    if (player[playerMoves-1] == "B") {
      health = health - 5;
    } else {
      health = health - 20;
    }
    cmdRecvd = recvd;
    redrawCmdRecvd = true;
    redrawHealth = true;
  }
  else if (recvd[0] == 'G')
  {
    youWin();
  }
}

void sentCallback(const uint8_t *macAddr, esp_now_send_status_t status)
// Called when data is sent
{
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void broadcast(const String &message)
// Emulates a broadcast
{
  // Broadcast a message to every device in range
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
  if (!esp_now_is_peer_exist(broadcastAddress))
  {
    esp_now_add_peer(&peerInfo);
  }
  // Send message
  esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t *)message.c_str(), message.length());

}

void IRAM_ATTR sendPunch(){
  punch = true;
}

void IRAM_ATTR sendBlock(){
  block = true;
}

void IRAM_ATTR sendKick(){
  kick = true;
}

void IRAM_ATTR onAskReqTimer(){
  scheduleCmdAsk = true;
}

void espnowSetup() {
  // Set ESP32 in STA mode to begin with
  delay(500);
  WiFi.mode(WIFI_STA);
  Serial.println("ESP-NOW Broadcast Demo");

  // Print MAC address
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Disconnect from WiFi
  WiFi.disconnect();

  // Initialize ESP-NOW
  if (esp_now_init() == ESP_OK)
  {
    Serial.println("ESP-NOW Init Success");
    esp_now_register_recv_cb(receiveCallback);
    esp_now_register_send_cb(sentCallback);
  }
  else
  {
    Serial.println("ESP-NOW Init Failed");
    delay(3000);
    ESP.restart();
  }
}

void buttonSetup(){
  pinMode(BUTTON_LEFT, INPUT);
  pinMode(BUTTON_RIGHT, INPUT);

  attachInterrupt(digitalPinToInterrupt(BUTTON_LEFT), sendPunch, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_RIGHT), sendBlock, FALLING);
}

void textSetup(){
  tft.init();
  tft.setRotation(0);

}


void setup()
{
  Serial.begin(115200);

  textSetup();
  buttonSetup();
  espnowSetup();

  tft.setSwapBytes(true);

  openingDisplay();

}

void drawAction(String cmd) {
  tft.fillScreen(TFT_WHITE);
  tft.setTextSize(3);
  tft.setTextColor(TFT_BLACK);
  if (cmd == "PUNCH") {
    tft.pushImage(0, 0, 135, 135, punch1); //fix position
  } else if (cmd == "BLOCK") {
    tft.pushImage(0, 0, 135, 121, block); //fix position
  } else if (cmd == "KICK") {
    //tft.pushImage(0, 0, 236, 135, kick); //fix position, get image
  }
  tft.drawString(cmd, 10, 150, 2);

}


void youWin() {
  tft.fillScreen(TFT_GREEN);
  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("YOU", 30, 80, 2);
  tft.drawString("WIN!", 30, 130, 2);
  delay(10000);
  ESP.restart();
}

void youLose() {
  tft.fillScreen(TFT_RED);
  tft.setTextSize(3);
  tft.setTextColor(TFT_BLACK);
  tft.drawString("YOU", 20, 80, 2);
  tft.drawString("LOSE!", 18, 130, 2);
  delay(10000);
  ESP.restart();
}

void openingDisplay() {
  while(!gameStart) {
    broadcast("READY");
    tft.fillScreen(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK);
    tft.drawString("WAITING", 6, 30, 2);
    tft.drawString("FOR", 6, 80, 2);
    tft.drawString("OTHER", 6, 130, 2);
    tft.drawString("PLAYER.", 6, 180, 2);
    delay(1000);
    tft.drawString("PLAYER .", 6, 180, 2);
    delay(1000);
    tft.drawString("PLAYER  .", 6, 180, 2);
    delay(1000);
  }
  
}

void loop()
{
  if (punch){
    player[playerMoves] = "P";
    playerMoves++;
    broadcast("PUNCH");
    punch = false;
  }
  if (block){
    player[playerMoves] = "B";
    playerMoves++;
    broadcast("BLOCK");
    block = false;
  }
  if (kick){
    player[playerMoves] = "K";
    playerMoves++;

    if (opp[oppMoves] == "B") {
      health = health - 10;
    }

    health = health - 5;
    broadcast("KICK");
    kick = false;
  }
  

  if (redrawCmdRecvd || redrawHealth) {
    //drawing the recieved action!
    drawAction(cmdRecvd);
    
    redrawCmdRecvd = false;
    
    if (health <= 0) {
      broadcast("GAMEOVER");
      youLose();
    } else {
      int spacing = 105; 
      tft.fillRect(15, spacing*2+5, 100, 6, TFT_BLACK);
      tft.fillRect(16, spacing*2+5+1, 100, 4, TFT_RED);
      tft.fillRect(16, spacing*2+5+1, 100-health, 4, TFT_WHITE);
    }
    redrawHealth = false;
  }

}
