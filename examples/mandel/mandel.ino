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
extern "C" {
#include "user_interface.h"
}
#include <FS.h>
#include "rboot.h"
#include "rboot-api.h"

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

#define UP      790
#define DOWN    630
#define RIGHT   530
#define LEFT    1024
#define OFFSET  30

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

#define COLORMAPSIZE 32
int joystick = 0; //Jostick register
bool isDirty = 1; //redraw if is dirty
int resolution = 1; //rendering resolution, scale down for faster moving
double reMin = -2.2;  //variables for the fractal itself
double imMin = -1.5;
double reMax = 0.8;
double imMax = 1.5;
int iterations = COLORMAPSIZE; //number of iterations
int menu = 1; //menucounter
int screen = 1; //screencounter
//colormaps
int dawn[32] = {0xFFF7, 0xFFF6, 0xFFF5, 0xFFF4, 0xFFF3, 0xFFF1, 0xFFF1, 0xFFEF, 0xFFAF, 0xFF2F, 0xFEAF, 0xFE2F, 0xFDAF, 0xFD2F, 0xFCAF, 0xFC2F, 0xEBAF, 0xDB2F, 0xD2AF, 0xC22F, 0xB1AF, 0x992F, 0x90AF, 0x802F, 0x700F, 0x600F, 0x500F, 0x400F, 0x300F, 0x200F, 0x100F, 0x000F};
int fire[32] = {0xFFFE, 0xFFFC, 0xFFFA, 0xFFF8, 0xFFF6, 0xFFF4, 0xFFF2, 0xFFF0, 0xFFAE, 0xFF2C, 0xFEAB, 0xFE28, 0xFDA6, 0xFD45, 0xFCA3, 0xFC21, 0xF3A0, 0xE320, 0xD2A0, 0xC220, 0xB1A0, 0xA120, 0x90A0, 0x8020, 0x7000, 0x6000, 0x5000, 0x4000, 0x3000, 0x2000, 0x1000, 0x0000};
int redblue[32] = {0x5800, 0x6800, 0x7000, 0x8000, 0x8800, 0x9800, 0xA000, 0xB000, 0xB841, 0xC165, 0xCA48, 0xD34D, 0xDC71, 0xE575, 0xEE99, 0xF75C, 0xE75E, 0xC65D, 0xA53C, 0x8C7B, 0x6B5A, 0x4259, 0x2138, 0x0858, 0x0016, 0x0015, 0x0013, 0x0012, 0x0010, 0x000E, 0x000D, 0x000B};
int kryptonite[32] = {0xFFFE, 0xFFFC, 0xFFFA, 0xFFF8, 0xFFF6, 0xFFF4, 0xFFF2, 0xFFF0, 0xF7EE, 0xE7EC, 0xD7EA, 0xC7E8, 0xB7E6, 0xA7E4, 0x97E2, 0x87E1, 0x77A0, 0x6700, 0x5680, 0x4620, 0x35A0, 0x2520, 0x14A0, 0x0420, 0x03A0, 0x0300, 0x02A0, 0x0220, 0x0180, 0x0100, 0x00A0, 0x0000};
int gray[32] = {0xF7DE, 0xEF9D, 0xE75C, 0xDF1B, 0xD6DA, 0xCE99, 0xC658, 0xBE17, 0xB5D6, 0xAD95, 0xA554, 0x9CF3, 0x94D2, 0x8C71, 0x8430, 0x7C0F, 0x73AE, 0x6B6D, 0x632C, 0x5B0B, 0x52CA, 0x4A89, 0x4248, 0x4208, 0x39C7, 0x3186, 0x2945, 0x2104, 0x18C3, 0x1082, 0x0841, 0x0000};
int bone[32] = {0x0000, 0x0000, 0x0863, 0x0863, 0x18E5, 0x18E5, 0x2968, 0x2968, 0x39CA, 0x39CA, 0x424C, 0x424C, 0x52EE, 0x52EE, 0x6390, 0x6390, 0x7432, 0x7432, 0x84D4, 0x84D4, 0x9576, 0x9576, 0x9E17, 0x9E17, 0xB679, 0xB679, 0xCEFB, 0xCEFB, 0xE77D, 0xE77D, 0xFFFF, 0xFFFF};
int parula[32] = {0x3150, 0x3150, 0x2A36, 0x2A36, 0x031B, 0x031B, 0x0BBA, 0x0BBA, 0x1439, 0x1439, 0x04D9, 0x04D9, 0x0557, 0x0557, 0x1D95, 0x1D95, 0x45D1, 0x45D1, 0x75EF, 0x75EF, 0xA5CD, 0xA5CD, 0xC5CB, 0xC5CB, 0xE5A9, 0xE5A9, 0xF606, 0xF606, 0xEEC4, 0xEEC4, 0xF7C1, 0xF7C1};
int lines[32] = {0x0396, 0x0396, 0xD283, 0xD283, 0xE563, 0xE563, 0x7971, 0x7971, 0x7545, 0x7545, 0x4DDC, 0x4DDC, 0x9885, 0x9885, 0x0396, 0x0396, 0xD283, 0xD283, 0xE563, 0xE563, 0x7971, 0x7971, 0x7545, 0x7545, 0x4DDC, 0x4DDC, 0x9885, 0x9885, 0x0396, 0x0396, 0xD283, 0xD283};
int jet[32] = {0x0017, 0x0017, 0x001F, 0x001F, 0x01FF, 0x01FF, 0x03FF, 0x03FF, 0x05FF, 0x05FF, 0x07FF, 0x07FF, 0x3FF7, 0x3FF7, 0x7FEF, 0x7FEF, 0xBFE7, 0xBFE7, 0xFFE0, 0xFFE0, 0xFDE0, 0xFDE0, 0xFBE0, 0xFBE0, 0xF9E0, 0xF9E0, 0xF800, 0xF800, 0xB800, 0xB800, 0x7800, 0x7800};
int cool[32] = {0x07FF, 0x07FF, 0x175F, 0x175F, 0x26DF, 0x26DF, 0x365F, 0x365F, 0x45DF, 0x45DF, 0x555F, 0x555F, 0x64BF, 0x64BF, 0x743F, 0x743F, 0x83BF, 0x83BF, 0x933F, 0x933F, 0xA2BF, 0xA2BF, 0xB21F, 0xB21F, 0xC19F, 0xC19F, 0xD11F, 0xD11F, 0xE09F, 0xE09F, 0xF81F, 0xF81F};
int hist[COLORMAPSIZE];//initital colormap
int iconcolor = 0xFFFF; //textcolor
int renderTime = 0; //holds the time of the last rendering
int palette = 0;  //counter for the palette

