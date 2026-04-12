#include <WiFi.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ESP32QRCodeReader.h>
#include <NeoPixelBus.h>
//#include <ArduinoOTA.h>

const uint16_t PixelCount = 1;
const uint8_t PixelPin = 0;

NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod> LED(PixelCount, PixelPin);

enum ledState {
  OPERATIONAL,
  GOOD,
  ERROR,
  WAITING,
  OFF
};

#define IR 12
#define IR_THRESHOLD 300

ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);
#define SCAN_INTERVAL 1000

//pins for stepper
#define STEPPER_A 13
#define STEPPER_B 15
#define STEPPER_C 14
#define STEPPER_D 2

#define FLASH 4
#define MAX_QR_RETRIES 5

const char* url_process = "https://pgdtypgzzdchqefcgcaz.supabase.co/functions/v1/process_transaction";
const char* url_finish = "https://pgdtypgzzdchqefcgcaz.supabase.co/functions/v1/finish_transaction";
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InBnZHR5cGd6emRjaHFlZmNnY2F6Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTg1MjAxNzEsImV4cCI6MjA3NDA5NjE3MX0.a0VHx01HJRa6G3MGTzR3pLHRjNr1cUiaiENOwWicizc"; //should be anon key

//new credentials
bool connectWiFi(String ssid, String password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int timeout = 60000;

  while (WiFi.status() != WL_CONNECTED) {
    if (timeout > 0)
    {
      timeout -= 500;
      delay(500);
    }
    else
    {
      return false;
    }
  }

  Preferences preferences;
  preferences.begin("WiFiCredentials", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  return true;
}

//saved credentials
bool connectWiFi() {
  Preferences preferences;
  preferences.begin("WiFiCredentials", true); 

  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");
  
  if (ssid == "" && password == "")
  {
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int timeout = 60000;

  while (WiFi.status() != WL_CONNECTED) {
    if (timeout > 0)
    {
      timeout -= 500;
      delay(500);
    }
    else
    {
      return false;
    }
  }
  
  return true;
}

void stepperMove() {
  //step 1
  digitalWrite(STEPPER_A, HIGH); 
  digitalWrite(STEPPER_B, LOW); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, LOW); 
  delay(1);

  //step 2
  digitalWrite(STEPPER_A, HIGH); 
  digitalWrite(STEPPER_B, HIGH); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, LOW); 
  delay(1);

  //step 3
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, HIGH); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, LOW); 
  delay(1);

  //step 4
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, HIGH); 
  digitalWrite(STEPPER_C, HIGH); 
  digitalWrite(STEPPER_D, LOW); 
  delay(1);

  //step 5
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, LOW);
  digitalWrite(STEPPER_C, HIGH); 
  digitalWrite(STEPPER_D, LOW); 
  delay(1);

  //step 6
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, LOW);
  digitalWrite(STEPPER_C, HIGH);
  digitalWrite(STEPPER_D, HIGH); 
  delay(1);

  //step 7
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, LOW); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, HIGH); 
  delay(1);

  //step 8
  digitalWrite(STEPPER_A, HIGH); 
  digitalWrite(STEPPER_B, LOW); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, HIGH); 
  delay(1);

  //coils off
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, LOW); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, LOW); 
}

bool dispense() {
  Serial.println("Dispensing now...");

  unsigned long timeout = millis() + 10000;

  while (millis() < timeout)
  {
    stepperMove();
    
    Serial.println(analogRead(IR));
    if (analogRead(IR) < IR_THRESHOLD)
    {
      return true;
    }
  }

  return false;
}

