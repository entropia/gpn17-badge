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

#include <Fonts/FreeSans9pt7b.h>
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

void setup() {
  initBadge();
  tft.setTextSize(1);
  tft.setFont(&FreeSans9pt7b);
}

void loop() {
  tft.fillScreen(BLACK);
  tft.setCursor(19,16);
  tft.print("Luxmeter");
  tft.setCursor(0, 50);

  float ldrVolt = getLDRVoltage();

  float resistorVolt = 3.3 - ldrVolt;
  float ldrResistance = ldrVolt / resistorVolt * 4750;
  float lux = 12518931 * pow(ldrResistance, -1.405);

  tft.print("V=");tft.print(ldrVolt); tft.println("V");
  tft.print("R=");tft.print(ldrResistance,1); tft.println("Ohm");
  tft.print("lx=");tft.print(lux,1); tft.println("lx");
  tft.writeFramebuffer();
  delay(1);
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
                   + 0.00000013832688622212447 * pow(ldr, 3)) / 1000;
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

  tft.setTextSize(2);
  tft.setCursor(25, 48);
  tft.print("GPN17");
  tft.setTextSize(1);

  tft.writeFramebuffer();
  setGPIO(LCD_LED, HIGH);

  pixels.clear(); //clear the WS2813 another time, in case they catched up some noise
  pixels.show();
}

