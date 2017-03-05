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

#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

// Stylesheet
int textColor = WHITE;
int textColorAlt = 0x8410;
int textacceptColor = GREEN;
int textdeclineColor = RED;
int acceptColor = GREEN;
int declineColor = RED;
int bgColor = BLACK;
int fgColor = 0xC618;
int infoboxColor = 0xEF5D;

TFT_ILI9163C tft = TFT_ILI9163C(GPIO_LCD_CS, GPIO_LCD_DC);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_LEDS, GPIO_WS2813, NEO_GRB + NEO_KHZ800);

#define SAMPLECNT 32     //number of samples to take for the final measurement

float readyValue = 0.8;  // value when the Sensor is ready (debugging = 2.75, Usecase = 0.42)

double v;   // input voltage
double PPM; // PPM Value
float BAC;  //Blood-alcohol value

float BACsumArray[5];        // averaging
float BACsum;              // averaging sum
float maxBACsum;           // max sum for BAC < 0,27
float BACsumLatch;         // sum storage
float oldBACsumLatch = 1;  // compare value
float ppmLatch;         // ppm latch value
float maxPPM;           //
int samples;            // sample counter for value stabilisation
bool ready = false;     // ready flag
bool running = false;   // running flag
bool readyOld = true;   // compare flags
bool runningOld = true;
int r, g, b;            //colorvalues
int cr, cg, cb;
float brightness = 0.2; //brightnesscontrol
int deltaval = 10 * brightness;

byte shiftConfig = 0; //stores the 74HC595 config

void setup() {
  initBadge();

  setGPIO(MQ3_EN, HIGH);  //enable Sensor

  setAnalogMUX(MUX_ALK);  //set analogmux right
}

void loop() {

  v = getVoltage();

  if (v <= readyValue) ready = true;  // Wait for the Sensor to stabilize at its "zero-point"
  if (v <= readyValue) maxBACsum = 0;  // Wait for the Sensor to stabilize at its "zero-point"
  if (v <= readyValue) maxPPM = 0;  // Wait for the Sensor to stabilize at its "zero-point"

  if (ready) {
    PPM = calculatePPM(v);    //calculate the ppm and the bac
    BAC = calculateBAC(PPM);

    if (PPM <= 0) {   //if the ppm is below 0 skip mostly everything
      BAC = 0.0;
      PPM = 0.0;
      running = false;
    } else {
      running = true;
    }

    BACsumArray[4] = BACsumArray[3];    //average the input signal
    BACsumArray[3] = BACsumArray[2];
    BACsumArray[2] = BACsumArray[1];
    BACsumArray[1] = BACsumArray[0];
    BACsumArray[0] = BAC;
    BACsum = (BACsumArray[0] + BACsumArray[1] + BACsumArray[2] + BACsumArray[3] + BACsumArray[4]) / 5;

    if (running) {  // check if the input signal has stabilized and if so increase counter
      if ((BACsum * 1.002) <= BAC || (BACsum * 0.998) >= BAC) {
        samples = 0;  //otherwise reset the counter
      }
      samples++;  //counter increases here

      if ( maxBACsum < BACsum) maxBACsum = BACsum;  //increase max BACsum
      if ( maxPPM < PPM) maxPPM = PPM;  //increase max PPMsum

      if (samples >= SAMPLECNT) { // if we reach our samplevalue, the measurement is complete and we can exit the measurement routine
        BACsumLatch = BACsum; // in BACsumLatch our result will be
        maxPPM = PPM;
        ready = false;
        running = false;
      }
    }
  }

  if ( runningOld == true && running == false ) ready = false;

  if ( ready == false && running == false && maxBACsum > 0) {
    BACsumLatch = maxBACsum; // use maximum value if averaging didnt work (usually this catches if the value is below 0,27 promille)
    maxBACsum = 0;
  }
  if ( ready == false && running == false && maxPPM > 0) {
    ppmLatch = maxPPM; // use maximum value if averaging didnt work (usually this catches if the value is below 0,27 promille)
    maxPPM = 0;
  }

  updateValues(16, v, PPM, BACsum, samples); //update all values in the upper csreen half
  updateLEDs(ready, running);

  if (runningOld != running || readyOld != ready) updateStatus(ready, running, 60); // update the sensor Stats if aviable
  if (oldBACsumLatch != BACsumLatch) updateBACandPPM(BACsumLatch, PPM, 80); // update the sums

  oldBACsumLatch = BACsumLatch; // reset old values
  runningOld = running;
  readyOld = ready;

}

