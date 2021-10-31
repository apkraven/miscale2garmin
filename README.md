# Mi Body Composition Scale 2 Garmin Connect

## 1. Introduction
This project is based on the following projects:
- https://github.com/rando-calrissian/esp32_xiaomi_mi_2_hass;
- https://github.com/lolouk44/xiaomi_mi_scale;
- https://github.com/davidkroell/bodycomposition.

It allows the Mi Body Composition Scale 2 to be fully automatically synchronized to Garmin Connect, the following parameters:
- Body Fat;
- Body Water;
- Bone Mass;
- Caloric Intake (beta, is not working properly yet);
- Metabolic Age;
- Physique Rating;
- Skeletal Muscle Mass;
- Time;
- Visceral Fat;
- Weight.

## 2. How does this work?
Code to read weight measurements from Mi Body Composition Scale 2:
![alt text](https://github.com/RobertWojtowicz/miscale2garmin/blob/master/app_states.png)
 - after weighing, Mi Body Composition Scale 2 is active for 15 minutes on bluetooth transmission;
 - ESP32 module operates in a deep sleep and wakes up every 7 minutes, queries scale for data, the process can be started immediately via the reset button;
 - ESP32 module sends the acquired data via the MQTT protocol to the MQTT broker installed on the server;
 - body weight and impedance data on the server are appropriately processed by scripts;
 - processed data are sent by the program bodycomposition to Garmin Connect;
 - raw data from the scale is backed up on the server in backup.csv file;
 - backup.csv file can be imported e.g. for analysis into Excel. 
 
## 3. Bluetooth gateway to WiFi (via MQTT) on ESP32
Use Arduino environment to compile and upload software to ESP32, following libraries required:
- PubSubClient: https://github.com/knolleary/pubsubclient;
- ESP32 Arduino (warning, use version 1.4, newer is unstable): https://github.com/espressif/arduino-esp32;
- Timestamps: https://github.com/alve89/Timestamps;
- Battery 18650 Stats: https://github.com/danilopinotti/Battery18650Stats.

In Arduino, select the WEMOS LOLIN32 board and set parameters:
- CPU Frequency: "80MHz (WiFi / BT)" for better energy saving;
- Partition Scheme: "No OTA (Large APP)";
- Port: "COM" on which ESP32 board is detected.

The following information must be entered before compiling code (esp32.ino):
- scale's mac address, it can be read from the Mi Fit application ("scale_mac_addr");
- parameters of your WiFi network ("ssid", "password");
- other settings ("led_pin", "Timestamps", "Battery18650Stats");
- connection parameters MQTT ("mqtt_server", "mqtt_port", "mqtt_userName", "mqtt_userPass").

Debug and other comments:
- project is prepared to work with the ESP32 board with the charging module (red LED indicates charging). I based my version on the Li-ion 18650 battery;
- program for ESP32 has implemented UART debug mode, you can verify if everything is working properly;
- after switching the device on, blue LED will light up for a moment to indicate that the module has started successfully;
- if the data are acquired correctly in the next step, blue LED will flash for a moment 2 times;
- if there is an error, e.g. the data is incomplete, no connection to the WiFi network or the MQTT broker, blue LED will light up for 5 seconds;
- program implements voltage measurement and battery level, which are sent together with the scale data in topic MQTT;
- device has 2 buttons, the first green is the reset button (monostable), the red one is the battery power switch (bistable).

Sample photo of the finished module with ESP32 (Wemos LOLIN D32 Pro) and Li-ion 18650 battery (LG 3600mAh, LGDBM361865):
![alt text](https://github.com/RobertWojtowicz/miscale2garmin/blob/master/esp32.jpg)

## 4. Preparing Linux system
- I based on a virtual machine with Debian Buster. I prefer the minimal version with an ssh server (Net Install);
- minimum hardware requirements are: 1 CPU, 512 MB RAM, 2 GB HDD, network connection (e.g. Raspberry Pi Zero W with Pi OS Lite);
- the following modules need to be installed: mosquitto, mosquitto-clients;
- you need to set up a password for MQTT (password must be the same as in ESP32): sudo mosquitto_passwd -c /etc/mosquitto/passwd admin;
- open a configuration file for Mosquitto (/etc/mosquitto/mosquitto.conf) and tell it to use this password file to require logins for all connections: allow_anonymous false, password_file /etc/mosquitto/passwd;
- copy the contents of this repository (miscale2garmin) to a directory e.g. /home/robert/;
- make a file executable with the command: chmod +x /home/robert/bodycomposition;
- create a "data" directory in /home/robert/.

## 5. Configuring scripts
- first script is "import_mqtt.sh", you need to complete data: "user", "password", "host", which are related to the MQTT broker;
- add in import_mqtt.sh" path to the folder where the copied files are, e.g. "/home/robert";
- add script import_mqtt.sh to CRON to run it every 1 minute: */1 * * * * /home/robert/import_mqtt.sh;
- second script is "export_garmin.py", you must complete data in the "user" section: sex, height in cm, birthdate in dd-mm-yyyy, email and password to Garmin Connect, max_weight in kg, min_weight in kg;
- add in "export_garmin.py", path to the folder where the copied files are, e.g. "/home/robert";
- script "export_garmin.py" supports multiple users with individual weights ranges, we can link multiple accounts with Garmin Connect;
- after weighing, your data will be automatically upload to Garmin Connect;
- if there is an error upload to Garmin Connect, data will be sent again in a minute, upload errors are saved in a temporary file, e.g. /home/robert/data/temp.log;
- script import_mqtt.sh has implemented debug mode, you can verify if everything is working properly, just execute it from console.

<a href="https://www.buymeacoffee.com/RobertWojtowicz" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-orange.png" alt="Buy Me A Coffee" height="41" width="174"></a>
