
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>

#include <ESP8266WiFi.h>

#include <IRremoteESP8266.h>


#define VERSION 2
#define USEWIFI

const char* ssid = "ssid"; // Put your SSID here
const char* password = "password"; // Put your PASSWORD here

#if (VERSION == 2)
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

#define UP      730
#define DOWN    590
#define RIGHT   496
#define LEFT    970
#define OFFSET  30

#define I2C_PCA 0x25

#define NUM_LEDS    4

#define BAT_CRITICAL 3300

IRsend irsend(GPIO_DP); //an IR led is connected to GPIO pin 4 (D2)
IRrecv irrecv(GPIO_DN);
#endif

// Color definitions
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

//global variables
byte shiftConfig = 0; //stores the 74HC595 config

void setup() {
  initBadge();
  Serial.println("Mau!"); //useful debug message

  setGPIO(LCD_LED, HIGH);
  
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(2, 2);
  tft.print("Hack me!");
  tft.setTextSize(1);
  tft.setTextColor(RED);
  tft.setCursor(2, 50);
  tft.print("https://entropia.de/GPN17:Hack_the_Badge");

  delay(2000);

  #ifdef USEWIFI //connect to WiFi
  //connectBadge();
  #endif

  Serial.print("Battery Voltage:  ");
  Serial.println(getBatVoltage());
  delay(500);

  Serial.print("Light LDR Level:  ");
  Serial.println(getLDRLvl());
  delay(500);

  pixels.setPixelColor(0, pixels.Color(30, 0, 0));
  pixels.setPixelColor(1, pixels.Color(30, 0, 0));
  pixels.setPixelColor(2, pixels.Color(30, 0, 0));
  pixels.setPixelColor(3, pixels.Color(30, 0, 0));
  pixels.show();

  
  setGPIO(VIBRATOR, HIGH);
  delay(500);
  setGPIO(VIBRATOR, LOW);

  setGPIO(IR_EN, LOW);

  setAnalogMUX(MUX_JOY);
}

void loop() {
  uint16_t adc = analogRead(A0);
  Serial.println(adc);
  
  if (adc < UP + OFFSET && adc > UP - OFFSET) {
    pixels.setPixelColor(0, pixels.Color(0, 50, 50));
  }

  else if (adc < DOWN + OFFSET && adc > DOWN - OFFSET) {
    pixels.setPixelColor(3, pixels.Color(0, 50, 50));
  }
    
  else if (adc < RIGHT + OFFSET && adc > RIGHT - OFFSET) {
    pixels.setPixelColor(2, pixels.Color(0, 50, 50));
  }
    
  else if (adc < LEFT + OFFSET && adc > LEFT - OFFSET) {
    pixels.setPixelColor(1, pixels.Color(0, 50, 50));
  }

  else if (digitalRead(GPIO_BOOT) == HIGH) {
    setGPIO(VIBRATOR, HIGH);
  }
  
  else {
    setGPIO(VIBRATOR, LOW);
    pixels.clear();
  }
  pixels.show();

  //Serial.println("Scanning");
  
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
  setAnalogMUX(MUX_LDR);
  uint16_t avg = 0;
  for (byte i = 0; i < 10; i++) {
    avg += analogRead(A0);
  }
  return (avg / 10);
}


void initBadge() { //initialize the badge
  
  #ifdef USEWIFI
  // Next 2 line seem to be needed to connect to wifi after Wake up 
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(20);
  #endif

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

  irsend.begin();
  irrecv.enableIRIn();
  Serial.begin(115200);

  delay(100);

  tft.begin(); //initialize the tft. This also sets up SPI to 80MHz Mode 0
  tft.setRotation(2);
  tft.scroll(32);
  
  pixels.clear(); //clear the WS2813 another time, in case they catched up some noise
  pixels.show();
}

void connectBadge() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  tft.setCursor(2, 22);
  tft.print("Connecting");
  tft.setCursor(63, 22);
  uint8_t cnt = 0;
  while (WiFi.status() != WL_CONNECTED) {   
    Serial.print(".");
    tft.print(".");
    cnt++;
    if (cnt % 4 == 0) {
      tft.fillRect(63, 22, 30, 10, BLACK);
      tft.setCursor(63, 22);
    }
    delay(250);
  }
  Serial.println("WiFi connected");
  Serial.println("");
  Serial.println(WiFi.localIP());

  tft.setTextColor(GREEN);
  tft.setCursor(2, 42);
  tft.print("WiFi connected!");
  tft.setCursor(2, 52);
  tft.print(WiFi.localIP());
}


