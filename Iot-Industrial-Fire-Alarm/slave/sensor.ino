#include <esp_now.h>
#include <WiFi.h>
#include <DHT.h>
#define DHTTYPE DHT11

#define tempLimit 33
#define pressLimit 1000
#define refresh 1000 

typedef struct struct_message {
  int humid, flame;
  float temp;

  int err, warn;

} struct_message;
struct_message myData;

esp_now_peer_info_t peerInfo;

//Master MAC Address
uint8_t broadcastAddress[] = {0x24, 0xD7, 0xEB, 0x18, 0x35, 0x34};

//Define pins
int bLED = 13;
int gLED = 12;
int rLED = 14;
int buttonPin = 15;
int flamePin = 34;
DHT dht(27, DHTTYPE);

int pressDelay = 0;
bool isAlert = false;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  digitalWrite(bLED, HIGH);
  delay(500);
  digitalWrite(bLED, LOW);
  Serial.println("Status: " + String(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail"));
}

// Setup()
void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(flamePin, INPUT);
  pinMode(bLED, OUTPUT);
  pinMode(gLED, OUTPUT);
  pinMode(rLED, OUTPUT);
  
  digitalWrite(rLED, HIGH);

  WiFi.mode(WIFI_STA);
  Serial.println("ESP32 Connected as " + WiFi.macAddress());

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  //If failed, return as RED LED
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  
  //Ready state
  digitalWrite(rLED, LOW);
  digitalWrite(gLED, HIGH);
}
 
void loop() {

  readData();

  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  Serial.println("Message: " + String(result == ESP_OK ? "Sent" : "Error"));

  delay(refresh);
}

void fireAlert(int test = 0){
  if (!isAlert) {
    //test alarm
    if (test == 0) {
      myData.warn = 1;

    } else {
    //real alarm
      myData.warn = 2;

    }
    isAlert = true;
  }
}

void checkAlert(int b, int f, int h, float t){
  //check button
  if (b == LOW){
    if (pressDelay == 0) pressDelay = millis();
    else if (millis() - pressDelay >= pressLimit) fireAlert(1);
  } else pressDelay = 0;

  if (f > 0 && t > tempLimit) fireAlert();
}

void readData(){
  int button  = digitalRead(buttonPin);
  int flame   = digitalRead(flamePin);
  int humid   = dht.readTemperature();
  float temp  = dht.readTemperature();
  
  if (isnan(temp) || isnan(humid)){
    digitalWrite(gLED, LOW);
    digitalWrite(rLED, HIGH);

    myData.err = 1;
  } else {
    digitalWrite(gLED, HIGH);
    digitalWrite(rLED, LOW);
    myData.err = 0;
    checkAlert(button, flame, humid, temp);

    myData.flame = flame;
    myData.humid = humid;
  }


  myData.temp = temp;

}
