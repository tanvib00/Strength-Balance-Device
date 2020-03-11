
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Queue.h"
#include "HX711.h"

#define DOUT  3 // Load cell pin 1
#define CLK  2 // Load cell pin 2
#define BUTX 8
#define BUTR 9
#define BUTL 10
#define CALIBRATION_FACTOR -7050
#define WINDOW_SIZE 5
#define AVERAGES_SIZE 5

LiquidCrystal_I2C screen(0x27, 20, 4);

HX711 scale;


const char LEFT = 0;
const char RIGHT = 1;

float calibrationFactor = -7050; 
//calibration factor used by HX711 library to do some sort of data filtering
unsigned long startMillis;
unsigned long timerMillis;
unsigned long currentMillis;
//const unsigned long period = 11000;
int exerciseTimer = 0;
int rightPressed = 1;
int leftPressed = 1;
int clearPressed = 1;
bool exerciseMode = false;
int leftMax = 0;
int rightMax = 0;
bool leftActivated = false;
bool rightActivated = false;
bool leftInUse = false;
bool rightInUse = false;

Queue<int> window = Queue<int>(WINDOW_SIZE);
int windowSum = 0;
Queue<float> averages = Queue<float>(AVERAGES_SIZE);
float averagesSum = 0;
String oldSlope = "flat";
int repLeftCount = 0;
int repLeftWeightSum = 0;
int repRightCount = 0;
int repRightWeightSum = 0;

//---  HELPERS  ---

void initLoadCell()
{
  scale.begin(DOUT, CLK);
  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare(); //Reset the scale to 0
}

void homeScreen() {
  screen.home();
  screen.clear();
  screen.print("Press L/R to begin");
  screen.setCursor(0, 1);
  screen.print("and end exercises.");
  screen.setCursor(0, 2);
  screen.print("Press X for results.");
  screen.display();
}

void beginExDisp() {
  for (int i = 5; i > 0; i--) {
    screen.home();
    screen.clear();
    screen.print("Start in ");
    screen.print(i);
    screen.print(" seconds");
    screen.display();
    delay(1000);
  }
  screen.clear();
  screen.print("Go!");
  delay(500);
  screen.display();
  startMillis = millis();
  exerciseTimer = 0;
}

void handleButtonSide(char side) {
  beginExDisp();
  if (side == LEFT)
  {
    leftActivated = true;
    leftInUse = true;
    rightInUse = false;
  }
  else
  {
    rightActivated = true;
    leftInUse = false;
    rightInUse = true;
  }
}

void handleButtonClear() {
  // if both the left and right buttons were pressed during 
  // the last exercise set
  screen.clear();
  if (leftActivated && rightActivated) 
  {
    screen.print("Left Max: ");
    screen.print(leftMax);
    screen.setCursor(0, 1);
    screen.print("Reps: ");
    screen.print(repLeftCount);
    screen.setCursor(0, 2);
    screen.print("Avg Weight: ");
    float leftAvg = (float)repLeftWeightSum / (float)repLeftCount;
    screen.print(leftAvg);
    delay(3000);
    screen.clear();
    screen.print("Right Max: ");
    screen.print(rightMax);
    screen.setCursor(0, 1);
    screen.print("Reps: ");
    screen.print(repRightCount);
    screen.setCursor(0, 2);
    screen.print("Avg Weight: ");
    float rightAvg = (float)repRightWeightSum / (float)repRightCount;
    screen.print(rightAvg);
    delay(3000);
    if (!(leftAvg == 0)) {
      screen.clear();
      //float ratio = rightAvg / leftAvg;
      //above: not a very clear representation of data
      screen.print("Right : Left = ");
      screen.setCursor(0, 1);
      screen.print(rightAvg);
      screen.print(" : ");
      screen.print(leftAvg);
      //screen.print("%");
      delay(3000);
    }
  }
  else if (leftActivated)
  {
    screen.print("Left Max: ");
    screen.print(leftMax);
    screen.setCursor(0, 1);
    screen.print("Reps: ");
    screen.print(repLeftCount);
    screen.setCursor(0, 2);
    screen.print("Avg Weight: ");
    float leftAvg = (float)repLeftWeightSum / (float)repLeftCount;
    Serial.print(leftAvg);
    screen.print(leftAvg);
    delay(3000);
  }
  else if (rightActivated) 
  {
    screen.print("Right Max: ");
    screen.print(rightMax);
    screen.setCursor(0, 1);
    screen.print("Reps: ");
    screen.print(repRightCount);
    screen.setCursor(0, 2);
    screen.print("Avg Weight: ");
    float rightAvg = (float)repRightWeightSum / (float)repRightCount;
    Serial.print(rightAvg);
    screen.print(rightAvg);
    delay(3000);
  }
  else
  {
    screen.print("No reps counted!");
    delay(500);
  }

  // reset globals
  leftMax = 0.0;
  rightMax = 0.0;
  leftActivated = false;
  rightActivated = false;
  leftInUse = false;
  rightInUse = false;
  homeScreen();
  initLoadCell();
  repLeftCount = 0;
  repLeftWeightSum = 0;
  repRightCount = 0;
  repRightWeightSum = 0;
  Serial.println("reset reps");
  
}

