/* ---------------------------------------------------------------
  Include require libraries to handle WIFI , MQTT and EEPROM , JSON functions
  connect Push button to switch ESP32 to access point on demand
  For ESP32 connect push button at GPIO4
  and ESP8266 connect push button at GPIO 6
  Press push button to continuously till the defined delay in Arduino sketch to make it access point.
*/

#ifdef ESP8266
#include <ESP8266WiFi.h>  // Pins for board ESP8266 Wemos-NodeMCU
#include <ESP8266WebServer.h>
ESP8266WebServer serverAP(80);
#define accessPointButtonPin D6   // Connect a button to this pin
#else
#include <WiFi.h>
#include <WebServer.h>
#define accessPointButtonPin 4    // Connect a button to this pin
WebServer serverAP(80);   // the Access Point Server
#endif
//------------Arduino Json Library------------//
#include <ArduinoJson.h>
//---------Addtional Arduino standard library-//
#include <stdio.h>
#include <stdint.h>

//-----------EEPROM Library---------------//
#include <EEPROM.h>
#define eepromTextVariableSize 33  // the max size of the ssid, password etc.  32+null terminated
//----------LED to indicate ESP32 in AP mode-------------//
#define accessPointLed 2

//---------- wifi default settings ------------------
char ssid[eepromTextVariableSize] = "WiFi Name";
char pass[eepromTextVariableSize] = "WiFi Password";

//------ MQTT broker settings and topics-------//
char broker[eepromTextVariableSize] = "broker.mqtt-dashboard.com";
char mqttUserName[eepromTextVariableSize] = "board_tester";
char mqttPass[eepromTextVariableSize] = "1234567890";

//-------MQTT Configuration-----------------------//
#include <MQTTClient.h>
WiFiClient net;
MQTTClient client;
const char* MQTT_TOPIC_1 = "microdigisoft_1";    // published
const char* MQTT_TOPIC_2 = "microdigisoft_2";    // published
const char* MQTT_TOPIC_3 = "microdigisoft_3";    // subscribed



//----Create a static buffer for posting data MQTT borker----//
StaticJsonBuffer<200> jsonBuffer;
JsonObject& root = jsonBuffer.createObject();
char JSONmessageBuffer[200];
//Device Chip ID in unsigned 64bit integer
uint64_t chipid;
/* Enable these lines in case you want to change the default Access Point ip: 192.168.4.1. You have to enable the line:
  WiFi.softAPConfig(local_ip, gateway, subnet);
  on the void initAsAccessPoint too */
//IPAddress local_ip(192,168,1,1);
//IPAddress gateway(192,168,1,1);
//IPAddress subnet(255,255,255,0);

boolean accessPointMode = false;    // is true every time the board is started as Access Point
boolean debug = true;
unsigned long lastUpdatedTime = 0;
int pushDownCounter = 0;
int lastConnectedStatus = 0;

//================================================ initAsAccessPoint
void initAsAccessPoint() {
  WiFi.softAP("ESP32-Access Point");      // or WiFi.softAP("ESP_Network","Acces Point Password");
  if (debug) Serial.println("AccesPoint IP: " + WiFi.softAPIP().toString());
  Serial.println("Mode= Access Point");
  //WiFi.softAPConfig(local_ip, gateway, subnet);  // enable this line to change the default Access Point IP address
  delay(100);
}

