// This code is a combination of the firing code and the communication code from the I2C Tests. This will feature triggering the example whenever a command is given.
#include <AccelStepper.h>
#include <Servo.h>
#include <Wire.h>
#include <AS5600.h>
#include <Ramp.h>

//Definitions
#define motorInterfaceType 1

//Stepper pins
const int ci_magStepPin = 2;
const int ci_magDirPin = A2;

//Servo Pins
const int ci_servoPin = 10;

//Motor Pins
const int ci_motorPin = 5;

//Declare objects of AccelStepper to handle pan and tilt axes, and magazine rotation
AccelStepper magStepper (motorInterfaceType, ci_magStepPin, ci_magDirPin);

//Declare Servo objects
Servo magServo;

//Declare RAMP objects
ramp motorRamp;

//Declare AS5600 Magentic Position Sensor
AMS_5600 magSensor;

//Const Variables
const int ci_dartPos[12] = { 250 , 568, 938, 1275, 1609, 1973, 2293, 2616, 2949, 3332, 3668, 3991 };
const byte cb_servoIn = 130;
const byte cb_servoOut = 10;
const byte cb_vMaxPWM = 30;
const int ci_motorSpinUpPeriod = 5000;
const int ci_maxMagSpeed = 100;

//Command received
String received_str = "";

//Integer variables
int toSend, x, y, ammo;

//boolean triggers
bool flag, trigger, movement = false;

void setup()
{
  Wire.begin(0x8);
  Serial.begin(115200);
  Serial.println(">>>>>>>>>>>>>>>>>>>");
  Wire.begin();

  //Firing Motor Setup
  digitalWrite(ci_motorPin, 0);
   
  //Output pins
  pinMode(ci_magStepPin, OUTPUT);
  pinMode(ci_magDirPin, OUTPUT);
  pinMode(ci_motorPin, OUTPUT);

  //Servo Pin Setup
  magServo.attach(ci_servoPin);
  magServo.write(cb_servoOut);

  //Stepper Setups
  magStepper.setMaxSpeed(ci_maxMagSpeed);

  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  //Example Sketch
  //Homes magazine, spins up motors, fires each dart, spinds down motors.
  /*
  home_mag();
  spinUp();
  for ( byte d = 0; d < 12; d++ ) {
    goToDart(d);
    fire();
  }
  spinDown();
  */
  toSend = 1;
  delay(3000);
  flag = true;
}

void loop()
{
  
} //End of 'loop'


void home_mag() //Contains the function for homing the magazine to dart zero
{
   goToDart(0);
}

void goToDart(byte dart) //Contains the function for selecting a dart
{
  while(1)
  {
    delay(30);
    int i_tmp = magSensor.getRawAngle();
    int i_d = ci_dartPos[dart] - i_tmp;
    if ( i_tmp > (ci_dartPos[dart] - 3) && i_tmp < (ci_dartPos[dart] + 3)) {
      return 0;
    }
    else if ( i_tmp > ci_dartPos[dart]) {
      magStepper.setSpeed(50); 
      magStepper.runSpeed();
    }
    else {
      magStepper.setSpeed(-50); 
      magStepper.runSpeed();
    }
  }
}

void spinUp() //Contains the function for spinning up the firing motors
{
  motorRamp.go(0);
  motorRamp.go(cb_vMaxPWM, ci_motorSpinUpPeriod);
  while(motorRamp.update() != cb_vMaxPWM)
  {
    analogWrite(ci_motorPin, motorRamp.update());
  }
}

void spinDown() //Contains the function for spinning down the firing motors
{
  analogWrite(ci_motorPin, 0);
}

void fire() //Contains the function to fire the selected dart
{
  magServo.write(cb_servoIn);
  delay(400);
  magServo.write(cb_servoOut);
  delay(400);
}







// Function that executes whenever data is received from master
void receiveEvent(int howMany) {
  while (Wire.available()) { // loop through all but the last
    // receive byte as a character, char converts unicode to character and add these characters into string
    received_str += (char)Wire.read();
  }
}

// Function that sends data to Master (Pi). Unlike receiveEvent, the Wire.available confirmation loop cannot be used as it stops data being sent.
void requestEvent() {
  Wire.write(toSend);
}

// Function that deals with status codes to control Arduino
void statusCodes() {
  //Static keyword used to create variable for only one function. This, unlike other variables, keeps its value after function calls, allowing it to
  // be compared with newer versions of values to make comparisons with them. This is used in this code to avoid the servo moving over to the same position again
  // even though it is receiving the same coordinates.
  static int x_old = 0;
  static int y_old = 0;

  if (received_str == "1") {
    Serial.println("Raspberry Pi - Connection Established");
  } else if (received_str == "2") {
    Serial.println("Raspberry Pi - Data Received");
  } else if (received_str == "100") {
    Serial.println("Raspberry Pi - Status OK");
  } else if (received_str[0] == '(' ) {
    Serial.println("Coordinates Received, Ready to Delete Target!");
    //sscanf is used here to read formatted input from a string, then assigns the values to be read onto integer varianbles.
    // in this case, they are set to coordinate "x" and "y" variables. 
    int r = sscanf(received_str.c_str(), "(%d, %d)", &x, &y);
    Serial.print("X: ");
    Serial.println(x);
    Serial.print("Y: ");
    Serial.println(y);
    // Below conditional statement checks if the current coordinate variables are equal to its previous versions stored on the static int variables.
    // In the case it is, the movement flag is set to false, blocking movement from occuring as no change has occured.
    // However, in the case that it is not the same as its previous version, movement is set to true, then the coordinate variables are stored in the static int variables,
    // so as to be compared to the next values sent.
    if (x == x_old && y == y_old) {
      movement = false;
    } else {
      movement = true;
      x_old = x;
      y_old = y;
    }
  }
}

//The Below function works on blocking strings/characters from appearing except brackets and numbers.
void convertToInt() {
  int n = received_str.toInt();
  if (n > 0 || received_str[0] == '(') {
    //Serial.println(received_str);
    received_str = "";
  } else {
    received_str = "";
  }
}

/* The below function handles target detection.
 * It first maps the values from 0 to 1023(current limit) to 0 to 180. This is done due the servo being limited at ~180 degrees.
 * While it is true that it could instead be done from -90 to 90, doing this does not work, as it limits the servo for some reason into just 90 degrees.
 * The conditional statement within the function first checks if movement is true. As you remember, movement was the flag within the 
 * statusCodes function to check if the values being received were the same or were different. If they were different, it would set "movement" to true.
 * Thus, the below statement checks if this flag is true, then writes on to both servo "x" and "y" the values that were sent over I2C. 
 * After it has finished writing the values, it sets movement to false to block any further movements.
 */
void onDetectTarget() {
  x = map(x, 0, 1023, 0, 180);
  y = map(y, 0, 1023, 0, 180);
  if (movement == true) {
    myservo.write(x);
    myservo2.write(y);
    /* Responsible for handling shooting
    if (ammo > 3) {
      trigger = true;
      //start shooting
    } else {
      trigger = false;
    }
    */
    delay(15);
    movement = false;
  }
}

//Functions from above all being run in loop()
void loop() {
  delay(100);
  toSend = 100;
  statusCodes();
  convertToInt();
  onDetectTarget();
}