void setup() {
  initBadge();
  tft.setTextSize(1);
  tft.setTextColor(iconcolor);
  generatePalette(5);
  rboot_config rboot_config = rboot_get_config();
  SPIFFS.begin();
  File f = SPIFFS.open("/rom"+String(rboot_config.current_rom),"w");
  f.println("Mandelbrot\n");
}

void loop() {

  joystick = getJoystick();

  if (joystick == 5) {
    menu++;
    menu = menu % 2;
    isDirty = 1;
    int buttonCounter = 0;
    while (getJoystick() == 5) {
      delay(1);
      buttonCounter++;
      if (buttonCounter >= 1000) {
        screen++;
        screen = screen % 2;
        menu = 0;
        setGPIO(VIBRATOR, 1);
        delay(60);
        setGPIO(VIBRATOR, 0);
        while (getJoystick() == 5) delay(0);
      }
    }
  }

  if (screen == 0) {
    if (menu == 0)  renderInfoScreen();
    else if (menu == 1) setPalette();
  }
  else if (screen == 1) moveMandel();

  if (isDirty) {
    if (screen == 1) {
      startTime = millis();
      tft.drawCircle(16, 6, 3, iconcolor); //draw Clock overlay
      tft.drawLine(16, 6, 16, 4, iconcolor);
      tft.drawLine(16, 6, 18, 4, iconcolor);
      tft.writeFramebuffer();

      renderMandel(reMin, imMin, reMax, imMax, 10000, 129, 129, iterations, resolution); //calculate and draw mandelbrot

      if (menu == 1) {
        tft.fillCircle(8, 4, 2, iconcolor);  //draw magnifing glasses
        tft.drawLine(4, 8, 8, 4, iconcolor);
      } else if (menu == 0) {
        tft.drawLine(4, 6, 8, 6, iconcolor); //draw lines
        tft.drawLine(6, 4, 6, 8, iconcolor);
      }
      endTime = millis();
      renderTime = (endTime - startTime);
    }
    tft.writeFramebuffer();
  }
}

