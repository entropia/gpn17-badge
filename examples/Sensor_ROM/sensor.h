#include <GPNBadge.hpp>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/Picopixel.h>

class Sensor {
  private:
    int color[6] = {0xF800,0x1FE0,0x401F,0xFC08,0x07FC,0xCFE0};
    float mapFloat(float val, float inMin, float inMax, float outMin, float outMax);
    void drawGraphOverlay(int graphHeight, float maxVal, float minVal, float deltaVal, float timeInterval, int x, int y);
  public:
    typedef struct arr2d;
    virtual int renderMenu(String names[],int arraySize);
    virtual void renderGraph(const arr2d* data, int graphNumber, float timeInterval,float multiplier, int x, int y);
    virtual arr2d* alloc2d(int r, int c);
    virtual void shiftData(const arr2d* data);
    virtual void renderData(const arr2d* data, int numberData,float multiplier, String prefix, String suffix);
    virtual void title(String Title);
    virtual float calculateBAC(double PPM);
    virtual double calculatePPM(double v);
};

typedef struct Sensor::arr2d{
  int r,c, **rows;
}arr2d;

double Sensor::calculatePPM(double v) {
  double PPM; // calculate the PPM value with a polynome generated from the datasheet's table
  PPM = 150.4351049 * pow(v, 5) - 2244.75988 * pow(v, 4) + 13308.5139 * pow(v, 3) - 39136.08594 * pow(v, 2) + (57082.6258 * v) - 32982.05333;
  return PPM;
}

float Sensor::calculateBAC(double PPM) {
  float BAC;  // calculate the blood alcohol value from the ppm value
  BAC = PPM / 260.0;
  return BAC;
}

void Sensor::title(String Title){
  tft.setTextSize(1);
  tft.setFont();
  int16_t x1, y1;
  uint16_t w, h;
  char titleArray[Title.length()+1];
  Title.toCharArray(titleArray,Title.length()+1);
  tft.getTextBounds(titleArray, 0, 0, &x1, &y1, &w, &h);
  Serial.println(w);
  tft.fillRect(0,0,128,8,0x0000);
  tft.setCursor(64-(w/2),0);
  tft.print(Title);
}

void Sensor::renderData(const arr2d* data, int numberData,float multiplier, String prefix, String suffix){
  tft.setTextSize(1);
  tft.setFont();
  tft.fillRect(0,8,128,32,0x0000);
  for(int i = 0; i < numberData; i++){
    tft.setTextColor(Sensor::color[i]);
    tft.setFont();
    if (i <= 1) tft.setCursor(1,10+i*8);
    else tft.setCursor(65,-6+i*8);
    if (i == 0) tft.print(prefix);
    else if(i == 1) tft.print("y=");
    else if(i == 2) tft.print("z=");
    else if(i == 3) tft.print("w=");
    tft.print(data->rows[0][i]*multiplier);
    tft.setFont(&Picopixel);
    tft.print(suffix);
  }
}

void Sensor::shiftData(const arr2d* data){
  for (int i = STORAGEDEPTH-2; i >= 0; i--) {
    for (int j = 0; j <= 3; j++) {
      data->rows[i + 1][j] = data->rows[i][j];
    }
  }
}

arr2d* Sensor::alloc2d(int r, int c){
  arr2d *Array=NULL;
  int *data, n;
  size_t size=(r*c)*sizeof(float);
  size += sizeof(float *)*(r+1);
  size += sizeof(arr2d);
  Array = (arr2d *)malloc(size);

  if (Array==NULL) return (NULL);

  Array->r=r;
  Array->c=c;
  Array->rows=(int **)(Array+1);
  data=(int *)(Array->rows+r+1);

  for(n=0;n<r;n++){
    Array->rows[n]=data;
    data+=c;
  }

  Array->rows[r]=NULL;
  return(Array);
}

void Sensor::renderGraph(const arr2d* data, int graphNumber, float timeInterval,float multiplier, int x, int y) {

  float graphHeight = 64; //graph height
  float maxVal = data->rows[0][0]*multiplier;
  float minVal = data->rows[0][0]*multiplier;


  for (int j = 0; j < graphNumber; j++) {
    for (int i = 0; i < STORAGEDEPTH; i++) if (data->rows[i][j]*multiplier > maxVal) maxVal = data->rows[i][j]*multiplier; // get min and max values of the array
    for (int i = 0; i < STORAGEDEPTH; i++) if (data->rows[i][j]*multiplier < minVal) minVal = data->rows[i][j]*multiplier;
    delay(0);
  }
  
  if (minVal == maxVal) { //extend the shown data
    maxVal++;
    minVal--;
  }

  float deltaVal = maxVal - minVal; // get the min/max delta

  Sensor::drawGraphOverlay(graphHeight, maxVal, minVal, deltaVal, timeInterval, x, y); //render the overlay

  uint16_t color;
  for (int i = 0; i < STORAGEDEPTH; i++) {  //draw the graph
    for (int j = 0; j <= graphNumber-1; j++) {
      if ( i > 0) {
        tft.drawLine((STORAGEDEPTH - i) + x, y + (graphHeight - mapFloat(data->rows[i][j]*multiplier, minVal, maxVal, 0.0, graphHeight)), (STORAGEDEPTH - (i - 1)) + x, y + (graphHeight - mapFloat(data->rows[i - 1][j]*multiplier, minVal, maxVal, 0.0, graphHeight)), Sensor::color[j]);
      } else {
        tft.drawPixel((STORAGEDEPTH - i) + x, y + (graphHeight - mapFloat(data->rows[i][j]*multiplier, minVal, maxVal, 0.0, graphHeight)), Sensor::color[j]);
      }
      delay(0);
    }
  }
}

