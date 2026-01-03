#include <WiFi.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <string.h>
#include <ESP32QRCodeReader.h>

//indicator pins
#define POWER 1
#define WIFI 3
#define EMPTY 0

#define IR 2
#define IR_THRESHOLD 3000

//pins for stepper
#define STEPPER_A 4
#define STEPPER_B 2
#define STEPPER_C 14
#define STEPPER_D 15

#define STEPPER_DELAY 1500

const char* host = "https://pgdtypgzzdchqefcgcaz.supabase.co";
const int httpsPort = 443;
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InBnZHR5cGd6emRjaHFlZmNnY2F6Iiwicm9sZSI6InNlcnZpY2Vfcm9sZSIsImlhdCI6MTc1ODUyMDE3MSwiZXhwIjoyMDc0MDk2MTcxfQ.GbV66aF7zwqLtsqrZ-gGKScqnaV-d3lTO7WDMBLKBY8";

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

bool deductToken(char* uid, char* temporary_key) {
  HTTPClient http;
  http.begin("https://pgdtypgzzdchqefcgcaz.supabase.co/rest/v1/rpc/subtract_token");
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  
  char* body = "{ \"user_id\":12 }";
  
  int httpResponseCode = http.POST(body);

  if (httpResponseCode > 0)
  {
    if (http.getString() == "true")
      return true;
    else
      return false;
  }

  http.end();

  /*if (httpResponseCode > 0) {
    Serial.println("Response: " + http.getString());
    http.end();
    return true;
  } 
  else {
    Serial.printf("Error: %s\n", http.errorToString(httpResponseCode).c_str());
    http.end();
    return false;
  }*/
}

char* scanQR() {
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
  digitalWrite(EMPTY, LOW);

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
  Serial.println("Scan QR");
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
  {
    Serial.println("Dispensing now...");

    unsigned long timeout = millis() + 10000;
    while (true)
    {
      stepperRotate();
      
      if (analogRead(IR) > IR_THRESHOLD)
        break;

      if (millis() >= timeout)
      {
        Serial.println("Error while dispensing");
        break;
      }
    }
  }
  else
  {
    blinkLED(POWER);
    digitalWrite(POWER, HIGH);
  }
}