#include <I2C.h>
#include <EtherCard.h>

#include <Wire.h>
#include "PCA9685.h"

#include <Get_params_parser.h>

// Library using default Wire and default linear phase balancing scheme
PCA9685 pwmController;                  

// LIGHTING
String ledStatus = "1";
String mode = "1";

String coldWhite = "0";
String warmWhite = "0";

String red = "110";
String green = "0";
String blue = "0";


int whiteLevel = 1;
int whiteLevelLock = 5;
int whiteLevelLockMax = 45;
int whiteLevelLockMin = 2;
int coldWhiteLevelMax = 250;
int coldWhiteLevelLockMin = 5;
int span = 1500;

int testTimeOffset = 30;

String rgbDriverMode = "";

// ETHERNET
static byte myip[] = { 192,168,1,46 };

// ethernet mac address - must be unique on your network
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };

byte Ethernet::buffer[400]; // tcp/ip send and receive buffer
BufferFiller bfill;

int timeOffset = 2500;
int sensVal = 0;

// PINS are on PCA9685 I2C 16-channel PWM driver module
uint8_t whitePin = 0;
uint8_t warmWhitePin = 1;

uint8_t redPin = 2;
uint8_t greenPin = 3;
uint8_t bluePin = 4;

void setup() {
  Serial.begin(115200);
  logLn(F("====== SETUP HAD BEGAN ========"));

  // Scan only to get address ONCE !!!
  //  I2c.scan();
  
  // ETHERCARD SETUP
  if (ether.begin(sizeof Ethernet::buffer, mymac, 53) == 0) {
    logLn(F("Failed to access Ethernet controller"));
  }
  ether.staticSetup(myip);
  ether.printIp("IP:  ", ether.myip);

//  // Setup sensor for light level monitoring
//  I2c.begin();
//  
//  I2c.write(0x4A, 0x01, 0x0);
//  I2c.write(0x4A, 0x02, 0x00);

  logLn(F("====== BASIC SETUP COMPLETED ========"));
  delay(500);
  logLn(F("====== WILL SET PWM GENERATOR ========"));

  Wire.begin();                       // Wire must be started first
  Wire.setClock(400000);              // Supported baud rates are 100kHz, 400kHz, and 1000kHz
  
  pwmController.resetDevices();       // Software resets all PCA9685 devices on Wire line
  pwmController.init(B000000);        // Address pins A5-A0 set to B000000
  pwmController.setPWMFrequency(200); // Default is 200Hz, supports 24Hz to 1526Hz
  
  logLn(F("====== SET PWM GENERATOR DONE ========"));
  logLn(F("====== FULL SETUP COMPLETED ========"));
}

void loop() {
  word len = ether.packetReceive();
  
  execute_RGB_program(ledStatus.toInt(), mode.toInt(), coldWhite.toInt(), warmWhite.toInt(), red.toInt(), green.toInt(), blue.toInt());
  
  word pos = ether.packetLoop(len);
    
  // check if valid tcp data is received
  if (pos) {
    handleNewValues(pos);
  }
  
  I2c.read(0x4A, 0x3, 6);
  sensVal = I2c.receive();
  
//  Serial.print(F("---- CURRENT LIGHT LEVEL SENSOR = "));
//  Serial.println(sensVal);
}

void execute_RGB_program(int state, int mode, int ww, int cw, int red, int green, int blue) {
  logLn(F("====== execute_RGB_program STATE = "));
  logLn(String(state));
  logLn(F("====== execute_RGB_program MODE = "));
  logLn(String(mode));
  
  if(state == 1) {
    if (mode == 0) {
      // ADAPTIVE
      logLn(F("====== WILL SET AJUST ========"));
//      adjustLight();
    } else if(mode == 1) {
      // MANUAL
      logLn(F("====== WILL SET MANUAL MODE 1 ========"));
      setManualMode(ww, cw, red, green, blue);
    } else if(mode == 2) {
      // TIMED
    } else if(mode == 3) {
      logLn(F("====== WILL SET DEMO ========"));
      testRgbCCTCycle();
    }
  } else {
    logLn(F("====== WILL TURN LEDS OFF ========"));
    turnLedsOff();
  }
}

