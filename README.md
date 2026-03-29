# EVERYTHING-OS
- Firmware for esp32, which features a lot of apps/games

# Applications/Games
- Flappy Bird
- Tetris
- Snake
- Pong
- Platform (its a game about jump on platforms)
- Wifi Scan (Scans wifi networks)
- Bluetooth Scan (Same as wifi scan, but scans bluetooth networks, and added a little tweak to it, it ignores ELK-BLEDOM bluetooth signals, and only shows one ELK-BLEDOM)
- 3D Cube (a spinning cube, demo app)
- Dice
- Stopwatch
- Laptop Monitor (compile the monitor.c file to get the executable, please edit "#define ESP32_IP" at the top of the monitor.c file to match your esp32s ip address, and compile the monitor.c with this command : "gcc -O2 -Wno-format-truncation -o monitor monitor.c" | and execute it with this command : "./monitor")
- IMPORTANT: about the Laptop Monitor app, somethings might not work fully, since this project wasnt designed to be public
- Shutdown (Restart the esp32 by pressing the RESET button)

# Parts needed :
- esp32-wroom (use a "esp32-wroom CH340", if you dont want to solder a battery/usb-c port onto it)
- breadboard (dosent matter how big/small, but i used the small 400 hole)
- SSD1306 oled screen (0,96 inch)
- KY-023 joystick (i personally used the Keyes_Sjoys joystick, its the same as the KY-023)

# Wiring :
- The "oled" means the SSD1306 oled screen
- esp32 3V > breadbaord "+ row"
- esp32 GND > breadboard "- row"
- esp32 GPIO 25 > joystick SW
- esp32 GPIO 35 > joystick VRY
- esp32 GPIO 34 > joystick VRX
- esp32 GPIO 21 > oled SDA
- esp32 GPIO 22 > oled SCL
- oled VCC > breadboard "+ row"
- oled GND > breadboard "- row"
- joystick 5V > breadboard "+ row"
- joystick GND > breadbaord "- row"

# Informations
- Uses LITTLEFS to create a 50kb partition to store game highscores
- Uses the Arduino framework
- Uses Platformio to Download the Dependencies/Librarys (Platformio VScode Version)

# Installation
1. install the Platformio Extension for vscode (and if you dont have vscode, install vscode)
2. Run this command to create the project directorys : mkdir -p ~/Documents && mkdir -p ~/Documents/PlatformIO && mkdir -p ~/Documents/PlatformIO/Projects && mkdir -p ~/Documents/PlatformIO/Projects/everything-os
3. clone the github repository : git clone https://github.com/fenyox-ok/everything-os
4. mv ./everything-os/project/* ~/Documents/PlatformIO/Projects/everything-os/
5. go to the everything-os directory : cd ~/Documents/PlatformIO/Projects/everything-os
6. compile the monitor tool : gcc -O2 -Wno-format-truncation -o monitor monitor.c
7. (optional) remove the monitor.c file : rm monitor.c
8. now upload the code to the esp32 with the "pio" command (make sure the pio command is in path) (make sure its plugged in) : pio run -t upload
9. (optionally) enable the systemd service to start the monitor executable, edit the YOURUSERNAME in the service file to your username : sudo cp everything-os/esp32-monitor.service /etc/systemd/system/ && sudo systemctl daemon-reexec && sudo systemctl daemon-reload && sudo systemctl enable esp32-monitor && sudo systemctl start esp32-monitor

# Pictures
![1](https://github.com/user-attachments/assets/3d21ff19-6f6f-4752-9051-7c1690e79fe6)
![2](https://github.com/user-attachments/assets/9b1bf816-98d4-49bd-b5f0-dcc16133b6b3)

# IMPORTANT
This repository was meant for other people to contribute this project, this will probally be the only release ever made.
