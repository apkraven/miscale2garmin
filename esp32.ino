#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Timestamps.h>

// Scale Mac Address, please use lowercase letters
#define scale_mac_addr "c8:b2:1e:c9:66:83"

// Network details
const char* ssid = "ssid_name";
const char* password = "password";

// Static IP addressing for ESP32
IPAddress ip(192,168,0,2);
IPAddress gateway(192,168,0,1);
IPAddress subnet(255,255,255,0);

// Other ESP32 settings
const int led_pin = 5;
const int adc_pin = 35;
Timestamps ts(-3600);

// MQTT details
const char* mqtt_server = "ip_address";
const int mqtt_port = 1883;
const char* mqtt_userName = "admin";
const char* mqtt_userPass = "user_password";
const char* clientId = "esp32_scale";
const char* mqtt_attributes = "data"; 

String mqtt_clientId = String( clientId );
String mqtt_topic_attributes = String( mqtt_attributes );
String publish_data;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

uint8_t unNoImpedanceCount = 0;
int16_t stoi( String input, uint16_t index1 ) {
    return (int16_t)( strtol( input.substring( index1, index1+2 ).c_str(), NULL, 16) );
}
int16_t stoi2( String input, uint16_t index1 ) {
    return (int16_t)( strtol( (input.substring( index1+2, index1+4 ) + input.substring( index1, index1+2 )).c_str(), NULL, 16) );
}

// Deep sleep for 7 minutes
void goToDeepSleep() {
  Serial.println( "* Waiting for next scan, going to sleep" );
  esp_sleep_enable_timer_wakeup( 7 * 60 * 1000000 );
  esp_deep_sleep_start();
}

// LED indicate error, is on for 5 seconds, for LOLIN32 LITE is pin 22, for LOLIN32 D32 PRO is pin 5, define in Other ESP32 settings section
void errorLED() {
  pinMode(led_pin, OUTPUT); 
  digitalWrite(led_pin, LOW);
  delay(5000);
  goToDeepSleep();
}

// WiFi/MQTT needs to connect each time we get new BLE scan data as wifi and BLE appear to work best when mutually exclusive
void reconnect() {
  int nFailCount = 0;
  if ( WiFi.status() != WL_CONNECTED ) {
    Serial.println();
    Serial.print("* Connecting to WiFi: ");
    Serial.println(ssid);
    WiFi.config(ip, gateway, subnet);
    WiFi.begin(ssid, password);
    WiFi.config(ip, gateway, subnet);
    while (WiFi.status() != WL_CONNECTED) {
      delay(10);
      Serial.print(".");
      nFailCount++;
      if ( nFailCount > 1500 )
         // Why can't we connect? Just try it after waking up
         errorLED();
    }
    Serial.println("");
    Serial.println("* WiFi connected!");    
  }
  
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("* Connecting to MQTT: ");
    mqtt_client.setServer(mqtt_server, mqtt_port);

    // Attempt to connect
    if (mqtt_client.connect(mqtt_clientId.c_str(),mqtt_userName,mqtt_userPass)) {
      Serial.println("MQTT connected!");
    }
    else {
      Serial.print("* MQTT failed!, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(", try again in 200 milliseconds");
      delay(200);
      nFailCount++;
      if ( nFailCount > 500 )
        errorLED();
    }  
  }
}

void publish() {
  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.publish(mqtt_topic_attributes.c_str(), publish_data.c_str(), true );
  Serial.print( "* Publishing MQTT data: " );
  Serial.println( publish_data.c_str() );
  delay( 2000 );
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.print("* BLE device found with address: ");
      Serial.print(advertisedDevice.getAddress().toString().c_str());
      if ( advertisedDevice.getAddress().toString() == scale_mac_addr ) {
        Serial.println(" <= target device!");
        BLEScan *pBLEScan = BLEDevice::getScan(); // found what we want, stop now
        pBLEScan->stop();
      }
      else {
        Serial.println(", non-target device");
      }      
    }
};

void StartESP32() {
  // LED indicate start ESP32, is on for 0.25 second
  pinMode(led_pin, OUTPUT); 
  digitalWrite(led_pin, LOW);
  delay(250);
  digitalWrite(led_pin, HIGH);

  // Initializing serial port for debugging purposes, version info
  Serial.begin(115200);
  Serial.println( "" );
  Serial.println( "Mi Body Composition Scale 2 Garmin Connect v2.1" );
  Serial.println( "" );
}

