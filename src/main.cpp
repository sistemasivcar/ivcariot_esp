#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Colors.h>
#include <IoTicosSplitter.h>
#include <ArduinoJson.h>

String dId = "5555";
String webhook_pass = "OpCHrljrjW";
String webhook_url = "https://app.ivcariot.com:3001/api/webhook/getdevicecredentials";
const char *mqtt_server = "https://app.ivcariot.com";

// PINS
#define led 2

// WIFI
const char *wifi_ssid = "Fibertel WiFi129 2.4GHz";
const char *wifi_password = "0041491096";

// FUNCTION DEFINITIONS
void clear();
bool getMqttCredentiales();

DynamicJsonDocument mqtt_data_doc(2048);

void setup()
{
  Serial.begin(921600);
  pinMode(led, OUTPUT);
  clear();

  Serial.print(underlinePurple + "WiFi Connection in Progress" + fontReset + Purple);

  WiFi.begin(wifi_ssid, wifi_password);

  int counter = 0;

  // Trying WIFi Connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    counter++;

    if (counter > 20)
    {
      Serial.print("  ⤵" + fontReset);
      Serial.print(Red + "\n\n         Ups WiFi Connection Failed :( ");
      Serial.println(" -> Restarting..." + fontReset);
      delay(3000);
      ESP.restart();
    }
  }

  // WIFi Connection Success
  Serial.print("  ⤵" + fontReset);
  Serial.print(boldGreen + "\n\n         WiFi Connection -> SUCCESS :)" + fontReset);
  Serial.print("\n\n         Local IP -> ");
  Serial.print(boldBlue + WiFi.localIP() + fontReset);

  getMqttCredentiales();
}

void loop()
{
}
bool getMqttCredentiales()
{
  Serial.print(underlinePurple + "\n\n\nGetting MQTT Credentials from WebHook" + fontReset + Purple + "  ⤵");
  delay(1000);

  String toSend = "dId=" + dId + "&whpassword=" + webhook_pass;

  HTTPClient http;
  http.begin(webhook_url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  // syncronous http request
  int response_code = http.POST(toSend);

  if (response_code < 0)
  {
    Serial.print(boldRed + "\n\n         Error Sending Post Request :( " + fontReset);
    http.end();
    return false;
  }

  if (response_code != 200)
  {
    Serial.print(boldRed + "\n\n         Error in response :(   e-> " + fontReset + " " + response_code);
    http.end();
    return false;
  }

  if (response_code == 200)
  {
    String response_body = http.getString(); // JSON format
    Serial.print(boldGreen + "\n\n         Mqtt Credentials Obtained Successfully :) " + fontReset);
    http.end();
    deserializeJson(mqtt_data_doc,response_body);
    delay(2000);
    return true;
  }

  return false;
}

void clear()
{
  Serial.write(27);    // ESC command
  Serial.print("[2J"); // clear screen command
  Serial.write(27);
  Serial.print("[H"); // cursor to home command
}