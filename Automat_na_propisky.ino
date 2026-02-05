#include <WiFi.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <string.h>
#include <ESP32QRCodeReader.h>

//indicator pins
// 1 seems to be unusable
#define POWER 1
#define WIFI 3
// 0 has inverted logic due to hardware limitations
#define EMPTY 0

#define IR 12
#define IR_THRESHOLD 3000

//pins for stepper
#define STEPPER_A 2
#define STEPPER_B 14
#define STEPPER_C 15
#define STEPPER_D 13

#define STEPPER_DELAY 1500

#define QR_SCAN_DELAY 100

const char* host = "https://pgdtypgzzdchqefcgcaz.supabase.co";
const int httpsPort = 443;
//should be anon key
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InBnZHR5cGd6emRjaHFlZmNnY2F6Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTg1MjAxNzEsImV4cCI6MjA3NDA5NjE3MX0.a0VHx01HJRa6G3MGTzR3pLHRjNr1cUiaiENOwWicizc";

ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);
struct QRCodeData qrCodeData;

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

void stepperRotate() {
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
    stepperRotate();
    
    if (analogRead(IR) > IR_THRESHOLD)
    {
      return true;
    }
  }

  return false;
}

bool deductToken(char* uid, char* temporary_key) {
  HTTPClient http;
  http.addHeader("Authorization", String("Bearer ") + supabaseKey);

  char jsonBody[64];
  snprintf(jsonBody, sizeof(jsonBody), "{\"user_id\":%d,\"temporary_key\":%s, \"machine_id\" \"}", uid, temporary_key, ESP.getEfuseMac());
  
  int httpResponseCode = http.POST(jsonBody);

  if (httpResponseCode != 200)
  {
    return false;
  }

  char* payload = http.getString();

  if (payload == "false")
  {
    Serial.println("Database declined transaction");
    http.end();
    return false;
  }

  if (!dispense())
  {
    return false;
  }

  // call finish transaction

  http.end();
  return false;
}

void scanQR(char** param1, char** param2) {
  while (true) 
  {
    if (reader.receiveQrCode(&qrCodeData, 100) && qrCodeData.valid) 
    {
      char* payload = (char*)qrCodeData.payload;
      char* pch = strtok(payload, "/");

      if (pch != NULL)
      {
        *param1 = pch;
        pch = strtok(NULL, "/");
        Serial.println("param1 acquired");
      }
      else 
      {
        Serial.println("Invalid QR data");
      }

      if (pch != NULL)
      {
        *param2 = pch;
        Serial.println("param2 acquired");
      }
      else 
      {
        Serial.println("Invalid QR data");
      }
    }
    
    delay(QR_SCAN_DELAY);
  }
}

void blinkLED(int pin) {
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(pin, LOW);
    delay(500);
    digitalWrite(pin, HIGH);
    delay(500);
  }
}

void setup() {
  pinMode(POWER, OUTPUT);
  pinMode(WIFI, OUTPUT);
  pinMode(EMPTY, OUTPUT);
  pinMode(IR, INPUT);

  pinMode(STEPPER_A, OUTPUT);
  pinMode(STEPPER_B, OUTPUT);
  pinMode(STEPPER_C, OUTPUT);
  pinMode(STEPPER_D, OUTPUT);

  digitalWrite(POWER, HIGH);
  digitalWrite(WIFI, LOW);
  digitalWrite(EMPTY, HIGH);

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
    char* credentials = scanQR();
    char* pch = strtok(credentials, "/");
    char* ssid;
    char* password;

    if (pch != NULL)
    {
      ssid = pch;
      pch = strtok(NULL, "/");
      Serial.println("SSID acquired");
    }
    else 
    {
      Serial.println("Invalid QR data");
    }

    if (pch != NULL)
    {
      password = pch;
      Serial.println("Password acquired");
    }
    else 
    {
      Serial.println("Invalid QR data");
    }

    if (connectWiFi(ssid, password))
    {
      Serial.println("WiFi connected");
    }
    else
    {
      Serial.println("WiFi failed");
      while (true)
      {
        delay(1000);
      }
    }
  }
}

void loop() {
  
  //
  // loop() should contain only concise code structure from simple decision logic and function calls
  // 
  // refactor this crap:
  //

  Serial.println("Scan QR");

  //Code waits here for QR scan
  char* data = scanQR();
  char* pch = strtok(data, "/");
  char* uid;
  char* tempKey;

  if (pch != NULL)
  {
    uid = pch;
    pch = strtok(NULL, "/");
    Serial.println("UID acquired");
  }
  else 
  {
    Serial.println("Invalid user QR data");
  }

  if (pch != NULL)
  {
    tempKey = pch;
    Serial.println("Temporary key acquired");
  }
  else 
  {
    Serial.println("Invalid user QR data");
  }

  if(deductToken(uid, tempKey))
  else
  {
    blinkLED(POWER);
    digitalWrite(POWER, HIGH);
  }
}