//---  MAIN CODE  ---

void setup() {
  Serial.begin(9600);
  pinMode(BUTR, INPUT_PULLUP);
  pinMode(BUTL, INPUT_PULLUP);
  pinMode(BUTX, INPUT_PULLUP);
  
  //initLoadCell();
  screen.init();
  screen.backlight();
  screen.print("Welcome, Gary!");
  delay(1000);
  homeScreen();

  scale.begin(DOUT, CLK);
  scale.set_scale(CALIBRATION_FACTOR);
  scale.tare();
  
  startMillis = millis();
  timerMillis = millis();
}


void loop() {
  screen.home();
  scale.set_scale(calibrationFactor);
  
  currentMillis = millis();

  rightPressed = !digitalRead(BUTR);
  leftPressed = !digitalRead(BUTL);
  clearPressed = !digitalRead(BUTX);

  if (clearPressed)
  {
    exerciseMode = false;
    handleButtonClear();
  }
  else if (leftPressed)
  {
    exerciseMode = true;
    handleButtonSide(LEFT);
  }
  else if (rightPressed)
  {
    exerciseMode = true;
    handleButtonSide(RIGHT);
  }
  
  // figure out average from window
  int newReading = abs(scale.get_units()); // why is everything here on down not a helper function it runs always why
  //Serial.print("start of loop: ");
  //Serial.println(newReading);
  window.push(newReading);

  float newAverage;
  if(window.count() >= WINDOW_SIZE)
  {
    int oldReading = window.pop();
    windowSum += newReading;
    windowSum -= oldReading;
    newAverage = float(windowSum) / WINDOW_SIZE;
  }
  else
  {
    windowSum += newReading;
    newAverage = float(windowSum) / window.count();
  }

  // figure out up/down from average
  averages.push(newAverage);

  float oldTotal = averagesSum;
  float newTotal;
  
  if(averages.count() >= AVERAGES_SIZE)
  {
    float oldAverage = averages.pop();
    averagesSum += newAverage;
    averagesSum -= oldAverage;
    newTotal = averagesSum;
  }
  else
  {
    averagesSum += newAverage;
    newTotal = averagesSum;
  }

  // decide if up/down, adjust vars
  float difference = newTotal - oldTotal;
  String newSlope = "";
  if (difference <= 0.02 && difference >= -0.02)
  {
    newSlope = "flat";
  }
  else if(difference >= 0.02)
  {
    newSlope = "positive";
  }
  else if(difference <= -0.02)
  {
    newSlope = "negative";
  }

  // decide if a repitition was performed
  if (newSlope == "negative" && oldSlope == "positive")
  {
    if (leftInUse)
    {
      repLeftCount++;
      repLeftWeightSum += newReading;
      Serial.println("left side reading: ");
      Serial.print(newReading);
    }
    else if(rightInUse)
    {
      repRightCount++;
      repRightWeightSum += newReading;
      Serial.println("Right side reading: ");
      Serial.print(newReading);
    }
  }
  oldSlope = newSlope;

  // update maximum reading

  if (leftInUse && (leftMax < newReading))
  {
    leftMax = newReading;
  }
  else if(rightInUse && (rightMax < newReading))
  {
    rightMax = newReading;
  }
  if (exerciseMode) {
    if (currentMillis - timerMillis >= 1000) { // keep track of time passed while exercising
      exerciseTimer++;
      screen.clear();
      screen.print("Time: ");
      screen.print(exerciseTimer);
      screen.print(" seconds");
      screen.setCursor(0,1);
      screen.print("Working ");
      if (leftInUse) { screen.print("left "); }
      else { screen.print("right "); }
      screen.print("side.");
      screen.display();
      timerMillis = millis();
    }
  }
}
