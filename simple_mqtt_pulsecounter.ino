/*****************************************************************************************
* Simple pulse counter which sends data over MQTT.
* This coutner uses interrupts on 2 pins - I have two water meters attached to ESP8266 
*                                          via simplest LM393 comparator circuit
* It is "talk-only" client. 
* It sends out data every 60 seconds by default.
* It keeps adding pulses until succesful connection.
*
* MQTT Message consists of 
*       P1, P2 - number of pulses counted since last succesful connection to server
*       Sec - period during which we collected P1 and P2, seconds. 
*       Per - number of base periods we collected data for. >1 if we failed to connect to server last time
*       Up  - ESP8266 uptime
*
* Copyright (C) 2016 Anton Viktorov <latonita@yandex.ru>
*
* This library is free software. You may use/redistribute it under The MIT License terms. 
*
*****************************************************************************************/
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

extern "C" {
#include "user_interface.h"
}

const char CompileDate[] = __DATE__ " " __TIME__;

#define WIFI_LED_PIN 5  // LED on when connected to Wifi
#define PULSE_PIN1 12   // P1
#define PULSE_PIN2 13   // P2

#define SAMPLE_MINUTES 1

#define SENSOR_FAMILY "ESP8266"  // final sensor name for MQTT will be SENSOR_FAMILY-SENSOR_NAME-SENSOR_ID
#define SENSOR_NAME "water"      // default values give: "ESP8266-water-1"
#define SENSOR_ID "1"

//#define MAX_MQTT_FAILURES_BEFORE_REBOOT 10 //not used

#define DEBOUNCE_MS 20 //debouncing for interrupts

const char *Ssid =  "YOURSSID";   // cannot be longer than 32 characters! there also might be an issue with connection if SSID is less than 8 chars
const char *Pass =  "PASSWORD";    //
char MqttServer[] = "192.168.1.1"; // address of your MQTT Server
unsigned int MqttPort = 1883;      // MQTT port number. default is 1883

char MqttTopic[] = "sensors/" SENSOR_NAME;
//char MqttTopicIn[] = "sensors-control/" SENSOR_NAME; // mqtt callback not implemented 

volatile unsigned int Pulses1 = 0;
volatile unsigned int PulsesLast1 = 0;
volatile unsigned int PulsesKept1 = 0;

volatile unsigned int Pulses2 = 0;
volatile unsigned int PulsesLast2 = 0;
volatile unsigned int PulsesKept2 = 0;

volatile unsigned int PulsesPeriods = 0;

unsigned int MqttFailures = 0;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("mqtt callback invoked. not implemented.");
}

WiFiClient WifiClient;
PubSubClient MqttClient(MqttServer, MqttPort, mqttCallback, WifiClient);

volatile unsigned long LastMicros1;
void pulseHandler1() {
  if((long)(micros() - LastMicros1) >= DEBOUNCE_MS * 1000) {
    Pulses1 = Pulses1 + 1;
    LastMicros1 = micros();
  }
}

volatile unsigned long LastMicros2;
void pulseHandler2() {
  if((long)(micros() - LastMicros2) >= DEBOUNCE_MS * 1000) {
    Pulses2 = Pulses2 + 1;
    LastMicros2 = micros();
  }
}

// TIMER
os_timer_t myTimer;
bool tickOccured;

void timerCallback(void *pArg) {
  PulsesLast1 = Pulses1;
  PulsesKept1 += Pulses1;
  Pulses1 = 0;
  
  PulsesLast2 = Pulses2;
  PulsesKept2 += Pulses2;
  Pulses2 = 0;

  PulsesPeriods++;

  tickOccured = true;
}

void timerInit(void) {
  os_timer_setfn(&myTimer, timerCallback, NULL);
  os_timer_arm(&myTimer, 1000 * 60 * SAMPLE_MINUTES, true);
}

void setup() {
  Serial.begin(74880);
  delay(10);
  Serial.println();
  Serial.print("Simple MQTT Pulse counter (C) Anton Viktorov, latonita@yandex.ru, "); Serial.println(CompileDate);
  Serial.print("Server: ");Serial.print(MqttServer);Serial.print(" Topic: "); Serial.println(MqttTopic);

  WiFi.mode(WIFI_STA);

//  pinMode(4, INPUT_PULLUP);
  pinMode(2, INPUT_PULLUP);
  pinMode(15, INPUT_PULLUP);
//  pinMode(12, INPUT_PULLUP);
//  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
  pinMode(16, INPUT_PULLDOWN_16);

  pinMode(WIFI_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, LOW);

  pinMode(PULSE_PIN1, INPUT);
  attachInterrupt(PULSE_PIN1, pulseHandler1, RISING);

  pinMode(PULSE_PIN2, INPUT);
  attachInterrupt(PULSE_PIN2, pulseHandler2, RISING);

  tickOccured = false;
  timerInit();
}
//
//void delay500() {
//  for(int i = 0; i < 20; i++) {
//    delay(10);
//    yield();
//  }
//}

void loop() {
  if (tickOccured == true) {
    Serial.print("Tick occured. Pulses kept so far: ");

    String payload = "{\"d\":{\"N\":\"" SENSOR_NAME "-" SENSOR_ID "\"";
    payload += ",\"P1\":";
    payload += PulsesKept1;
    payload += ",\"P2\":";
    payload += PulsesKept2;
    payload += ",\"Sec\":";
    payload += PulsesPeriods * 60 * SAMPLE_MINUTES;
    payload += ",\"Per\":";
    payload += PulsesPeriods;
    payload += ",\"Up\":";
    payload += ((unsigned long)millis()/1000);
    payload += "}}";

    Serial.println(payload);
    
    if (MqttClient.publish(MqttTopic, (char*) payload.c_str())) {
      Serial.println("mqtt publish ok");
      PulsesPeriods = 0;
      PulsesKept1 = 0;
      PulsesKept2 = 0;
    } else {
      Serial.println("mqtt publish fail");

//      MqttFailures++;                                             // not sure why I wanted to do this :) so I commented this out later
//      if (MqttFailures >= MAX_MQTT_FAILURES_BEFORE_REBOOT) {
//        Serial.println("too many mqtt failures, rebooting");
//        ESP.reset();
//      }
      if (MqttClient.connected()) {
        Serial.println("mqtt connection still up");
      } else {
        Serial.println("mqtt connection down");
        if (MqttClient.connect("ESP8266-" SENSOR_NAME "-" SENSOR_ID)) {
          yield();
        }
      }
    }

    tickOccured = false;
  }

  digitalWrite(WIFI_LED_PIN, 0);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to ");
    Serial.print(Ssid);
    Serial.println("...");
//    digitalWrite(WIFI_LED_PIN, 1);
//    delay500();
//    digitalWrite(WIFI_LED_PIN, 0);
//    delay500();
    WiFi.begin(Ssid, Pass);

    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      WiFi.printDiag(Serial);
      return;
    }
    Serial.println("WiFi connected");
  } else {
    digitalWrite(WIFI_LED_PIN, 1);
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!MqttClient.connected()) {
      yield();
      if (MqttClient.connect(SENSOR_FAMILY "-" SENSOR_NAME "-" SENSOR_ID)) {
        yield();
      	MqttClient.publish("welcome",SENSOR_FAMILY "-" SENSOR_NAME "-" SENSOR_ID);
        Serial.println("MQTT connected");
//	      MqttClient.subscribe(MqttTopicIn);
      }
    }

    if (MqttClient.connected())
      MqttClient.loop();
  }

  yield();
}
