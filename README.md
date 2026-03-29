# EVERYTHING-OS

# Parts needed :
 esp32-wroom
 breadboard (dosent matter how big/small, but i used the small 400 hole)
 SSD1306 oled screen
 KY-023 joystick (i personally used the Keyes_Sjoys, its the same as the KY-023)

# Wiring :
esp32 3V > breadbaord "+ row"
esp32 GND > breadboard "- row"
esp32 GPIO 25 > joystick SW
esp32 GPIO 35 > joystick VRY
esp32 GPIO 34 > joystick VRX
esp32 GPIO 21 > oled SDA
esp32 GPIO 22 > oled SCL
oled VCC > breadboard "+ row"
oled GND > breadboard "- row"
joystick 5V > breadboard "+ row"
joystick GND > breadbaord "- row"
