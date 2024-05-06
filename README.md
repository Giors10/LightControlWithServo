# Lights_ON_OFF
This is a project where you can control your lights with a servo that is controlled by your phone and multiple sensors on the switch.
You will need the following items:
Servo
PIR sensor
2 Buttons
LDR and a 10k resistor
Servo
D1 mini esp8266 (A esp8266 will also work)
Wires

Connect all the items as shown in the LightsOn_OFF_Project_Circuit_Diagram.png.

Then make an account with Blynk, create a new template and select esp8266 for hardware
Then go to web dashbord and add a slider and press the setting button on it.
click on create datastream and select virtual pin, Data type Interger and units none then for the values those will be the values that says where the servo goes i set it to 30 - 139 and default value 45 or you can just set it to 0 - 200 as you will anyway control this in the arduino code.

Then for the Cad i have attached a step file which you can edit to fit your light you can also create your own.

For the code i have included the code in the attachments as a .ino file coppy that code and change the #define BLYNK_TEMPLATE_ID "TMPL5ym-mRDx2" #define BLYNK_TEMPLATE_NAME "LightControl" to the one you have on blynk which is shown on the home tab on the right and then coppy the auth tocken by cliking on it and add it on the auth line in the code also change ssid to your internet name and put wifi password on the pass.

if there are any questions please feel free to email me or comment on the github. 
My email is: Suramgiorgi10@gmail.com
