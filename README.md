# ESP32 Pomodoro Timer
 
A cloud-connected Pomodoro timer built on the ESP32 with an OLED display. In addition to working as a standalone timer, it polls an AWS API endpoint to receive remote control commands and custom messages, and pushes its state back up so the timer can be monitored or controlled from anywhere.
 
## Features
 
- **Classic Pomodoro cycle** — 25-minute work sessions, 5-minute short breaks, and a 15-minute long break after every 4 cycles
- **OLED display** showing the current state, countdown timer, cycle count, and WiFi status
- **Physical button** for local start/pause control
- **Remote control via REST API** — start, pause, and reset the timer from any client that can hit the endpoint
- **Custom messages** — push text from the API to display on the device (with automatic word wrap)
- **Live status updates** — the device POSTs its state back to the API every 10 seconds and on every state change
- **Offline mode** — if WiFi fails to connect, the timer still works as a standalone device

## Hardware
 
- ESP32 development board
- SSD1306 OLED display (128×64, I²C)
- Push button
- Jumper wires / breadboard or soldered protoboard

### Wiring
 
| Component       | ESP32 Pin |
|-----------------|-----------|
| OLED SDA        | GPIO 21   |
| OLED SCL        | GPIO 22   |
| OLED VCC / GND  | 3.3V / GND|
| Button          | GPIO 23   |
 
The OLED uses I²C address `0x3C`. The button is wired with a pull-down configuration (active HIGH).
 
## Software Dependencies
 
Install the following libraries through the Arduino Library Manager:
 
- `WiFi` (built-in for ESP32)
- `HTTPClient` (built-in for ESP32)
- `ArduinoJson`
- `SSD1306Wire` (ThingPulse OLED library)
## Setup
 
1. Clone this repository and open `ESP32_Pomodoro.ino` in the Arduino IDE.
2. Make sure the ESP32 board package is installed and the correct board / port are selected.
3. Fill in your credentials and API endpoint near the top of the sketch:
   ```cpp
   const char* ssid = "WiFi_NAME";
   const char* password = "WiFi_PASSWORD";
   const char* apiEndpoint = "https://your-api-gateway-url.amazonaws.com/stage";
   ```
4. Upload the sketch to your ESP32.
## API
 
The device communicates with two endpoints under the configured base URL.
 
### `GET /command`
 
Polled every 5 seconds. Expected to return a JSON payload of pending commands:
 
```json
{
  "commands": [
    { "type": "control", "action": "start" },
    { "type": "message", "text": "Time to focus!" }
  ]
}
```
 
Supported `control` actions: `start`, `pause`, `reset`.
`message` commands display the given text on the OLED
 
### `POST /status`
 
The device pushes its current state to this endpoint:
 
```json
{
  "id": "device_status",
  "isRunning": true,
  "timeRemaining": 1480,
  "currentState": "work",
  "cyclesCompleted": 2,
  "timestamp": 123456
}
```
 
This is intended to be paired with an AWS API Gateway + Lambda + DynamoDB backend, but any server that implements the two endpoints will work.
 
## How It Works
 
The main loop handles three things on every iteration: polling the button, polling the API for commands, and (if the timer is running) decrementing the countdown once per second. When the timer hits zero, the device automatically transitions to the next state — work to break, break back to work — and tracks completed cycles to know when to insert a long break.
 
State updates are sent both on user-driven changes (button presses, remote commands) and periodically while running, so any connected dashboard stays in sync.
 
