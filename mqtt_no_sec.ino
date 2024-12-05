#include <SPIFFS.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <string.h>
#include <Arduino.h>
#include "cert.h"



// MQTT and WiFi configuration
const char* mqtt_server = "test.mosquitto.org"; // MQTT server
const char* ssid = "Humberto_ETECH_ULTRA";//"Humberto's S23+";
const char* password = "astronauta3005";//"ft3tvrax5retv5f";
String updateUrl;
String updateVersionCheck;

WiFiClientSecure espClient;
PubSubClient client(espClient); // MQTT client

String encrypted_url_hex = "";
String downloadUrl = "";

File file;


void callback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

     if (String(topic) == "inTopic") {
        encrypted_url_hex = message;
        //Serial.println(encrypted_url_hex.c_str());
        has_encrypted_url = true;
    } 

    // Proceed if all data is received
    if (has_encrypted_url) {
        
        
            Serial.println("HMAC verified successfully!");
            if (decryptAndDownloadFirmware()) {
                flashESP32();
            } else {
                Serial.println("Decryption failed. Check key, IV, and encrypted data.");
            }
        

        resetFlags();
    }
}

void reconnect() {
    while (!client.connected()) {
        Serial.println("Attempting MQTT connection...");
        if (client.connect("ESP32_clientID")) {
            Serial.println("connected");
            client.subscribe("inTopic");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
     if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    WiFi.begin(ssid, password);
    espClient.setCACert(mosquittoCertificate);
    client.setServer(mqtt_server, 8883); // Secure MQTT port (8883)
    client.setCallback(callback);
    connectmqtt();
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
}

void connectmqtt() {
    if (client.connect("ESP32_clientID")) {
        Serial.println("connected to MQTT");
        client.subscribe("inTopic");
    } else {
        reconnect();
    }
}

bool downloadFirmware(const char* url) {
    Serial.println(url);
    WiFiClientSecure * client = new WiFiClientSecure;

    if (client) {
        client->setCACert(rootCACertificate);  // Set root CA certificate for HTTPS

        HTTPClient https;
        if (https.begin( * client, url)) {
            Serial.print("[HTTPS] GET...\n");
            delay(100);
            int httpCode = https.GET();
            if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND || httpCode == HTTP_CODE_SEE_OTHER) {
              String redirectURL = https.getLocation();  // Get the new URL from the Location header
              Serial.print("Redirecting to: ");
              Serial.println(redirectURL);
              https.end();  // Close the initial connection
          
              // Begin a new connection with the redirect URL
              https.begin(* client, redirectURL);
              httpCode = https.GET();  // Make the new request
            }
            delay(100);
            if (httpCode == HTTP_CODE_OK) {
              
                Serial.println("Downloading file...");

                file = SPIFFS.open("/update.bin", FILE_WRITE);

                WiFiClient* stream = https.getStreamPtr();
                
                int totalDownloaded = 0;
                unsigned long startTime = millis();
                while (https.connected() && (https.getSize() > 0 || https.getSize() == -1)) {
                  static uint8_t buffer[16384];
                    size_t size = stream->available();
                    if (size) {
                        int bytesRead = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
                        file.write(buffer, bytesRead);
                        totalDownloaded += bytesRead;
                        Serial.print("Downloaded: ");
                        Serial.print(totalDownloaded);
                        Serial.println(" bytes");
                    }
                     // Break the loop when all data has been downloaded
                    if (https.getSize() != -1 && totalDownloaded >= https.getSize()) {
                        break;
                    }
                    
                }

                file.close();
                Serial.print("Download completed in ");
                Serial.print(millis() - startTime);
                Serial.println(" ms");
                //writtenToSD = true;
                //Serial.println("Start to transfer firmware via UDS...");
                //sendUDSExtendedDiagnosticSessionRequest();
                //delay(1000);
            } else {
                Serial.print("Error Occurred During Download: ");
                Serial.println(httpCode);
                
            }
            https.end();
        }
        delete client;
        return true;
    }
    return false;
}

void flashESP32() {
    File updateBin = SPIFFS.open("/update.bin", FILE_READ);
    if (!updateBin) return;
    if (!Update.begin(updateBin.size())) {
        updateBin.close();
        return;
    }
    size_t written = Update.writeStream(updateBin);
    if (Update.end() && Update.isFinished()) {
        Serial.println("Update successfully completed. Rebooting.");
        ESP.restart();
    }
    updateBin.close();
}

void resetFlags() {
   
    has_encrypted_url = false;
     encrypted_url_hex = "";
}
