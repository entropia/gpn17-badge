
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WebServer.h>
#include <Wire.h>


#include "rboot.h"
#include "rboot-api.h"

#include <ArduinoJson.h>

#include "url-encode.h"
#include <FS.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

ESP8266WebServer server(80);
//holds the current upload
File fsUploadFile;


#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Fonts/FreeSans9pt7b.h>
#include <gfxfont.h>

#define GPIO_LCD_DC 0
#define GPIO_TX     1
#define GPIO_WS2813 4
#define GPIO_RX     3
#define GPIO_DN     2
#define GPIO_DP     5

#define GPIO_BOOT   16
#define GPIO_MOSI   13
#define GPIO_CLK    14
#define GPIO_LCD_CS 15
#define GPIO_BNO    12

#define MUX_JOY 0
#define MUX_BAT 1
#define MUX_LDR 2
#define MUX_ALK 4
#define MUX_IN1 5

#define VIBRATOR 3
#define MQ3_EN   4
#define LCD_LED  5
#define IR_EN    6
#define OUT1     7

#define UP      807
#define DOWN    648
#define RIGHT   540
#define LEFT    1024
#define OFFSET  50

#define I2C_PCA 0x25

#define NUM_LEDS    4

// Color definitions
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

TFT_ILI9163C tft = TFT_ILI9163C(GPIO_LCD_CS, GPIO_LCD_DC);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_LEDS, GPIO_WS2813, NEO_GRB + NEO_KHZ800);

//global variables
byte shiftConfig = 0; //stores the PCA config

