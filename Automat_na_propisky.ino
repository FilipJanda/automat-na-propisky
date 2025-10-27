#include <WiFi.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <string.h>
#include <ESP32QRCodeReader.h>

//const char* ssid = "I_hear_you";
//const char* password = "silver11";

const char* host = "https://pgdtypgzzdchqefcgcaz.supabase.co";
const int httpsPort = 443;
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InBnZHR5cGd6emRjaHFlZmNnY2F6Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NTg1MjAxNzEsImV4cCI6MjA3NDA5NjE3MX0.a0VHx01HJRa6G3MGTzR3pLHRjNr1cUiaiENOwWicizc";

const char* userId = "2";

//new credentials
bool connectWiFi(String ssid, String password)
{
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
  preferences.begin("WiFiCredentials", false); // false = read/write mode
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);

  return true;
}

//saved credentials
bool connectWiFi()
{
  Preferences preferences;
  preferences.begin("WiFiCredentials", true); 

  char* ssid = preferences.getString("ssid", "");
  char* password = preferences.getString("password", "");
  
  if (ssid == "" && password == "")
  {
    return false;
  }

  WiFi.begin(ssid, password);
  
  int timeout = 60000;

  while (WiFi.status() != WL_CONNECTED {
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

String readQR()
{

}

int getUserID(int )
{
  
}

bool deductToken(int userid) {
  HTTPClient http;

  // Target URL for the "users" table
  String url = String(host) + "/rest/v1/users?id=eq." + userid;

  // Prepare JSON body – e.g. set tokens to 4
  String body = "{\"token_count\":4}";

  http.begin(url);
  http.addHeader("apikey", supabaseKey);
  http.addHeader("Authorization", String("Bearer ") + supabaseKey);
  http.addHeader("Content-Type", "application/json");  
  http.addHeader("Prefer", "return=representation");
  
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

void setup() {
  Serial.begin(115200);

  if (connectWiFi())
  {
    Serial.println("WiFi connected")
  }
  else
  {
    Serial.println("No saved/incorrect WiFi credentials")
  }
}

void loop() {
  //read qr
}