void handleNewValues(word pos) {
  logLn(F("HAR RECEIVED SOME DATA !!! "));
    
  bfill = ether.tcpOffset();

  char* data = (char *) Ethernet::buffer + pos;

  // Get your params.
  Get_params_parser *parmParser  = new Get_params_parser(data);
  
  String ledStatusTmp = parmParser->getParamValue("&1");
    
  if(isDigit(ledStatusTmp.charAt(0))) {
    String ledStatusOld = ledStatus;
  
    if (ledStatusTmp.toInt() == 3) {
      logLn(F("The ledStatusTmp was a number 3"));
      ledStatus = ledStatusOld;
    } else {
      ledStatus = ledStatusTmp;
    }
  
    if (ledStatus.toInt() == 1) {
      String modeTmp = parmParser->getParamValue("&2");     
      String coldWhiteTmp = parmParser->getParamValue("&3");
      String warmWhiteTmp = parmParser->getParamValue("&4");
      String redTmp = parmParser->getParamValue("&5");
      String greenTmp = parmParser->getParamValue("&6");
      String blueTmp = parmParser->getParamValue("&7");

      delete parmParser;
  
      if (isDigit(modeTmp.charAt(0))) {
          mode = modeTmp;
      }
      if (isDigit(coldWhiteTmp.charAt(0))) {
        coldWhite = coldWhiteTmp;
      }
      if (isDigit(warmWhiteTmp.charAt(0))) {
        warmWhite = warmWhiteTmp;
      }
      if (isDigit(redTmp.charAt(0))) {
        red = redTmp;
      }
      if (isDigit(greenTmp.charAt(0))) {
        green = greenTmp;
      }
      if (isDigit(blueTmp.charAt(0))) {
        blue = blueTmp;
      }
    } 
  
    if (ledStatus.toInt() == 0) {
      ledStatus = "0";
    }
  }
    
  word rspns = httpResponse(sensVal, ledStatus.toInt(), mode.toInt(), coldWhite.toInt(), warmWhite.toInt(), red.toInt(), green.toInt(), blue.toInt());
  ether.httpServerReply(rspns);
}

void turnLedsOff() {
  setColdWhite(0);
  setWarmWhite(0);
  setRed(0);
  setGreen(0);
  setBlue(0); 
  logLn(F("====== LEDS ARE OFF ========"));
}

void setManualMode(int ww, int cw, int red, int green, int blue) {
  setColdWhite(ww);
  setWarmWhite(cw);
  setRed(red);
  setGreen(green);
  setBlue(blue);
}

void setColor(uint8_t pin, int value ) {
  pwmController.setChannelPWM(pin, value); // Set PWM channel 0 to 255
}

void setColdWhite(int value) {
  setColor(whitePin, value);
}

void setWarmWhite(int value) {
  setColor(warmWhitePin, value);
}

void setRed(int value) {
  setColor(redPin, value);
}

void setGreen(int value) {
  setColor(greenPin, value);
}

void setBlue(int value) {
  setColor(bluePin, value);
}

int adjustLight() {
  setRed(whiteLevel);
    
  if (sensVal < whiteLevelLockMax && whiteLevel <= 240){
    whiteLevel++;
    setColdWhite(whiteLevel);
    setWarmWhite(whiteLevel);
    setRed(whiteLevel);

    if (whiteLevel > 140 && whiteLevel < 240) {
      setGreen(whiteLevel - 120);
    }
    
    delay(timeOffset);
  } else if (sensVal > whiteLevelLockMin && whiteLevel > 0){
    whiteLevel--;
    setColdWhite(whiteLevel);
    setWarmWhite(whiteLevel);
    setRed(whiteLevel);
    
    delay(timeOffset);
  }
}

static word httpResponse(int sensorValue, int state, int mode, int coldWhite, int warmWhite, int red, int green, int blue) {
  bfill = ether.tcpOffset();
  bfill.emit_p(
  PSTR(
    "HTTP/1.0 200 You got the DATA\r\n"
    "Content-Type: application/json\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "\r\n"
      "{"
      "\"version\": \"0.0.2\","
      "\"reading\": \"$D\","
      "\"state\": \"$D\","
      "\"mode\": \"$D\","
      "\"coldWhite\": \"$D\","
      "\"warmWhite\": \"$D\","
      "\"red\": \"$D\","
      "\"green\": \"$D\","
      "\"blue\": \"$D\""
      "}"
    ), 
    sensorValue, state, mode, coldWhite, warmWhite, red, green, blue);
    logLn("---- RESPONSE WAS SENT  ---- ");
    return bfill.position();
}

void logInLn(String text) {
  // Serial.print(text);
}

void logLn(String text) {
  // Serial.println(text);
}

void testRgbCCTCycle() {
  
  logLn("---- ENTERED INTO LED DEMO MODE  ---- ");
  
  setColdWhite(0);
  delay(testTimeOffset);
  setColdWhite(50);
  delay(testTimeOffset);
  setColdWhite(155);
  delay(testTimeOffset);
  setColdWhite(255);
  delay(testTimeOffset);
  setColdWhite(0);


  setWarmWhite(0);
  delay(testTimeOffset);
  setWarmWhite(50);
  delay(testTimeOffset);
  setWarmWhite(155);
  delay(testTimeOffset);
  setWarmWhite(255);
  delay(testTimeOffset);
  setWarmWhite(0);

  
  setRed(0);
  delay(testTimeOffset);
  setRed(50);
  delay(testTimeOffset);
  setRed(155);
  delay(testTimeOffset);
  setRed(255);
  delay(testTimeOffset);
  setRed(0);

  setGreen(0);
  delay(testTimeOffset);
  setGreen(50);
  delay(testTimeOffset);
  setGreen(155);
  delay(testTimeOffset);
  setGreen(255);
  delay(testTimeOffset);
  setGreen(0);

  setBlue(0);
  delay(testTimeOffset);
  setBlue(1);
  delay(testTimeOffset);
  setBlue(50);
  delay(testTimeOffset);
  setBlue(155);
  delay(testTimeOffset);
  setBlue(255);
  delay(testTimeOffset);
  setBlue(0);
}

