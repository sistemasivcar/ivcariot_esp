#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Colors.h>
#include <IoTicosSplitter.h>
#include <ArduinoJson.h>


int mqtt_port = 1883;
/* String dId = "5555";
String webhook_pass = "OpCHrljrjW";
String webhook_url = "https://app.ivcariot.com:3001/api/webhook/getdevicecredentials";
const char *mqtt_server = "app.ivcariot.com";
 */


String dId = "7777";
String webhook_pass = "uwOvotZDpH";
String webhook_url = "http://192.168.0.8:3001/api/webhook/getdevicecredentials";
const char *mqtt_server = "192.168.0.8";

int flag_led_status = 0;

// PINS
#define led 2

// WIFI
const char *wifi_ssid = "Fibertel WiFi129 2.4GHz";
const char *wifi_password = "0041491096";

// const char *wifi_ssid = "WIFI_IVCAR";
// const char *wifi_password = "Gaston2001";

// Function Definitions TEMPLATE
void clear();
bool getMqttCredentiales();
void checkMqttConnection();
bool reconnect();
void sendToBroker();
void callback(char* topic, byte* payload, unsigned int length);
void processIncomingMsg(String topic, String incoming);
void print_stats();
void reportPresence();

// Functiones Definitions APPLICATION
void processSensors();
void processActuators();
void publicData(boolean value);


// Instances
WiFiClient wifiClient;
PubSubClient client(wifiClient);
DynamicJsonDocument mqtt_data_doc(2048);
DynamicJsonDocument presence(350);
IoTicosSplitter splitter;

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

  client.setServer(mqtt_server,mqtt_port);
  client.setCallback(callback);
  client.setKeepAlive(120); // 5 mins


}

void loop()
{

  checkMqttConnection();


}



/* -------- APPLICATION TASKS -------- */

void processSensors(){

  int led_status = digitalRead(led);

  if(led_status == 1 && flag_led_status==0){
    publicData(digitalRead(led)==HIGH);
    flag_led_status=1;
  }
  if(led_status == 0 && flag_led_status==1){
    publicData(digitalRead(led)==HIGH);
    flag_led_status=0;
  }

  // get temp simulation
  int temp = random(1,100);
  // get hum simulation
  int hum = random(0,100);
  
  mqtt_data_doc["variables"][0]["last"]["value"]=temp;
  mqtt_data_doc["variables"][0]["last"]["save"]=1;

  mqtt_data_doc["variables"][1]["last"]["value"]=hum;
  mqtt_data_doc["variables"][1]["last"]["save"]=1;

}

void processActuators(){

  if(mqtt_data_doc["variables"][3]["last"]["value"]=="on_led"){

    digitalWrite(led,HIGH);
    mqtt_data_doc["variables"][3]["last"]["value"]="";

  }else if(mqtt_data_doc["variables"][4]["last"]["value"]=="off_led"){

    digitalWrite(led,LOW);
    mqtt_data_doc["variables"][4]["last"]["value"]="";
  }

}

void publicData(boolean value){

  String root_topic = mqtt_data_doc["topic"];
  String variable = mqtt_data_doc["variables"][2]["variable"];
  String topic = root_topic + variable + "/sdata";

  String toSend = "";
  mqtt_data_doc["variables"][2]["last"]["value"] = value; // true/false
  mqtt_data_doc["variables"][2]["last"]["save"] = 1;
  serializeJson(mqtt_data_doc["variables"][2]["last"], toSend);
  client.publish(topic.c_str(),toSend.c_str(),true);
}
/* -------- TEMPLATE TASKS -------- */

void reportPresence(){
  String root_topic = mqtt_data_doc["topic"];
  String topic = root_topic + "dummy_var/status";
  String toSend = "";
  presence["online"]["status"]="online";
  presence["online"]["name"]=mqtt_data_doc["device_name"];
  serializeJson(presence["online"], toSend);
  client.publish(topic.c_str(), toSend.c_str(),true);
  Serial.print(boldGreen + "\n\n Presence Message Send!" + fontReset);
}

String last_received_topic = "";
String last_received_msg = "";

void processIncomingMsg(String topic, String incoming){
  last_received_topic = topic;
  last_received_msg = incoming;

  String variable = splitter.split(topic, '/', 2);

  for (int i = 0; i < mqtt_data_doc["variables"].size(); i++ ){

    if (mqtt_data_doc["variables"][i]["variable"] == variable){
      
      DynamicJsonDocument inc_msg_doc(256);
      deserializeJson(inc_msg_doc, incoming);
      mqtt_data_doc["variables"][i]["last"] = inc_msg_doc;

      long counter = mqtt_data_doc["variables"][i]["counter"];
      counter++;
      mqtt_data_doc["variables"][i]["counter"] = counter;
    }
  }

  processActuators();
}

void callback(char* topic, byte* payload, unsigned int length){
  String incoming = "";

  for(int i=0; i < length; i++){
    incoming += (char)payload[i]; 
  }

  incoming.trim();
  processIncomingMsg(String(topic),incoming);
}

