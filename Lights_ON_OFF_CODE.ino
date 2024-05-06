#define BLYNK_TEMPLATE_ID "TMPL5ym-mRDx2" //change to your own 
#define BLYNK_TEMPLATE_NAME "LightControl"
#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Servo.h>
#include "RTClib.h"

const int BUTTON_PIN = D7;
const int BUTTON_PINs = D6;
const int MOTION_PIN = D5;
const int LDR_PIN = A0;

unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long currentMillis;
const unsigned long period = 1000;

unsigned long startMillis1;  //some global variables available anywhere in the program
unsigned long currentMillis1;
const unsigned long period1 = 1200000;

unsigned long startMillis2;  //some global variables available anywhere in the program
unsigned long currentMillis2;
const unsigned long period2 = 1200000;



Servo servo1;
RTC_DS1307 rtc;



int lastState = LOW;
int lastStates = LOW;
int currentState; 
int currentStates;
int motionStatus = 0; 


char auth[] = "authcode on blynk"; // change it to your auth code 
char ssid[] = "WifiName";                    // change it 
char pass[] = "Wifi password";                 // change it 

void setup() {
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);
  servo1.attach(15); // NodeMCU D8 pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PINs, INPUT_PULLUP);
  pinMode(MOTION_PIN, INPUT);
  
  Serial.println("Looking for Real Time Clock...");
  
  if (!rtc.begin()) {
    Serial.println("Could NOT find RTC");
    Serial.flush();
    //abort();
  } else {
    Serial.println("Found RTC");
  }

  if (!rtc.isrunning()) {
    Serial.println("\nReal Time Clock is NOT running, Setting the time now...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("Time has been set");
  } else {
    Serial.println("\nReal Time Clock is already running - NOT setting the time now.");
  }
}

void loop() {
  Blynk.run(); // Call Blynk.run() in the loop to maintain communication with the Blynk server


  // Read LDR sensor value
  int ldrValue = analogRead(LDR_PIN);
  DateTime now = rtc.now();
  currentState = digitalRead(BUTTON_PIN);
  currentStates = digitalRead(BUTTON_PINs);
  motionStatus = digitalRead(MOTION_PIN);
  currentMillis = millis();
  int hour = now.hour();
  Serial.println(hour);

  if (hour <= 20 || hour >= 6) {
    if (ldrValue < 200 && motionStatus == HIGH) {
      if (currentMillis - startMillis >= period){
        if (ldrValue < 200 && motionStatus == HIGH) {
          servo1.write(115);
          Serial.println(" - ON");
        }
      }
    }
  }  
  if (lastState == HIGH && currentState == LOW) {
    servo1.write(115);
    delay(600);
  }


  if (lastStates == HIGH && currentStates == LOW) {
    servo1.write(61);
    delay(600);
  }
  // Save the last state
  lastState = currentState;
  lastStates = currentStates;
 

  // Your remaining code logic goes here...
}




BLYNK_WRITE(V1){
  servo1.write(param.asInt());
  delay(500);

}
