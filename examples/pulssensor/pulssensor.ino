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
#include <FS.h>
#include "rboot.h"
#include "rboot-api.h"

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

uint32_t endTime = 10;
uint32_t startTime = 0;

int _width = 128;
int _height = 128;

#define STORAGEDEPTH 96
float graphStorage[STORAGEDEPTH];     //Buffer for single graph
float graphStorage3[STORAGEDEPTH][3]; //Buffer for multi graph
int state = 0;
float analogVal = 0;
int sleepVal = 0; //sensor sleep value
int ledVal = 0;
int ledIs = 0;
int sleepCounter = 0; //sensor sleep counter
imu::Vector<3> bnoOutput;
//
#define OFFSET 16
#define SIGNAL 6
float avgOffset[OFFSET];
float avgSignal[SIGNAL];
float bpmOffset, bpmSignal;
//signal processing
bool pulse = 0;
bool pulseOld = 0;
float hysteresis = 0.00005;
float freq = 0;
float bpm = 0;

void setup() {
  initBadge();
  tft.setTextSize(1);
  tft.setFont(&FreeSerif9pt7b);
  setAnalogMUX(MUX_LDR);
  pixels.setPixelColor(1, 0, 255, 0);
  pixels.show();
  delay(100);
  pixels.show();
  
  rboot_config rboot_config = rboot_get_config();
  SPIFFS.begin();
  File f = SPIFFS.open("/rom"+String(rboot_config.current_rom),"w");
  f.println("Pulssensor\n");
}

void loop() {
  analogVal = getLDRVoltage();
  
  avgOffset[0] = analogVal; //signal will be calculated twice (side by side and then substraced)
  avgSignal[0] = analogVal;

  for (int i = OFFSET - 1; i >= 0; i--) avgOffset[i + 1] = avgOffset[i]; //shift signal
  for (int i = SIGNAL - 1; i >= 0; i--) avgSignal[i + 1] = avgSignal[i];

  bpmOffset = arraySum(avgOffset, OFFSET); //moving average for zero point
  bpmSignal = arraySum(avgSignal, SIGNAL); //moving average for signal

  graphStorage[0] = bpmOffset - bpmSignal; //remove dc offset

  if ((graphStorage[0] + hysteresis) <= 0 && pulse == 0) pulse = 1; //detect pulse
  else if ((graphStorage[0] - hysteresis) >= 0 && pulse == 1) pulse = 0;
  else;

  if (pulse == 1 && pulseOld == 0) {  //claculate bpm
    endTime = millis();
    bpm = 1.0 / (endTime - startTime) * 1000.0 * 60.0;
    startTime = millis();
  }
  pulseOld = pulse;

  for (int i = STORAGEDEPTH-2; i >= 0; i--) { //buffer for the graph
    graphStorage[i + 1] = graphStorage[i];
  }

  tft.setTextColor(WHITE);  //set Text color and fill screen
  tft.fillRect(0, 0, 128, 40, BLACK);
  tft.setCursor(39, 20);
  tft.setFont(&FreeSerif9pt7b);
  tft.print((int)bpm);  // Print bpm
  tft.print(" bpm");
  
  renderGraph(graphStorage, "V", "S", 0.25, 20, 50); //finally Render the graph
}

void renderTitle(String title) {
  tft.setFont();
  tft.setCursor(0, 15);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.print(title);
}

void renderValue(float value, String prefix, String suffix) {
  tft.setFont();
  tft.setCursor(8, 4);
  tft.setTextColor(WHITE);
  tft.setTextSize(1);
  tft.print(prefix);
  tft.print(value);
  tft.print(suffix);
}


float arraySum(float inputArray[], int arraySize) {
  double output = 0;
  float result = 0;
  for (int i = 0; i < arraySize; i++) output += inputArray[i];
  result = (output / (float(arraySize)));
  return result;
}

void renderGraph(float data[STORAGEDEPTH], String nameX, String nameY, float timeInterval, int x, int y) {

  float graphHeight = 64; //graph height
  float maxVal = data[0];
  float minVal = data[0];

  for (int i = 0; i < STORAGEDEPTH; i++) if (data[i] > maxVal) maxVal = data[i]; // get min and max values of the array
  for (int i = 0; i < STORAGEDEPTH; i++) if (data[i] < minVal) minVal = data[i];

  if (minVal == maxVal) { //extend the shown data
    maxVal++;
    minVal--;
  }
  float deltaVal = maxVal - minVal;// get the min/max delta

  drawGraphOverlay(graphHeight, maxVal*1000, minVal*1000, deltaVal*1000, nameX, nameY, timeInterval, x, y);//render the overlay

  for (int i = 0; i < STORAGEDEPTH; i++) { //draw the graph
    if ( i > 0) {
      tft.drawLine((STORAGEDEPTH - i) + x, y + (graphHeight - mapFloat(data[i], minVal, maxVal, 0.0, graphHeight)), (STORAGEDEPTH - (i - 1)) + x, y + (graphHeight - mapFloat(data[i - 1], minVal, maxVal, 0.0, graphHeight)), RED);
    } else {
      tft.drawPixel((STORAGEDEPTH - i) + x, y + (graphHeight - mapFloat(data[i], minVal, maxVal, 0.0, graphHeight)), RED);
    }
    delay(0);
  }
  tft.writeFramebuffer();
}

void drawGraphOverlay(int graphHeight, float maxVal, float minVal, float deltaVal, String nameX, String nameY, float timeInterval, int x, int y) {
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