long varsLastSend[20];
void sendToBroker(){

  long now = millis();

  for(int i = 0; i < mqtt_data_doc["variables"].size(); i++){
    String variableType = mqtt_data_doc["variables"][i]["variableType"];
    String sendMethod = mqtt_data_doc["variables"][i]["sendMethod"];

    if(variableType=="output" || (variableType=="input" && sendMethod=="change_status")){
      continue;
    }
    // "input" variable type
    int send_freq = mqtt_data_doc["variables"][i]["variableSendFreq"];

    if(now - varsLastSend[i] > send_freq * 1000){
      varsLastSend[i] = millis();
      String str_root_topic = mqtt_data_doc["topic"];
      String str_variable = mqtt_data_doc["variables"][i]["variable"];
      String str_topic = str_root_topic + str_variable + "/sdata"; // ex: uid/did/var/sdata
      String toSend = "";

      serializeJson(mqtt_data_doc["variables"][i]["last"],toSend);
      // ex: toSend = "{"value":3}"

      client.publish(str_topic.c_str(),toSend.c_str(), true); // retained msg
      long counter = mqtt_data_doc["variables"][i]["counter"];
      counter++;
      mqtt_data_doc["variables"][i]["counter"] = counter;
    }

  } 
}

bool reconnect(){
  if(!getMqttCredentiales()){

    Serial.println(boldRed + "\n\n      Error getting mqtt credentials :( \n\n RESTARTING IN 10 SECONDS");
    Serial.println(fontReset);
    delay(10000);
    ESP.restart();
    return false;
  }

  
  Serial.print(underlinePurple + "\n\n\nTrying MQTT Connection" + fontReset + Purple + "  ⤵");
  String str_clientId = "device_" + dId;
  const char* username = mqtt_data_doc["username"];
  const char* password = mqtt_data_doc["password"];
  String str_topic = mqtt_data_doc["topic"];
  String will_topic = str_topic + "dummy_var/status";
  String will_message = "";
  presence["offline"]["status"]="offline";
  presence["offline"]["name"]=mqtt_data_doc["device_name"];
  serializeJson(presence["offline"], will_message);

  if(client.connect(str_clientId.c_str(), username, password, will_topic.c_str(), 1,
   true, will_message.c_str(), false)){ //will retain :true- clean session:false

    // WE ARE CONNECTED TO THE MQTT BROKER
    Serial.print(boldGreen + "\n\n         Mqtt Client Connected :) " + fontReset);
    delay(2000);   
    reportPresence();
    client.subscribe((str_topic + "+/actdata").c_str());   
    return true;

  }

  Serial.print(boldRed + "\n\n         Mqtt Client Connection Failed :( " + fontReset);
  return false;




}

long lastReconnectAttempt=0;

void checkMqttConnection(){
  // Handle WiFi Connection
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(Red + "\n\n         Ups WiFi Connection Failed :( ");
    Serial.println(" -> Restarting..." + fontReset);
    delay(15000);
    ESP.restart(); // es como llamar al setup nuevamente
  }

  // Handle MQTT/TCP Connection
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
    processSensors();
    sendToBroker();
    print_stats();
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

long lastStats = 0;
void print_stats(){
  long now = millis();

  if (now - lastStats > 2000)
  {
    lastStats = millis();
    clear();

    Serial.print("\n");
    Serial.print(Purple + "\n╔══════════════════════════╗" + fontReset);
    Serial.print(Purple + "\n║       SYSTEM STATS       ║" + fontReset);
    Serial.print(Purple + "\n╚══════════════════════════╝" + fontReset);
    Serial.print("\n\n");
    Serial.print("\n\n");

    Serial.print(boldCyan + "#" + " \t Name" + " \t VarName" + " \t Type" + " \t Count" + " \t Last V" + fontReset + "\n\n");

    for (int i = 0; i < mqtt_data_doc["variables"].size(); i++)
    {

      String variableFullName = mqtt_data_doc["variables"][i]["variableFullName"];
      String variable = mqtt_data_doc["variables"][i]["variable"];
      String variableType = mqtt_data_doc["variables"][i]["variableType"];
      String lastMsg = mqtt_data_doc["variables"][i]["last"];
      long counter = mqtt_data_doc["variables"][i]["counter"];

      Serial.println(String(i) + " \t " + variableFullName.substring(0,5) + " \t\t " + variable.substring(0,10) + " \t " + variableType.substring(0,5) + " \t\t " + String(counter).substring(0,10) + " \t\t " + lastMsg);
    }

    Serial.print(boldGreen + "\n\n Free RAM -> " + fontReset + ESP.getFreeHeap() + " Bytes");

    Serial.print(boldGreen + "\n\n Last Incomming Msg -> " + fontReset + last_received_msg);
  }
}

void clear()
{
  Serial.write(27);    // ESC command
  Serial.print("[2J"); // clear screen command
  Serial.write(27);
  Serial.print("[H"); // cursor to home command
}