void renderMandel(double re_min, double im_min, double re_max, double im_max, double max_betrag_2, int xpixels, int ypixels, int max_iter, int res) {
  int iterationen;
  for (int y = 0; y < ypixels - 1; y += res) {
    double c_im = im_min + (im_max - im_min) * y / ypixels;
    for (int x = 0; x < xpixels - 1; x += res) {
      double c_re = re_min + (re_max - re_min) * x / xpixels;

      iterationen = calcMandel(c_re, c_im, c_re, c_im, max_betrag_2, max_iter);
      if (iterationen < max_iter) {
        if (res == 1) tft.drawPixel(x, y, hist[iterationen % COLORMAPSIZE]);
        else tft.fillRect(x, y, res, res, hist[iterationen % COLORMAPSIZE]);
      } else {
        if (res == 1) tft.drawPixel(x, y, 0x0000);
        else tft.fillRect(x, y, res, res, 0x0000);
      }
      delay(0);
    }
  }
  isDirty = 0;
}

int calcMandel(double x, double y, double xadd, double yadd, double max_betrag_2, int max_iter) {
  int remain_iter = max_iter;
  double xx = x * x;
  double yy = y * y;
  double xy = x * y;
  double betrag_2 = xx + yy;

  while ((betrag_2 <= max_betrag_2) && (remain_iter > 0)) {
    remain_iter -= 1;
    x  = xx - yy + xadd;
    y  = xy + xy + yadd;
    xx = x * x;
    yy = y * y;
    xy = x * y;
    betrag_2 = xx + yy;
  }
  return max_iter - remain_iter;
}

void moveMandel() {
  if (menu == 0) {
    float addValue = abs(reMin - reMax) / 16;
    if (joystick == 3) {
      reMin += addValue;
      reMax += addValue;
      isDirty = 1;
    } else if (joystick == 4) {
      reMin -= addValue;
      reMax -= addValue;
      isDirty = 1;
    } else if (joystick == 2) {
      imMin += addValue;
      imMax += addValue;
      isDirty = 1;
    } else if (joystick == 1) {
      imMin -= addValue;
      imMax -= addValue;
      isDirty = 1;
    }
  } else if (menu == 1) {
    float addValuere = abs(reMin - reMax) * 0.05;
    float addValueim = abs(imMin - imMax) * 0.05;
    if (joystick == 1) {
      reMin += addValuere;
      imMin += addValueim;
      reMax -= addValuere;
      imMax -= addValueim;
      iterations += 2;
      isDirty = 1;
    } else if (joystick == 2) {
      reMin -= addValuere;
      imMin -= addValueim;
      reMax += addValuere;
      imMax += addValueim;
      iterations -= 2;
      isDirty = 1;
    } else if (joystick == 3) {
      resolution *= 2;
      isDirty = 1;
      if (resolution > 8) {
        resolution = 8;
        isDirty = 0;
      }
    } else if (joystick == 4) {
      resolution /= 2;
      isDirty = 1;
      if (resolution < 1) {
        resolution = 1;
        isDirty = 0;
      }
    }
  }
}

