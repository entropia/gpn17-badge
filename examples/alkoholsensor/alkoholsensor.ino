#define SAMPLECNT 128 //number of samples to take for the final measurement

float readyValue = 1.90;  // value when the Sensor is ready (debugging = 2.75, Usecase = 0.42)

double v;   // input voltage
double PPM; // PPM Value
float BAC;  //Blood-alcohol value

float BACsum[5];        // averaging
float sum;              // averaging sum
float sumlatch;         // sum storage
int samples;            // sample counter for value stabilisation
bool ready = false;     // ready flag
bool running = false;   // running flag

int analogIn = A7;

void setup() {
  pinMode(analogIn, INPUT);
  Serial.begin(115200);
}

void loop() {

  v = getVoltage();

  if (v <= readyValue) ready = true;  // Wait for the Sensor to stabilize at its "zero-point"

  if (ready) {
    PPM = calculatePPM(v);    //calculate the ppm and the bac
    BAC = calculateBAC(PPM);

    if (PPM <= 0) {   //if the ppm is below 0 skip mostly everything
      BAC = 0.0;
      PPM = 0.0;
      running = false;
    }
    else {
      running = true;
    }

    BACsum[4] = BACsum[3];    //average the input signal
    BACsum[3] = BACsum[2];
    BACsum[2] = BACsum[1];
    BACsum[1] = BACsum[0];
    BACsum[0] = BAC;
    sum = (BACsum[0] + BACsum[1] + BACsum[2] + BACsum[3] + BACsum[4]) / 5;

    if (running) {  // check if the input signal has stabilized and if so increase counter
      if ((sum * 1.002) <= BAC || (sum * 0.998) >= BAC) {
        samples = 0;  //otherwise reset the counter
      }
      samples++;  //counter increases here
      if (samples >= SAMPLECNT) { // if we reach our samplevalue, the measurement is complete and we can exit the measurement routine
        sumlatch = sum; // in sumlatch our result will be
        ready = false;
        running = false;
      }
    }
  }

  if ( ready == true) { //spit out everything \o/
    Serial.print("Voltage: ");
    Serial.print(v);
    Serial.print(" PPM: ");
    Serial.print(PPM);
    Serial.print(" BAC: ");
    Serial.print(sum);
    Serial.print(" last BAC: ");
    Serial.print(sumlatch);
    Serial.print(" samples: ");
    Serial.print(samples);
    if (ready == false) Serial.println(" Heating");
    else if (ready == true) Serial.println(" Ready");
  } else {
    Serial.print("Voltage: ");
    Serial.print(v);
    Serial.print(" sum: ");
    Serial.print(sumlatch);
    Serial.println(" Promille, Sensor is Not Ready");
  }
}

double getVoltage() {
  double voltage;   // oversampling to increase the resolution
  long analog = 0;
  for (int i = 0; i <= 63; i++) {
    analog = analog + analogRead(analogIn);
    delayMicroseconds(10);
  }
  voltage = (5.0 * (analog / 64.0)) / 1024.0;
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

