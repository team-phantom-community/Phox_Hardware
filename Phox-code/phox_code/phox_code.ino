#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Base.h"
#include "esp_http_client.h"
#include "HTTPClient.h"
#include <ArduinoJson.h>
#include <string>
#include <Wire.h>
#include <VL53L0X.h>

VL53L0X sensor;

#define CAMERA_ESP32_S3_PhoxMB
#include "camera_pins.h"

const int Hall_Pin = 47;
const int Pwr_Pin = 35;
const int LED_Pin = 37;
const int C_LED = 21;
const int Batt_Pin = 14;
#define PIN_SDA 4
#define PIN_SCL 5
float batt = 0.0;
int p = 0;

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "---";
const char* password = "---";

#define AWS_ENDPOINT "---" //Enter your API GateWay URL
#define AWS_API_KEY "---" //Enter your API Key
  
int waitingTime = 30000; //Wait 30 seconds to google response.


void setup() {
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("Hello");

  pinMode(Hall_Pin, INPUT);
  pinMode(Pwr_Pin, OUTPUT);
  pinMode(LED_Pin, OUTPUT);
  pinMode(C_LED, OUTPUT);
  pinMode(Batt_Pin, INPUT);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_DRAM; // DRAMを使う（PSRAM使わない）
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  digitalWrite(Pwr_Pin, HIGH);
  digitalWrite(C_LED, HIGH);
  
}

boolean enviar = true;

void loop() {
  // Do nothing. Everything is done in another task by the web server
  if(digitalRead(Hall_Pin) == 0){
    digitalWrite(C_LED, LOW);
    Serial.println("hall detect");
    delay(2000);
    send2AWS();
    //saveCapturedImage();
    // enviar = false;
    delay(10000);
    digitalWrite(Pwr_Pin, LOW);
    while(digitalRead(Hall_Pin) == 0){
      delay(100);
    }
  }
  else{
    float BT = 0;
    for(int i = 0; i< 20; i++){
      BT += int(analogRead(Batt_Pin)*3.3*2);
      delay(200);
    }
    batt = float(BT / 4095 / 20) + 0.8;
    Serial.println(batt);
    delay(1000);
  }
}

String urlencode(String str){
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (c == '/'){
        encodedString+='_';
      }else if (c == '+'){
        encodedString+='-';
      } else{
        encodedString+=c;
      }
      yield();
    }
    return encodedString;
}

void send2AWS(){
  StaticJsonDocument<JSON_OBJECT_SIZE(4)> json_array;
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return;
  }

  char *input = (char *)fb->buf;
  char output[base_enc_len(3)];
  String imageFile = "";
  for (int i=0;i<fb->len;i++) {
    base_encode(output, (input++), 3);
    if (i%3==0) imageFile += urlencode(String(output));
  }
  Serial.println(imageFile);
  HTTPClient http;

  String serverPath = AWS_ENDPOINT;
  http.addHeader("x-api-key", AWS_API_KEY);
  
  // Your Domain name with URL path or IP address with path
  http.begin(serverPath.c_str());
  http.addHeader("Content-Type", "application/json");
  // POSTしてステータスコードを取得する
  Serial.println("cam captured");
  // char json[1000];
  String postdata = String("{\"myjpg\":\"") + imageFile + String("\", \"position\":\"") + String(p) + String("\", \"batt\":\"") + String(batt) + String("\"}");

  int httpResponseCode = http.POST(postdata.c_str());
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
    digitalWrite(C_LED, HIGH);
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
}