void setup() {
  //ESP.eraseConfig();
  WiFi.persistent(false);

  initBadge();
reset:
  tft.clear();
  tft.writeFramebuffer();

  Serial.println("Purr!"); //useful debug message

  if (!SPIFFS.begin()) { //initialize the spiffs
    // TODO
    Serial.println("Error init SPIFFS!");
  }

  Serial.println(ESP.getFlashChipSize());


  setGPIO(LCD_LED, HIGH);

  tft.setFont(&FreeSans9pt7b);
  tft.setTextColor(YELLOW);
  tft.setCursor(2, 15);
  tft.print("ROM Manager");
  tft.drawLine(2, 18, 126, 18, WHITE);
  tft.drawLine(2, 112, 126, 112, WHITE);

  tft.setFont();
  tft.setTextSize(1);

  int adc;
  int bat;
  int romNumber = 1, lastRomNumber = 0;
  bool stillPressed = true;
  setAnalogMUX(MUX_JOY);
  String romNames[11] ;
  for (int i = 1; i < 10; i++) {
    File f = SPIFFS.open("/rom" + String(i), "r");
    if (f) {
      romNames[i] = f.readStringUntil('\n');
      Serial.println(romNames[i]);
    }
  }
  romNames[0] = "ROM Manager";

  while ((digitalRead(16) == LOW) || stillPressed) {
    delay(1);
    adc = analogRead(A0);

    if (digitalRead(16) == LOW)
      stillPressed = false;

    if (adc < UP + OFFSET && adc > UP - OFFSET) {
      romNumber--;
    }

    else if (adc < DOWN + OFFSET && adc > DOWN - OFFSET) {
      romNumber++;
    }

    if (romNumber > 11)
      romNumber = 1;

    if (romNumber < 1)
      romNumber = 11;

    if ( lastRomNumber != romNumber) {
      for (int i = 1; i < 10; i++) {

        if (i == romNumber) {
          //tft.fillRect(2, 12+10*i, 125, 9, WHITE);
          tft.setTextColor(BLACK, WHITE);
        }

        else {
          //tft.fillRect(2, 12+10*i, 125, 9, BLACK);
          tft.setTextColor(WHITE, BLACK);
        }

        tft.setCursor(3, 13 + 10 * i);
        String entry = "Slot " + String(i) + ": " + romNames[i];
        int whitespace = 20 - entry.length();
        for (int i = 0; i < whitespace; i++) {
          entry += " ";
        }
        tft.print(entry);
      }

      if (romNumber == 10) {
        //tft.fillRect(2, 116, 60, 10, WHITE);
        tft.setTextColor(BLACK , WHITE);
        tft.setCursor(3, 117);
        tft.print("ROM Store");

        //tft.fillRect(66, 116, 60, 10, BLACK);
        tft.setTextColor(YELLOW, BLACK);
        tft.setCursor(72, 117);
        tft.print("Utilities");
      }

      else if (romNumber == 11) {
        //tft.fillRect(66, 116, 60, 10, WHITE);
        tft.setTextColor(BLACK, WHITE);
        tft.setCursor(72, 117);
        tft.print("Utilities");

        //tft.fillRect(2, 116, 60, 10, BLACK);
        tft.setTextColor(YELLOW, BLACK);
        tft.setCursor(3, 117);
        tft.print("ROM Store");
      }

      else {
        //tft.fillRect(2, 116, 125, 10, BLACK);
        tft.setTextColor(YELLOW, BLACK);
        tft.setCursor(3, 117);
        tft.print("ROM Store");
        tft.setCursor(72, 117);
        tft.print("Utilities");
      }
      lastRomNumber = romNumber;
      tft.writeFramebuffer();
    }

  }

  Serial.print("Selected: ");
  Serial.println(romNumber);

  tft.clear();
  tft.writeFramebuffer();

  bool ledVal = 0;

  if (romNumber == 11) {
    tft.setFont(&FreeSans9pt7b);
    tft.setTextColor(YELLOW);
    tft.setCursor(2, 15);
    tft.print("Utilities");
    tft.drawLine(2, 18, 126, 18, WHITE);
    tft.drawLine(2, 112, 126, 112, WHITE);

    tft.setFont();
    tft.setTextSize(1);

    romNumber = 4;
    stillPressed = true;
    setAnalogMUX(MUX_JOY);
    String menu[] = {"SPIFFS Uploader", "Vibrator", "Torch", "About", "Back"};
    while ((digitalRead(16) == LOW) || stillPressed) {
      delay(10);
      adc = analogRead(A0);

      if (digitalRead(16) == LOW)
        stillPressed = false;

      if (adc < UP + OFFSET && adc > UP - OFFSET) {
        romNumber--;
        delay(15);
      }

      else if (adc < DOWN + OFFSET && adc > DOWN - OFFSET) {
        romNumber++;
        delay(15);
      }

      if (romNumber > 4)
        romNumber = 0;

      if (romNumber < 0)
        romNumber = 4;

      for (int i = 0; i < 5; i++) {

        if (i == romNumber) {
          tft.setTextColor(BLACK, WHITE);
        }

        else {
          tft.setTextColor(YELLOW, BLACK);
        }

        tft.setCursor(3, 23 + 10 * i);
        String entry = " " + menu[i];
        int whitespace = 19 - menu[i].length();
        for (int i = 0; i < whitespace; i++) {
          entry += " ";
        }
        tft.print(entry);

      }
      tft.writeFramebuffer();

      if (romNumber == 1) {
        if (digitalRead(16) == HIGH) {
          setGPIO(VIBRATOR, HIGH);
        }
        while (digitalRead(16) == HIGH) {
          delay(10);
        }
        setGPIO(VIBRATOR, LOW);
        stillPressed = true;
      }

      if (romNumber == 2) {
        if (digitalRead(16) == HIGH) {
          ledVal = !ledVal;
        }
        while (digitalRead(16) == HIGH) {
          delay(10);
        }
        stillPressed = true;
      }

      pixels.setPixelColor(1, pixels.Color(ledVal * 255, ledVal * 255, ledVal * 255));
      pixels.setPixelColor(2, pixels.Color(ledVal * 255, ledVal * 255, ledVal * 255));
      pixels.setPixelColor(3, pixels.Color(ledVal * 255, ledVal * 255, ledVal * 255));
      pixels.setPixelColor(0, pixels.Color(ledVal * 255, ledVal * 255, ledVal * 255));
      pixels.show();
    }

    if (romNumber == 0) {
      tft.clear();
      tft.writeFramebuffer();
      tft.setFont(&FreeSans9pt7b);
      tft.setTextColor(YELLOW);
      tft.setCursor(2, 15);
      tft.print("File Editor");
      tft.drawLine(2, 18, 126, 18, WHITE);

      initServer();

      while (digitalRead(16) == LOW) {
        server.handleClient();
      }

      ESP.eraseConfig();
      ESP.reset();
    }

    if (romNumber == 3) {
      tft.clear();
      tft.writeFramebuffer();
      tft.setFont(&FreeSans9pt7b);
      tft.setTextColor(YELLOW);
      tft.setCursor(0, 15);
      tft.println("GPN17 Badge\nROM Manager");

      tft.setFont();
      tft.setTextColor(WHITE);
      tft.println("Developed by \nNiklas Fauth\nTwitter: @FauthNiklas\n\n*otter*");

      while (digitalRead(16) == LOW) {
        delay(100);
      }

      goto reset;
    }

    if (romNumber == 4) {
      ledVal = 0;
      pixels.setPixelColor(1, pixels.Color(ledVal * 255, ledVal * 255, ledVal * 255));
      pixels.setPixelColor(2, pixels.Color(ledVal * 255, ledVal * 255, ledVal * 255));
      pixels.setPixelColor(3, pixels.Color(ledVal * 255, ledVal * 255, ledVal * 255));
      pixels.setPixelColor(0, pixels.Color(ledVal * 255, ledVal * 255, ledVal * 255));
      pixels.show();
      tft.clear();
      goto reset;
    }
  }


  if (romNumber == 10) {
    int ledVal = 0;
    bool up = true;
    Serial.println("Loading config.");

    tft.setFont();
    tft.setTextSize(1);
    tft.setTextColor(RED);
    tft.setCursor(0, 2);
    tft.println("Reading wifi.conf...");
    tft.writeFramebuffer();

    tft.setTextColor(WHITE);

    File wifiConf = SPIFFS.open("/wifi.conf", "r");

    if (!wifiConf) {
      tft.println("File not found!");
      tft.writeFramebuffer();
      delay(5000);
      goto reset;
    }
    String configString;
    while (wifiConf.available()) {
      configString += char(wifiConf.read());
    }
    wifiConf.close();
    UrlDecode confParse(configString.c_str());
    Serial.println(configString);
    configString = String();
    char* ssid = confParse.getKey("ssid");
    char* pw = confParse.getKey("pw");
    Serial.printf("Connecting to wifi '%s' with password '%s'...\n", ssid, pw);

    tft.println("Connecting to SSID:");
    tft.println(ssid);
    tft.writeFramebuffer();

    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    delay(400);
    WiFi.mode(WIFI_AP);
    delay(400);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    delay(400);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pw);

    unsigned long startTime = millis();
    //delete[] pw;
    while (WiFi.status() != WL_CONNECTED) {
      pixels.setPixelColor(1, pixels.Color(0, 0, ledVal));
      pixels.setPixelColor(2, pixels.Color(0, 0, ledVal));
      pixels.setPixelColor(3, pixels.Color(0, 0, ledVal));
      pixels.setPixelColor(0, pixels.Color(0, 0, ledVal));
      pixels.show();
      if (ledVal == 100) {
        up = false;
      }
      if (ledVal == 0) {
        up = true;
      }
      if (up) {
        //ui->root->setSub("Connecting...");
        //ui->draw();
        ledVal++;
      } else {

        ledVal--;
      }
      if (millis() - startTime > 30 * 1000) {

        pixels.setPixelColor(1, pixels.Color(50, 0, 0));
        pixels.setPixelColor(2, pixels.Color(50, 0, 0));
        pixels.setPixelColor(3, pixels.Color(50, 0, 0));
        pixels.setPixelColor(0, pixels.Color(50, 0, 0));
        pixels.show();
        tft.setTextColor(RED);
        tft.println("");
        tft.println("WiFi connect failed!");
        delay(2000);

        ESP.reset();
      }
      delay(10);
    }

    pixels.setPixelColor(1, pixels.Color(0, 0, 0));
    pixels.setPixelColor(2, pixels.Color(0, 0, 0));
    pixels.setPixelColor(3, pixels.Color(0, 0, 0));
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    
    //delete[] ssid;
    tft.setTextColor(GREEN);
    tft.println("");
    tft.println("WiFi connected!");
    tft.setTextColor(WHITE);
    tft.println(WiFi.localIP().toString());
    tft.writeFramebuffer();
    Serial.println("WiFi connected");
    Serial.println("");
    Serial.println(WiFi.localIP());

    pixels.show();

    delay(2000);

    pixels.setPixelColor(1, pixels.Color(0, 0, 0));
    pixels.setPixelColor(2, pixels.Color(0, 0, 0));
    pixels.setPixelColor(3, pixels.Color(0, 0, 0));
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();

    tft.clear();
    tft.writeFramebuffer();

    tft.setFont();
    tft.setTextSize(1);
    tft.setTextColor(RED);
    tft.setCursor(0, 2);
    tft.println("Reading store.conf...");
    tft.writeFramebuffer();

    File storeConf = SPIFFS.open("/store.conf", "r");
    if (!storeConf) {
      tft.println("File not found!");
      tft.writeFramebuffer();
      delay(5000);
      goto reset;
    }

    tft.setTextColor(WHITE);
    configString;
    while (storeConf.available()) {
      configString += char(storeConf.read());
    }
    storeConf.close();
    UrlDecode storeParse(configString.c_str());
    Serial.println(configString);
    configString = String();
    char* store = storeParse.getKey("url");
    char* httpPort = storeParse.getKey("port");
    char* fingerprint = storeParse.getKey("fingerprint");
    Serial.printf("Store address is '%s'\n", store);

    tft.println("Found ROM Store:");
    tft.println(store);
    tft.println("Port:");
    tft.println(atoi(httpPort));
    tft.writeFramebuffer();

    String updateURL = "";
{

String json = "";
    //{
      WiFiClientSecure client;
    
    if (!client.connect(store, atoi(httpPort))) {
      Serial.println("connection failed");
      tft.setTextColor(RED);
      tft.println("Connection failed!");
      tft.writeFramebuffer();
      delay(5000);
      goto reset;
    }

    if (client.verify(fingerprint, store)) {
      Serial.println("fingerprint matches");
    } else {
      Serial.println("fingerprint doesn't match");
      tft.setTextColor(RED);
      tft.println("SSL failed!\nCertificate invalid.");
      tft.writeFramebuffer();
      delay(5000);
      goto reset;
    }
    


    tft.setTextColor(GREEN);
    tft.println("Connected!");
    tft.println("");
    tft.setTextColor(WHITE);
    tft.println("Choose the ROM ID youwant to download:");
    tft.println("");
    tft.println("");
    tft.println("");
    tft.println("");
    tft.println("Use the Joystick.");
    tft.println("Then press enter.");
    tft.writeFramebuffer();

    tft.setFont(&FreeSans9pt7b);
    tft.writeFramebuffer();
    tft.setTextColor(YELLOW);

    int8_t digit0 = 0, digit1 = 0, digit2 = 0, digit3 = 0, selDigit = 0, lastSelDigit;
    uint16_t lastRomID = 1337, romID;
    while (digitalRead(16) == LOW) {

      delay(10);
      adc = analogRead(A0);

      if (adc < UP + OFFSET && adc > UP - OFFSET) {
        if (selDigit == 0)
          digit0++;
        if (selDigit == 1)
          digit1++;
        if (selDigit == 2)
          digit2++;
        if (selDigit == 3)
          digit3++;
        delay(150);
      }

      else if (adc < DOWN + OFFSET && adc > DOWN - OFFSET) {
        if (selDigit == 0)
          digit0--;
        if (selDigit == 1)
          digit1--;
        if (selDigit == 2)
          digit2--;
        if (selDigit == 3)
          digit3--;
        delay(150);
      }

      else if (adc < LEFT + OFFSET && adc > LEFT - OFFSET) {
        selDigit--;
        delay(150);
      }

      else if (adc < RIGHT + OFFSET && adc > RIGHT - OFFSET) {
        selDigit++;
        delay(150);
      }

      if (digit0 > 9)
        digit0 = 0;

      if (digit0 < 0)
        digit0 = 9;

      if (digit1 > 9)
        digit1 = 0;

      if (digit1 < 0)
        digit1 = 9;

      if (digit2 > 9)
        digit2 = 0;

      if (digit2 < 0)
        digit2 = 9;

      if (digit3 > 9)
        digit3 = 0;

      if (digit3 < 0)
        digit3 = 9;

      if (selDigit > 3)
        selDigit = 0;

      if (selDigit < 0)
        selDigit = 3;

      romID = digit0 * 1000 + digit1 * 100 + digit2 * 10 + digit3;
      if (romID != lastRomID || selDigit != lastSelDigit) {
        tft.fillRect(44, 80, 50, 20, BLACK);
        tft.fillRect(45 + 10 * selDigit, 82, 10, 15, 0x738E);

        tft.setCursor(45, 95);
        tft.print(digit0);
        tft.print(digit1);
        tft.print(digit2);
        tft.print(digit3);
        tft.writeFramebuffer();
        lastRomID = romID;
        lastSelDigit = selDigit;
      }
    }

    tft.setFont();
    tft.clear();
    tft.setTextSize(1);
    tft.setTextColor(RED);
    tft.setCursor(0, 2);
    tft.println("Searching the ROM...");
    tft.writeFramebuffer();

    client.print(String("GET ") + "/roms/json/details/" + String(romID) + "/" + " HTTP/1.1\r\n" +
                 "Host: " + store + "\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 30000) {
        Serial.println(">>> Client Timeout !");
        tft.println("Client Timeout!");
        client.stop();
        tft.writeFramebuffer();
        delay(5000);
        goto reset;
      }
    }

    boolean httpBody = false;
    while (client.available()) {
      String line = client.readStringUntil('\r');
      if (!httpBody && line.charAt(1) == '{') {
        httpBody = true;
      }
      if (httpBody) {
        json += line;
      }
    }

    
   
    //}
    

    //StaticJsonBuffer<1500> jsonBuffer;
    DynamicJsonBuffer jsonBuffer;
    Serial.println("Got data:");
    Serial.println(json);
    JsonObject& root = jsonBuffer.parseObject(json);
    if (!root.success()) {
      Serial.println("Failed to parse config file!");
      tft.println("Failed to parse config file!");
      //client.stop();
      tft.writeFramebuffer();
      delay(5000);
      goto reset;
    }
    String romName = root["name"];

    Serial.println(romName);

    tft.setTextColor(WHITE);
    tft.println("");
    tft.println("Found ROM:");
    tft.println(romName);

    tft.setCursor(3, 100);
    tft.println("Download this?");

    tft.writeFramebuffer();

    int8_t choice = 1;

    while (digitalRead(16) == LOW) {
      delay(10);
      adc = analogRead(A0);

      if (adc < LEFT + OFFSET && adc > LEFT - OFFSET) {
        choice--;
        delay(150);
      }

      else if (adc < RIGHT + OFFSET && adc > RIGHT - OFFSET) {
        choice++;
        delay(150);
      }

      if (choice > 1)
        choice = 0;

      if (choice < 0)
        choice = 1;

      if (choice == 1) {
        //tft.fillRect(2, 116, 60, 10, WHITE);
        tft.setTextColor(BLACK, WHITE);
        tft.setCursor(3, 117);
        tft.print("Eeyup     ");

        //tft.fillRect(66, 116, 60, 10, BLACK);
        tft.setTextColor(YELLOW, BLACK);
        tft.setCursor(66, 117);
        tft.print("      Nope");
      }

      else if (choice == 0) {
        //tft.fillRect(66, 116, 60, 10, WHITE);
        tft.setTextColor(BLACK, WHITE);
        tft.setCursor(66, 117);
        tft.print("      Nope");

        //tft.fillRect(2, 116, 60, 10, BLACK);
        tft.setTextColor(YELLOW, BLACK);
        tft.setCursor(3, 117);
        tft.print("Eeyup     ");
      }
      tft.writeFramebuffer();
    }

    if (choice == 0) {
      goto reset;
    }
    tft.clear();
    tft.drawLine(2, 112, 126, 112, WHITE);

    tft.setTextColor(YELLOW);

    tft.setCursor(2, 2);
    tft.print("Select a slot:");

    tft.writeFramebuffer();

    stillPressed = true;
    setAnalogMUX(MUX_JOY);
    lastRomNumber = 1337;
    while ((digitalRead(16) == LOW) || stillPressed) {
      delay(10);
      adc = analogRead(A0);

      if (digitalRead(16) == LOW)
        stillPressed = false;

      if (adc < UP + OFFSET && adc > UP - OFFSET) {
        romNumber--;
      }

      else if (adc < DOWN + OFFSET && adc > DOWN - OFFSET) {
        romNumber++;
      }

      if (romNumber > 10)
        romNumber = 0;

      if (romNumber < 0)
        romNumber = 10;

      if (lastRomNumber != romNumber) {

        for (int i = 0; i < 10; i++) {

          if (i == romNumber) {
            //tft.fillRect(2, 12+10*i, 125, 9, WHITE);
            tft.setTextColor(BLACK, WHITE);
          }

          else {
            //tft.fillRect(2, 12+10*i, 125, 9, BLACK);
            tft.setTextColor(WHITE, BLACK);
          }

          tft.setCursor(3, 13 + 10 * i);
          String entry = "Slot " + String(i) + ": " + romNames[i];
          int whitespace = 20 - entry.length();
          for (int i = 0; i < whitespace; i++) {
            entry += " ";
          }
          tft.print(entry);
        }

        if (romNumber == 10) {
          //tft.fillRect(2, 116, 125, 10, WHITE);
          tft.setTextColor(BLACK, WHITE);
          tft.setCursor(3, 117);
          tft.print("Cancel");
        }


        else {
          //tft.fillRect(2, 116, 125, 10, BLACK);
          tft.setTextColor(YELLOW, BLACK);
          tft.setCursor(3, 117);
          tft.print("Cancel              ");
        }
        tft.writeFramebuffer();
      }
    }
    if (romNumber == 10) {
      goto reset;
    }

    char* jsonAddress;
    if (romNumber % 2 == 0 || romNumber > 5) {
      jsonAddress = "low_binary";
    }

    else {
      jsonAddress = "high_binary";
    }


    String updateURLtemp = root[jsonAddress];
    updateURL = updateURLtemp;
    tft.clear();

    tft.setTextColor(RED);

    tft.setCursor(2, 2);
    tft.println("Starting update...");
    tft.println("");

    tft.setTextColor(WHITE);
    tft.println("This could take some time...");
    tft.println("");
    Serial.println(updateURL);

    tft.writeFramebuffer();

    //client.stop();

  }

    t_httpUpdate_return ret;
    if (romNumber < 6) {
      ret = ESPhttpUpdate.update(store, atoi(httpPort), updateURL, "", romNumber * 0x80000 + 0x2000, fingerprint);
    }

    else {
      ret = ESPhttpUpdate.update(store, atoi(httpPort), updateURL, "", (romNumber - 6) * 0x100000 + 0x402000, fingerprint);
    }

    /*t_httpUpdate_return ret;
      ret = ESPhttpUpdate.update(store, atoi(httpPort), "/media/roms/3dc42c86-9747-44d3-8e0c-af140075322f.bin", "", 2 * 0x80000 + 0x2000, fingerprint);
    */switch (ret) {
      case HTTP_UPDATE_FAILED:

        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        Serial.println();
        Serial.println();
        Serial.println();
        tft.setTextColor(RED);
        tft.println("Error:");
        tft.println(ESPhttpUpdate.getLastErrorString().c_str());
        tft.writeFramebuffer();
        delay(3000);
        goto reset;
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        Serial.println();
        Serial.println();
        Serial.println();
        tft.setTextColor(GREEN);
        tft.println("Yay!!");
        break;
    }


    tft.setCursor(2, 100);
    tft.setTextColor(RED);
    tft.print("Rebooting NOW!");
    tft.writeFramebuffer();
    delay(1000);

    ESP.reset();

  }





  tft.setFont();
  tft.setTextSize(1);
  tft.setTextColor(RED);
  tft.setCursor(2, 2);
  tft.print("Selected ROM: ");
  tft.print(romNumber);
  tft.writeFramebuffer();
  tft.setCursor(2, 12);
  tft.print("Writing config to");
  tft.setCursor(2, 22);
  tft.print("RTC Memory...");
  tft.writeFramebuffer();
  if (rboot_set_current_rom(romNumber)) {
    tft.setCursor(2, 50);
    tft.setTextColor(GREEN);
    tft.print("Success!");
  }

  else {
    tft.setCursor(2, 50);
    tft.setTextColor(RED);
    tft.print("Failed!");
    delay(5000);
  }
  tft.writeFramebuffer();

  rboot_config rboot_config = rboot_get_config();
  Serial.print("changed to: ");
  Serial.println(rboot_config.current_rom);

  tft.setCursor(2, 70);
  tft.setTextColor(WHITE);
  tft.print("Current config: ");
  tft.print(rboot_config.current_rom);
  tft.writeFramebuffer();

  delay(100);

  tft.setCursor(2, 100);
  tft.setTextColor(RED);
  tft.print("Rebooting NOW!");
  tft.writeFramebuffer();



  delay(500);

  ESP.reset();

}

