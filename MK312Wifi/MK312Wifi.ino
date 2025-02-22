/*
 *  MK-312 Wifi interface
 *  This project is hosted here: https://github.com/Rangarig/MK312WIFI
 */

#include "ESP8266WiFi.h"
#include <ESP8266WebServer.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>        // https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>
#include <WebSocketsServer.h>   // https://github.com/Links2004/arduinoWebSockets
#include <FS.h>
#include <LittleFS.h>

#define VERSION 1.2.05
#define AP_NAME "MK312CONFIG-AP"

#define UDP_DISCOVERY_PORT 8842 // UDP port to listen to, so devices can find the interface by sending a broadcast packet
#define COMM_PORT          8843 // main communication port in TCP

#define LED_PIN               1 // radio LED of the mk312 (we use TX, since the system will output garbage on start, and we do not want to confuse the mk312)
#define RX_PIN                0 // rx pin to be used by the software implementation
#define RESET_WIFI_PIN        3 // The pin that needs to be pushed to ground to reset the wifi settings
#define TX_PIN                2 // tx pin to be used by the software implementation

#define FAIL_CHECKSUM         1
#define FAIL_HANDSHAKE_A      2
#define FAIL_HANDSHAKE_B      3
#define FAIL_HANDSHAKE_C      4
#define FAIL_REPLY            5
#define FAIL_PEEKREPLY       10
#define FAIL_POKEREPLY       11

char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1]; // buffer to hold incoming packet,

WiFiServer wifiServer(COMM_PORT); // The wifiserver, that sends data and receives controls
WiFiUDP udp; // The UDP Server that is used to tell the client the IP Address
SoftwareSerial mySerial(RX_PIN, TX_PIN, false);

void setStatusLed(bool status) {
  if (status) {
    digitalWrite(LED_PIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  }
  else {
    digitalWrite(LED_PIN, LOW);   // turn the LED off
  }
}

byte mk312key = 0; // The key to be used to talk to the mk312
byte wifikey = 0;  // The key used when we are talked to from wifi

// Writes a byte to the mk312
void mk312write(byte b) {
  mySerial.write(b);
}

// Writes an encrypted byte to the mk312
void mk312write_enc(byte b) {
  mySerial.write(b ^ mk312key);
}

// Waits for a byte from the mk312 and returns it
int mk312read() {
  unsigned long timeout = millis() + 1000; // We wait for one second until we go into errorstate

  while (mySerial.available() == 0) {
    delay(10);
    if (millis() > timeout)
      return -1;
  }
  return mySerial.read();
}

// Flashes an error code until the end of time
void errorstate(byte e) {
  while (true) {
    for (byte i=0;i<e;i++) {
      digitalWrite(LED_PIN, HIGH);   // turn the LED on
      delay(300);
      digitalWrite(LED_PIN, LOW);    // turn the LED off
      delay(150);
    }
    delay(2000);
  }
}

// Internal Poke Command
void poker(int addr, byte b) {
  byte lo = addr >> 8;
  byte hi = addr & 0xff;
  mk312write_enc(0x4d);
  mk312write_enc(lo);
  mk312write_enc(hi);
  mk312write_enc(b);
  mk312write_enc((0x4d + lo + hi + b) % 256);
  if (mk312read()!=0x06) errorstate(FAIL_POKEREPLY);
}

// Peeks an address from the memory of the device
byte peeker(int addr) {
  byte lo = addr >> 8;
  byte hi = addr & 0xff;
  mk312write_enc(0x3c);
  mk312write_enc(lo);
  mk312write_enc(hi);
  mk312write_enc((0x3c + lo + hi) % 256);
  if (mk312read()!=0x22) errorstate(FAIL_PEEKREPLY);
  byte val = mk312read();
  byte chk = mk312read();
  if (((val + 0x22) % 256) != chk) errorstate(FAIL_CHECKSUM);
  return val;
}

// Write to screen
void writeText(const char myMsg[]) {
  int len = strlen(myMsg);

  // display the message text...
  for (int i=0;i<len;i++) {
    poker(0x4180,myMsg[i]);
    poker(0x4181,64+i);
    poker(0x4070,0x13);
    while (peeker(0x4070) != 0xff) delay(1);
  }

  // ... then clear the rest of the screen's line with spaces
  for (int i=len;i<16;i++) {
    poker(0x4180,' ');
    poker(0x4181,64+i);
    poker(0x4070,0x13);
    while (peeker(0x4070) != 0xff) delay(1);
  }
}

// initializes wifi
void wifi_setup() {
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.autoConnect(AP_NAME);

  bool statusled = false;
  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    statusled = !statusled;
    setStatusLed(statusled);
  }

  setStatusLed(false);
  udp.begin(UDP_DISCOVERY_PORT);
  wifiServer.begin();
}

