#include <WiFi.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <string.h>
#include <ESP32QRCodeReader.h>

//indicator pins
#define POWER 4
#define WIFI 16
#define EMPTY 0

#define IR 2

//pins for stepper
#define STEPPER_A 14
#define STEPPER_B 15
#define STEPPER_C 13
#define STEPPER_D 12

#define STEPPER_DELAY 1500

const char* host = "https://pgdtypgzzdchqefcgcaz.supabase.co";
const int httpsPort = 443;
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InBnZHR5cGd6emRjaHFlZmNnY2F6Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTg1MjAxNzEsImV4cCI6MjA3NDA5NjE3MX0.a0VHx01HJRa6G3MGTzR3pLHRjNr1cUiaiENOwWicizc";

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
  
  return true;
}

void stepperRotate()
{
  //krok 1
  digitalWrite(STEPPER_A, HIGH); 
  digitalWrite(STEPPER_B, LOW); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, LOW); 
  delay(rychlost);

  //krok 2
  digitalWrite(STEPPER_A, HIGH); 
  digitalWrite(STEPPER_B, HIGH); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, LOW); 
  delay(rychlost);

  //krok 3
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, HIGH); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, LOW); 
  delay(rychlost);

  //krok 4
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, HIGH); 
  digitalWrite(STEPPER_C, HIGH); 
  digitalWrite(STEPPER_D, LOW); 
  delay(rychlost);

  //krok 5
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, LOW);
  digitalWrite(STEPPER_C, HIGH); 
  digitalWrite(STEPPER_D, LOW); 
  delay(rychlost);

  //krok 6
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, LOW);
  digitalWrite(STEPPER_C, HIGH);
  digitalWrite(STEPPER_D, HIGH); 
  delay(rychlost);

  //krok 7
  digitalWrite(STEPPER_A, LOW); 
  digitalWrite(STEPPER_B, LOW); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, HIGH); 
  delay(rychlost);

  //krok 8
  digitalWrite(STEPPER_A, HIGH); 
  digitalWrite(STEPPER_B, LOW); 
  digitalWrite(STEPPER_C, LOW); 
  digitalWrite(STEPPER_D, HIGH); 
  delay(rychlost);
}

bool deductToken(char* uid, char* temporary_key) {
  HTTPClient http;

  // Target URL for the "users" table
  String url = "https://pgdtypgzzdchqefcgcaz.supabase.co/rest/v1/rpc/subtract_token" + uid;

  // Prepare JSON body – e.g. set tokens to 4
  String body = "{\"token_count\":4}";

  http.begin(url);
  http.addHeader("apikey", supabaseKey);
  //http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  http.addHeader("Content-Type", "application/json");  
  http.addHeader("Prefer", "count=exact");
  
  int httpResponseCode = http.PATCH(body);

  if (httpResponseCode > 0) {
    Serial.println("Response: " + http.getString());
    http.end();
    return true;
  } 
  else {
    Serial.printf("Error: %s\n", http.errorToString(httpResponseCode).c_str());
    http.end();
    return false;
  }
}

char* scanQR()
{
  ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER);
  struct QRCodeData qrCodeData;

  reader.setup();
  reader.begin();

  while (true) 
  {
    if (reader.receiveQrCode(&qrCodeData, 100) && qrCodeData.valid) 
    {
      char* payload = (char*)qrCodeData.payload;
      return payload;
    }
    
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);

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
  char* data = scanQR();
  char* pch = strtok(data, "/");
  int uid;
  char* tempKey;

  if (pch != NULL)
  {
    uid = pch;
    pch = strtok(NULL, "/");
    Serial.println("UID acquired");
  }
  else 
  {
    Serial.println("Invalid QR data");
  }

  if (pch != NULL)
  {
    tempKey = pch;
    Serial.println("Temporary key acquired");
  }
  else 
  {
    Serial.println("Invalid QR data");
  }

  if(deductToken(uid, tempKey))
  {
    Serial.println("Dispensing now...")

    bool dispensed = false;
    while (!dispensed)
    {
      stepperRotate();
      // continuous sensing
      // if sensor triggers, dispensed = true;
    }
  }
  else
  {
    //indicate problems
  }
}
