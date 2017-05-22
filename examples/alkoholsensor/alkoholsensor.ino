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

#define SAMPLECNT 16     //number of samples to take for the final measurement

float readyValue = 0.8;  // value when the Sensor is ready (debugging = 2.75, Usecase = 0.8)

double inputV;   // input voltage
double PPM; // PPM Value
float BAC;  //Blood-alcohol value

float BACsumArray[5];      // array for averaging
float BACsum;              // averaging sum
float maxBACsum;           // max sum for BAC < 0,27
float BACsumLatch;         // sum storage
float oldBACsumLatch = 1;  // compare value
float maxPPMsum;           // sum of the ppm value
int samples;            // sample counter for value stabilisation
bool measuring = false;
int r, g, b;            //colorvalues
int cr, cg, cb;
float brightness = 0.2; //brightnesscontrol
int deltaval = 10 * brightness;

void setup() {
  initBadge();

  setGPIO(MQ3_EN, HIGH);  //enable Sensor

  setAnalogMUX(MUX_ALK);  //set analogmux right

  startScreen(); // clear screen
}

void loop() {

  inputV = getVoltage();

  if (measuring == false && inputV <= readyValue) {
    measuring = true;
  } else if (measuring == true && inputV >= readyValue) {

    PPM = calculatePPM(inputV);    //calculate the ppm and the bac
    BAC = calculateBAC(PPM);

    if (PPM <= 0) {   //if the ppm is below 0 set values to 0
      BAC = 0.0;
      PPM = 0.0;
    }

    BACsumArray[4] = BACsumArray[3];    //average the input signal
    BACsumArray[3] = BACsumArray[2];
    BACsumArray[2] = BACsumArray[1];
    BACsumArray[1] = BACsumArray[0];
    BACsumArray[0] = BAC;
    BACsum = (BACsumArray[0] + BACsumArray[1] + BACsumArray[2] + BACsumArray[3] + BACsumArray[4]) / 5;

    if ((BACsum * 1.002) <= BAC || (BACsum * 0.998) >= BAC) samples = 0;  //see if the signal has stabilised
    else samples++;  // increase sample counter

    if ( maxBACsum < BACsum) maxBACsum = BACsum;  //increase max BACsum
    if ( maxPPMsum < PPM) maxPPMsum = PPM;  //increase max PPMsum

    if (samples >= SAMPLECNT) { // if we reach our samplevalue, the measurement is complete and we can exit the measurement routine
      BACsumLatch = BACsum; // move results
      maxPPMsum = PPM;
      measuring = false;
    }
  }

  if (maxBACsum > 0 && maxPPMsum > 0 &&  inputV <= readyValue) { //if the averaging didnt work use the maximal emasured value
    BACsumLatch = maxBACsum; // in BACsumLatch our result will be
    maxPPMsum = 0;  // Wait for the Sensor to stabilize at its "zero-point"
    maxBACsum = 0;  // Wait for the Sensor to stabilize at its "zero-point"
    measuring = false;
  }

  updateValues(16, inputV, PPM, BACsum, samples); // update all values in the upper screen half
  updateLEDs(measuring);  //color the leds
  updateStatus(measuring, 55); // update the status

  if (oldBACsumLatch != BACsumLatch) updateBACandPPM(BACsumLatch, maxPPMsum, 80); // update the measured values
  oldBACsumLatch = BACsumLatch; // save old value

}

void updateLEDs(bool ready) {
  b = 0;  // blus is not used, so it is static
  if (ready == false) { //set colors accordingly to the status
    r = 255;
    g = 0;
  }
  else if (ready == true) {
    r = 0;
    g = 255;
  }
  if (cr <= r) cr += deltaval;  // fade
  if (cr >= r) cr -= deltaval;
  if (cg <= g) cg += deltaval;
  if (cg >= g) cg -= deltaval;

  int fcr = int(cr * brightness); // brightness
  int fcg = int(cg * brightness);

  for (int i = 0; i < NUM_LEDS; i++) {  //set color
    pixels.setPixelColor(i, fcr, fcg, b);
  }
  pixels.show();
}

void updateValues(int offset, float v, float PPM, float BACsum, int samples) {
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setTextSize(1);
  tft.setCursor(0, offset);
  tft.print("Voltage: ");
  tft.print(v);
  tft.println("   ");
  tft.print("PPM: ");
  tft.print(PPM);
  tft.println("   ");
  tft.print("BAC: ");
  tft.print(BACsum);
  tft.println("   ");
  tft.print("samples: ");
  tft.print(samples);
  tft.println("   ");
  tft.writeFramebuffer();
}

void updateStatus(bool ready, int offset) {
  tft.setTextSize(2);
  tft.setTextColor(0x0000);
  tft.fillRect(0, offset - 1, 128, 18, 0x0000);
  // set info sign according to state
  if (ready == false) {
    tft.setCursor(25, offset);
    tft.fillRect(24, offset - 1, 84, 18, RED);
    tft.println("Heating");
  }
  else if (ready == true) {
    tft.setCursor(35, offset);
    tft.fillRect(34, offset - 1, 60, 18, GREEN);
    tft.println("Ready");
  }
  tft.writeFramebuffer();
}

void startScreen() {
  //informative text
  tft.fillScreen(0x0000);
  tft.setCursor(28, 3);
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.println("Alkoholsensor");
  tft.setTextSize(1);
  //line
  tft.drawFastHLine(0, 12, 128, 0xFFFF);
  tft.drawFastHLine(0, 1, 128, 0xFFFF);
  tft.writeFramebuffer();
}


void updateBACandPPM(float BAC, float PPM, int offset) {
  int sign_offset = offset + 2;
  int y_offset = 5;
  tft.setTextColor(0xFFFF, 0x0000);
  // print Text and BAC
  tft.setTextSize(2);
  tft.setCursor(y_offset, offset);
  tft.print("BAC ");
  tft.print(BAC);
  tft.setCursor(y_offset, offset + 20);
  tft.print("PPM ");
  tft.print(PPM);
  // promille
  tft.drawCircle(100 + y_offset, sign_offset, 2, 0xFFFF);
  tft.drawCircle(110 + y_offset, sign_offset + 9, 2, 0xFFFF);
  tft.drawCircle(116 + y_offset, sign_offset + 9, 2, 0xFFFF);
  tft.drawLine(100 + y_offset, sign_offset + 10, 110 + y_offset, offset, 0xFFFF);
  // square
  tft.drawRect(y_offset - 2, offset - 2, 123, 18, 0xFFFF);
  tft.drawRect(y_offset - 2, offset - 2 + 20, 123, 18, 0xFFFF);
  tft.writeFramebuffer();
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