void Sensor::drawGraphOverlay(int graphHeight, float maxVal, float minVal, float deltaVal, float timeInterval, int x, int y) {
  tft.setFont(&Picopixel);  //set to a small font and white color
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF);

  tft.fillRect(x - 20, y - 16, STORAGEDEPTH + 32, graphHeight + 29, 0x0000); //clear screen section
  tft.drawRect(x - 19, y - 15, STORAGEDEPTH + 30, graphHeight + 28, 0xFFFF); //Draw Outline

  tft.drawLine(x - 4, y + graphHeight, x + STORAGEDEPTH + 4, y + graphHeight, 0xFF80); //draw outlines
  tft.drawLine(x, y - 4, x, y + graphHeight + 4, 0xFF80);

  tft.drawLine(x - 2, y + 2 - 4, x , y - 4, 0xFF80); //Arrow X
  tft.drawLine(x + 2, y + 2 - 4, x , y - 4, 0xFF80);
  tft.drawLine(x + STORAGEDEPTH + 4, y + graphHeight, x + STORAGEDEPTH - 2 + 4, y + graphHeight + 2, 0xFF80); //Arrow Y
  tft.drawLine(x + STORAGEDEPTH + 4, y + graphHeight, x + STORAGEDEPTH - 2 + 4, y + graphHeight - 2, 0xFF80);

  float setSize = deltaVal / graphHeight;
  for (int i = 0; i <= graphHeight; i += 16) { //Axis title X
    tft.drawLine(x - 2, i + y , x + 2, i + y , 0xFF80);
    tft.setCursor(x - 16, i - 8 + y + 10);
    tft.print(setSize * (graphHeight - i) + minVal);
    delay(0);
  }

  for (int i = 0; i <= STORAGEDEPTH; i += 16) { //Axis title Y
    tft.drawLine(i + x, y - 2 + graphHeight, i + x, y + 2 + graphHeight, 0xFF80);
    tft.setCursor(i + x, y + 10 + graphHeight);
    tft.print(int(timeInterval * (STORAGEDEPTH - i)));
    delay(0);
  }
}

float Sensor::mapFloat(float val, float inMin, float inMax, float outMin, float outMax) { //mapping floating values
  return (float)(val - inMin) * (outMax - outMin) / (float)(inMax - inMin) + outMin;
}


int Sensor::renderMenu(String names[],int arraySize) {

  int counter = 0;
  int menuAddition = 0;
  int textSize = 0;

  if (arraySize <= 7) textSize = 2;
  else if (arraySize > 7) textSize = 1;

  tft.setTextSize(textSize);
  tft.setTextColor(WHITE);
  tft.setFont();
  badge.setAnalogMUX(MUX_JOY);

  while (true) {
    if (badge.getJoystickState() == JoystickState::BTN_UP) {
      counter--;
      while (badge.getJoystickState() == JoystickState::BTN_UP) delay(0);
    } else if (badge.getJoystickState() == JoystickState::BTN_DOWN) {
      counter++;
      while (badge.getJoystickState() == JoystickState::BTN_DOWN) delay(0);
    }

    if (counter > arraySize) counter = 0;
    else if (counter < 0) counter = arraySize;
    
    tft.fillScreen(BLACK);

    for (int i = 0; i <= arraySize; i++) {
      if (counter == i) {
        tft.fillRect( 0 , (textSize * 8) * i + 16, 128, (textSize * 8), WHITE);
        tft.setTextColor(BLACK);
      } else tft.setTextColor(WHITE);
      tft.setCursor(5, (textSize * 8) * i + 16);
      tft.print(names[i]);
    }
    
    tft.setTextColor(WHITE);
    tft.setCursor(43, 5);
    tft.println("Sensors");

    tft.writeFramebuffer();
    delay(0);

    if (badge.getJoystickState() == JoystickState::BTN_ENTER) {
      tft.fillScreen(BLACK);
      tft.setCursor(5,(textSize * 8) * counter + 16);
      tft.print(names[counter]);
      tft.writeFramebuffer();
      while(badge.getJoystickState() == JoystickState::BTN_ENTER) delay(0);
      tft.fillScreen(BLACK);
      return counter;
    }
  }
}
