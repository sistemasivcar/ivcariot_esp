#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Colors.h>
#include <IoTicosSplitter.h>
#include <Ticker.h>

int mqtt_port = 1883;

// CONFIG DEVICE
String dId = "";
String webhook_pass = "";
String webhook_url = "https://app.ivcariot.com:3001/api/webhook/getdevicecredentials";
const char *mqtt_host = "app.ivcariot.com";

// FLAGS
byte flag_led_status = 0;

// CONSTANTS
#define CONNECTIVITY_STATUS 2
#define FLASH 26

// Function Definitions TEMPLATE
void clear();
void checkWiFiConnection();
bool getMqttCredentiales();
void checkMqttConnection();
void checkDeviceConnectivity();
bool reconnect();
void sendToBroker();
void callback(char *topic, byte *payload, unsigned int length);
void processIncomingMsg(String topic, String incoming);
void print_stats();
void reportPresence();
void setupMqttClient();
void setupWiFiManagerClient();
void initialize();
void checkEnterAP();
void saveParamCallback();
String getParam(String name);
void changeStatusLed();

// Functiones Definitions APPLICATION
void processSensors();
void processActuators();

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
  pinMode(FLASH, INPUT_PULLUP);
  
  checkEnterAP();
  
  setupMqttClient();
  setupWiFiManagerClient();
  
  initialize();

  Serial.print(backgroundGreen + "\n\n LOOP RUNNING..." + fontReset);
}

void loop()
{

  checkDeviceConnectivity();
  
}

/* -------- APPLICATION FUNCTIONS -------- */

void processSensors()
{
  /*
  *
  * MANJEO DE LAS VARIABLES DE ENTRADA
  *
  * En esta funcion deberias leer los datos de los sensores. Es código que depende 
  * de qué variables maneja tu dispositivo (temperatura, estado de luz, bomba, etc...)
  * 
  * Si la variable es analógica, la publicacion se hará automaticamente segun la frecuencia
  * de envío que hallas configurado cuando crease la plantilla.
  * 
  * Si la variable es digital y configuraste que el dispositivo la envía por cada cambio de estado,
  * te tenés que encargar de detectar cuando la variable cambia de estado (ej. led on - led off) y
  * hacer la publicacion MQTT
  */

}

void processActuators()
{
  /* 
  * MANJEO DE LAS VARIABLES DE SALIDA
  *
  * Esta funcion es llamada automaticamente cada vez que recibas un mensaje MQTT. El contenido de
  * ese mensaje lo vas a encontrar en: mqtt_data_doc["variables"][index]["last"]["value"] y tenes 
  * que asegurarte de que "index" sea el indice que ocupa esa variable (widget en la plantilla) en el array 
  * de variables (o de widgets) de la plantilla asociada a este dispositivo
  * 
  * Recordá que el conteido del mensaje lo configuraste a la hora de crear la plantilla, asique debe conincidir
  * en tu código para saber QUÉ HACER CUANDO RECIBA TAL MENSAJE
  */

}


/* -------- TEMPLATE FUNCTIONS -------- */

void setupMqttClient()
{

  /* 
  * We configure parameters for the MQTT instance
  */

  client.setServer(mqtt_host, mqtt_port);
  client.setCallback(callback);
  client.setKeepAlive(120);
}

