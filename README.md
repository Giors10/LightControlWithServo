# Hyper-Smart Light Controller
![Version](https://img.shields.io/badge/version-2.0-blue) ![License](https://img.shields.io/badge/license-MIT-green)

_A complete hardware and software ecosystem to automate any physical light switch._



---

## The Project

The Hyper-Smart Light Controller is a DIY project that transforms a standard bedroom light switch into a fully-featured smart device. It uses an ESP8266 microcontroller and a servo motor housed in a custom 3D-printed enclosure to physically flip the switch. The powerful firmware provides intelligent automation, while the companion mobile app gives you total control from anywhere.

**➡️ [Download the Companion App (APK)](https://median.co/share/abqqkwr#apk)**

## Core Features

### 🧠 Intelligent Automation
- **Motion Activation:** Light turns on automatically when you enter the room.
- **Sun Awareness:** Uses local sunrise/sunset times to enable/disable automation.
- **Quiet Hours:** Prevents motion activation during your configured sleeping hours.
- **Wake-Up Alarm:** Turns the light on at a set time to help you wake up naturally.

### 📱 Total Control via Companion App
- **Multi-Device Management:** Control all your smart switches from one place.
- **Live Status & Control:** See real-time status and instantly toggle the light or change modes.
- **Full Configuration:** Adjust all settings, including motion timeouts, schedules, and location.
- **Live Servo Calibration:** Fine-tune the servo's ON/OFF angles in real-time from the app.

### 🛠️ Robust Hardware & On-Device UI
- **3D-Printed Enclosure:** A custom CAD file ensures a perfect, professional-looking fit on your light switch.
- **OLED Display:** On-device screen shows status and provides a full configuration menu.
- **Physical Buttons:** Instantly toggle the light or navigate menus without your phone.
- **Built for Stability:** Firmware is hardened against crashes, memory leaks, and hardware burnout.

## Getting Started: A 4-Step Guide

### Step 1: Gather The Hardware

| Component              | Description                                      |
| ---------------------- | ------------------------------------------------ |
| **ESP8266**            | A NodeMCU or D1 Mini is recommended.             |
| **SG90 Servo Motor**   | The muscle that flips the switch.                |
| **PIR Motion Sensor**  | HC-SR501 or similar for motion detection.        |
| **OLED Display**       | 0.96" I2C SSD1306 (128x64).                       |
| **Push Buttons**       | Two tactile push buttons for the UI.             |
| **Power Supply**       | A reliable 5V USB power supply (1A+).            |

### Step 2: Assemble The Device

This project includes a custom-designed CAD file for a 3D-printable enclosure. This ensures all components fit together perfectly and mount securely over your existing light switch.

1.  **Download the CAD file** from this repository (e.g., `light_switch_enclosure.stl`).
2.  **3D print the parts.** PLA or PETG are excellent material choices.
3.  **Assemble the electronics** inside the enclosure as designed.
4.  **Mount the final assembly** over your wall light switch.

### Step 3: Set Up The Software

1.  **Prerequisites:**
    - Install the [Arduino IDE](https://www.arduino.cc/en/software).
    - Install the [ESP8266 Board Manager](https://github.com/esp8266/Arduino).
    - Get a free API key from [OpenWeatherMap](https://openweathermap.org/).
    - Set up an [MQTT Broker](https://www.emqx.com/en/cloud) (like EMQX Cloud or Mosquitto).

2.  **Install Arduino Libraries:**
    Open the Arduino Library Manager (`Tools > Manage Libraries...`) and install the following:
    - `Adafruit SSD1306`
    - `Adafruit GFX Library`
    - `PubSubClient`
    - `WiFiManager`
    - `ArduinoJson`

3.  **Configure Your Broker:**
    > **CRITICAL:** Your device will not connect without this. In your MQTT broker, you must create both an **Authentication** user and an **Authorization (ACL)** rule allowing that user to **Publish & Subscribe** to the topic `switchmote/#`.

4.  **Configure & Upload Firmware:**
    - Open the `.ino` sketch.
    - It's highly recommended to set the default values for your MQTT broker and API keys in the `Configuration` struct at the top of the file.
    - Select your ESP8266 board, port, and upload the code.

### Step 4: First Boot & Wi-Fi Setup

On its first boot, the device creates a Wi-Fi network named **"SmartLight_Setup"**. Connect to it with your phone, and a captive portal should open. If not, navigate to `192.168.4.1` in a browser. Follow the on-screen steps to connect the device to your home Wi-Fi and finalize the configuration.

---

## Technical Details

<details>
<summary><strong>Click to expand Wiring Diagram and MQTT API</strong></summary>

### Wiring Diagram

| ESP8266 Pin (NodeMCU) | Connects To                               |
| --------------------- | ----------------------------------------- |
| `D4 (GPIO2)`          | Servo Signal Pin                          |
| `D5 (GPIO14)`         | PIR Sensor Output Pin                     |
| `D6 (GPIO12)`         | ON/OFF Button (to GND)                    |
| `D7 (GPIO13)`         | MENU Button (to GND)                      |
| `D1 (GPIO5)`          | OLED SCL (Clock) Pin                      |
| `D2 (GPIO4)`          | OLED SDA (Data) Pin                       |
| `GND`                 | Common Ground for all components          |
| `Vin (5V)`            | 5V Power for Servo, PIR, and OLED         |

### MQTT API

The device ID is unique to your ESP8266 chip and is printed to the Serial Monitor on boot.

-   **Command Topic:** `switchmote/<device-id>/command`
-   **Status Topic:** `switchmote/<device-id>/status`


```json
{
  "action": "setLight",
  "value": "on"
}
```

```json
{
  "action": "setMode",
  "value": "auto"
}
```

```json
{
  "action": "calibrate",
  "angle": 90
}
```

```json
{
  "action": "getStatus"
}
```

```json
{
  "action": "reboot"
}
```


## Troubleshooting

-   **Device is stuck or won't connect:** Perform a **Factory Reset**. Unplug the device, press and hold the ON/OFF button (D6), and plug it back in while holding for 5 seconds.
-   **MQTT Fails with `rc=5` (Unauthorized):** This is a server-side error. Double-check your MQTT broker's **Authentication** and **Authorization (ACL)** rules.
-   **MQTT Fails with `rc=-2` (Connection Refused):** Check your MQTT server address and port. This can also be caused by a failed NTP time sync on boot.

