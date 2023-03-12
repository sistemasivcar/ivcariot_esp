#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <Colors.h>
#include <IoTicosSplitter.h>
#include <ArduinoJson.h>
#include <Ticker.h>

// include MDNS
// #ifdef ESP8266
// #include <ESP8266mDNS.h>
// #elif defined(ESP32)
// #include <ESPmDNS.h>
// #endif

int mqtt_port=1883;

/* String dId = "5555";
String webhook_pass = "OpCHrljrjW";
String webhook_url = "https://app.ivcariot.com:3001/api/webhook/getdevicecredentials";
const char *mqtt_host = "app.ivcariot.com";
 */

String webhook_url = "http://192.168.0.8:3001/api/webhook/getdevicecredentials";
String dId = "7777";
String webhook_pass = "uwOvotZDpH"; //uwOvotZDpH
const char *mqtt_host = "192.168.0.8";

// FLAGS
int flag_led_status = 0;

// PINS
#define CONNECTIVITY_STATUS 2
#define TRIGGER_PIN 4
#define ESTADO_ALARMA 5

// WIFI

// Function Definitions TEMPLATE
void clear();
void checkWiFiConnection();
bool getMqttCredentiales();
void checkMqttConnection();
bool reconnect();
void sendToBroker();
void callback(char *topic, byte *payload, unsigned int length);
void processIncomingMsg(String topic, String incoming);
void print_stats();
void reportPresence();
void setupMqttClient();
void setupWiFiManagerClient();
void inizialice();
void checkEnterAP();
void saveParamCallback();
void getParam();
void changeStatusLed();

// Functiones Definitions APPLICATION
void processSensors();
void processActuators();
void publicData(boolean value);

// Instances
WiFiClient espClient;           // me sirve para usar la conexion wifi
PubSubClient client(espClient); // se la paso a PubSubClient para que se conecte
DynamicJsonDocument mqtt_data_doc(2048);
DynamicJsonDocument presence(350);
IoTicosSplitter splitter;
WiFiManager wm;
WiFiManagerParameter custom_param_mqtt_port;
WiFiManagerParameter custom_param_mqtt_host;
WiFiManagerParameter custom_param_whpassword;
WiFiManagerParameter custom_param_dId;
Ticker ticker;