void loop() {
}

void HSVtoRGB(int hue, int sat, int val, int colors[3])
{
  // hue: 0-359, sat: 0-255, val (lightness): 0-255
  int r, g, b, base;
  if (sat == 0)
  { // Achromatic color (gray).
    colors[0] = val;
    colors[1] = val;
    colors[2] = val;
  }
  else
  {
    base = ((255 - sat) * val) >> 8;
    switch (hue / 60)
    {
      case 0:
        r = val;
        g = (((val - base) * hue) / 60) + base;
        b = base;
        break;
      case 1:
        r = (((val - base) * (60 - (hue % 60))) / 60) + base;
        g = val;
        b = base;
        break;
      case 2:
        r = base;
        g = val;
        b = (((val - base) * (hue % 60)) / 60) + base;
        break;
      case 3:
        r = base;
        g = (((val - base) * (60 - (hue % 60))) / 60) + base;
        b = val;
        break;
      case 4:
        r = (((val - base) * (hue % 60)) / 60) + base;
        g = base;
        b = val;
        break;
      case 5:
        r = val;
        g = base;
        b = (((val - base) * (60 - (hue % 60))) / 60) + base;
        break;
    }
    colors[0] = r;
    colors[1] = g;
    colors[2] = b;
  }
}





void initServer() {

  tft.setFont();
  tft.setTextSize(1);
  tft.setCursor(0, 23);
  tft.setTextColor(WHITE);
  tft.println("Creating WiFi AP...");

  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }

  Serial.printf("Free heap at init: %d\n", ESP.getFreeHeap());
  
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress (10, 0, 0, 1), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0));
  char pw[20];
  char ssid[20];
  sprintf(pw, "%d", random(10000000, 100000000)); // TODO richtig machen
  sprintf(ssid, "ESP%d", ESP.getChipId());
  //sprintf(pw, "%s", "hallohallo");
  
  Serial.println(WiFi.softAP(ssid, pw));

  tft.setTextColor(GREEN);
  tft.println("Done!");
  tft.setTextColor(WHITE);
  tft.println(" ");
  tft.println("SSID: ");
  tft.println(ssid);
  tft.setCursor(0, 67);
  tft.println("Password: ");

  //FullScreenBMPStatus* webStatus = new FullScreenBMPStatus();
  //connectWizard(ssid, pw, webStatus);
  bool dispPw = false;

  int last_b = millis();

  bool b_state = false;

  tft.setCursor(0, 75);
  tft.setTextColor(WHITE, BLACK);
  tft.println("Press Center.");
  tft.writeFramebuffer();
  bool toogleSwitch = false;
  bool up = true;
  int ledVal = 0;
  while (true) {
    pixels.setPixelColor(1, pixels.Color(0, ledVal, 0));
    pixels.setPixelColor(2, pixels.Color(0, ledVal, 0));
    pixels.setPixelColor(3, pixels.Color(0, ledVal, 0));
    pixels.setPixelColor(0, pixels.Color(0, ledVal, 0));
    pixels.show();
    if (ledVal == 100) {
      up = false;
    }
    if (ledVal == 0) {
      up = true;
    }
    if (up) {
      //ui->root->setSub("Connecting...");
      //ui->draw();
      ledVal++;
    } else {

      ledVal--;
    }

    if ((digitalRead(16) == HIGH) && !toogleSwitch) {
      toogleSwitch = true;
      dispPw = !dispPw;
      if (dispPw) {
        tft.setCursor(0, 75);
        tft.setTextColor(WHITE, BLACK);
        tft.print(pw);
        tft.println("          ");
        tft.writeFramebuffer();
      } else {
        tft.setCursor(0, 75);
        tft.setTextColor(WHITE, BLACK);
        tft.println("Press Center.");
        tft.writeFramebuffer();
      }
    }
    if (digitalRead(16) == LOW) {
      toogleSwitch = false;
    }

    if (WiFi.softAPgetStationNum() > 0) {
      tft.setTextColor(WHITE, BLACK);;
      tft.setCursor(0, 90);
      tft.println("Please visit:");
      tft.println("10.0.0.1");
      tft.setCursor(0, 118);
      tft.println("Press Center to abort.");
      tft.writeFramebuffer();
      break;
    }
    delay(10);
  }
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  pixels.setPixelColor(1, pixels.Color(0, 0, 0));
  pixels.setPixelColor(2, pixels.Color(0, 0, 0));
  pixels.setPixelColor(3, pixels.Color(0, 0, 0));
  pixels.show();


  //SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, []() {
    if (!handleFileRead("/mng/edit.htm")) server.send(404, "text/plain", "FileNotFound");
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, []() {
    server.send(200, "text/plain", "");
  }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([]() {
    if (!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, []() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += ", \"analog\":" + String(analogRead(A0));
    json += ", \"gpio\":" + String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });
  server.begin();
  Serial.println("HTTP server started");
}

