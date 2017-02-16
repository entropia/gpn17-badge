
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <TFT_ILI9163C.h>
#include <Adafruit_NeoPixel.h>

#include <ESP8266WiFi.h>

#define VERSION 1
#define USEWIFI

const char* ssid = "MyLittleWiFi"; // Put your SSID here
const char* password = "WoonaBestPony"; // Put your PASSWORD here

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

  tft.setTextColor(RED);
  tft.setCursor(2, 2);
  tft.print("Mau!");

  #ifdef USEWIFI //connect to WiFi
  connectBadge();
  #endif

  Serial.print("Battery Level:  ");
  Serial.println(getBatLvl());
  delay(500);

  Serial.print("Light LDR Level:  ");
  Serial.println(getLDRLvl());
  delay(500);

  Serial.print("NTC Level:  ");
  Serial.println(getNTCLvl());
  delay(500);

  Serial.print("Temperature:  ");
  Serial.println(getTemp());
  delay(500);

  setGPIO(VIBRATOR, HIGH);
  delay(500);
  setGPIO(VIBRATOR, LOW);

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

  else if (adc < CENTER + OFFSET && adc > CENTER - OFFSET) {
    setGPIO(VIBRATOR, HIGH);
  }

  else {
    setGPIO(VIBRATOR, LOW);
    pixels.clear();
  }

  pixels.show();
  delay(50);
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

uint16_t getBatLvl() {
  setAnalogMUX(MUX_BAT);
  uint16_t avg = 0;
  for (byte i = 0; i < 10; i++) {
    avg += analogRead(A0);
  }
  return (avg / 10);
}

uint16_t getLDRLvl() {
  setAnalogMUX(MUX_LDR);
  uint16_t avg = 0;
  for (byte i = 0; i < 10; i++) {
    avg += analogRead(A0);
  }
  return avg / 10;
}

uint16_t getNTCLvl() {
  setAnalogMUX(MUX_NTC);
  uint16_t avg = 0;
  for (byte i = 0; i < 10; i++) {
    avg += analogRead(A0);
  }
  return (avg / 10);
}

uint16_t getTemp() {
  //TODO
  return getNTCLvl();
}


void initBadge() { //initialize the badge

  #ifdef USEWIFI
  // Next 2 line seem to be needed to connect to wifi after Wake up
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(20);
  #endif

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

  Serial.begin(115200);

  delay(100);

  tft.begin(); //initialize the tft. This also sets up SPI to 80MHz Mode 0

  analogWrite(GPIO_LCD_BL, 1023); //switch on LCD backlight

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