void setup()
{
  Serial.begin(921600);
  clear();
  Serial.print(boldGreen + "\nChipID -> " + fontReset + WIFI_getChipId());
  pinMode(CONNECTIVITY_STATUS, OUTPUT);
  pinMode(ESTADO_ALARMA, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  digitalWrite(ESTADO_ALARMA, HIGH);

  setupWiFiManagerClient();
  inizialice();

  Serial.print(backgroundGreen + "\n\n LOOP RUNNING...");

}

void loop()
{

  checkEnterAP();
  checkWiFiConnection();
  checkMqttConnection();

}

/* -------- APPLICATION TASKS -------- */

void processSensors()
{

  int led_status = digitalRead(ESTADO_ALARMA);

  if (led_status == 1 && flag_led_status == 0)
  {
    publicData(digitalRead(ESTADO_ALARMA) == HIGH);
    flag_led_status = 1;
  }
  if (led_status == 0 && flag_led_status == 1)
  {
    publicData(digitalRead(ESTADO_ALARMA) == HIGH);
    flag_led_status = 0;
  }

  // get temp simulation
  int temp = random(1, 100);
  // get hum simulation
  int hum = random(0, 100);

  mqtt_data_doc["variables"][0]["last"]["value"] = temp;
  mqtt_data_doc["variables"][0]["last"]["save"] = 1;

  mqtt_data_doc["variables"][1]["last"]["value"] = hum;
  mqtt_data_doc["variables"][1]["last"]["save"] = 1;
}

void processActuators()
{

  if (mqtt_data_doc["variables"][3]["last"]["value"] == "on_led")
  {

    digitalWrite(ESTADO_ALARMA, HIGH);
    mqtt_data_doc["variables"][3]["last"]["value"] = "";
  }
  else if (mqtt_data_doc["variables"][4]["last"]["value"] == "off_led")
  {

    digitalWrite(ESTADO_ALARMA, LOW);
    mqtt_data_doc["variables"][4]["last"]["value"] = "";
  }
}

void publicData(boolean value)
{

  String root_topic = mqtt_data_doc["topic"];
  String variable = mqtt_data_doc["variables"][2]["variable"];
  String topic = root_topic + variable + "/sdata";

  String toSend = "";
  mqtt_data_doc["variables"][2]["last"]["value"] = value; // true/false
  mqtt_data_doc["variables"][2]["last"]["save"] = 1;
  serializeJson(mqtt_data_doc["variables"][2]["last"], toSend);
  client.publish(topic.c_str(), toSend.c_str(), true);
}

/* -------- TEMPLATE TASKS -------- */

void setupMqttClient(){

  /* We configure parameters for the MQTT instance */

  client.setServer(mqtt_host, mqtt_port);
  client.setCallback(callback);
  client.setKeepAlive(120); 

}

void setupWiFiManagerClient(){

  /* We configure parameters for the WiFiManager instance */
  wm.setDebugOutput(false);
  wm.setConnectTimeout(20);
  wm.setCountry("AR");
  wm.setConfigPortalTimeout(120);  // duracion del AP
  wm.setBreakAfterConfig(true);    // always exit configportal even if wifi save fails
  wm.setEnableConfigPortal(false); // if true (default) then start the config portal from autoConnect if connection failed
  // Web Styles
  wm.setRemoveDuplicateAPs(false); // do not remove duplicate ap names (true)
  wm.setMinimumSignalQuality(20);  // set min RSSI (percentage) to show in scans, null = 8%
  wm.setShowInfoErase(false);      // do not show erase button on info page
  wm.setScanDispPerc(true);        // show RSSI as percentage not graph icons

  wm.setTitle("IVCAR IoT");
  wm.setCustomHeadElement("Device Managment - IvcarIoT");

  // test custom html(radio)
  const char *mqtt_port_input_str = "<br/><label for='mqttportid'>MQTT PORT</label><input type='text' name='mqttportid' value='1883'>";
  const char *mqtt_host_input_str = "<br/><label for='mqtthostid'>MQTT HOST</label><input type='text' name='mqtthostid' value='192.168.0.8'>";
  const char *mqtt_did_input_str = "<br/><label for='deviceid'>DEVICE ID</label><input type='text' name='deviceid' placeholder='Enter your own dId'>";
  const char *mqtt_whpassword_input_str = "<br/><label for='whpasswordid'>DEVICE PASSWORD</label><input type='text' name='whpasswordid' placeholder='Enter the password'>";
  new (&custom_param_mqtt_port) WiFiManagerParameter(mqtt_port_input_str); // custom HTML input
  new (&custom_param_mqtt_host) WiFiManagerParameter(mqtt_host_input_str);
  new (&custom_param_dId) WiFiManagerParameter(mqtt_did_input_str);
  new (&custom_param_whpassword) WiFiManagerParameter(mqtt_whpassword_input_str);

  wm.addParameter(&custom_param_mqtt_port);
  wm.addParameter(&custom_param_mqtt_host);
  wm.addParameter(&custom_param_dId);
  wm.addParameter(&custom_param_whpassword);
  wm.setSaveParamsCallback(saveParamCallback);
  std::vector<const char *> menu = {"wifi", "info", "param", "sep", "restart", "exit"};
  wm.setMenu(menu);

}

void inizialice(){

  /* Aca hacemos el primer intento de conexion WIFI. Si hay credenciales guardadas, 
  inicia la conexion con eso. Si no nunca se configuraron las credenciales entra
  en modo AP. En ambos casos si la conexion falla, seguimos intentado (de forma no bloqueante)
  en el loop */

  bool wifiConnectionSuccess;

  /* Necesito este if/else unicamente para cambiar el comportamiento del
  LED. Si indica que el esp está en modo AP o intentando conexion WIFI */

  if (wm.getWiFiIsSaved()){

    Serial.print(underlinePurple + "\n\nWiFi Connection in Progress..." + fontReset + Purple);
    // try to connect with the last SSID and PASSWORD saved
    
    ticker.attach(0.7, changeStatusLed);
    wifiConnectionSuccess = wm.autoConnect("IvcarIoT"); // blocking
    ticker.detach();
  }
  else{
    
    // SI no tengo credenciales guardadas WIFiManager entra en modo AP automaticamente
    Serial.print(underlinePurple + "\n\nNo Previous WIFI Credentials Saved" + Purple);
    Serial.print("  ⤵" + fontReset);
    Serial.print(boldGreen + "\n\n         Access Point Started:" + fontReset);
    Serial.print("\nSSID: IvcarIoT");
    Serial.print("\nPASS: 12345678");
    ticker.attach(0.2, changeStatusLed);
    wifiConnectionSuccess = wm.autoConnect("IvcarIoT","12345678"); // blocking
    ticker.detach();
  }

  /* Para este punto, salió del modo AP bloqueante ya sea porque el usuario guardó credenciales,
  salio del portal, o se cumplió el tiempo limite de estar en modo AP que son 120 seg (timeout).
  La libreria me retoran un boolean diciendo si se pudo conectar o no.*/

  if (wifiConnectionSuccess)
  {
    Serial.print("  ⤵" + fontReset);
    Serial.print(boldGreen + "\n\n         WiFi Connection SUCCESS :)" + fontReset);
    digitalWrite(CONNECTIVITY_STATUS, HIGH);    
  }
  else
  {
    Serial.print("  ⤵" + fontReset);
    Serial.print(Red + "\n\n         Ups WiFi Connection Failed :( " + fontReset);
    digitalWrite(CONNECTIVITY_STATUS, LOW);
  }


}

void changeStatusLed()
{
  digitalWrite(CONNECTIVITY_STATUS, !digitalRead(CONNECTIVITY_STATUS));
}

String getParam(String name)
{
  // read parameter from server, for customhmtl input
  String value;
  if (wm.server->hasArg(name))
  {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback()
{
  /* Callback triggerd whenever the user save
  the setup page. Capture the data and save it in global
  variables to use later */

  dId = getParam("deviceid");
  webhook_pass = getParam("whpasswordid");
  mqtt_host = getParam("mqtthostid").c_str();
  mqtt_port = getParam("mqttportid").toInt();
}

void checkEnterAP()
{
  /* Check permanently if user triggerd the button
  to enter AP MODE. Only exit this status if user click on
  save - exit - or timout expiress, otherwire the loop is blocked */
  if (digitalRead(TRIGGER_PIN) == LOW)
  {
    Serial.print(backgroundRed + "\n\nLOOP BLOCKED" + fontReset);
    Serial.print(boldYellow + "\n\nAccess Point Started:");
    Serial.print("  ⤵" + fontReset);
    Serial.print(boldWhite + "\nSSID:" + fontReset + "IvcarIoT");
    Serial.print(boldWhite + "\nPASS:" + fontReset + "12345678");
    ticker.attach(0.1, changeStatusLed);

    wm.startConfigPortal("IvcarIoT"); // loop is blocked

    ticker.detach();
    Serial.print(backgroundGreen + "\n\nLOOP RUNNING..." + fontReset);
  }
}

void reportPresence()
{
  String root_topic = mqtt_data_doc["topic"];
  String topic = root_topic + "dummy_var/status";
  String toSend = "";
  presence["online"]["status"] = "online";
  presence["online"]["name"] = mqtt_data_doc["device_name"];
  serializeJson(presence["online"], toSend);
  client.publish(topic.c_str(), toSend.c_str(), true);
  Serial.print(boldGreen + "\n\n          ¡DEVICE ONLINE!" + fontReset);
}

String last_received_topic = "";
String last_received_msg = "";

void processIncomingMsg(String topic, String incoming)
{
  last_received_topic = topic;
  last_received_msg = incoming;

  String variable = splitter.split(topic, '/', 2);

  for (int i = 0; i < mqtt_data_doc["variables"].size(); i++)
  {

    if (mqtt_data_doc["variables"][i]["variable"] == variable)
    {

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

void callback(char *topic, byte *payload, unsigned int length)
{
  String incoming = "";

  for (int i = 0; i < length; i++)
  {
    incoming += (char)payload[i];
  }

  incoming.trim();
  processIncomingMsg(String(topic), incoming);
}

long varsLastSend[20];
void sendToBroker()
{

  long now = millis();

  for (int i = 0; i < mqtt_data_doc["variables"].size(); i++)
  {
    String variableType = mqtt_data_doc["variables"][i]["variableType"];
    String sendMethod = mqtt_data_doc["variables"][i]["sendMethod"];

    if (variableType == "output" || (variableType == "input" && sendMethod == "change_status"))
    {
      continue;
    }
    // "input" variable type
    int send_freq = mqtt_data_doc["variables"][i]["variableSendFreq"];

    if (now - varsLastSend[i] > send_freq * 1000)
    {
      varsLastSend[i] = millis();
      String str_root_topic = mqtt_data_doc["topic"];
      String str_variable = mqtt_data_doc["variables"][i]["variable"];
      String str_topic = str_root_topic + str_variable + "/sdata"; // ex: uid/did/var/sdata
      String toSend = "";

      serializeJson(mqtt_data_doc["variables"][i]["last"], toSend);
      // ex: toSend = "{"value":3}"

      client.publish(str_topic.c_str(), toSend.c_str(), true); // retained msg
      long counter = mqtt_data_doc["variables"][i]["counter"];
      counter++;
      mqtt_data_doc["variables"][i]["counter"] = counter;
    }
  }
}

long lastRequestCredentilasAttempt = 0;
bool reconnect()
{

  long now = millis();
  if (now - lastRequestCredentilasAttempt > 5000)
  {
    lastRequestCredentilasAttempt = millis();
    if (!getMqttCredentiales())
    {
      Serial.println(boldRed + "\n\n      Error getting mqtt credentials :( \n\n NEW ATTEMPT IN 5 SECONDS");
      Serial.println(fontReset);
      return false;
    }
  }

  setupMqttClient();
  Serial.print(underlinePurple + "\n\n\nTrying MQTT Connection" + fontReset + Purple + "  ⤵");
  String str_clientId = "device_" + dId;
  const char *username = mqtt_data_doc["username"];
  const char *password = mqtt_data_doc["password"];
  String str_topic = mqtt_data_doc["topic"];
  String will_topic = str_topic + "dummy_var/status";
  String will_message = "";
  presence["offline"]["status"] = "offline";
  presence["offline"]["name"] = mqtt_data_doc["device_name"];
  serializeJson(presence["offline"], will_message);

  if (client.connect(str_clientId.c_str(), username, password, will_topic.c_str(), 1,
                     true, will_message.c_str(), false))
  { // will retain :true- clean session:false

    // WE ARE CONNECTED TO THE MQTT BROKER
    Serial.print(boldGreen + "\n\n         Mqtt Client Connected :) " + fontReset);
    delay(2000);
    reportPresence();
    delay(2000);
    client.subscribe((str_topic + "+/actdata").c_str());
    return true;
  }

  Serial.print(boldRed + "\n\n         Mqtt Client Connection Failed :( " + fontReset);
  return false;
}

long lastWiFiConnectionAttempt = 0;

void checkWiFiConnection()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(CONNECTIVITY_STATUS, LOW);

    long now = millis();
    if (now - lastWiFiConnectionAttempt > 30000)
    {
      lastWiFiConnectionAttempt = millis();
      Serial.print(Red + "\n\n         Ups WiFi Connection Failed :( ");
      Serial.print(underlinePurple + "\n\nTrying Connection Again to: ");
      Serial.print(underlineWhite + boldWhite + wm.getWiFiSSID()+fontReset);
      WiFi.begin(wm.getWiFiSSID().c_str(), wm.getWiFiPass().c_str());
    }
  }
  else
  {
    digitalWrite(CONNECTIVITY_STATUS, HIGH);
  }
}

long lastMqttReconnectAttempt = 0;
void checkMqttConnection()
{
  // Intento la conexion MQTT SIEMPRE Y CUANDO TENGA PRIMERO
  // UNA CONEXION WIFI EXITOSA
  if (!client.connected() && WiFi.status() == WL_CONNECTED)
  {
    long now = millis();

    if (now - lastMqttReconnectAttempt > 5000)
    {
      // hago el el proximo intento sea en 5 segundos
      lastMqttReconnectAttempt = millis();
      if (reconnect())
      {
        // si me logre conectar pero mas tarde se cae la conexion,
        // entonces voy a intentar la conexion inmediatamente
        lastMqttReconnectAttempt = 0;
      }
    }
  }
  else
  {
    client.loop();
    //processSensors();
    sendToBroker();
    //print_stats();
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

  if (response_code < 0){
    Serial.print(boldRed + "\n\n         Error Sending Post Request :( " + fontReset);
    http.end();
    return false;
  }

  else if (response_code != 200){
    Serial.print(boldRed + "\n\n         Error in response :(   e-> " + fontReset + " " + response_code);
    http.end();
    return false;
  }

  else if (response_code == 200)
  {
    String response_body = http.getString(); // JSON format
    Serial.print(boldGreen + "\n\n         Mqtt Credentials Obtained Successfully :) " + fontReset);
    http.end();
    deserializeJson(mqtt_data_doc, response_body);
    mqtt_data_doc["obtained"] = "yes";
    delay(2000);
    return true;
  }

  return false;
}

long lastStats = 0;
void print_stats()
{
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

      Serial.println(String(i) + " \t " + variableFullName.substring(0, 5) + " \t\t " + variable.substring(0, 10) + " \t " + variableType.substring(0, 5) + " \t\t " + String(counter).substring(0, 10) + " \t\t " + lastMsg);
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