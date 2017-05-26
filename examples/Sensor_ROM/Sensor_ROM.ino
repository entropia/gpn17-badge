#include<GPNBadge.hpp>
#include <FS.h>
#include "rboot.h"
#include "rboot-api.h"

#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

#define BNO055_SAMPLERATE_DELAY_MS (100)

Adafruit_BNO055 bno = Adafruit_BNO055(BNO055_ID, BNO055_ADDRESS_B);

Badge badge;

#define STORAGEDEPTH 112
#include "sensor.h"

Sensor sens;

String titles[13] = {"Temperature", "Light", "Gyro", "ACC", "Compass", "Linear ACC", "Gravity", "Quaternion", "Euler", "Ain", "Battery", "Gassensor","Alcohol"};
String prefix[13] = {"T=","lx=","x=","x=","x=","x=","x=","x=","x=","V=","V=","V=","BAC="};
String suffix[13] = {"C","lx","rad/s","m/s^2","uT","m/s^2","m/s^2","deg","deg","V","V","V","BAC"};

int sensorNumber;
int NumberOfGraphs = 1;
arr2d *data=sens.alloc2d(112,4);
 
imu::Quaternion quat; 
imu::Vector<3> euler;

struct sensorData{
  float x,y,z,w;
};

sensorData dat;

void setup() {
  badge.init();
  badge.setBacklight(true);
  bno.begin();
  delay(300);
  badge.setGPIO(MQ3_EN,1);
  sensorNumber = sens.renderMenu(titles, (sizeof(titles) / sizeof(*titles)) - 1);

  rboot_config rboot_config = rboot_get_config();
  SPIFFS.begin();
  File f = SPIFFS.open("/rom"+String(rboot_config.current_rom),"w");
  f.println("Sensors\n");
}

void loop() {
  switch(sensorNumber){
  case 0:
    dat.x = bno.getTemp()*1000.0;
    NumberOfGraphs = 1;
  break;
  case 1:
    badge.setAnalogMUX(MUX_LDR);
    dat.x = badge.getLDRLvl()*1000.0;
    NumberOfGraphs = 1;
  break;
  case 2:
    euler = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
    NumberOfGraphs = 3;
    dat.x = euler.x()*1000.0;
    dat.y = euler.y()*1000.0;
    dat.z = euler.z()*1000.0;
  break;
  case 3:
    euler = bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
    NumberOfGraphs = 3;
    dat.x = euler.x()*1000.0;
    dat.y = euler.y()*1000.0;
    dat.z = euler.z()*1000.0;
  break;
  case 4:
    euler = bno.getVector(Adafruit_BNO055::VECTOR_MAGNETOMETER);
    NumberOfGraphs = 3;
    dat.x = euler.x()*1000.0;
    dat.y = euler.y()*1000.0;
    dat.z = euler.z()*1000.0;
  break;
  case 5:
    euler = bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);
    NumberOfGraphs = 3;
    dat.x = euler.x()*1000.0;
    dat.y = euler.y()*1000.0;
    dat.z = euler.z()*1000.0;
  break;
  case 6:
    euler = bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);
    NumberOfGraphs = 3;
    dat.x = euler.x()*1000.0;
    dat.y = euler.y()*1000.0;
    dat.z = euler.z()*1000.0;
  break;
  case 7:
    quat = bno.getQuat();
    NumberOfGraphs = 4;
    dat.x = quat.x()*1000.0;
    dat.y = quat.y()*1000.0;
    dat.z = quat.z()*1000.0;
    dat.w = quat.w()*1000.0;
  break;
  case 8:
    euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
    NumberOfGraphs = 3;
    dat.x = euler.x()*1000.0;
    dat.y = euler.y()*1000.0;
    dat.z = euler.z()*1000.0;
  break;
  case 9:
    badge.setAnalogMUX(MUX_IN1);
    dat.x = analogRead(A0)*0.94;
    NumberOfGraphs = 1;
  break;
  case 10:
    badge.setAnalogMUX(MUX_BAT);
    dat.x = badge.getBatVoltage();
    NumberOfGraphs = 1;
  break;
  case 11:
    badge.setAnalogMUX(MUX_ALK);
    dat.x = badge.getALKVoltage();
    NumberOfGraphs = 1;
  break;
  case 12:
    badge.setAnalogMUX(MUX_ALK);
    double PPM = sens.calculatePPM(badge.getALKVoltage()/1000);
    if (PPM <= 0) PPM = 0.0;
    dat.x = sens.calculateBAC(PPM);
    NumberOfGraphs = 1;
  break;
  }
  delay(0);

	data->rows[0][0] = dat.x;
  data->rows[0][1] = dat.y;
  data->rows[0][2] = dat.z;
  data->rows[0][3] = dat.w;

  sens.shiftData(data);
  sens.title(titles[sensorNumber]);
	sens.renderData(data,NumberOfGraphs,0.001,prefix[sensorNumber],suffix[sensorNumber]);
	sens.renderGraph(data, NumberOfGraphs, 5.0,0.001,20,50);

  tft.writeFramebuffer();

  if (badge.getJoystickState() == JoystickState::BTN_ENTER){
    while(badge.getJoystickState() == JoystickState::BTN_ENTER) delay(0);
      sensorNumber = sens.renderMenu(titles, (sizeof(titles) / sizeof(*titles)) - 1);
  }
}