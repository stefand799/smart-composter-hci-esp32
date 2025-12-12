# ESP32 Code
Steps
1. Open `smart-composter-esp32/smart-composter-esp32.ino` with ArduinoIDE
2. Inside "env_var.h" add <br>
    API_KEY = secret key used on the server side the same inside .env <br>
    SSID = network name (or hotspot) <br>
    SSID_PASSWORD = network password (or hotspot) [leave blank if there is none] <br>
    SERVER_HOST = public IPv4 of the device server is running <br>
    SERVER_PORT = port of the server (default is 8000) <br>

2. Deploy the code on ESP32 board

3. Check serial monitor for connection to network and request loops
