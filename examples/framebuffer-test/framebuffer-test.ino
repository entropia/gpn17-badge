
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <TFT_ILI9163C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <IRremoteESP8266.h>

#include <Fonts/FreeSans12pt7b.h>

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

uint16_t color = 0xFFFF;

uint32_t endTime = 0;
uint32_t startTime = 0;

int _width = 128;
int _height = 128;

float fps = 0;

int offsetCounter = 16;
int offsetDir = 1;

//
int circleSize = 16;
int circleCount = 8;
float circlePosition[16] = {62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62};
float circleDir[8] = {20, 50, 60, 80, 120, 150, 170, 275};
float circleVelocity[8] = {1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5};
//

void setup() {
  initBadge();
  tft.setFont(&FreeSans12pt7b);
}

void loop() {

  startTime = micros();

  for (int i = 0; i < 128; i++) {
    for (int j = 0; j < 128; j++) {
      tft.drawPixel(i, j, tft.Color565(i, j, (i + j) / 2));
    }
  }

  tft.writeLine(16, 16, 115, 115, YELLOW);
  tft.writeLine(16, 115, 115, 16, BLUE);
  tft.drawRect(16, 16, 100, 100, 0xFFFF);

  for (int i = 0; i < circleCount * 2 - 1; i += 2) {
    int cs = circleSize / 16.0 * (16.0 - (i / 2));
    tft.drawCircle(circlePosition[i], circlePosition[(i) + 1], cs, GREEN); //draw circles

    float dX =  circleVelocity[i / 2] * cos(((circleDir[i / 2])* 1000) / 57296.0); //move by n foreward depending on angel
    float dY =  circleVelocity[i / 2] * sin(((circleDir[i / 2])* 1000) / 57296.0); //move by n foreward depending on angel

    circlePosition[i] += dX;
    circlePosition[i + 1] += dY;

    if (circlePosition[i] - cs < 1 ) circleDir[i / 2] = 180 - circleDir[i / 2];
    if ( circlePosition[i] + cs > 126 ) circleDir[i / 2] = 2 * 180 + 180 - circleDir[i / 2];
    if ( circlePosition[i + 1] + cs > 126) circleDir[i / 2] = 2 * 90 + 180 - circleDir[i / 2];
    if ( circlePosition[i + 1] - cs < 1) circleDir[i / 2] = 2 * 270 + 180 - circleDir[i / 2];

    if (circleDir[i / 2] >= 360) circleDir[i / 2] -= 360;
    if (circleDir[i / 2] <= 0) circleDir[i / 2] += 360;
  }

  if (offsetCounter < 18) offsetDir = 1;
  if (offsetCounter > 106) offsetDir = -1;

  offsetCounter += offsetDir;

  tft.setCursor(8, offsetCounter);
  tft.setTextSize(1);
  tft.setTextColor(RED);

  tft.print("FPS:");
  tft.println(fps);
  tft.setCursor(8, offsetCounter + 20);
  tft.print("BAT:");
  tft.println(getBatVoltage());

  tft.writeFramebuffer();
  
  endTime = micros();
  fps = 1000000.0 / (endTime - startTime);
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
  for (byte i = 0; i < 10; i++) {
    avg += analogRead(A0);
  }
  return (avg / 10);
}

uint16_t getBatVoltage() { //battery voltage in mV
  return (getBatLvl() * 4.4);
}

uint16_t getLDRLvl() {
  if (portExpanderConfig != 34) {
    setAnalogMUX(MUX_LDR);
    delay(20);
  }
  uint16_t avg = 0;
  for (byte i = 0; i < 10; i++) {
    avg += analogRead(A0);
  }
  return (avg / 10);
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
  tft.writeFramebuffer();
  setGPIO(LCD_LED, HIGH);

  pixels.clear(); //clear the WS2813 another time, in case they catched up some noise
  pixels.show();
}