void updateLEDs(bool ready, bool running) {
  b = 0;  // blus is not used, so it is static
  if (ready == false && running == false) { //set colors accordingly to the status
    r = 255;
    g = 0;
  }
  else if (ready == true && running == false) {
    r = 0;
    g = 255;
  }
  else if (ready == true && running == true) {
    r = 255;
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
  tft.setTextColor(textColor, bgColor);
  tft.setTextSize(1);
  tft.setCursor(0, offset);
  tft.print("Voltage: ");
  tft.println(v);
  tft.print("PPM: ");
  tft.println(PPM);
  tft.print("BAC: ");
  tft.println(BACsum);
  tft.print("samples: ");
  tft.println(samples);
}

void updateStatus(bool ready, bool running, int offset) {
  tft.setTextSize(2);
  tft.setTextColor(bgColor);
  tft.fillRect(0, offset - 1, 128, 18, bgColor);
  // set info sign according to state
  if (ready == false && running == false) {
    tft.setCursor(25, offset);
    tft.fillRect(24, offset - 1, 84, 18, textdeclineColor);
    tft.println("Heating");
  }
  else if (ready == true && running == false) {
    tft.setCursor(35, offset);
    tft.fillRect(34, offset - 1, 60, 18, textacceptColor);
    tft.println("Ready");
  }
  else if (ready == true && running == true) {
    tft.setCursor(30, offset);
    tft.fillRect(29, offset - 1, 80, 18, YELLOW);
    tft.println("Running");
  }
}

void startScreen() {
  //informative text
  tft.setCursor(28, 3);
  tft.setTextSize(1);
  tft.setTextColor(textColor, bgColor);
  tft.println("Alkoholsensor");
  tft.setTextSize(1);
  //line
  tft.drawFastHLine(0, 12, 128, fgColor);
  tft.drawFastHLine(0, 1, 128, fgColor);
}


void updateBACandPPM(float BAC, float PPM, int offset) {
  int sign_offset = offset + 2;
  int y_offset = 5;
  tft.setTextColor(textColor, bgColor);
  // print Text and BAC
  tft.setTextSize(2);
  tft.setCursor(y_offset, offset);
  tft.print("BAC ");
  tft.print(BAC);
  tft.setCursor(y_offset, offset + 20);
  tft.print("PPM ");
  tft.print(PPM);
  // Promillezeichen
  tft.drawCircle(100 + y_offset, sign_offset, 2, fgColor);
  tft.drawCircle(110 + y_offset, sign_offset + 9, 2, fgColor);
  tft.drawCircle(116 + y_offset, sign_offset + 9, 2, fgColor);
  tft.drawLine(100 + y_offset, sign_offset + 10, 110 + y_offset, offset, fgColor);
  // square
  tft.drawRect(y_offset - 2, offset - 2, 123, 18, fgColor);
  tft.drawRect(y_offset - 2, offset - 2 + 20, 123, 18, fgColor);
}

double getVoltage() {
  double voltage;   // oversampling to increase the resolution
  long analog = 0;
  for (int i = 0; i <= 63; i++) {
    analog = analog + analogRead(A0);
    delayMicroseconds(1);
  }
  voltage = (analog / 64.0) * 0.004535-0.5;
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

  startScreen();

  setGPIO(VIBRATOR, LOW); // turn of vibrator

  analogWrite(GPIO_LCD_BL, 1023); //switch on LCD backlight

  pixels.clear(); //clear the WS2813 another time, in case they catched up some noise
  pixels.show();
}