void configModeCallback (WiFiManager *myWiFiManager) {
  (void)myWiFiManager; // -Wunused-parameter
  writeText("WifiAP");
}

// Establishes or reestablishes communication with the mk312 device
void mk312_setup() {
  // Clear potential garbage from buffer
  delay(200);

  byte attempts = 12;
  while (attempts > 0) {
    attempts --;
    mySerial.flush();
    mk312write(0x00); // Initiate handshake
    delay(1); // Give the system time to answer
    if (mk312read() == 0x07) break;
  }

  if (attempts == 0) errorstate(FAIL_HANDSHAKE_A); // Failed, we did not obtained any 0x07 reply

  mk312key = 0x00;

  // Set encryption key
  mk312write(0x2f); // Set key command
  mk312write(0x00); // To keep things simple we will use 00 as a key
  mk312write(0x2f); // Checksum (no key, so really just the commmand)

  byte rep = mk312read();
  byte boxkey = mk312read();
  byte check = mk312read();

  if (rep != 0x21) errorstate(FAIL_HANDSHAKE_B); // handshake fail
  if (check != (rep + boxkey)) errorstate(FAIL_CHECKSUM); // checksum fail

  // Store the encryption key for later use
  mk312key = boxkey ^ 0x55;
}

void setup() {
  pinMode(RESET_WIFI_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  // Setup communications with the MK312.
  mySerial.begin(19200);
  pinMode(TX_PIN, OUTPUT);
  pinMode(RX_PIN, INPUT); // For some reason the ESP insists on a pullup for the RX pin, which will then not be understood, so... we rectify that.
  mk312_setup();

  // Ready for incoming connections
  wifi_setup();
  IPAddress ip = WiFi.localIP();
  char s[17];
  sprintf(s, ">%i.%i.%i.%i", ip[0],ip[1],ip[2],ip[3]);
  writeText(s);
  webservers_setup();
}

// Checks if the AP button is pressed
void checkForAP() {
    bool resetWifi = !digitalRead(RESET_WIFI_PIN);
    if (resetWifi) {
      writeText("WifiAP");
      webservers_stop();
      WiFiManager wifiManager;
      wifiManager.startConfigPortal(AP_NAME);
    }
}

bool wifiEncryption = false; // Do we use encryption on wifi side?

// Waits for a byte from wifi and returns it
byte wifiread(WiFiClient client) {
  unsigned long timeout = millis() + 1000; // We wait for one second until we go into errorstate

  while (client.available() == 0) {
    delay(10);
    if (millis() > timeout)
      return -1;
  }
  if (wifiEncryption)
    return client.read() ^ wifikey; // Decrypt
  else
    return client.read();
}

void handleLedBlinking(int setCount=-1, bool wantedFinalLedState=false) {
  static unsigned long ledBlinkTimeout = 0;
  static int ledCount = 0;
  static bool ledState = false;
  static bool finalLedState = false;

  if (setCount != -1) {
    ledCount = setCount;
    finalLedState = wantedFinalLedState;
  }

  if (!ledCount) {
    setStatusLed(finalLedState);
    ledState = finalLedState;
    return;
  }

  if (millis() > ledBlinkTimeout) {
    ledBlinkTimeout = millis() + 200;
    ledCount--;
    ledState = !ledState;
    setStatusLed(ledState);
  }
}

void loop() {
  handleUDP();
  handleTCPIP();
  handleWebservers();
  checkForAP();
  handleLedBlinking();
}

// Handles the incoming TCPIP requests
void handleTCPIP() {
  byte cmd = 0; // conmmand read
  byte val1 = 0; // Value
  byte hi = 0; // Hi address
  byte lo = 0; // Lo address
  byte chk = 0; // Checksum
  byte rep = 0; // Reply byte
  byte readbuf[16]; // Read buffer for write byte passthrough
  long chksum = 0; // Checksum for readbuffer

  WiFiClient client = wifiServer.accept();

  if (client) {
    client.setNoDelay(true);
    wifiEncryption = true;
    wifikey = 0;

    while (client.connected()) {
        handleLedBlinking();

        WiFiClient new_client = wifiServer.accept();
        if (new_client) {
          client.stop();
          client = new_client;
          wifikey = 0;
          handleLedBlinking(0, true);
        }

        // Check if a control message has been sent
        while (client.available() > 0) {
          cmd = wifiread(client);

          // Ping command is replied to with 07
          if (cmd == 0x00) {
            wifikey = 0;
            client.write(0x07);
            continue;
          }

          // Intercept set key command and change the local key only, not the mk312 one
          if (cmd == 0x2f) { // Set key command
            val1 = wifiread(client);
            chk = wifiread(client);

            // If key and checksum are 0x42 encryption is disabled
            if ((val1 == 0x42) && (chk == 0x42)) {
              wifiEncryption = false;
              client.write(0x69); // Reply code, key accepted
              continue;
            }

            if (chk != ((val1 + cmd) % 256)) {
              client.write(0x07); // Checksum error
              continue;
            }
            wifikey = val1 ^ 0x55;
            uint8_t response[3];
            response[0] = 0x21; // Reply code, key accepted
            response[1] = 0x00; // Our own "box" key, which for simplicity will always be 0
            response[2] = 0x21; // The checksum
            client.write(response, 3);
            continue;
          }

          // As the rest of the code is for communicating with the mk312,
          // we're making sure serial reception buffer is empty
          mySerial.flush();

          // Read byte command
          if (cmd == 0x3c) { // read byte command
            lo = wifiread(client);
            hi = wifiread(client);
            chk = wifiread(client);

            if (((cmd + lo + hi) % 256) != chk) {
              client.write(0x07); // Wrong checksum
              continue;
            }

            // Command checks out, send it to device
            mk312write_enc(cmd);
            mk312write_enc(lo);
            mk312write_enc(hi);
            mk312write_enc(chk);

            // Handle reply
            rep = mk312read();
            val1 = mk312read();
            chk = mk312read();

            // in software version previous or equal than 1.2.02
            // here was a checksum check, replying 0x07 in case of errors.
            // this made code debugging more difficult and is removed for now,
            // the checksum validity is also checked by the client application.

            uint8_t response[3] = {rep, val1, chk};
            client.write(response, 3);
            continue;
          }

          // Write byte command implementation
          if ((cmd & 0x0f) == 0x0d) { // write byte command
            handleLedBlinking(3, true);
            val1 = (cmd & 0xf0) >> 4; // Number of bytes to write

            hi = wifiread(client);
            lo = wifiread(client);

            chksum = cmd + hi + lo;

            // Intercept bytes
            for (int i = 0; i < (val1 - 3); i++) {
              readbuf[i] = wifiread(client);
              chksum += readbuf[i];
            }

            chk = wifiread(client);

            // Make sure checksum is ok
            if ((chksum % 256) != chk) {
              client.write(0x07); // Wrong checksum
              continue;
            }

            // Intercept key change by a poke command
            // TODO: Theoretically someone could write to 4213 by a command writing several bytes
            // But nobody would do that... right? RIGHT?
            if ((val1 == 4) && (hi == 0x42) && (lo == 0x13)) {
              wifikey = readbuf[0];
              client.write(0x06); // OK, we changed the local key
              continue;
            }
            mk312write_enc(cmd);
            mk312write_enc(hi);
            mk312write_enc(lo);
            for (int i = 0; i < (val1 - 3); i++) {
              mk312write_enc(readbuf[i]);
            }
            mk312write_enc(chk);

            rep = mk312read();
            client.write(rep);
            continue;
          }

          // unknown command?
          client.write(0x07);
        }
    }

    client.stop();
    handleLedBlinking(0, false);
  }
}

void handleUDP() {
  // if there's data available, read a packet
  int packetSize = udp.parsePacket();
  if (packetSize) {
    // read the packet into packetBufffer
    int n = udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    packetBuffer[n] = 0;

    if(strcmp(packetBuffer, "ICQ-MK312") == 0) {
      // send a reply, to the IP address and port that sent us the packet we received
      handleLedBlinking(1, false);
      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      IPAddress ip = WiFi.localIP();
      udp.write(ip[0]);
      udp.write(ip[1]);
      udp.write(ip[2]);
      udp.write(ip[3]);
      udp.endPacket(); // "flush" the output as we're sending the packet UDP now
    }
  }
}

/************************************************************************
 * All the following code is for implementing web and websocker servers *
 ************************************************************************/

#define CONFIG_LITTLEFS_SPIFFS_COMPAT 1

ESP8266WebServer webserver(80);
WebSocketsServer websocketserver(81);

void webservers_setup() {
  webserver.on("/EXEC", handleHttpGetEXEC);
  webserver.on("/RAW", handleHttpGetRAW);

  webserver.onNotFound([](){
    if(!handleFileRead(webserver.uri()))
      webserver.send(404, "text/plain", "FileNotFound");
  });

  LittleFS.begin();
  webserver.begin();
  websocketserver.begin();
  websocketserver.onEvent(websocketevent);
}

void webservers_stop() {
  webserver.close();
  webserver.stop();
  websocketserver.close();
}

void handleWebservers() {
  websocketserver.loop();
  webserver.handleClient();
}

bool handleFileRead(String path){
  if (path.endsWith("/")) path += "index.html";
  if (LittleFS.exists(path)){
    File file = LittleFS.open(path, "r");
    webserver.streamFile(file, getContentType(path));
    file.close();
    return true;
  }
  return false;
}

void handleHttpGetRAW() {
  handleHttpGetBase(true);
}

void handleHttpGetEXEC() {
  handleHttpGetBase(false);
}

void handleHttpGetBase(bool raw) {
  String res = "ERR";

  if(webserver.hasArg("cmd")) {
    String cmd = webserver.arg("cmd");

    String val = "";
    if(webserver.hasArg("val")) {
      val = webserver.arg("val");
    }

    if(raw) {
      poker(str2hex(cmd.c_str()), str2hex(val.c_str())); res="OK";
    }
    else {
      res = websocket_parse_cmd(cmd, val) ?"OK":"ERR";
    }
  }

  webserver.send(200, "text/plain", res);
}

String getContentType(String filename){
    if(filename.endsWith(".html")) return "text/html";
    else if(filename.endsWith(".css")) return "text/css";
    else if(filename.endsWith(".js")) return "application/javascript";
    return "text/plain";
}

void websocketevent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if(type == WStype_CONNECTED) {
    handleLedBlinking(0, true);
    IPAddress ip = websocketserver.remoteIP(num);
    String message = ip.toString() + String(" connected.");
    websocketserver.broadcastTXT(message);
  }

  if (type == WStype_TEXT) {
    String key = "";
    String val = "";
    bool bval = false;

    for (size_t x = 0; x < length; x++) {
      if (((char)payload[x]) == '=') {
        bval = true;
        continue;
      }

      if (bval) {
        val+=(char)payload[x];
      }
      else {
        key+=(char)payload[x];
      }
    }

    // send response to all connected clients
    String res = key;
    if (val != "") { res+="="+val; }
    res+= websocket_parse_cmd(key, val) ?" OK":" ERR";
    websocketserver.broadcastTXT(res);
  }
}

