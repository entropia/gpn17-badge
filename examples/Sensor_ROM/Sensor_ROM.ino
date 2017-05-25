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
#include "rboot.h"

#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/Picopixel.h>
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


String menus[8] = {"Temperatur", "Licht", "Gyro", "ACC", "Compass", "Euler", "Ain", "Akku"}; //
String titleX[8] = {"C", "lx", "rad", "m/s^2", "uT", "degrees", "V", "V"};

#define STORAGEDEPTH 96
float graphStorage3[STORAGEDEPTH][3]; //Buffer for multi graph

int state = 0;
int sleepVal = 0; //sensor sleep value
int sleepCounter = 0; //sensor sleep counter
imu::Vector<3> bnoOutput;

void setup() {
  tft.setFont(&FreeSerif9pt7b);
  initBadge();
  tft.setTextSize(1);
  bno.begin();
  delay(400);
  state = renderMenu();
}

void loop() {

  sleepCounter = 0;
  do {
    if (digitalRead(GPIO_BOOT) == 1) {
      while (digitalRead(GPIO_BOOT) == 1) delay(1);
      float graphStorage[STORAGEDEPTH];     //clear buffer
      float graphStorage3[STORAGEDEPTH][3];
      state = renderMenu();
    }
    delay(1);
    sleepCounter++;
  } while (sleepCounter <= sleepVal); // sleep counter for slower measurements (temp & batterie)

  switch (state) {
    case 0://Temp
      bnoOutput.x() = bno.getTemp();
      sleepVal = 1000;
      break;
    case 1://Licht
      setAnalogMUX(MUX_LDR);
      bnoOutput.x() = getLDRVoltage();
      sleepVal = 0;
      break;
    case 2://Gyro
      bnoOutput = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
      sleepVal = 0;
      break;
    case 3://ACC
      bnoOutput = bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
      sleepVal = 0;
      break;
    case 4://Compass
      bnoOutput = bno.getVector(Adafruit_BNO055::VECTOR_MAGNETOMETER);
      sleepVal = 0;
      break;
    case 5://Fused
      bnoOutput = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
      sleepVal = 0;
      break;
    case 6://Ain
      setAnalogMUX(MUX_IN1);
      bnoOutput.x() = getVoltage();
      break;
    case 7://Akku
      setAnalogMUX(MUX_BAT);
      bnoOutput.x() = getBatVoltage() / 1000;
      sleepVal = 600;
      break;
  }

  graphStorage3[0][0] = bnoOutput.x();
  graphStorage3[0][1] = bnoOutput.y();
  graphStorage3[0][2] = bnoOutput.z();

  for (int i = STORAGEDEPTH; i >= 0; i--) {
    graphStorage3[i + 1][0] = graphStorage3[i][0];
    graphStorage3[i + 1][1] = graphStorage3[i][1];
    graphStorage3[i + 1][2] = graphStorage3[i][2];
  }

  endTime = millis();
  if (state == 0 || state == 1 || state == 6 || state == 7) renderGraph(graphStorage3, 1, ((endTime - startTime) / 1000.0) * 8.0, 20, 50);
  else if (state == 2 || state == 3 || state == 4 || state == 5) renderGraph(graphStorage3, 3, ((endTime - startTime) / 1000.0) * 8.0, 20, 50);
  startTime = millis();
}

void renderGraph(float data[STORAGEDEPTH][3], int graphNumber, float timeInterval, int x, int y) {

  float graphHeight = 64; //graph height
  float maxVal = data[0][0];
  float minVal = data[0][0];

  for (int j = 0; j < graphNumber; j++) {
    for (int i = 0; i < STORAGEDEPTH; i++) if (data[i][j] > maxVal) maxVal = data[i][j]; // get min and max values of the array
    for (int i = 0; i < STORAGEDEPTH; i++) if (data[i][j] < minVal) minVal = data[i][j];
    delay(0);
  }

  if (minVal == maxVal) { //extend the shown data
    maxVal++;
    minVal--;
  }

  float deltaVal = maxVal - minVal; // get the min/max delta

  drawGraphOverlay(graphHeight, maxVal, minVal, deltaVal, timeInterval, x, y); //render the overlay

  uint16_t color;
  for (int i = 0; i < STORAGEDEPTH; i++) {  //draw the graph
    for (int j = 0; j <= graphNumber - 1; j++) {
      if (j == 0) color = RED;  //set color accordingly to graph nr
      if (j == 1) color = GREEN;
      if (j == 2) color = BLUE;
      if ( i > 0) {
        tft.drawLine((STORAGEDEPTH - i) + x, y + (graphHeight - mapFloat(data[i][j], minVal, maxVal, 0.0, graphHeight)), (STORAGEDEPTH - (i - 1)) + x, y + (graphHeight - mapFloat(data[i - 1][j], minVal, maxVal, 0.0, graphHeight)), color);
      } else {
        tft.drawPixel((STORAGEDEPTH - i) + x, y + (graphHeight - mapFloat(data[i][j], minVal, maxVal, 0.0, graphHeight)), color);
      }
      delay(0);
    }
  }
  tft.writeFramebuffer();
}

