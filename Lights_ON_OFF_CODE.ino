#define BLYNK_TEMPLATE_ID "TMPL5ym-mRDx2"
#define BLYNK_TEMPLATE_NAME "LightControl"
#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Servo.h>
#include "RTClib.h"
#define BUTTON_PIN D7
#define BUTTON_PINs D6
#define MOTION_PIN D5

unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long currentMillis;
const unsigned long period = 1000;
Servo servo1;
RTC_DS1307 rtc;



int lastState = LOW;
int lastStates = LOW;
int currentState; 
int currentStates;
int motionStatus = 0; 


char auth[] = "DsMLuPYNbOQ9IskAqAfxGZMGCZLk6gt3"; // change it to your auth code 
char ssid[] = "VM6179617";                    // change it 
char pass[] = "j6nkFmjJgdfj";                 // change it 

void setup()
{
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);
  servo1.attach(15); // NodeMCU D8 pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PINs, INPUT_PULLUP);
  pinMode(MOTION_PIN, INPUT);
  startMillis = millis();
  Serial.println("Looking for Real Time Clock...");
  
  if (! rtc.begin())
    {
    Serial.println("Could NOT find RTC");
    Serial.flush();
    abort();
    }
  else
    {
    Serial.println("Found RTC");
    }

  if (! rtc.isrunning())
    {
    Serial.println("\nReal Time Clock is NOT running, Setting the time now...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("Time has been set");
    }
  else
    {
    Serial.println("\nReal Time Clock is already running - NOT setting the time now.");
    }
    

}

void loop()
{
  DateTime now = rtc.now();
  currentState = digitalRead(BUTTON_PIN);
  currentStates = digitalRead(BUTTON_PINs);
  motionStatus = digitalRead(MOTION_PIN);
  int analogValue = analogRead(A0);
  currentMillis = millis();
  Serial.println(motionStatus);
  int hour = now.hour();




  if (hour <= 20 || hour >= 6) {
    if (analogValue < 200) {
      if (currentMillis - startMillis >= period){
        if (analogValue < 200) {
          servo1.write(115);
          Serial.println(" - ON");
    if (motionStatus == HIGH) { 
      Serial.println("Motion Detected");
      servo1.write(115);
    }
    else { 
      Serial.println("Motion Ended"); 
      
    }
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
 
  Blynk.run(); // Call Blynk.run() in the loop to maintain communication with the Blynk server
}

BLYNK_WRITE(V1)
{
  servo1.write(param.asInt());
  delay(500);

}
