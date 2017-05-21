
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>


#include "rboot.h"
#include "rboot-api.h"

#include <ArduinoJson.h>

#include "url-encode.h"
#include <FS.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>


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

#define UP      720
#define DOWN    570
#define RIGHT   480
#define LEFT    960
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
  initBadge();
  reset:
  tft.fillScreen(BLACK);
  tft.writeFramebuffer();
  
  Serial.println("Purr!"); //useful debug message

  if (!SPIFFS.begin()) { //initialize the spiffs
    // TODO
    Serial.println("Error init SPIFFS!");
  }

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
  int romNumber = 1;
  bool stillPressed = true;
  setAnalogMUX(MUX_JOY);
  while((digitalRead(16) == LOW) || stillPressed) {
    delay(10);
    adc = analogRead(A0);

    if (digitalRead(16) == LOW)
      stillPressed = false;
  
    if (adc < UP + OFFSET && adc > UP - OFFSET) {
      romNumber--;
      delay(150);
    }

    else if (adc < DOWN + OFFSET && adc > DOWN - OFFSET) {
      romNumber++;
      delay(150);
    }

    if (romNumber > 11)
      romNumber = 1;

    if (romNumber < 1)
      romNumber = 11;

    for (int i = 1; i < 10; i++) {
      File f = SPIFFS.open("/rom" + String(i), "r");
      if(i == romNumber) {
        tft.fillRect(2, 12+10*i, 125, 9, WHITE);
        tft.setTextColor(BLACK);
      }

      else {
        tft.fillRect(2, 12+10*i, 125, 9, BLACK);
        tft.setTextColor(WHITE);
      }

      tft.setCursor(3, 13+10*i); 
      tft.print("Slot ");
      tft.print(i);
      tft.print(": ");

      if (f)  {
        tft.print(f.readStringUntil('\n'));
      }
    }

    if (romNumber == 10) {
      tft.fillRect(2, 116, 60, 10, WHITE);
      tft.setTextColor(BLACK);
      tft.setCursor(3, 117); 
      tft.print("ROM Store");

      tft.fillRect(66, 116, 60, 10, BLACK);
      tft.setTextColor(YELLOW);
      tft.setCursor(72, 117); 
      tft.print("Utilities");
    }

    else if (romNumber == 11) {
      tft.fillRect(66, 116, 60, 10, WHITE);
      tft.setTextColor(BLACK);
      tft.setCursor(72, 117); 
      tft.print("Utilities");

      tft.fillRect(2, 116, 60, 10, BLACK);
      tft.setTextColor(YELLOW);
      tft.setCursor(3, 117); 
      tft.print("ROM Store");
    }

    else {
      tft.fillRect(2, 116, 125, 10, BLACK);
      tft.setTextColor(YELLOW);
      tft.setCursor(3, 117); 
      tft.print("ROM Store");
      tft.setCursor(72, 117);      
      tft.print("Utilities");
    } 
    tft.writeFramebuffer();
    
  }

  Serial.print("Selected: ");
  Serial.println(romNumber);

  tft.fillScreen(BLACK);
  tft.writeFramebuffer();

  if (romNumber == 11) {
    tft.setFont(&FreeSans9pt7b);
    tft.setTextColor(YELLOW);
    tft.setCursor(2, 15); 
    tft.print("Utilities");
    tft.drawLine(2, 18, 126, 18, WHITE);
    tft.drawLine(2, 112, 126, 112, WHITE);

    tft.setFont();
    tft.setTextSize(1);

    romNumber = 1;
    stillPressed = true;
    setAnalogMUX(MUX_JOY);
    while((digitalRead(16) == LOW) || stillPressed) {
      delay(10);
      adc = analogRead(A0);

      if (digitalRead(16) == LOW)
        stillPressed = false;
  
      if (adc < UP + OFFSET && adc > UP - OFFSET) {
        romNumber--;
        delay(150);
      }

      else if (adc < DOWN + OFFSET && adc > DOWN - OFFSET) {
        romNumber++;
        delay(150);
      }

      if (romNumber > 11)
        romNumber = 1;

      if (romNumber < 1)
        romNumber = 11;
      tft.writeFramebuffer();
    }
  }


  if (romNumber == 10) {
    int ledVal = 0;
    bool up = true;
    WiFi.mode(WIFI_STA);
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
    
    unsigned long startTime = millis();
    WiFi.begin(ssid, pw);
    delete[] pw;
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
        //ui->draw();
        delay(5000);
        //initialConfig();
        ESP.restart();
      }
      delay(10);
    }

    pixels.setPixelColor(1, pixels.Color(0, 0, 0));
    pixels.setPixelColor(2, pixels.Color(0, 0, 0));
    pixels.setPixelColor(3, pixels.Color(0, 0, 0));
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    
    delete[] ssid;
    tft.setTextColor(GREEN);
    tft.println("");
    tft.println("WiFi connected!");
    tft.setTextColor(WHITE);
    tft.println(WiFi.localIP().toString());
    tft.writeFramebuffer();
    Serial.println("WiFi connected");
    Serial.println("");
    Serial.println(WiFi.localIP());

    delay(2000);

    tft.fillScreen(BLACK);
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
    Serial.printf("Store address is '%s'\n", store);
    
    tft.println("Found ROM Store:");
    tft.println(store);
    tft.println("Port:");
    tft.println(atoi(httpPort));
    tft.writeFramebuffer();
    
    WiFiClient client;
    if (!client.connect(store, atoi(httpPort))) {
      Serial.println("connection failed");
      tft.setTextColor(RED);
      tft.println("Connection failed!");
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

    tft.setFont(&FreeSans9pt7b);
    tft.setTextColor(YELLOW);

    int8_t digit0=0, digit1=0, digit2=0, digit3=0, selDigit=0;
    while(digitalRead(16) == LOW) {

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
        
      tft.fillRect(44, 80, 50, 20, BLACK);
      tft.fillRect(45+10*selDigit, 82, 10, 15, 0x738E);
      
      tft.setCursor(45, 95); 
      tft.print(digit0);
      tft.print(digit1);
      tft.print(digit2);
      tft.print(digit3);
      tft.writeFramebuffer();
    }

    uint16_t romID = digit0 * 1000 + digit1 * 100 + digit2 * 10 + digit3;

    tft.setFont();
    tft.fillScreen(BLACK);
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
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        tft.println("Client Timeout!");
        client.stop();
        tft.writeFramebuffer();
        delay(5000);
        goto reset;
      }
    }

    String json = "";
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
    StaticJsonBuffer<1500> jsonBuffer;
    Serial.println("Got data:");
    Serial.println(json);
    JsonObject& root = jsonBuffer.parseObject(json);
    if (!root.success()) {
      Serial.println("Failed to parse config file!");
      tft.println("Failed to parse config file!");
      client.stop();
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

    while(digitalRead(16) == LOW) {
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
        tft.fillRect(2, 116, 60, 10, WHITE);
        tft.setTextColor(BLACK);
        tft.setCursor(3, 117); 
        tft.print("Eeyup");

        tft.fillRect(66, 116, 60, 10, BLACK);
        tft.setTextColor(YELLOW);
        tft.setCursor(102, 117); 
        tft.print("Nope");
      }

      else if (choice == 0) {
        tft.fillRect(66, 116, 60, 10, WHITE);
        tft.setTextColor(BLACK);
        tft.setCursor(102, 117); 
        tft.print("Nope");

        tft.fillRect(2, 116, 60, 10, BLACK);
        tft.setTextColor(YELLOW);
        tft.setCursor(3, 117); 
        tft.print("Eeyup");
      }
      tft.writeFramebuffer();
    }

    if (choice == 0) {
      goto reset;
    }
    tft.fillScreen(BLACK);
    tft.drawLine(2, 112, 126, 112, WHITE);

    tft.setTextColor(YELLOW);
    
    tft.setCursor(2, 2); 
    tft.print("Select a slot:");
    
    tft.writeFramebuffer();

    stillPressed = true;
    setAnalogMUX(MUX_JOY);
    while((digitalRead(16) == LOW) || stillPressed) {
      delay(10);
      adc = analogRead(A0);

      if (digitalRead(16) == LOW)
        stillPressed = false;
  
      if (adc < UP + OFFSET && adc > UP - OFFSET) {
        romNumber--;
        delay(150);
      }

      else if (adc < DOWN + OFFSET && adc > DOWN - OFFSET) {
        romNumber++;
        delay(150);
      }

      if (romNumber > 10)
        romNumber = 0;

      if (romNumber < 0)
        romNumber = 10;

      for (int i = 0; i < 10; i++) {
        File f = SPIFFS.open("/rom" + String(i), "r");
        if(i == romNumber) {
          tft.fillRect(2, 12+10*i, 125, 9, WHITE);
          tft.setTextColor(BLACK);
        }

        else {
          tft.fillRect(2, 12+10*i, 125, 9, BLACK);
          tft.setTextColor(WHITE);
        }

        tft.setCursor(3, 13+10*i); 
        tft.print("Slot ");
        tft.print(i);
        tft.print(": ");

        if (i == 0) {
          tft.print("ROM Manager");
        }
        else if (f)  {
          tft.print(f.readStringUntil('\n'));
        }
      }
  
      if (romNumber == 10) {
        tft.fillRect(2, 116, 125, 10, WHITE);
        tft.setTextColor(BLACK);
        tft.setCursor(3, 117); 
        tft.print("Cancel");
      }


      else {
        tft.fillRect(2, 116, 125, 10, BLACK);
        tft.setTextColor(YELLOW);
        tft.setCursor(3, 117); 
        tft.print("Cancel");
      }  
      tft.writeFramebuffer();
    }
    if (romNumber == 10) {
      goto reset;
    }

     char* jsonAddress;
    if(romNumber % 2 == 0 || romNumber > 5) {
      jsonAddress = "low_binary";
    }

    else {
      jsonAddress = "high_binary";
    }


    String updateURL = root[jsonAddress];
    tft.fillScreen(BLACK);
  
    tft.setTextColor(RED);
    
    tft.setCursor(2, 2); 
    tft.println("Starting update...");
    tft.println("");

    tft.setTextColor(WHITE);
    tft.println("This could take some time...");
    tft.println("");
    
    
    tft.writeFramebuffer();

    t_httpUpdate_return ret;
    if (romNumber < 6) {
      ret = ESPhttpUpdate.update(store, atoi(httpPort), updateURL, "", romNumber * 0x80000 + 0x2000);
    }

    else {
      ret = ESPhttpUpdate.update(store, atoi(httpPort), updateURL, "", (romNumber-6) * 0x100000 + 0x402000);
    }

    switch(ret) {
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
  if(rboot_set_current_rom(romNumber)) {
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
  
  pixels.clear(); //clear the WS2813 another time, in case they catched up some noise
  pixels.show();
}

