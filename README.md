Lights_ON_OFF
This project allows you to control your lights with a servo managed by your phone and multiple sensors on the switch.

Required Components:
Servo
PIR sensor
2 Buttons
LDR and a 10k resistor
D1 mini ESP8266 (an ESP8266 will also work)
Wires
Setup Instructions:
Hardware Connections: Connect all the items as shown in the LightsOn_OFF_Project_Circuit_Diagram.png.
Blynk Configuration:
Sign up for an account with Blynk.
Create a new template and select ESP8266 for hardware.
Navigate to the web dashboard, add a slider, and press the settings button.
Click on "create datastream" and select virtual pin V1. Set the Data type to Integer with no units. The values represent the servo position; typically set between 30 and 139, with a default value of 45. Alternatively, you can set it to 0 - 200, which you'll control in the Arduino code.
CAD Model: A STEP file is provided, which you can edit to fit your light. You can also create your own model.
Arduino Code:
The code is included in the attachments as a .ino file.
Replace the following lines:
cpp
Copy code
#define BLYNK_TEMPLATE_ID "TMPL5ym-mRDx2"
#define BLYNK_TEMPLATE_NAME "LightControl"
with the values you have on Blynk, which are displayed on the home tab.
Copy the auth token by clicking on it in Blynk, and add it to the auth line in the code.
Change SSID to match your internet name and put your WiFi password in the pass field.
For any questions, please feel free to email me or comment on GitHub.

Contact Email: Suramgiorgi10@gmail.com

