#include <ESP8266WiFi.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <IRremoteESP8266.h>

#include <TFT_ILI9163C.h>

#define BNO055_SAMPLERATE_DELAY_MS (10)

#define VERSION 2

#define USEWIFI
//#define USEIR

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
Adafruit_BNO055 bno = Adafruit_BNO055(BNO055_ID, BNO055_ADDRESS_B);

IRsend irsend(GPIO_DP);
IRrecv irrecv(GPIO_DN);
decode_results results;

byte portExpanderConfig = 0; //stores the 74HC595 config

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

double getVoltage() {
  double voltage;   // oversampling to increase the resolution
  long analog = 0;
  for (int i = 0; i <= 63; i++) {
    analog = analog + analogRead(A0);
    delayMicroseconds(1);
  }
  voltage = (analog / 64.0) * 4.8 / 1000;
  return voltage;
}

double calculatePPM(double v) {
  double PPM; // calculate the PPM value with a polynome generated from the datasheet's table
  PPM = 150.4351049 * pow(v, 5) - 2244.75988 * pow(v, 4) + 13308.5139 * pow(v, 3) - 39136.08594 * pow(v, 2) + (57082.6258 * v) - 32982.05333;
  return PPM;
}

float calculateBAC(double PPM) {
  float BAC;  // calculate the blood alcohol value from the ppm value
  BAC = PPM / 260.0;
  return BAC;
}

void setGPIO(byte channel, boolean level) {
  bitWrite(portExpanderConfig, channel, level);
  Wire.beginTransmission(I2C_PCA);
  Wire.write(portExpanderConfig);
  Wire.endTransmission();
}

void setAnalogMUX(byte channel) {
  portExpanderConfig = portExpanderConfig & 0b11111000;
  portExpanderConfig = portExpanderConfig | channel;
  Wire.beginTransmission(I2C_PCA);
  Wire.write(portExpanderConfig);
  Wire.endTransmission();
}

uint16_t getBatLvl() {
  if (portExpanderConfig != 33) {
    setAnalogMUX(MUX_BAT);
    delay(20);
  }
  uint16_t avg = 0;
  for (byte i = 0; i < 16; i++) {
    avg += analogRead(A0);
  }
  return (avg / 16);
}

uint16_t getBatVoltage() { //battery voltage in mV
  return (getBatLvl() * 4.8);
}

float getLDRLvl() {
  if (portExpanderConfig != 34) {
    setAnalogMUX(MUX_LDR);
    delay(20);
  }
  float avg = 0;
  for (byte i = 0; i < 64; i++) {
    avg += analogRead(A0);
  }
  return (avg / 64);
}

uint16_t getALKLvl() {
  if (portExpanderConfig != 36) {
    setAnalogMUX(MUX_ALK);
    delay(20);
  }
  uint16_t avg = 0;
  for (byte i = 0; i < 10; i++) {
    avg += analogRead(A0);
  }
  return (avg / 10);
}

void initBadge() { //initialize the badge

#ifdef USEIR
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
  setGPIO(IR_EN, HIGH);
  irrecv.enableIRIn(); // Start the receiver
  irsend.begin();
#else
  Serial.begin(115200);
#endif

#ifdef USEWIFI
  // Next 2 line seem to be needed to connect to wifi after Wake up
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(20);
#endif

  pinMode(GPIO_BOOT, INPUT_PULLDOWN_16);  // settings for the leds
  pinMode(GPIO_WS2813, OUTPUT);

  pixels.begin(); //initialize the WS2813
  pixels.clear();
  pixels.show();

  Wire.begin(9, 10); // Initalize i2c bus
  Wire.beginTransmission(I2C_PCA);
  Wire.write(0b00000000); //...clear the I2C extender to switch off vibrator and backlight
  Wire.endTransmission();

  delay(100);

  tft.begin(); //initialize the tft. This also sets up SPI to 80MHz Mode 0
  tft.setRotation(2); //turn screen
  tft.scroll(32); //move down by 32 pixels (needed)
  tft.fillScreen(BLACK);  //make screen black

  tft.setTextSize(2);
  tft.setCursor(25, 48);
  tft.print("GPN17");
  tft.setTextSize(1);

  tft.writeFramebuffer();
  setGPIO(LCD_LED, HIGH);

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