//========================================= connect
void checkWiFiConnection() {
  Serial.print("\nconnecting to wifi.");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nConnecting to WiFi...Please Wait!");
    delay(500);
    checkIfModeButtonPushed();
  }
  Serial.println("Connected to WIFI..");
  //--- create a random client id

  char clientID[] = "ESP8266_0000000000"; // For random generation of client ID.
  for (int i = 8; i < 18 ; i++) clientID[i] =  char(48 + random(10));

  Serial.print("\nconnecting to MQTT broker...Please Wait!");
  while (!client.connect(clientID)) {
    Serial.println("Connecting to MQTT Broker... ");
    delay(500);
  }
  Serial.println("\nconnected!");
  client.subscribe(MQTT_TOPIC_3);
}
//================================================ setup
//================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  // Json serialization print ESp32 chip ID ///

  chipid = ESP.getEfuseMac();
  Serial.printf("ESP32 Chip ID = %04X", (uint16_t)(chipid >> 32)); //print High 2 bytes
  Serial.printf("%08X\n", (uint32_t)chipid); //print Low 4bytes.
  char temp1[10];
  sprintf(temp1, "ESPID-%04X", (uint16_t)(chipid >> 32));
  char temp2[10];
  sprintf(temp2, "%08X", (uint32_t)chipid);
  Serial.println(temp1);
  Serial.println(temp2);
  root["ESP32 ID"] = temp1;

  
  //--- Check the first EEPROM byte. If this byte is "2" the board will start as Access Point
  int st = getStatusFromEeprom();
  if (st == 2) accessPointMode = true;
  else if (st != 0) saveSettingsToEEPPROM(ssid, pass, broker, mqttUserName, mqttPass); // run the void saveSettingsToEEPPROM on the first running or every time you want to save the default settings to eeprom
  Serial.println("\n\naccessPointMode=" + String(accessPointMode));

  readSettingsFromEEPROM(ssid, pass, broker, mqttUserName, mqttPass); // read the SSID and Passsword from the EEPROM
  Serial.println(ssid);
  Serial.println(pass);
  if (accessPointMode) {     // start as Access Point
    initAsAccessPoint();
    serverAP.on("/", handle_OnConnect);
    serverAP.onNotFound(handle_NotFound);
    serverAP.begin();
    saveStatusToEeprom(0);  // enable the Client mode for the the next board starting
  }
  else {            // start as client
    Serial.println("Mode= Client");
    WiFi.begin(ssid, pass);
    // Enter your client setup code here
  }
  client.begin(broker, net);
  client.onMessage(messageReceived);
  pinMode(accessPointButtonPin, INPUT);
  pinMode(accessPointLed, OUTPUT);
}

//========================================= messageReceived
void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);
  /* if (topic==topic_3){
     int v = payload.toInt();
     if (v==1) digitalWrite(D4,HIGH);
     else digitalWrite(D4,LOW);
    }
  */
}
char buf[5];
String message = "";
unsigned long x;
//============================================== loop
//==============================================
void loop() {
  if (accessPointMode) {
    serverAP.handleClient();
    playAccessPointLed();       // blink the LED every time the board works as Access Point
  }
  else {
    client.loop();
    delay(10);  // <- fixes some issues with WiFi stability
    if (!client.connected())checkWiFiConnection();
    // enter your client code here
    if (millis() - lastUpdatedTime > 5000) {
      int sensorValue_1 = random(100); // replace with your sensor value
      root["Sensor Reading: "] = sensorValue_1;
      root.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
      Serial.print("MQTT Data Ready to published");
      Serial.println(JSONmessageBuffer);
      client.publish(MQTT_TOPIC_1, JSONmessageBuffer);
      delay(1000);
      lastUpdatedTime = millis();
    }
  }
  checkIfModeButtonPushed();
}

//============================================
//--- Mode selector Button - push down handler
void checkIfModeButtonPushed() {
  while (digitalRead(accessPointButtonPin)) {
    pushDownCounter++;
    if (debug) Serial.println(pushDownCounter);
    delay(1000);
    if (pushDownCounter == 20) {  // after 2 seconds the board will be restarted
      if (!accessPointMode) saveStatusToEeprom(2);   // write the number 2 to the eeprom
      ESP.restart();
    }
  }
  pushDownCounter = 0;
}

//============================================ playAccessPointLed
unsigned long lastTime = 0;
void playAccessPointLed() {
  if (millis() - lastTime > 300) {
    lastTime = millis();
    digitalWrite(accessPointLed, !digitalRead(accessPointLed));
  }
}

//================ WiFi Manager necessary functions ==============
//================================================================
//================================================================