void setupWiFiManagerClient()
{

  /* 
  * We configure parameters for the WiFiManager instance 
  */ 

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

void initialize(){

  /* 
  * Aca hacemos el primer intento de conexion WIFI. Si hay credenciales guardadas,
  * inicia la conexion con eso. Si no nunca se configuraron las credenciales entra
  * en modo AP automaticamente. En ambos casos si la conexion falla, seguimos intentado
  * (de forma no bloqueante) en el loop
  */

  bool wifiConnectionSuccess;

  /*
  * Necesito este if/else unicamente para cambiar el comportamiento del
  * LED. En un caso el LED indica que el ESP está en modo AP y en el otro caso
  * que está intentando hacer la conexion WIFI con las ultimas credenciales guardadas
  */

  if (wm.getWiFiIsSaved()){

    Serial.print(underlinePurple + "\n\nWiFi Connection in Progress..." + fontReset + Purple);
    // try to connect with the last SSID and PASSWORD saved

    ticker.attach(0.7, changeStatusLed);
    wifiConnectionSuccess = wm.autoConnect("IvcarIoT"); // blocking
    ticker.detach();

  }else {

    // SI no tengo credenciales guardadas WIFiManager entra en modo AP automaticamente
    Serial.print(underlinePurple + "\n\nNo Previous WIFI Credentials Saved" + Purple);
    Serial.print("  ⤵" + fontReset);
    Serial.print(boldGreen + "\n\n         Access Point Started:" + fontReset);
    Serial.print("\nSSID: IvcarIoT");
    Serial.print("\nPASS: 12345678");
    ticker.attach(0.2, changeStatusLed);
    wifiConnectionSuccess = wm.autoConnect("IvcarIoT", "12345678"); // blocking
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
  /* 
  * Esta funcion es llamda n veces cada "x" segundos por Ticker para hacer el parpadeo del led
  * segun especifique en los parametros que recibe la libreria:
  * ticker.attach(x, funcion_a_llamar);
  * ticker.detach() -> deja de llamar a la funcion
   */
  digitalWrite(CONNECTIVITY_STATUS, !digitalRead(CONNECTIVITY_STATUS));
}

String getParam(String name)
{
  /*
  * Funcion para obtener parametros personalizados de la web que sirve el 
  * ESP cuando está en AP. Siempre se retornan como un Stirng 
  */

  String value;
  if (wm.server->hasArg(name))
  {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback()
{
  /*
  * Callback triggerd whenever the user save the setup page. 
  * Capture the data and save it in global
  * variables to use later
  */

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

  if (digitalRead(FLASH) == LOW)
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
  /* 
  * Publico mensaje MQTT (retenido) avisando que el dispositivo se encuentra ONLINE.
  * Esto se ejecuta cada vez que el dispoistivo se conecta al BROKER, ya sea la primera
  * vez que se conecta o por cada reconexión (El el backend envio notificacion SOLO SI 
  * EL DISPOSITIVO CAMBIÓ DE ONLINE A OFFLINE o viceversa)
  */

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

  /* 
  * Aca tengo que ver primero de qué variable me llegó el mensaje. Esto lo saco a partir
  * del tópico que tiene la forma: userid/deviceid/variableid/actdata
  * 
  * Despues tengo que encontrar esa variable en el array de variables que maneja el
  * dispoistivo e insertar en esa variable en la propiedad "last" el valor que me llego
  * habiendolo parseado previamente. Tener en cuenta que la plataforma envia en formato JSON 
  * algo como "{"value":"led_on"}"
  *  
  */

  String variable = splitter.split(topic, '/', 2);

  for (int i = 0; i < mqtt_data_doc["variables"].size(); i++)
  {

    if (mqtt_data_doc["variables"][i]["variable"] == variable)
    {

      DynamicJsonDocument inc_msg_doc(256);
      deserializeJson(inc_msg_doc, incoming);
      mqtt_data_doc["variables"][i]["last"] = inc_msg_doc;

      // STATS
      long counter = mqtt_data_doc["variables"][i]["counter"];
      counter++;
      mqtt_data_doc["variables"][i]["counter"] = counter;
    }
  }

  // STATS
  last_received_topic = topic;
  last_received_msg = incoming;

  processActuators();
}

void callback(char *topic, byte *payload, unsigned int length)
{
  /* 
  * Con esta funcion callback le estoy diciendo que tiene que hacer la libreria MQTT cada vez que detecte
  * que llego un mensaje. Entonces cuando llegue el mje Y SE EJECUTE LA INTRUCCION client.loop(), esta funcion
  * es llamda automaticamente.
  * 
  * Los mensajes MQTT se transmiten como una sucesion de códigos ASCII, por eso lo que me pasa la 
  * libreria es un array de bytes (pedazo de memoria) donde cada byte representa un codigo ASCII.
  * Entonces voy armando el mensaje como tal concatenando cada uno de estos
  */

  String incoming = "";

  for (int i = 0; i < length; i++)
  {
    // cada payload[i] es un ASCII entonces lo parseo a tipo char
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

void checkDeviceConnectivity(){
  checkWiFiConnection();
  checkMqttConnection();
}

long lastWiFiConnectionAttempt = 0;
void checkWiFiConnection()
{

  /* 

  * Chequeamos permanentemente la conectividad WiFi del dispositivo. Si no estoy conectado a WiFi 
  * inicio un intento de conexion con las ultimas credenciales almacenadas, pregunto por el estado a los 30 seg.
  * Si sigo desconectado hago otro intento, y así sucesivamente. 
  * ******Entre un intento y otro el loop sigue ejecutando******
  * 
  */

  if (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(CONNECTIVITY_STATUS, LOW);

    long now = millis();
    if (now - lastWiFiConnectionAttempt > 30000)
    {
      lastWiFiConnectionAttempt = millis();
      Serial.print(Red + "\n\n         Ups WiFi Connection Failed :( ");
      Serial.print(underlinePurple + "\n\nTrying Connection Again to: ");
      Serial.print(underlineWhite + boldWhite + wm.getWiFiSSID() + fontReset);
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
  /* 
  *
  * Sólo si estoy conectado a WiFi, chequeo la conexion MQTT con el broker. Si no estoy conectado, hago la
  * peticion HTTP para obtener credenciales al backend. Si la obtuve, intento la conexion MQTT. Estas ultimas 
  * dos acciones son bloqueantes (detienen el loop).  
  * 
  */

  if (!client.connected() && WiFi.status() == WL_CONNECTED)
  {
    long now = millis();

    if (now - lastMqttReconnectAttempt > 5000)
    {
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

    /*
    * Cando me llega un mje MQTT, éste va al BUFFER de la ESP. Esta línea lo que hace es decirle a la 
    * librería MQTT que vaya al buffer y vea se hay mensajes pendientes de ser procesados, en ese caso
    * se ejecuta la funcion callback y es ahi cuando el mensaje ya está disponible en mi programa. 
    * De lo contrario, pueden llegar mensajes pero nunca me voy a enterar si no llamo a client.loop()
    */

    client.loop();
    processSensors();
    sendToBroker();
    print_stats();
  }
}

bool getMqttCredentiales(){
  
  /*
  * Hacemos una peticion HTTP POST hacia nuestro backend y de forma BLOQUEANTE  (se frena el loop)
  * esperamos la respuesta para obtener toda la inforamcion que necesita el dispositivo como: 
  * credenciales MQTT, topico raíz al cual subscribirse (userId/dId) y todas las variables 
  * con su configuracion que se crearon el la APP WEB.
  * 
  * Si la respuesta es 200 (OK) guardamos la respuesta que viene en formato JSON a el documento
  * para que luego pueda ser usado como variables de C++ (es un parseo) 
  */

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

  else if (response_code != 200)
  {
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