#include <Adafruit_NeoPixel.h>
#include <ArduinoLowPower.h>
#include <Adafruit_PN532.h>

//setup RFID board with SPI communication 
//do not modify pmw pins needed for mosi and miso
const uint8_t PN532_SCK = 2;
const uint8_t PN532_MOSI = 3;
const uint8_t PN532_SS = 4;
const uint8_t PN532_MISO = 5;
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);
uint8_t controllerUid[] = {0, 0, 0, 0, 0, 0, 0};    
uint8_t operatorUid[] = {0, 0, 0, 0, 0, 0, 0};    
boolean controllerInitiated = false;
boolean operatorInitiated = false;
boolean operatorAuthorized = false;
boolean controllerAuthorized = false;

//setup neopixel ring and create predefined colors and brightness
const uint8_t LED_PIN = 10;
const uint8_t NUM_LED = 24;
const uint8_t DEFAULT_BRIGHT = 30;
Adafruit_NeoPixel ring = Adafruit_NeoPixel(NUM_LED, LED_PIN, NEO_GRB);
const uint32_t RED = ring.Color(255, 0, 0);
const uint32_t GREEN = ring.Color(0, 255, 0);
const uint32_t BLUE = ring.Color(0 , 0, 255);
const uint32_t NONE = ring.Color(0, 0, 0); //LED off


void setup() {
  Serial.begin(9600); // set up serial port with baud rate of 9600
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  //neopixel setup
  ring.begin();
  ring.show();
  ring.setBrightness(DEFAULT_BRIGHT);

  //RFID setup
  nfc.setPassiveActivationRetries(0xFF);
  nfc.begin();
  nfc.SAMConfig(); // configure board to read RFID tags 
  
  Serial.println("Waiting for an ISO14443A Card ..."); 
}


void loop() {  
  uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0};                                                                       // Buffer to store the returned UID
  uint8_t uidLength;                                                                                           // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength,1000)) readRFID(&uid[0], uidLength); // search for RFID signal from badge
}


//Read data stored on RFID badge and print to the serial port
void readRFID(uint8_t uid[], uint8_t uidLength){
  //print data from RFID badge 
  Serial.println("Found a card!");
  Serial.print("UID Length: "); Serial.print(uidLength, DEC); Serial.println(" bytes");
  Serial.print("UID Value: ");

  for (uint8_t i=0; i < uidLength; i++) {
    Serial.print(" 0x");Serial.print(uid[i], HEX); 
  }

  Serial.println();

  ///////query tables to determine if operator or controller is certified... this will be done using the ACK kit
  /////// create a labda funtion in the cloud that queries an s3 server and checks if uid matches a certified EOT have lamda return 0 or 1 depending on if matching was successful

  //if controller has already succesfully badged check if new badge correlates with certified EOT operator
  if(controllerInitiated && !(operatorInitiated)){
    //check if badge uid correlates with an EOT who is a certified operator
    if(operatorAuthorized){
      operatorUid[0] = uid[0]; //certified operator
      operatorInitiated = true;
      successOperatorAni();
    }
    else{
      Serial.print("You are not certified to operate"); //uncertified operator
      errorOperatorAni();
    }
  }

  //if controller has not badged in check if the badge correlates with a certied EOT controller
  else if(!(controllerInitiated && operatorInitiated)){
    if(controllerAuthorized){
      controllerUid[0] = uid[0]; //certified controler 
      controllerInitiated = true;
      successControllerAni();
    }
    else{
      Serial.print("You are not certified to control"); //uncertified EOT
      errorControllerAni();
    } 
  }

  //controller and operator are badeged in
  //secondary badge from the operator should signal the start of a procedure
  //secondary badge from the controller should signal the end of maintainence
  else{
    if(uid == controllerUid){
      Serial.print("staring operation");
      startOperationAni();
    }
    else if(uid == operatorUid){
      Serial.print("end of procedure");  
      operatorInitiated = false;
      operatorAuthorized = false;
      controllerInitiated = false;
      controllerAuthorized = false;
      endOperationAni();
    }
  }
}


//Animation for start of operation
void startOperationAni(){
  //time of operation should be recorded along with the battery charge and time since operator successful badge
  uint8_t chaseTime = 2500;             //Determinied to be optimal time for proactivley planned slowdown moment
  uint8_t greenTime = 5000;             //Fill greeen for 5 seconds after chase animation is complete
  uint8_t wait = chaseTime/(NUM_LED*2); //slow down time for each loop iteration to create a 2.5 second chase
  uint8_t count = 0;
  uint8_t g = 0;                        //green value in rgb
  uint8_t gInc = int(150/(NUM_LED*2));  //increase g value to go from red to yellow in chase
  
  ring.clear();
  ring.show();
  for(uint8_t i=0; count<48; (i==24 ? i=0 : i++)){
    ring.setPixelColor(i, 255, g, 0);
    ring.show();
    delay(wait);
    g += gInc;
    count++;
   }

  ring.fill(GREEN);
  ring.show();
  delay(greenTime);
  ring.fill(BLUE);
  ring.show();
}


//Animation for the end of the operation
//not sure what we want to do here
void endOperationAni(){
  //data about the operation should stop being recorded here
  ring.clear();
  ring.show();
}


//operator is not certified oor there was some other error in the badging of operator
//half of the ring will flash red three times
void errorOperatorAni(){
  for(uint8_t i=0; i<3; i++){
    ring.fill(RED, 0, NUM_LED/2);
    ring.show();
    delay(100);
    ring.fill(NONE, 0, NUM_LED/2);
    ring.show(); 
    delay(100);
  }
}


//controller is not certified oor there was some other error in the badging of controller
//other half of the ring will flash red three times
void errorControllerAni(){
  for(uint8_t i=0; i<3; i++){
    ring.fill(RED, NUM_LED/2, NUM_LED);
    ring.show();
    delay(100);
    ring.fill(NONE, NUM_LED/2, NUM_LED);
    ring.show(); 
    delay(100);
  } 
}


//success verifying operator
//other half of the ring will turn blue
//the ring should be all blue now -> beacon is ready to operatate
void successOperatorAni(){
  //operator EOT alias should be recorded here
  ring.fill(BLUE, 0, NUM_LED/2);
  ring.show();
}


//success verifying controller
//first half of the ring will turn blue
void successControllerAni(){
  //controller EOT alias should be recorded here
  ring.fill(BLUE, NUM_LED/2, NUM_LED);
  ring.show();
}