bool websocket_parse_cmd(String cmd, String val) {
  // todo: validate input vals
  // todo: possibility to read the current values
  // todo: websockets client should handle broadcasted messages to them
  if (cmd == "startRamp")        poker(0x4070, 0x21);
  else if(cmd == "CutLevels")    cutLevels(val.toInt());
  else if(cmd == "EnableADC")    enableADC(val.toInt());
  else if(cmd == "DisableADC")   enableADC(!val.toInt());
  else if(cmd == "LevelA")       poker(0x4064, val.toInt());
  else if(cmd == "LevelB")       poker(0x4065, val.toInt());
  else if(cmd == "MultiAdjust") {
    //  multiadust_scaled based on minimal and maximal ranges values
    int ma_min = peeker(0x4086); // eg. 15, right position
    int ma_max = peeker(0x4087); // eg. 127, left position;
    int ma_newval = ma_max - (val.toFloat() * 0.01 * (float)(ma_max - ma_min));
    if (ma_newval > ma_max) ma_newval = ma_max;
    else if (ma_newval < ma_min) ma_newval = ma_min;
    poker(0x420d, ma_newval);
  }
  else if(cmd == "Mode") {
    poker(0x407b,str2hex(val.c_str()));
    poker(0x4070,0x4);
    poker(0x4070,0x12); // execute mode
  }
  else {
    return false;
  }

  handleLedBlinking(3, true);
  return true;
}

void cutLevels(bool enabled) {
  if (enabled) {
    enableADC(false);
    poker(0x4064, 0);
    poker(0x4065, 0);
  }
  else {
    enableADC(true);
  }
}

void enableADC(bool enabled) {
  poker(0x400f, enabled?0x00:0x01);
}

int str2hex(const char str[]) {
  return (int)strtol(str, 0, 16);
}