//==============================================
void handle_OnConnect() {
  if (debug) Serial.println("Client connected:  args=" + String(serverAP.args()));
  if (serverAP.args() >= 2)  {
    handleGenericArgs();
    serverAP.send(200, "text/html", SendHTML(1));
  }
  else  serverAP.send(200, "text/html", SendHTML(0));
}

//==============================================
void handle_NotFound() {
  if (debug) Serial.println("handle_NotFound");
  serverAP.send(404, "text/plain", "Not found");
}

//=================================
void handleGenericArgs() { //Handler
    for (int i = 0; i < serverAP.args(); i++) {
    if (debug) Serial.println("*** arg(" + String(i) + ") =" + serverAP.argName(i));
    if (serverAP.argName(i) == "ssid") {
      if (debug)  Serial.print("sizeof(ssid)="); Serial.println(sizeof(ssid));
      memset(ssid, '\0', sizeof(ssid));
      strcpy(ssid, serverAP.arg(i).c_str());
    }
    else   if (serverAP.argName(i) == "pass") {
      if (debug) Serial.print("sizeof(pass)="); Serial.println(sizeof(pass));
      memset(pass, '\0', sizeof(pass));
      strcpy(pass, serverAP.arg(i).c_str());
    }
    else   if (serverAP.argName(i) == "broker") {
      memset(broker, '\0', sizeof(broker));
      strcpy(broker, serverAP.arg(i).c_str());
    }
    else   if (serverAP.argName(i) == "mqtt_username") {
      memset(mqttUserName, '\0', sizeof(mqttUserName));
      strcpy(mqttUserName, serverAP.arg(i).c_str());
    }
    else   if (serverAP.argName(i) == "mqtt_pass") {
      memset(mqttPass, '\0', sizeof(mqttPass));
      strcpy(mqttPass, serverAP.arg(i).c_str());
    }
  }
  if (debug) Serial.println("*** New settings have received");
  if (debug) Serial.print("*** ssid="); Serial.println(ssid);
  if (debug) Serial.print("*** password="); Serial.println(pass);
  if (debug) Serial.print("*** broker="); Serial.println(broker);
  if (debug) Serial.print("*** mqttUserName="); Serial.println(mqttUserName);
  if (debug) Serial.print("*** mqttPass="); Serial.println(mqttPass);


  saveSettingsToEEPPROM(ssid, pass, broker, mqttUserName, mqttPass);

}
//===================================
String SendHTML(uint8_t st) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>ESP32 WiFi Manager</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 30px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr += "label{display:inline-block;width: 160px;text-align: right;}\n";
  ptr += "form{margin: 0 auto;width: 360px;padding: 1em;border: 1px solid #CCC;border-radius: 1em; background-color: #6e34db;}\n";
  ptr += "input {margin: 0.5em;}\n";
  if (st == 1) ptr += "h3{color: green;}\n";
  ptr += "</style>\n";
  ptr += "<meta charset=\"UTF-8\">\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>ESP32 and MQTT WiFiManager Using EEPROM</h1>\n";
  if (st == 1)ptr += "<h3>WiFi settings has saved successfully!</h3>\n";
  else if (st == 2)ptr += "<h3>WIFI Credentials has saved successfully!</h3>\n";
  else ptr += "<h3>Enter the WiFi settings</h3>\n";
  ptr += "<form>";

  ptr += "<div><label for=\"label_1\">WiFi SSID</label><input id=\"ssid_id\" required type=\"text\" name=\"ssid\" value=\"";
  ptr += ssid;
  ptr += "\" maxlength=\"32\"></div>\n";

  ptr += "<div><label for=\"label_2\">WiFi Password</label><input id=\"pass_id\" type=\"text\" name=\"pass\" value=\"";
  ptr += pass;
  ptr += "\" maxlength=\"32\"></div>\n";
  
  ptr += "<div><label for=\"label_3\">MQTT Server</label><input id=\"broker_id\" required type=\"text\" name=\"broker\" value=\"";
  ptr += broker;
  ptr += "\" maxlength=\"32\"></div>\n";

  ptr += "<div><label for=\"label_4\">MQTT Username</label><input id=\"username_id\" required type=\"text\" name=\"mqtt_username\" value=\"";
  ptr += mqttUserName;
  ptr += "\" maxlength=\"32\"></div>\n";

  ptr += "<div><label for=\"label_5\">MQTT Password</label><input id=\"mqtt_pass_id\" type=\"text\" name=\"mqtt_pass\" value=\"";
  ptr += mqttPass;
  ptr += "\" maxlength=\"32\"></div>\n";

  ptr += "<div><input type=\"submit\" value=\"Submit\"accesskey=\"s\"></div></form>";

  ptr += "<h5></h5>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}






