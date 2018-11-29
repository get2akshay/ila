//JSON Parsing code
#include <ArduinoJson.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>

//Global Variables
bool desiredState = false;
bool currentState;
int pinNumber = 0;
int pinNumber1 = 1;
int pinNumber2 = 2;


//count messages arrived
int arrivedcount = 0;
    
#include <Arduino.h>
#include <Stream.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

//AWS
#include "sha256.h"
#include "Utils.h"

#include <Hash.h>
#include <WebSocketsClient.h>


//MQTT PAHO
#include <SPI.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>

//AWS MQTT Websocket
#include "Client.h"
#include "AWSWebSocketClient.h"
#include "CircularByteBuffer.h"

//AWS IOT config, change these:
char wifi_ssid[]       = "ByPlayIT";
char wifi_password[]   = "9899093163";
char aws_endpoint[]    = "a3qh8617optzvb.iot.us-west-2.amazonaws.com";
char aws_key[]         = "AKIAIS3YZBZTCDDHGVXA";
char aws_secret[]      = "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";

char aws_region[]      = "us-west-2";
const char* aws_topic  = "/things/AkshayThing/shadow/update";
int port = 443;

//MQTT config
const int maxMQTTpackageSize = 512;
const int maxMQTTMessageHandlers = 1;

ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient(1000);

IPStack ipstack(awsWSclient);
MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers> *client = NULL;

//# of connections
long connection = 0;

void setup() {
    //Initialise Serial port.
    Serial.begin (115200);
    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    //reset settings - for testing
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect()) {
    Serial.println("Failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  } 

  //if you get here you have connected to the WiFi
  Serial.println("Connection successful...yeey :)");

    //fill AWS parameters    
    awsWSclient.setAWSRegion(aws_region);
    awsWSclient.setAWSDomain(aws_endpoint);
    awsWSclient.setAWSKeyID(aws_key);
    awsWSclient.setAWSSecretKey(aws_secret);
    awsWSclient.setUseSSL(true);
    
    pinMode(pinNumber, INPUT);
    // read the input pin:
    bool pinState = digitalRead(pinNumber);

    if (connect ()){
      subscribe ();
      sendmessage (pinNumber,pinState);
    }
    //LED_BUILTIN State
    pinMode(pinNumber,OUTPUT);
    pinMode(pinNumber1,OUTPUT);
    pinMode(pinNumber2,OUTPUT);
}


void loop() {
  //keep the mqtt up and running
  if (awsWSclient.connected ()) {    
      client->yield();
  } else {
    //handle reconnection
    if (connect ()){
      subscribe ();      
    }
  }
  
}

//callback to handle mqtt messages
void messageArrived(MQTT::MessageData& md)
{
  MQTT::Message &message = md.message;
  
  Serial.print("Message ");
  Serial.print(++arrivedcount);
  Serial.print(" arrived: qos ");
  Serial.print(message.qos);
  Serial.print(", retained ");
  Serial.print(message.retained);
  Serial.print(", dup ");
  Serial.print(message.dup);
  Serial.print(", packetid ");
  Serial.println(message.id);
  Serial.print("Payload ");
  char* msg = new char[message.payloadlen+1]();
  memcpy (msg,message.payload,message.payloadlen);
  Serial.println("Debug prints");
  Serial.println(msg);
  Serial.println("End of Debug prints!");

  const size_t bufferSize = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3) + 50;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  const char* json = msg;
  JsonObject& root = jsonBuffer.parseObject(json);
  int PIN_Number = root["PIN"];
  bool PIN_desired = root["desired"]; // "LOW" or "HIGH"
  
  
  // Test if parsing succeeds.
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }
  // Print values.
  pinNumber = PIN_Number;
  if (PIN_desired) {
    Serial.println("AWS asks the Light to turn ON PIN Number:");
    Serial.println(pinNumber);
    digitalWrite(pinNumber,HIGH);
        } else {
      Serial.println("AWS asks the Light to turn OFF");
      delay(20);
      digitalWrite(pinNumber,LOW);
      }
  delete msg;
}


//send a message to a mqtt topic
void sendmessage (int pinNumber,bool currentState) {
     StaticJsonBuffer<200> jsonBuffer;
     JsonObject& root = jsonBuffer.createObject();
     root["PIN"] = pinNumber;
     root["desired"] = false;
     root["reported"] = currentState;
     String var;
     root.printTo(var);    
    //send a message
    MQTT::Message message;
    
    char buf[var.length()];

    strcpy(buf, var.c_str());
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)buf;
    message.payloadlen = strlen(buf);
    int rc = client->publish(aws_topic, message); 
}



//connects to websocket layer and mqtt layer
bool connect () {

    if (client == NULL) {
      client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
    } else {

      if (client->isConnected ()) {    
        client->disconnect ();
      }  
      delete client;
      client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
    }


    //delay is not necessary... it just help us to get a "trustful" heap space value
    delay (1000);
    Serial.print (millis ());
    Serial.print (" - conn: ");
    Serial.print (++connection);
    Serial.print (" - (");
    Serial.print (ESP.getFreeHeap ());
    Serial.println (")");




   int rc = ipstack.connect(aws_endpoint, port);
    if (rc != 1)
    {
      Serial.println("error connection to the websocket server");
      return false;
    } else {
      Serial.println("websocket layer connected");
    }


    Serial.println("MQTT connecting");
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    char* clientID = generateClientID ();
    data.clientID.cstring = clientID;
    rc = client->connect(data);
    delete[] clientID;
    if (rc != 0)
    {
      Serial.print("error connection to MQTT server");
      Serial.println(rc);
      return false;
    }
    Serial.println("MQTT connected");
    return true;
}

//subscribe to a mqtt topic
void subscribe () {
   //subscript to a topic
    int rc = client->subscribe(aws_topic, MQTT::QOS0, messageArrived);
    if (rc != 0) {
      Serial.print("rc from MQTT subscribe is ");
      Serial.println(rc);
      return;
    }
    Serial.println("MQTT subscribed");
}

//generate random mqtt clientID
char* generateClientID () {
  char* cID = new char[23]();
  for (int i=0; i<22; i+=1)
    cID[i]=(char)random(1, 256);
  return cID;
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

