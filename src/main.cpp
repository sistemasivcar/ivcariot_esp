#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Colors.h>
#include <IoTicosSplitter.h>
#include <Ticker.h>

// PINS
#define CONNECTIVITY_STATUS 15
#define FLASH 16
#define CENTRAL 14
#define SIRENAS 12
#define INTERIOR 04
#define ABERTURAS 13
#define OUT 05
#define TX 02

// CONFIG DEVICE
String dId = "";          // la voy a leer de la EEPROM justo antes de obtener las credenciales
String webhook_pass = ""; // la voy a leer de la EEPROM justo antes de obtener las credenciales
String webhook_url = "http://app.ivcariot.com:3002/api/webhook/getdevicecredentials";
// String webhook_url = "http://app.ivcariot.com:3002/api/webhook/getdevicecredentials";

// MQTT
int mqtt_port = 1883;
// const char *mqtt_host = "app.ivcariot.com";
const char *mqtt_host = "app.ivcariot.com";

// LECTURA DE SENSORES
byte central;
byte sirena;
byte abertura;
byte interior;

// FLAGS
byte flag_central;
byte flag_sirena;
byte flag_interior;
byte flag_aberturas;

// GLOBALS

String last_received_topic = "";
String last_received_msg = "";
long varsLastSend[20];
long lastRequestCredentilasAttempt = 0;
long lastWiFiConnectionAttempt = 0;
long lastMqttReconnectAttempt = 0;
long lastStats = 0;

// Function Definitions (TEMPLATE)
void clear();
void setupMqttClient();
void setupWiFiManagerClient();
void checkEnterAP();
void initialize();
void checkDeviceConnectivity();
void checkWiFiConnection();
void checkMqttConnection();
bool getMqttCredentiales();
bool reconnect();
void reportPresence();
void sendToBroker();
void callback(char *topic, byte *payload, unsigned int length);
void processIncomingMsg(String topic, String incoming);
void print_stats();
void saveParamCallback();
String getParam(String name);
void changeStatusLed();

// Functiones Definitions (AUXILIAR)
void writeFlash(int addr, String valor);
String readFlash(int addr);
void setFlag(int addr, byte flag);
byte getFlag(int addr);
String getTopicToPublish(byte index);
String serializeMesageToSend(byte index, boolean lectura, boolean save);
void publicarCambio(byte lectura, byte index);
void incrementCounter(byte index);

// Functiones Definitions (APPLICATION)
void processSensors();
void processActuators();
void procesarComandosCentral();

// INSTANCES
WiFiClient espClient;
PubSubClient client(espClient);
DynamicJsonDocument mqtt_data_doc(2048);
DynamicJsonDocument presence(350);
IoTicosSplitter splitter;
WiFiManager wm;
WiFiManagerParameter custom_param_whpassword;
Ticker ticker;

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(512);
  
  clear();
  dId = String(ESP.getChipId());
  Serial.print(boldGreen + "\nChipID -> " + fontReset + dId);
  pinMode(CONNECTIVITY_STATUS, OUTPUT);
  pinMode(FLASH, INPUT_PULLUP);
  pinMode(CENTRAL, INPUT);
  pinMode(SIRENAS, INPUT);
  pinMode(ABERTURAS, INPUT);
  pinMode(INTERIOR, INPUT);
  pinMode(OUT, OUTPUT);
  pinMode(TX, OUTPUT);

  setupWiFiManagerClient();
  checkEnterAP();
  initialize();

  Serial.print(backgroundGreen + "\n\n LOOP RUNNING..." + fontReset);
}

void loop()
{
  checkDeviceConnectivity();
}

/* *************************************** */

/* -------- APPLICATION FUNCTIONS -------- */

/* *************************************** */

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
   * de envío que hallas configurado cuando crease la plantilla. Sólo tenes que encargarte de
   * estar actualizando los valores que lees de los sensores y almacenarlos en:
   * mqtt_data_doc["variables"][index]["last"]["value"]=dato_del_sensor.
   *
   * Importante: debes asegurarte de que "index" sea la posicion en el array que ocupa el widget
   * (de la plantilla asociada al dispositivo) en el cual querés ver los datos
   *
   * Si la variable es digital y configuraste que el dispositivo la envía por cada cambio de estado,
   * te tenés que encargar de detectar cuando la variable cambia de estado (ej. led on - led off) y
   * hacer la publicacion MQTT
   */

  long now = millis();


}


void processActuators()
{
  /*
   * MANJEO DE LAS VARIABLES DE SALIDA
   *
   * Esta funcion es llamada automaticamente cada vez que recibas un mensaje MQTT. El contenido de
   * ese mensaje lo vas a encontrar en: mqtt_data_doc["variables"][index]["last"]["value"] y tenes
   * que ASEGURARTE de que "index" sea el indice que ocupa esa variable en el array
   * de widgets de la plantlla asociada al dispositivo
   *
   * Recordá que el conteido del mensaje lo configuraste a la hora de crear la plantilla, asique debe conincidir
   * en tu código para saber QUÉ HACER CUANDO RECIBAS TAL MENSAJE
   */

  procesarComandosCentral();
}