void ScanBLE() {
  Serial.println( "* Starting BLE scan:" );
  if ( WiFi.status() == WL_CONNECTED ) {
    mqtt_client.disconnect();
    WiFi.disconnect();
  }
  BLEDevice::init("");
  BLEScan *pBLEScan = BLEDevice::getScan(); //Create new scan.
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false); //Active scan uses more power.
  pBLEScan->setInterval(0x50);
  pBLEScan->setWindow(0x30);
  
  // Scan for 30 seconds
  BLEScanResults foundDevices = pBLEScan->start(30);
  int count = foundDevices.getCount();
  for (int i = 0; i < count; i++) {
    BLEAdvertisedDevice d = foundDevices.getDevice(i);
    if ( d.getAddress().toString() != scale_mac_addr )
      continue;
    String hex;
    if (d.haveServiceData()) {
      std::string md = d.getServiceData();
      uint8_t* mdp = (uint8_t*)d.getServiceData().data();
      char *pHex = BLEUtils::buildHexData(nullptr, mdp, md.length());
      hex = pHex;
      free(pHex);
    }
    float weight = stoi2( hex, 22 ) * 0.005;
    float impedance = stoi2( hex, 18 );
    if ( unNoImpedanceCount < 3 && impedance <= 0 ) {
      Serial.println( "* Reading BLE data incomplete, finished BLE scan" );
      errorLED();
    }
    unNoImpedanceCount = 0;
    int user = stoi( hex, 6 );
    int units = stoi( hex, 0 );

    // Battery voltage measurement, for LOLIN32 D32 PRO is internal pin 35 (no voltage divider need, already fitted to board), define in Other ESP32 settings section
    // NODEMCU ESP32 with 100K+100K voltage divider
    uint8_t percentage = 100;
    float voltage = analogRead(adc_pin) / 4096.0 * 7.23;
    percentage = 2808.3808 * pow(voltage, 4) - 43560.9157 * pow(voltage, 3) + 252848.5888 * pow(voltage, 2) - 650767.4615 * voltage + 626532.5703;
    if ( voltage > 4.19 ) 
      percentage = 100;
    else if ( voltage <= 3.50 )
      percentage = 0;

    String strUnits;
    if ( units == 1 )
      strUnits = "jin";
    else if ( units == 2 )
      strUnits = "kg";
    else if ( units == 3 )
      strUnits = "lbs";

    // Instantiating object of class Timestamp with an time offset of -3600 seconds for UTC+01:00, define in Other ESP32 settings section
    int time_unix = ts.getTimestampUNIX( stoi2( hex, 4 ), stoi( hex, 8 ), stoi( hex, 10 ), stoi( hex, 12 ), stoi( hex, 14 ), stoi( hex, 16) );  
    String time = String( String( stoi2( hex, 4 ) ) + "-" + String( stoi( hex, 8 ) ) + "-" + String( stoi( hex, 10 ) ) + " " + String( stoi( hex, 12 ) ) + ":" + String( stoi( hex, 14 ) ) + ":" + String( stoi( hex, 16 ) ) );
    
    if ( weight > 0 ) {
      // LED blinking for 0.75 second, indicate finish reading BLE data
      Serial.println( "* Reading BLE data complete, finished BLE scan" );
      digitalWrite(led_pin, LOW);
      delay(250);
      digitalWrite(led_pin, HIGH);
      delay(250);
      digitalWrite(led_pin, LOW);
      delay(250);
      digitalWrite(led_pin, HIGH);

      // Currently we just send the raw values over and let app figure out the rest
      publish_data += String( weight );
      publish_data += String(";");
      publish_data += String( impedance, 0 );
      publish_data += String(";");
      publish_data += String( strUnits );
      publish_data += String(";");
      publish_data += String( user );
      publish_data += String(";");
      publish_data += String( time_unix );
      publish_data += String(";");
      publish_data += time;
      publish_data += String(";");
      publish_data += String( voltage );
      publish_data += String(";");
      publish_data += String( percentage );
      publish();
    }  
  }
}

void setup() {
  StartESP32();
  ScanBLE();
  goToDeepSleep();
}

void loop() {
}