void drawGraphOverlay(int graphHeight, float maxVal, float minVal, float deltaVal, float timeInterval, int x, int y) {
  tft.setFont(&Picopixel);  //set to a small font and white color
  tft.setTextSize(1);
  tft.setTextColor(WHITE);

  tft.fillRect(x - 20, y - 16, STORAGEDEPTH + 32, graphHeight + 29, BLACK); //clear screen section
  tft.drawRect(x - 19, y - 15, STORAGEDEPTH + 30, graphHeight + 28, WHITE); //Draw Outline

  tft.drawLine(x - 4, y + graphHeight, x + STORAGEDEPTH + 4, y + graphHeight, YELLOW); //draw outlines
  tft.drawLine(x, y - 4, x, y + graphHeight + 4, YELLOW);

  tft.drawLine(x - 2, y + 2 - 4, x , y - 4, YELLOW); //Arrow X
  tft.drawLine(x + 2, y + 2 - 4, x , y - 4, YELLOW);
  tft.drawLine(x + STORAGEDEPTH + 4, y + graphHeight, x + STORAGEDEPTH - 2 + 4, y + graphHeight + 2, YELLOW); //Arrow Y
  tft.drawLine(x + STORAGEDEPTH + 4, y + graphHeight, x + STORAGEDEPTH - 2 + 4, y + graphHeight - 2, YELLOW);

  float setSize = deltaVal / graphHeight;
  for (int i = 0; i <= graphHeight; i += 16) { //Axis title X
    tft.drawLine(x - 2, i + y , x + 2, i + y , YELLOW);
    tft.setCursor(x - 16, i - 8 + y + 10);
    tft.print(setSize * (graphHeight - i) + minVal);
    delay(0);
  }

  for (int i = 0; i <= STORAGEDEPTH; i += 16) { //Axis title Y
    tft.drawLine(i + x, y - 2 + graphHeight, i + x, y + 2 + graphHeight, YELLOW);
    tft.setCursor(i + x, y + 10 + graphHeight);
    tft.print(int(timeInterval * (STORAGEDEPTH - i)));
    delay(0);
  }
}

float mapFloat(float val, float inMin, float inMax, float outMin, float outMax) { //mapping floating values
  return (float)(val - inMin) * (outMax - outMin) / (float)(inMax - inMin) + outMin;
}

int renderMenu() {
  int counter = 0;
  tft.setTextSize(1);
  tft.setFont(&FreeSerif9pt7b);
  setAnalogMUX(MUX_JOY);
  tft.fillScreen(BLACK);
  while (true) {
    if (getJoystick() == 2) {
      counter++;
      while (getJoystick() == 2) delay(0);
    } else if (getJoystick() == 1) {
      counter--;
      while (getJoystick() == 1) delay(0);
    }
    if (counter > 7) counter = 0;
    else if (counter < 0) counter = 7;

    tft.fillScreen(BLACK);
    tft.setTextColor(WHITE);
    tft.setCursor(0, 0);
    tft.print("");

    for (int i = 0; i <= 7; i++) {
      if (counter == i) {
        tft.fillRect(0, 16 * i, 128, 16, WHITE);
        tft.setTextColor(BLACK);
      } else tft.setTextColor(WHITE);
      tft.setCursor(5, 16 * i + 12);
      tft.print(menus[i]);
    }

    tft.writeFramebuffer();
    delay(0);

    if (getJoystick() == 5) {
      tft.fillScreen(BLACK);
      tft.setCursor(5, 16 * counter + 12);
      tft.print(menus[counter]);
      tft.writeFramebuffer();
      delay(500);
      tft.fillScreen(BLACK);
      return counter;
    }
  }
}

int getJoystick() {

  uint16_t adc = analogRead(A0);

  if (adc < UP + OFFSET && adc > UP - OFFSET)             return 1;
  else if (adc < DOWN + OFFSET && adc > DOWN - OFFSET)    return 2;
  else if (adc < RIGHT + OFFSET && adc > RIGHT - OFFSET)  return 3;
  else if (adc < LEFT + OFFSET && adc > LEFT - OFFSET)    return 4;
  if (digitalRead(GPIO_BOOT) == 1) return 5;
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

float getLDRVoltage() {
  float ldr = getLDRLvl();

  float ldrVolt = (-9.4300168971096241 * pow(ldr, 0)
                   + 1.0877899879077804 * pow(ldr, 1)
                   + -0.00019748711244579100 * pow(ldr, 2)
                   + 0.00000013832688622212447 * pow(ldr, 3)) / 1000 + 0.002;
  getLDRLvl();
  return ldrVolt;
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

  tft.setTextSize(1);
  tft.setCursor(25, 48);
  tft.print("GPN17");

  tft.writeFramebuffer();
  setGPIO(LCD_LED, HIGH);

  pixels.clear(); //clear the WS2813 another time, in case they catched up some noise
  pixels.show();
}