void procesarComandosCentral()
{
  /*
   * PROCESAMIENTO DE BOTONES: ACTIVAR - DESACTIVAR
   *
   * Aca estoy definiendo que hacer en caso de pulsar el boton de ACTIVAR en
   * la aplicacion, y lo mimsmo cuando se presiona DESACTIVAR
   *
   * Cada uno manda un mensaje MQTT "activar" y "desactivar" que fueron configurados a la
   * hora de crear el widget en la plantilla asociada al dispositivo
   */

  Serial.println(digitalRead(CENTRAL));

  if (mqtt_data_doc["variables"][1]["last"]["value"] == "activar")
  {

    digitalWrite(CONNECTIVITY_STATUS, HIGH);

    mqtt_data_doc["variables"][1]["last"]["value"] = "";
  }

  else if (mqtt_data_doc["variables"][2]["last"]["value"] == "desactivar")
  {
    digitalWrite(CONNECTIVITY_STATUS, LOW);

    mqtt_data_doc["variables"][2]["last"]["value"] = "";
  }
}

/* *************************************** */

/* -------- AUXILIAR FUNCTIONS ----------- */

/* *************************************** */

void writeFlash(int addr, String valor)
{
  int size = valor.length();
  char inchar[15];
  valor.toCharArray(inchar, size + 1);
  // grabo caracter por caracter en cada celda de la EEPROM
  for (int i = 0; i < size; i++)
  {
    EEPROM.write(addr + i, inchar[i]);
  }
  // si el tamaño de "valor" es menor que el limite maximo limpio las demas posiciones
  for (int i = size; i < 15; i++)
  {
    EEPROM.write(addr + i, 255);
  }
  EEPROM.commit();
}

String readFlash(int addr)
{
  byte lectura;
  String str_lectura;
  // leo 15 posiciones de memoria desde la posicion "addr"
  for (int i = addr; i < addr + 15; i++)
  {
    lectura = EEPROM.read(i);
    if (lectura != 255)
    {
      str_lectura += (char)lectura;
    }
  }
  return str_lectura;
}

void setFlag(int addr, byte value)
{

  /*
   * Las primeras 30 posiciones de la EEPROM (de la 0 a la 29) ya estan ocupadas
   * Las banderas las guardo con el siguiente orden
   * 30 -> flag_central
   * 31 -> flag_sirena
   *
   */
  EEPROM.write(addr, value);
  EEPROM.commit();
}

byte getFlag(int addr)
{
  byte lectura;
  lectura = EEPROM.read(addr);
  if (lectura != 255)
  {
    return lectura;
  }
  return 0;
}

String getTopicToPublish(byte index)
{
  /*
   * @index: posicion que ocupa en el array de widgets la variable que quiero enviar
   *
   * Armo el topico para publicar mensaje MQTT hacia la aplicacion WEB y lo retorno
   *
   */

  String str_root_topic = mqtt_data_doc["topic"];
  String variable = mqtt_data_doc["variables"][index]["variable"]; // obtengo el variableId
  String str_topic = str_root_topic + variable + "/sdata";
  return str_topic;
}

String serializeMesageToSend(byte index, boolean lectura, boolean save)
{
  /*
   * @index: posicion que ocupa en el array de widgets la variable a la cual le voy a agregar
   * la data que quiero enviar para luego hacer la serializacion (pasar a JSON)
   *
   * @lectura: es la data a enviar, el cambio de estado.
   *
   * @save: indica si esa data debe ser guardada en base de datos o no
   *
   * El objetivo de la funcion es tomar el valor de "lectura" y ponerselo en la variable
   * que le corresponde para luego serializarlo con la libreria y retorno ese String
   * que es lo que finalmente se publicará
   *
   */

  String toSend = "";

  // armo el objeto a enviar:
  mqtt_data_doc["variables"][index]["last"]["value"] = lectura; // 0 / 1
  mqtt_data_doc["variables"][index]["last"]["save"] = save;

  // lo paso a JSON
  serializeJson(mqtt_data_doc["variables"][index]["last"], toSend);
  return toSend;
}

void publicarCambio(byte lectura, byte index)
{
  /*
  * @lectura: es el ultimo valor leido que debe ser publicado (COMO RETENIDO PARA QUE NO SE PIERDA
  SI EL CLIENTE WEB NO ESTA CONECTADO EN LA APLICACION EN ESE MOMENTO Y LO RECIBA AL CONECTARSE)
  *
  * En la funcion primero obtengo el topico a publicar. Como el mensaje lo tengo que mandar en fomrato
  * JSON tengo que llamar a la libreria para hacer la serializaicon de lo que quiero enviar que tendrá
  * la forma: "{"value":1, "save":1}" pero eso lo tengo que escribir en el documento mqtt_data_doc
  * de la variable correspondiente a enviar y luego obtengo el resultado listo para enviar
  *
  */

  String str_topic = getTopicToPublish(index);
  String message_to_send = serializeMesageToSend(index, lectura, true);
  client.publish(str_topic.c_str(), message_to_send.c_str(), true);
  incrementCounter(index);
}

