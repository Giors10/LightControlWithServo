# Lights_ON_OFF

This project allows you to control your lights with a servo that is managed by your phone and multiple sensors on the switch.

## Required Components:

- Servo
- PIR sensor
- 2 Buttons
- LDR and a 10k resistor
- D1 mini ESP8266 (an ESP8266 will also work)
- Wires

## Setup Instructions:

1. **Hardware Connections**: Connect all the items as shown in the LightsOn_OFF_Project_Circuit_Diagram.png.

2. **Blynk Configuration**: 
   - Sign up for an account with Blynk.
   - Create a new template and select ESP8266 for hardware.
   - Navigate to the web dashboard, add a slider, and press the setting button.
   - Click on "create datastream" and select virtual pin V1. Set the Data type to Integer with no units. The values represent the servo position; typically set between 30 and 139, with a default value of 45. Alternatively, you can set it to 0 - 200, which you'll control in the Arduino code.

3. **CAD Model**: A STEP file is provided, which you can edit to fit your light. You can also create your own model.

4. **Arduino Code**:
   - The code is included in the attachments as a .ino file.
   - Replace the following lines:
     #define BLYNK_TEMPLATE_ID "TMPL5ym-mRDx2"
     #define BLYNK_TEMPLATE_NAME "LightControl"
     with the values you have on Blynk, which are displayed on the home tab.
   - Copy the auth token by clicking on it in Blynk, and add it to the auth line in the code.
   - Change `ssid` to match your internet name and put your WiFi password in the `pass` field.

For any questions, please feel free to email me or comment on GitHub.

Contact Email: Suramgiorgi10@gmail.com