void setPalette() {
  tft.fillScreen(0x0000);
  tft.setCursor(40, 2);
  tft.println("Settings");
  if (joystick == 3) palette++;
  if (joystick == 4) palette--;
  if (palette > 9) palette = 0;
  if (palette < 0) palette = 9;
  generatePalette(palette);
  while (getJoystick() > 0) delay(0);
  tft.setCursor(30, 12);
  tft.print("Palette = ");
  tft.println(palette);

  for (int i = 0; i < COLORMAPSIZE; i++) {
    tft.fillRect(i * (128.0 / COLORMAPSIZE), 30,  (128.0 / COLORMAPSIZE), 50, hist[i]);
  }

  tft.writeFramebuffer();
}

void renderInfoScreen() {
  tft.fillScreen(0x0000); //draw stats
  tft.setCursor(50, 2);
  tft.print("Info");
  tft.setCursor(0, 16);
  tft.println("X-Position:");
  tft.setCursor(0, 26);
  tft.println(reMin + (abs(reMin - reMax) / 2.0), 18);
  tft.setCursor(0, 36);
  tft.println("Y-Position:");
  tft.setCursor(0, 46);
  tft.println(imMin + (abs(imMin - imMax) / 2.0), 18);
  tft.setCursor(0, 56);
  tft.println("Window size:");
  tft.setCursor(0, 66);
  tft.println(sqrt(pow(abs(imMin - imMax), 2) + pow(abs(reMin - reMax), 2)), 18);
  tft.println();
  tft.print("Iterations:");
  tft.println(iterations);
  tft.print("Resolution:");
  tft.println(resolution);
  tft.print("Render-time:");
  tft.print(renderTime);
  tft.println(" ms");
  tft.println();
  tft.print("Press Joystick");
  tft.writeFramebuffer();
}

void generatePalette(int pal) {
  switch (pal) {
    case 0:
      copyArray(dawn);
      break;
    case 1:
      copyArray(fire);
      break;
    case 2:
      copyArray(redblue);
      break;
    case 3:
      copyArray(kryptonite);
      break;
    case 4:
      copyArray(gray);
      break;
    case 5:
      copyArray(bone);
      break;
    case 6:
      copyArray(parula);
      break;
    case 7:
      copyArray(lines);
      break;
    case 8:
      copyArray(jet);
      break;
    case 9:
      copyArray(cool);
      break;
  }
}

void copyArray(int arr[COLORMAPSIZE]) {
  for (int i = 0; i <= COLORMAPSIZE; i++) {
    hist[i] = arr[i];
  }
}

void softReset() {
  int offs = 10;
  tft.setCursor(offs, 16);
  tft.setTextColor(RED);
  tft.setTextSize(2);
  tft.fillRect(offs - 1, 15, offs + 100, 19, BLACK);
  tft.drawRect(offs - 2, 14, offs + 101, 20, RED);
  tft.print("Rebooting");
  tft.writeFramebuffer();
  abort();
}

void printDouble(double val, int prec) {
  tft.print(int(val));
  tft.print(".");
  unsigned int frac;
  if (val >= 0) frac = (val - int(val)) * prec;
  else frac = (int(val) - val) * prec;
  tft.print(frac);
}

int getJoystick() {
  uint16_t adc = analogRead(A0);

  if (adc < UP + OFFSET && adc > UP - OFFSET)             return 1;
  else if (adc < DOWN + OFFSET && adc > DOWN - OFFSET)    return 2;
  else if (adc < RIGHT + OFFSET && adc > RIGHT - OFFSET)  return 3;
  else if (adc < LEFT + OFFSET && adc > LEFT - OFFSET)    return 4;
  if (digitalRead(GPIO_BOOT) == 1) return 5;
  return 0;
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

