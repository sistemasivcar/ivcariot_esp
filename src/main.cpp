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

// Function Definitions
void clear();
bool getMqttCredentiales();
void checkMqttConnection();
bool reconnect();
// Instances
WiFiClient wifiClient;
PubSubClient client(wifiClient);
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


}

void loop()
{
  checkMqttConnection();

}

bool reconnect(){
  if(!getMqttCredentiales()){
    Serial.println(boldRed + "\n\n      Error getting mqtt credentials :( \n\n RESTARTING IN 10 SECONDS");
    Serial.println(fontReset);
    delay(10000);
    ESP.restart();
    return false;
  }

  client.setServer("app.ivcariot.com",1883);
  Serial.print(underlinePurple + "\n\n\nTrying MQTT Connection" + fontReset + Purple + "  ⤵");
  String str_clientId = "device_" + dId + "_" + random(1,9999);
  const char* username = mqtt_data_doc["username"];
  const char* password = mqtt_data_doc["password"];
  String str_topic = mqtt_data_doc["topic"];

  if(client.connect(str_clientId.c_str(),username,password)){
    Serial.print(boldGreen + "\n\n         Mqtt Client Connected :) " + fontReset);
    delay(2000);
    client.subscribe((str_topic + "+/actdata").c_str());
    return true;

  }

  return false;
  Serial.print(boldRed + "\n\n         Mqtt Client Connection Failed :( " + fontReset);




}

long lastReconnectAttempt=0;

void checkMqttConnection(){
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(Red + "\n\n         Ups WiFi Connection Failed :( ");
    Serial.println(" -> Restarting..." + fontReset);
    delay(15000);
    ESP.restart(); // es como llamar al setup nuevamente
  }

  if(!client.connected()){
    long now = millis();
    
    if(now - lastReconnectAttempt > 5000){
      lastReconnectAttempt=millis();

      if(reconnect()){
        lastReconnectAttempt=0;
      }
    }
  }else{
    client.loop();
  }

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