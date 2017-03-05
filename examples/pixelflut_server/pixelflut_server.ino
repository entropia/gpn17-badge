#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>

#define VERSION 1

#if (VERSION == 1)
#define GPIO_LCD_DC 0
#define GPIO_TX     1
#define GPIO_WS2813 2
#define GPIO_RX     3
#define GPIO_DN     4
#define GPIO_DP     5
#define GPIO_LATCH  9
#define GPIO_LCD_BL 10
#define GPIO_MISO   12
#define GPIO_MOSI   13
#define GPIO_CLK    14
#define GPIO_LCD_CS 15
#define GPIO_MPU_CS 16

#define MUX_JOY 0
#define MUX_BAT 1
#define MUX_LDR 2
#define MUX_NTC 3
#define MUX_ALK 4
#define MUX_IN1 5
#define MUX_IN2 6
#define MUX_IN3 7

#define VIBRATOR 3
#define MQ3_EN   4
#define OUT1     5
#define OUT2     6
#define OUT3     7

#define UP      660
#define DOWN    580
#define RIGHT   500
#define LEFT    800
#define CENTER  1020
#define OFFSET  25

#define NUM_LEDS    4
#endif

#define  BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

TFT_ILI9163C tft = TFT_ILI9163C(GPIO_LCD_CS, GPIO_LCD_DC);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_LEDS, GPIO_WS2813, NEO_GRB + NEO_KHZ800);

#define MAX_SRV_CLIENTS 10  //max connections/clients

const char* ssid = "ssid";
const char* password = "password";

WiFiServer server(1234);  //set up server
WiFiClient serverClients[MAX_SRV_CLIENTS];

char Message[20]; // incoming String
int x;  //x pos
int y; //ypos
uint32_t c; //color
char *pos2; //pointer 1
char *pos1; //pos pointer 2
uint8_t i;  //client counter
bool showIPflag = false;  //flags for the info statemaschine
bool drawIPflag = true;
byte shiftConfig = 0; //stores the 74HC595 config

void setup() {
  initBadge();
  connectBadge();

  setGPIO(VIBRATOR, HIGH);
  delay(500);
  setGPIO(VIBRATOR, LOW);
}

void loop() {
   if (server.hasClient()) { // slot handler
    for (i = 0; i < MAX_SRV_CLIENTS; i++) {
      if (!serverClients[i] || !serverClients[i].connected()) {
        if (serverClients[i]) serverClients[i].stop();
        serverClients[i] = server.available();
        continue;
      }
    }
    WiFiClient serverClient = server.available();
    serverClient.stop();
  }

  for (i = 0; i < MAX_SRV_CLIENTS; i++) { //iterate throu the slots
    if (serverClients[i].connected() && serverClients[i].available()) { //skip unconnected slots
      pos1 = 0; //needs to be cleared for every client
      pos2 = 0; //this one too
      for (int j = 0; j < 20; j++) { //fetch the message
        Message[j] = serverClients[i].read(); //faster the readBytesUntil()
        if (Message[j] == '\n') { //exit on newline
          break;
        }
      }
      if (!strncmp(Message, "PX ", 3)) {  //parsing part, it works...
        pos1 = Message + 3;
        x = strtoul(Message + 3, &pos1, 10);
        if (Message != pos1) {
          pos1++;
          pos2 = pos1;
          y = strtoul(pos1, &pos2, 10);
          if (pos1 != pos2) {
            pos2++;
            pos1 = pos2;
            c = strtoul(pos2, &pos1, 16);
          }
        }
      }
      if ( x > -1 && x < 128 && y > -1 && y < 128 && !showIPflag) {
        tft.drawPixel(x, y, tft.Color24To565(c));   //draw on the display
      }
      if ( x > -1 && x < 128 && y > -1 && y < 120 && showIPflag) {
        tft.drawPixel(x, y, tft.Color24To565(c));   //draw on the display leaving space for the IP
      }
    }
  }
  if (drawIPflag) { //draw IP only once and smaller the drawing area
    showIPflag = true;
    drawIP();
    drawIPflag = false;
  }
}

void drawIP() {
  tft.setCursor(0, 120);
  tft.print(WiFi.localIP());
  tft.print(":1234");
}

void setGPIO(byte channel, boolean level) {
  bitWrite(shiftConfig, channel, level);
  SPI.transfer(shiftConfig);
  digitalWrite(GPIO_LATCH, LOW);
  digitalWrite(GPIO_LATCH, HIGH);
}

void setAnalogMUX(byte channel) {
  shiftConfig = shiftConfig & 0b11111000;
  shiftConfig = shiftConfig | channel;
  SPI.transfer(shiftConfig);
  digitalWrite(GPIO_LATCH, LOW);
  digitalWrite(GPIO_LATCH, HIGH);
}
void initBadge() { //initialize the badge

  // Next 2 line seem to be needed to connect to wifi after Wake up
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(20);

  pinMode(GPIO_LATCH, OUTPUT);
  pinMode(GPIO_LCD_BL, OUTPUT);
  pinMode(GPIO_WS2813, OUTPUT);

  //the ESP is very power-sensitive during startup, so...
  digitalWrite(GPIO_LCD_BL, LOW);                //...switch off the LCD backlight
  shiftOut(GPIO_MOSI, GPIO_CLK, MSBFIRST, 0x00); //...clear the shift register
  digitalWrite(GPIO_LATCH, HIGH);

  pixels.begin(); //initialize the WS2813
  pixels.clear();
  pixels.show();

  tft.begin(); //init library
  tft.setRotation(2); //rotate display to badge rotation
  tft.scroll(32); //move the scolliingbuffer down
  tft.fillScreen(0x0000); //clear the screen

  tft.setCursor(0, 0);  //informative text
  tft.setTextSize(1);
  tft.println("Starting Server");


  analogWrite(GPIO_LCD_BL, 1023); //switch on LCD backlight

  pixels.clear(); //clear the WS2813 another time, in case they catched up some noise
  pixels.show();
}

void connectBadge() {
  WiFi.begin(ssid, password);
  tft.setCursor(2, 22);
  tft.print("Connecting");
  tft.setCursor(63, 22);
  uint8_t cnt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    tft.print(".");
    cnt++;
    if (cnt % 4 == 0) {
      tft.fillRect(63, 22, 30, 10, BLACK);
      tft.setCursor(63, 22);
    }
    delay(250);
  }
  server.begin();
  server.setNoDelay(true);

  tft.setTextColor(GREEN);
  tft.setCursor(2, 42);
  tft.print("WiFi connected!");
  tft.setCursor(2, 52);
  tft.print(WiFi.localIP());
}