void incrementCounter(byte index)
{

  /*
   * Cada vez que se envia un mensaje, a esa variable le incremento un contado.
   * Es solo para verlo por la terminar si activo la funcion print_sats()
   */

  long counter = mqtt_data_doc["variables"][index]["counter"];
  counter++;
  mqtt_data_doc["variables"][index]["counter"] = counter;
}

/* ************************************ */

/* -------- TEMPLATE FUNCTIONS -------- */

/* ************************************ */

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
  wm.setShowInfoErase(true);       // show erase (borrar) button on info page
  wm.setScanDispPerc(true);        // show RSSI as percentage not graph icons

  wm.setTitle("IVCAR IoT");
  wm.setCustomHeadElement("Device Managment - IvcarIoT");

  const char *mqtt_whpassword_input_str = "<br/><label for='whpasswordid'>DEVICE PASSWORD</label><input type='text' name='whpasswordid' placeholder='Enter the password'>";

  new (&custom_param_whpassword) WiFiManagerParameter(mqtt_whpassword_input_str);

  wm.addParameter(&custom_param_whpassword);

  wm.setSaveParamsCallback(saveParamCallback);

  std::vector<const char *> menu = {"wifi", "info", "param", "sep", "restart", "exit"};
  wm.setMenu(menu);
}

void initialize()
{

  /*
   * Aca hacemos el primer intento de conexion WIFI.
   *
   * Si hay credenciales guardadas, inicia la conexion con eso.
   * Si la conexion falla, seguimos intentado (de forma no bloqueante) en el loop.
   *
   * Si no nunca se configuraron las credenciales, ni siquiera intenta una conexion y
   * entra directamente al else y luego se va al loop
   */

  bool wifiConnectionSuccess;

  Serial.print(underlinePurple + "\n\nWiFi Connection in Progress..." + fontReset + Purple);
  ticker.attach(0.5, changeStatusLed);
  wifiConnectionSuccess = wm.autoConnect("IvcarIoT"); // blocking
  ticker.detach();

  if (wifiConnectionSuccess)
  {
    Serial.print("  ⤵" + fontReset);
    Serial.print(boldGreen + "\n\n         WiFi Connection SUCCESS :)" + fontReset);
    digitalWrite(CONNECTIVITY_STATUS, HIGH);
    delay(5000);
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

  webhook_pass = getParam("whpasswordid");
  writeFlash(15, webhook_pass);

  if (WiFi.status() == WL_CONNECTED)
    wm.stopConfigPortal();
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
    ticker.attach(0.2, changeStatusLed);

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

void sendToBroker()
{

  /*
   * Funcion que se ejecuta en el loop siempre y cuando este conectado al broker. Estoy chequeando
   * permanentemente para las variables de entrada que se configuraron para enviar datos periodicamente, si
   * se cumplió la frequencia de envío, y en ese caso publico.
   *
   */

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

void checkDeviceConnectivity()
{
  checkWiFiConnection();
}

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
    checkMqttConnection();
  }
}

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
    // sendToBroker(); no es necesario activar esta funcion para este dispositivo!
    // print_stats();
  }
}

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
  else
  {
    return false;
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

  bool mqttConnectionSuccess = client.connect(str_clientId.c_str(), username, password, will_topic.c_str(), 1, true, will_message.c_str(), false);
  ticker.detach();

  if (mqttConnectionSuccess)
  {

    // WE ARE CONNECTED TO THE MQTT BROKER
    Serial.print(boldGreen + "\n\n         Mqtt Client Connected :) " + fontReset);
    delay(2000);
    reportPresence();
    client.subscribe((str_topic + "+/actdata").c_str());
    digitalWrite(CONNECTIVITY_STATUS, HIGH);
    return true;
  }

  Serial.print(boldRed + "\n\n         Mqtt Client Connection Failed :( " + fontReset);
  digitalWrite(CONNECTIVITY_STATUS, LOW);
  return false;
}

bool getMqttCredentiales()
{

  /*
   * Hacemos una peticion HTTP POST hacia nuestro backend y de forma BLOQUEANTE  (se frena el loop)
   * esperamos la respuesta para obtener toda la inforamcion que necesita el dispositivo como:
   * credenciales MQTT, topico raíz al cual subscribirse (userId/dId) y todas las variables
   * con su configuracion que se crearon el la APP WEB.
   *
   * Si la respuesta es 200 (OK) guardamos la respuesta que viene en formato JSON a el documento
   * para que luego pueda ser usado como variables de C++ (es un parseo)
   */

  ticker.attach(0.5, changeStatusLed);
  Serial.print(underlinePurple + "\n\n\nGetting MQTT Credentials from WebHook" + fontReset + Purple + "  ⤵");

  webhook_pass = readFlash(15);

  Serial.println(fontReset + "\nURL: " + webhook_url);
  Serial.println("dId: " + dId + "\tpass: " + webhook_pass);

  String toSend = "dId=" + dId + "&whpassword=" + webhook_pass;

  HTTPClient http;
  http.begin(espClient, webhook_url);
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
    delay(3000);
    return true;
  }

  return false;
}

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