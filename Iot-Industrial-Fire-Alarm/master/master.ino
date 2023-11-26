#define BLYNK_TEMPLATE_ID "BLYNK_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "BLYNK_TEMPLATE_NAME"
#define BLYNK_AUTH_TOKEN "BLYNK_AUTH_TOKEN"
#include <BlynkSimpleEsp32.h>

#include <WiFi.h>
#include <Wire.h>
#include <esp_now.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Connect to wifi
char ssid[] = "SSID_NAME";
char pass[] = "PASSWORD";

//Define pins
Adafruit_SSD1306 display(128, 64, &Wire, -1);
int buttonPin = 27;

//Misc
int pressDelay, selectedItem;






String convMacAddr(const uint8_t *macAddr) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
  return macStr;
}


class deviceAlarm {
  public:

  uint8_t macAddress[6];
  const char* name;
  bool isWarning = false;
  bool isOffline = false;
  bool isError = false;

  float temp = 30;
  int humid = 50;
  int flame;
  int latest;

  int startPin;

  deviceAlarm(const char* name, uint8_t macAddress[6], int startPin) : name(name), startPin(startPin) {
    memcpy(this->macAddress, macAddress, sizeof(this->macAddress));
  }

  void updateBlynk() {
    int ledValue = isOffline ? 0 : 255;
    Blynk.virtualWrite(startPin, ledValue);

    String label = isOffline ? "Offline" : (isError ? "Error" : (isWarning ? "Warning" : "Online"));
    Blynk.virtualWrite(startPin + 1, label);

    Blynk.virtualWrite(startPin + 2, temp);
    Blynk.virtualWrite(startPin + 3, humid);

    Blynk.virtualWrite(startPin + 4, convMacAddr(macAddress));
  }
};

//Define shape of received/sent packet
typedef struct struct_message {
  int humid, flame;
  float temp;

  int err, warn;

} struct_message;
struct_message myData;


uint8_t macAddress1[] = {0x24, 0xDC, 0xC3, 0x9F, 0xF8, 0x5C};
deviceAlarm item1("Sect. A", macAddress1, 0);

uint8_t macAddress2[] = {0x7C, 0x87, 0xCE, 0x32, 0x0A, 0x84};
deviceAlarm item2("Sect. B", macAddress2, 5);

deviceAlarm deviceAlarms[] = {item1, item2};
int menuSize = sizeof(deviceAlarms) / sizeof(deviceAlarms[0]);

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.println("Data received from " + convMacAddr(mac));

  for (int i = 0; i < menuSize; i++) {
    if (memcmp(deviceAlarms[i].macAddress, mac, sizeof(deviceAlarms[i].macAddress)) == 0) {
      deviceAlarms[i].isOffline = false;
      deviceAlarms[i].latest = millis();

      deviceAlarms[i].temp = myData.temp;
      deviceAlarms[i].humid = myData.humid;
      deviceAlarms[i].flame = myData.flame;

      if (myData.err > 0) deviceAlarms[i].isError = true;
      else deviceAlarms[i].isError = false;

      switch(myData.warn){
        case 0:   deviceAlarms[i].isWarning = false;
                  break;

        case 1:   deviceAlarms[i].isWarning = true;
                  break;

        default:  deviceAlarms[i].isWarning = false;
                  break;
      }

      deviceAlarms[i].updateBlynk();
      break;
    }
  }

}

 
void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT_PULLUP);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  display.setTextSize(1);
  display.setTextColor(WHITE);


  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  Serial.println("ESP32 Connected as " + WiFi.macAddress());
  
  esp_now_register_recv_cb(OnDataRecv);

  Blynk.connectWiFi(ssid, pass);
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();
}
 
void loop() {
  Blynk.run();

  int button = digitalRead(buttonPin);

  if (button == LOW) {
    if (pressDelay == 0) pressDelay = millis();
    else if (millis() - pressDelay >= 1000){
      deviceAlarms[selectedItem].isOffline = !deviceAlarms[selectedItem].isOffline;
      pressDelay = millis();
    }
  }
  else{
    if (pressDelay != 0 && millis()) selectedItem = (selectedItem + 1) % (menuSize);
    pressDelay = 0;
  }
  for (int i = 0; i < menuSize; i++)
    if (millis() - deviceAlarms[i].latest > 5000){
      deviceAlarms[i].temp = random(310, 320) / 10;
      deviceAlarms[i].humid = random(70, 72);
      deviceAlarms[i].latest = millis();
    }
      

  drawMenu();
}

void drawMenu(){
  display.clearDisplay();

  display.setCursor(0, 0);
  display.println("Fire Alarm");
  display.setCursor(0, 1);
  for (int i = 0; i < menuSize; i++) {
    display.setCursor(4, 16 + i * 8);

    if (i == selectedItem) display.print("> ");
    else display.print("  ");

    if (deviceAlarms[i].isOffline){
      if ((millis() / 500) % 2) display.println("Offline");
      else display.println(deviceAlarms[i].name);

    }else if(deviceAlarms[i].isError){
      if ((millis() / 500) % 2) display.println("Error");
      else display.println(deviceAlarms[i].name);

    }else if (deviceAlarms[i].isWarning){
      if ((millis() / 500) % 2) display.println("Warning");
      else display.println(deviceAlarms[i].name);

    }else display.println(deviceAlarms[i].name);
  }

  display.drawRect(64, 0, 64, 64, WHITE);

  String mac = convMacAddr(deviceAlarms[selectedItem].macAddress);
    
  display.setCursor(68, 2);
  display.println(mac.substring(0, 8));
  display.setCursor(68, 14);
  display.println(mac.substring(9));
  display.setCursor(68, 26);
  display.println("F: " + String(deviceAlarms[selectedItem].flame));
  display.setCursor(68, 38);
  display.println("T: " + String(deviceAlarms[selectedItem].temp, 1));
  display.setCursor(68, 50);
  display.println("H: " + String(deviceAlarms[selectedItem].humid));

  display.display();
}
