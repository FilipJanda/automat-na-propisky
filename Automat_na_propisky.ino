#include <WiFi.h>
#include <Preferences.h>
#include <HTTPClient.h>
//#include <Arduino_JSON.h>
//#include <string.h>
#include <ESP32QRCodeReader.h>
#include <FastLED.h>

//indicator pins
// 1 seems to be unusable
#define POWER 1
#define WIFI 3
// 0 has inverted logic due to hardware limitations
#define EMPTY 0

#define NUM_LEDS 10
#define DATA_PIN 16

#define IR 12
#define IR_THRESHOLD 3000

//pins for stepper
#define STEPPER_A 2
#define STEPPER_B 14
#define STEPPER_C 15
#define STEPPER_D 13

#define QR_SCAN_DELAY 100

CRGB leds[NUM_LEDS];

const char* url_process = "https://pgdtypgzzdchqefcgcaz.supabase.co/functions/v1/process_transaction";
const char* url_finish = "https://pgdtypgzzdchqefcgcaz.supabase.co/functions/v1/finish_transaction";
//should be anon key
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InBnZHR5cGd6emRjaHFlZmNnY2F6Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTg1MjAxNzEsImV4cCI6MjA3NDA5NjE3MX0.a0VHx01HJRa6G3MGTzR3pLHRjNr1cUiaiENOwWicizc";

ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);
struct QRCodeData qrCodeData;

HTTPClient http;

//new credentials
bool connectWiFi(String ssid, String password) {
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

  digitalWrite(WIFI, HIGH);
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
  
  digitalWrite(WIFI, HIGH);
  return true;
}

void stepperMove() {
  //krok 1
  digitalWrite(STEPPER_A, HIGH); 
  digitalWrite(STEPPER_B, LOW); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, LOW); 
  delay(1);

  //krok 2
  digitalWrite(STEPPER_A, HIGH); 
  digitalWrite(STEPPER_B, HIGH); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, LOW); 
  delay(1);

  //krok 3
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, HIGH); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, LOW); 
  delay(1);

  //krok 4
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, HIGH); 
  digitalWrite(STEPPER_C, HIGH); 
  digitalWrite(STEPPER_D, LOW); 
  delay(1);

  //krok 5
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, LOW);
  digitalWrite(STEPPER_C, HIGH); 
  digitalWrite(STEPPER_D, LOW); 
  delay(1);

  //krok 6
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, LOW);
  digitalWrite(STEPPER_C, HIGH);
  digitalWrite(STEPPER_D, HIGH); 
  delay(1);

  //krok 7
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, LOW); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, HIGH); 
  delay(1);

  //krok 8
  digitalWrite(STEPPER_A, HIGH); 
  digitalWrite(STEPPER_B, LOW); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, HIGH); 
  delay(1);
}

bool dispense() {
  Serial.println("Dispensing now...");

  unsigned long timeout = millis() + 10000;

  while (millis() < timeout)
  {
    stepperMove();
    
    if (analogRead(IR) > IR_THRESHOLD)
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
  http.begin(url_finish);
  http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  int httpResponseCode = http.POST(jsonBody);
  http.end();

  if (httpResponseCode == 200) return true;
  return false;
}

void scanQR(char* param1, char* param2) {
  while (true) 
  {
    if (reader.receiveQrCode(&qrCodeData, 100) && qrCodeData.valid) 
    {
      blinkLED(0);

      char* payload = (char*)qrCodeData.payload;
      char* pch = strtok(payload, "/");

      if (pch != NULL)
      {
        snprintf(param1, 64, pch);
        pch = strtok(NULL, "/");
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

      return;
    }
    
    delay(QR_SCAN_DELAY);
  }
}

void blinkLED(int pin) {
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(pin, LOW);
    delay(100);
    digitalWrite(pin, HIGH);
    delay(100);
  }
}

void setup() {
  //pinMode(POWER, OUTPUT);
  //pinMode(WIFI, OUTPUT);
  //pinMode(EMPTY, OUTPUT);
  //pinMode(IR, INPUT);

  //pinMode(STEPPER_A, OUTPUT);
  //pinMode(STEPPER_B, OUTPUT);
  //pinMode(STEPPER_C, OUTPUT);
  //pinMode(STEPPER_D, OUTPUT);

  leds[0] = CRGB(255, 0, 0);
  FastLed.show();

  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);

  Serial.begin(115200);

  reader.setup();
  reader.begin();

  if (connectWiFi())
  {
    //Credentials are stored
    Serial.println("WiFi connected");
  }
  else
  {
    //Credentials missing or incorrect, acquire now
    Serial.println("Scanning for WiFi credentials...");    
    char ssid[64];
    char password[64];
    scanQR(ssid, password);

    if (connectWiFi(ssid, password))
    {
      Serial.println("WiFi connected");
    }
    else
    {
      Serial.println("WiFi failed");
      //replace infinite loop with something that doesnt block forever and indicates issues even without serial
      while (true)
      {
        delay(1000);
      }
    }
  }
}

void loop() {
  Serial.println("Scan QR");

  char uid[64];
  char temporary_key[64];
  xQueueReset(reader.qrCodeQueue);
  scanQR(uid, temporary_key); // QR scan is blocking

  char transaction_id[64];
  bool infoOK = DBProcessTransaction(uid, temporary_key, transaction_id);

  if(!infoOK)
  {
    //blinkLED(POWER);
    //digitalWrite(POWER, HIGH);
    return;
  }

  bool dispenseOK = dispense();

  bool proceed;

  if(!dispenseOK)
    proceed = DBFinishTransaction(transaction_id, "false"); // Should always return false
  else
    proceed = DBFinishTransaction(transaction_id, "true"); // Returns false if inventory runs out or machine manually set to inoperational

  if (!proceed)
  {
    // implement waiting for fix of problem, should read the status from the database i guess?
    while(true) { }
  }
}