bool DBProcessTransaction(char* uid, char* temporary_key, char* transaction_id) {
  char jsonBody[256];
  snprintf(jsonBody, sizeof(jsonBody), "{\"user_id\":%s,\"temporary_key\":\"%s\",\"machine_id\":%llu}", uid, temporary_key, ESP.getEfuseMac());
  Serial.println(jsonBody);
  HTTPClient http;
  http.begin(url_process);
  http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  int httpResponseCode = http.POST(jsonBody);

  if (httpResponseCode != 200)
  {
    Serial.print("HTTP error: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }

  String s = http.getString();
  snprintf(transaction_id, 64, "%s", s.c_str());

  http.end();
  return true;
}

bool DBFinishTransaction(char* transaction_id, char* success) {
  char jsonBody[128];
  snprintf(jsonBody, sizeof(jsonBody), "{\"transaction_id\":%s,\"success\":\"%s\"}", transaction_id, success);
  Serial.println(jsonBody);
  HTTPClient http;
  http.begin(url_finish);
  http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  int httpResponseCode = http.POST(jsonBody);
  http.end();

  if (httpResponseCode == 200) return true;

  Serial.println(httpResponseCode);
  return false;
}

bool DBIsOperational() {
  char url[128];
  snprintf(
    url,
    sizeof(url),
    "https://your-project-id.supabase.co/rest/v1/machines?id=eq.%s&select=is_operational",
    ESP.getEfuseMac()
  );
  Serial.println(url);

  HTTPClient http;
  http.begin(url);
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  int httpResponseCode = http.GET();

  if (httpResponseCode != 200) return false;

  String body = http.getString();
  http.end();
  Serial.println(body);
}

void waitForFix(){
  LEDstatus(ledState::ERROR);

  while(true) 
  { 
    for (int interval = 0; interval < 20000; interval += 1000)
    {
      digitalWrite(33, LOW);
      delay(500);
      digitalWrite(33, HIGH);
      delay(500);
    }

    if(DBIsOperational()) break;
  }

  Serial.println("Issue fixed");
}

void reclaimPins() {
  gpio_reset_pin((gpio_num_t)13);
  gpio_reset_pin((gpio_num_t)14);
  gpio_reset_pin((gpio_num_t)15);
  gpio_reset_pin((gpio_num_t)2);

  pinMode(13, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(15, OUTPUT);
  pinMode(2, OUTPUT);
}

void scanQR(char* param1, char* param2) {
  reader.begin();

  while (true) 
  {
    LEDstatus(OPERATIONAL);
    Serial.print(".");

    struct QRCodeData qrCodeData;
    if (reader.receiveQrCode(&qrCodeData, SCAN_INTERVAL) && qrCodeData.valid) 
    {
      // display success to user
      analogWrite(FLASH, 20);
      delay(200);
      analogWrite(FLASH, LOW);

      char* payload = (char*)qrCodeData.payload;
      char* pch = strtok(payload, "/");

      if (pch != NULL)
      {
        snprintf(param1, 64, pch);
        pch = strtok(NULL, "/");
        Serial.println();
        Serial.println("param1 acquired");
      }
      else 
      {
        Serial.println("Invalid QR data");
      }

      if (pch != NULL)
      {
        snprintf(param2, 64, pch);
        Serial.println("param2 acquired");
      }
      else 
      {
        Serial.println("Invalid QR data");
      }

      reader.end();
      reclaimPins();

      break;
    }
  }
}

void LEDstatus(enum ledState s) {
  switch(s)
  {
    case OPERATIONAL:
      LED.SetPixelColor(0, RgbColor(0, 0, 255));
      break;

    case GOOD:
      LED.SetPixelColor(0, RgbColor(0, 255, 0));
      break;

    case ERROR:
      LED.SetPixelColor(0, RgbColor(255, 0, 0));
      break;

    case WAITING:
      LED.SetPixelColor(0, RgbColor(255, 255, 0));
      break;

    case OFF:
      LED.SetPixelColor(0, RgbColor(0, 0, 0));
      break;
  }
  
  LED.Show();
}

void setup() {
  LED.Begin();
  LED.Show();
  LEDstatus(ledState::WAITING);

  pinMode(STEPPER_A, OUTPUT);
  pinMode(STEPPER_B, OUTPUT);
  pinMode(STEPPER_C, OUTPUT);
  pinMode(STEPPER_D, OUTPUT);
  pinMode(FLASH, OUTPUT);
  pinMode(33, OUTPUT);

  Serial.begin(115200);

  reader.setup();

  if (connectWiFi())
  {
    //Credentials are stored
    Serial.println("WiFi connected");
    LEDstatus(ledState::OPERATIONAL);
  }
  else //Credentials missing or incorrect, acquire now
  {
    for (int i = 0; i < MAX_QR_RETRIES; i++)
    {
      Serial.println("Scanning for WiFi credentials...");    
      char ssid[64];
      char password[64];
      scanQR(ssid, password);

      if (connectWiFi(ssid, password))
      {
        Serial.println("WiFi connected");
        LEDstatus(ledState::OPERATIONAL);
        //setupOTA();
        return;
      }

      LEDstatus(ledState::ERROR);
    }
  }

  if(!DBIsOperational()) waitForFix();
}

void loop() {
  LEDstatus(ledState::OPERATIONAL);

  char uid[64];
  char temporary_key[64];
  xQueueReset(reader.qrCodeQueue);
  Serial.print("Scan QR");
  scanQR(uid, temporary_key);

  char transaction_id[64];
  bool infoOK = DBProcessTransaction(uid, temporary_key, transaction_id);
  if(!infoOK)
  {
    LEDstatus(ledState::ERROR);
    return;
  }
  LEDstatus(ledState::GOOD);

  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);

  bool dispenseOK = dispense();
  if(connectWiFi()) Serial.println("Reconnect successful");

  bool proceed;

  if(!dispenseOK)
  {
    proceed = DBFinishTransaction(transaction_id, "false"); // Should always return false
    LEDstatus(ledState::ERROR);
  }
  else
  {
    proceed = DBFinishTransaction(transaction_id, "true"); // Returns false if inventory runs out or machine manually set to inoperational
  }

  if (!proceed)
  {
    waitForFix();
  }
}