//====================== EEPROM necessary functions ==============
//================================================================
//================================================================
#define eepromBufferSize 200     // have to be >  eepromTextVariableSize * (eepromVariables+1)   (33 * (5+1))

//========================================== writeDefaultSettingsToEEPPROM
void saveSettingsToEEPPROM(char* ssid_, char* pass_, char* broker_, char* mqttUsername_, char* mqttPass_) {
  if (debug) Serial.println("\n============ saveSettingsToEEPPROM");
  writeEEPROM(1 * eepromTextVariableSize , eepromTextVariableSize , ssid_);
  writeEEPROM(2 * eepromTextVariableSize , eepromTextVariableSize ,  pass_);

  writeEEPROM(3 * eepromTextVariableSize , eepromTextVariableSize ,  broker_);
  writeEEPROM(4 * eepromTextVariableSize , eepromTextVariableSize ,  mqttUsername_);
  writeEEPROM(5 * eepromTextVariableSize , eepromTextVariableSize ,  mqttPass_);
  delay(100);
}
//========================================== readSettingsFromEeprom
void readSettingsFromEEPROM(char* ssid_, char* pass_, char* broker_, char* mqttUsername_, char* mqttPass_) {
   readEEPROM( 1 * eepromTextVariableSize , eepromTextVariableSize , ssid_);
  readEEPROM( (2 * eepromTextVariableSize) , eepromTextVariableSize , pass_);

  readEEPROM( (3 * eepromTextVariableSize) , eepromTextVariableSize , broker_);
  readEEPROM( (4 * eepromTextVariableSize) , eepromTextVariableSize , mqttUsername_);
  readEEPROM( (5 * eepromTextVariableSize) , eepromTextVariableSize , mqttPass_);


  if (debug) Serial.println("\n============ readSettingsFromEEPROM");
  if (debug) Serial.print("\n============ ssid="); if (debug) Serial.println(ssid_);
  if (debug) Serial.print("============ password="); if (debug) Serial.println(pass_);
  if (debug) Serial.print("============ broker="); if (debug) Serial.println(broker_);
  if (debug) Serial.print("============ mqttUsername="); if (debug) Serial.println(mqttUsername_);
  if (debug) Serial.print("============ mqttPassword="); if (debug) Serial.println(mqttPass_);


}

//================================================================
void writeEEPROM(int startAdr, int length, char* writeString) {
  EEPROM.begin(eepromBufferSize);
  yield();
  for (int i = 0; i < length; i++) EEPROM.write(startAdr + i, writeString[i]);
  EEPROM.commit();
  EEPROM.end();
}

//================================================================
void readEEPROM(int startAdr, int maxLength, char* dest) {
  EEPROM.begin(eepromBufferSize);
  delay(10);
  for (int i = 0; i < maxLength; i++) dest[i] = char(EEPROM.read(startAdr + i));
  dest[maxLength - 1] = 0;
  EEPROM.end();
}

//================================================================ writeEepromSsid
void saveStatusToEeprom(byte value) {
  EEPROM.begin(eepromBufferSize);
  EEPROM.write(0, value);
  EEPROM.commit();
  EEPROM.end();
}
//===================================================================
byte getStatusFromEeprom() {
  EEPROM.begin(eepromBufferSize);
  byte value = 0;
  value = EEPROM.read (0);
  EEPROM.end();
  return value;
}