//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType(String filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) {
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "mng/index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload() {
  if (server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    //Serial.print("handleFileUpload Data: "); Serial.println(upload.currentSize);
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile)
      fsUploadFile.close();
    Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
  }
}

void handleFileDelete() {
  if (server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileDelete: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate() {
  if (server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileCreate: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if (file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if (!server.hasArg("dir")) {
    server.send(500, "text/plain", "BAD ARGS");
    return;
  }

  String path = server.arg("dir");
  Serial.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while (dir.next()) {
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}

void setGPIO(byte channel, boolean level) {
  bitWrite(shiftConfig, channel, level);
  Wire.beginTransmission(I2C_PCA);
  Wire.write(shiftConfig);
  Wire.endTransmission();
}

void setAnalogMUX(byte channel) {
  shiftConfig = shiftConfig & 0b11111000;
  shiftConfig = shiftConfig | channel;
  Wire.beginTransmission(I2C_PCA);
  Wire.write(shiftConfig);
  Wire.endTransmission();
}

uint16_t getBatLvl() {
  setAnalogMUX(MUX_BAT);
  delay(10);
  uint16_t avg = 0;
  for (byte i = 0; i < 10; i++) {
    avg += analogRead(A0);
  }
  return (avg / 10);
}

uint16_t getBatVoltage() { //battery voltage in mV
  return (getBatLvl() * 4.8);
}

uint8_t getBatPercent() { //battery voltage in %
  return (map(constrain(getBatLvl() * 4.8, 3200, 4100), 3200, 4100, 0, 100));
}

uint16_t getLDRLvl() {
  setAnalogMUX(MUX_LDR);
  uint16_t avg = 0;
  for (byte i = 0; i < 10; i++) {
    avg += analogRead(A0);
  }
  return (avg / 10);
}


void initBadge() { //initialize the badge

  Serial.begin(115200);

  // Next 2 line seem to be needed to connect to wifi after Wake up
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(20);

  Wire.begin(9, 10);

  pinMode(GPIO_BOOT, INPUT_PULLDOWN_16);
  pinMode(GPIO_WS2813, OUTPUT);

  //the ESP is very power-sensitive during startup, so...
  Wire.beginTransmission(I2C_PCA);
  Wire.write(0b00000000); //...clear the I2C extender to switch off vobrator and LCD backlight
  Wire.endTransmission();


  pixels.begin(); //initialize the WS2813
  pixels.clear();
  pixels.show();

  delay(100);

  tft.begin(); //initialize the tft. This also sets up SPI to 80MHz Mode 0
  tft.setRotation(2);
  tft.scroll(32);

  tft.fillDummy(BLACK);
  tft.clear();

  pixels.clear(); //clear the WS2813 another time, in case they catched up some noise
  pixels.show();
}

