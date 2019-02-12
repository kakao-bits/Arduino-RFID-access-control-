#include <LiquidCrystal.h>
#include <X113647Stepper.h>
#include <EEPROM.h>     // We are going to read and write PICC's UIDs from/to EEPROM
#include <SPI.h>        // RC522 Module uses SPI protocol
#include <MFRC522.h>  // Library for Mifare RC522 Devices
#define ON HIGH
#define OFF LOW
static const int STEPS_PER_REVOLUTION = 64 * 32;  // steps per revolution for stepper motor
int i = 0; // Counter for alarm
int val = LOW, pre_val = LOW; // Alarming variables
const int Buzz = A5;
const int greenLed = A4;
const int wipeB = 8;     // Button pin for WipeMode
bool programMode = false;  // initialize programming mode to false

uint8_t successRead;    // Variable integer to keep if we have Successful Read from Reader

byte stobuzzerCard[4];   // Stores an ID read from EEPROM
byte readCard[4];   // Stores scanned ID read from RFID Module
byte masterCard[4];   // Stores master card's ID read from EEPROM

// Create MFRC522 Pins.
constexpr uint8_t RST_PIN = 9;
constexpr uint8_t SS_PIN = 10;

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal lcd(7, 6, 5, 4, 3, 2); // LCD Pins connection Rs,E,D4,D5,D6,D7
X113647Stepper myStepper(STEPS_PER_REVOLUTION, A0, A1, A2, A3); //stepper on pins A0 through A3
// Creat a set of new characters
byte smiley[8] = {0b00000, 0b00000, 0b01010, 0b00000, 0b00000, 0b10001, 0b01110, 0b00000};
byte armsUp[8] = {0b00100, 0b01010, 0b00100, 0b10101, 0b01110, 0b00100, 0b00100, 0b01010};
byte frownie[8] = {0b00000, 0b00000, 0b01010, 0b00000, 0b00000, 0b00000, 0b01110, 0b10001};

void setup() {
  pinMode(Buzz, OUTPUT);// Buzzer pin as Output
  pinMode(greenLed, OUTPUT);// buzzer Led pin as Output
  digitalWrite(Buzz, OFF);
  digitalWrite(greenLed, OFF);
  pinMode(wipeB, INPUT_PULLUP);   // Enable pin's pull up resistor
  myStepper.setSpeed(6.5);// set the speed in rpm

  lcd.begin(16, 2);              // initialize the lcd
  lcd.createChar (0, smiley);    // load character to the LCD
  lcd.createChar (1, armsUp);    // load character to the LCD
  lcd.createChar (2, frownie);   // load character to the LCD
  lcd.home ();                   // go home
  lcd.print("  Inter. SUDAN ");
  lcd.setCursor ( 0, 1 );        // go to the next line
  lcd.print (" University S&T");
  delay(3000);
  lcd.clear();
  lcd.home ();                   // go home
  lcd.print("* RFID Access *");
  lcd.setCursor ( 0, 1 );        // go to the next line
  lcd.print ("Control SystemX");
  delay(2000);
  Serial.begin(9600);  // Initialize serial communications with PC
  SPI.begin();           // MFRC522 Hardware uses SPI protocol
  mfrc522.PCD_Init();    // Initialize MFRC522 Hardware
  //If you set Antenna Gain to Max it will increase reading distance
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  Serial.println(F("RFID Access Control System"));   // For debugging purposes
  //  ShowReaderDetails();  // Show details of PCD - MFRC522 Card Reader details

  //Wipe Code - If the Button (wipeB) Pressed while setup run (powebuzzer on) it wipes EEPROM
  if (digitalRead(wipeB) == LOW) {  // when button pressed pin should get low, button connected to ground
    lcd.clear();
    lcd.home ();                   // go home
    lcd.print("Wiping 10 sec");
    lcd.setCursor ( 0, 1 );        // go to the next line
    digitalWrite(Buzz, ON); // buzzer Led stays on to inform user we are going to wipe
    Serial.println(F("Wipe Button Pressed"));
    Serial.println(F("You have 10 seconds to Cancel"));
    Serial.println(F("This will be remove all records and cannot be undone"));
    bool buttonState = monitorWipeButton(10000); // Give user enough time to cancel operation
    if (buttonState == true && digitalRead(wipeB) == LOW) {    // If button still be pressed, wipe EEPROM
      Serial.println(F("Starting Wiping EEPROM"));
      for (uint16_t x = 0; x < EEPROM.length(); x = x + 1) {    //Loop end of EEPROM address
        if (EEPROM.read(x) == 0) {              //If EEPROM address 0
          // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
        }
        else {
          EEPROM.write(x, 0);       // if not write 0 to clear, it takes 3.3mS
        }
      }
      Serial.println(F("EEPROM Successfully Wiped"));
      lcd.print("EEPROM Wiped");
      lcd.print(char(1));
      digitalWrite(Buzz, OFF);  // visualize a successful wipe
      delay(200);
      digitalWrite(Buzz, ON);
      delay(200);
      digitalWrite(Buzz, OFF);
      delay(200);
      digitalWrite(Buzz, ON);
      delay(200);
      digitalWrite(Buzz, OFF);
    }
    else {
      Serial.println(F("Wiping Cancelled")); // Show some feedback that the wipe button did not pressed for 10 seconds
      lcd.print("Wiping Cancelled.");
      digitalWrite(Buzz, OFF);
      delay(1000);
    }
  }
  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine the Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'
  if (EEPROM.read(1) != 143) {
    Serial.println(F("No Master Card Defined"));
    Serial.println(F("Scan A PICC to Define as Master Card"));
    lcd.clear();
    lcd.home ();                   // go home
    lcd.print("No Admin card");
    lcd.setCursor ( 0, 1 );        // go to the next line
    do {
      successRead = getID();            // sets successRead to 1 when we get read from reader otherwise 0
      digitalWrite(greenLed, ON);    // Visualize Master Card need to be defined
      delay(200);
      digitalWrite(greenLed, OFF);
      delay(200);
    }
    while (!successRead);                  // Program will not go further while you not get a successful read
    for ( uint8_t j = 0; j < 4; j++ ) {        // Loop 4 times
      EEPROM.write( 2 + j, readCard[j] );  // Write scanned PICC's UID to EEPROM, start from address 3
    }
    EEPROM.write(1, 143);                  // Write to EEPROM we defined Master Card.
    Serial.println(F("Master Card Defined"));
    delay(3000);
    lcd.clear();
    lcd.home ();                   // go home
    lcd.print("Admin card OK");
    delay(1000);
  }

  Serial.println(F("-------------------"));
  Serial.println(F("Master Card's UID"));
  lcd.clear();
  lcd.home ();                   // go home
  lcd.print("Admin card UID:");
  lcd.setCursor ( 0, 1 );        // go to the next line
  for ( uint8_t i = 0; i < 4; i++ ) {          // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(2 + i);    // Write it to masterCard
    Serial.print(masterCard[i], HEX);
    lcd.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Everything is ready"));
  Serial.println(F("Waiting PICCs to be scanned"));
  cycling();
  delay(2000);
  lcd.clear();
  lcd.home ();                   // go home
  lcd.print("System is Ready");
  lcd.setCursor ( 0, 1 );        // go to the next line
  testing123();

}

// the loop function runs over and over again forever
void loop() {
  do {
    lcd.clear();
    lcd.home ();                   // go home
    lcd.print("*Scan or Wipe");
    lcd.setCursor ( 0, 1 );
    successRead = getID();  // sets successRead to 1 when we get read from reader otherwise 0
    // When device is in use if wipe button pressed for 10 seconds initialize Master Card wiping
    if (digitalRead(wipeB) == LOW) { // Check if button is pressed
      // Visualize normal operation is iterrupted by pressing wipe button buzzer is like more Warning to user
      digitalWrite(Buzz, ON);  // Make sure led is off
      digitalWrite(greenLed, OFF);  // Make sure led is off
      // Give some feedback
      lcd.clear();
      lcd.home ();                   // go home
      lcd.print("Wiping 10 sec");
      lcd.setCursor ( 0, 1 );        // go to the next line
      Serial.println(F("Wipe Button Pressed"));
      Serial.println(F("Master Card will be Erased! in 10 seconds"));
      bool buttonState = monitorWipeButton(10000); // Give user enough time to cancel operation
      if (buttonState == true && digitalRead(wipeB) == LOW) {    // If button still be pressed, wipe EEPROM
        EEPROM.write(1, 0);                  // Reset Magic Number.
        Serial.println(F("Master Card Erased from device"));
        Serial.println(F("Please reset to re-program Master Card"));
        lcd.print("*RESET NOW...");
        while (1);
      }
      Serial.println(F("Master Card Erase Cancelled"));
      lcd.print("Wiping Cancelled.");
    }
    if (programMode) {
      cycling();              // Program Mode cycles through buzzer Green green waiting to read a new card
    }
    else {
      normalModeOn();     // Normal mode, green Power LED is on, all others are off
    }
  }
  while (!successRead);   //the program will not go further while you are not getting a successful read
  if (programMode) {
    if ( isMaster(readCard) ) { //When in program mode check First If master card scanned again to exit program mode
      Serial.println(F("Master Card Scanned"));
      Serial.println(F("Exiting Program Mode"));
      Serial.println(F("-----------------------------"));
      lcd.clear();
      lcd.home ();                   // go home
      lcd.print("Exit Prog. Mode");
      lcd.setCursor ( 0, 1 );        // go to the next line
      programMode = false;
      delay(2000);
      return;
    }
    else {
      if ( findID(readCard) ) { // If scanned card is known delete it
        Serial.println(F("I know this PICC, removing..."));
        lcd.clear();
        lcd.home ();                   // go home
        lcd.print("Removing Card!");
        lcd.setCursor ( 0, 1 );
        deleteID(readCard);
        delay(2000);
        lcd.print("Scanning...");
        Serial.println("-----------------------------");
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      }
      else {                    // If scanned card is not known add it
        Serial.println(F("I do not know this PICC, adding..."));
        lcd.clear();
        lcd.home ();                   // go home
        lcd.print("Adding Card!");
        lcd.setCursor ( 0, 1 );
        writeID(readCard);
        lcd.print("Scanning...");
        Serial.println(F("-----------------------------"));
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      }
    }
  }
  else {
    if ( isMaster(readCard)) {    // If scanned card's ID matches Master Card's ID - enter program mode
      programMode = true;
      Serial.println(F("Hello Master - Entebuzzer Program Mode"));
      lcd.clear();
      lcd.home ();                   // go home
      lcd.print("Hello Admin A*");
      lcd.print(char(0));
      lcd.setCursor ( 0, 1 );
      uint8_t count = EEPROM.read(0);   // Read the first Byte of EEPROM that
      lcd.print(String(count));
      lcd.print(" cards in");
      Serial.print(F("I have "));     // stores the number of ID's in EEPROM
      Serial.print(count);
      Serial.print(F(" record(s) on EEPROM"));
      Serial.println("");
      delay(10000);
      lcd.clear();
      lcd.home ();                   // go home
      lcd.print("*ADD or REMOVE");
      lcd.setCursor ( 0, 1 );
      lcd.print("Scan A* to Exit");
      Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      Serial.println(F("Scan Master Card again to Exit Program Mode"));
      Serial.println(F("-----------------------------"));
    }
    else {
      if ( findID(readCard) ) { // If not, see if the card is in the EEPROM
        Serial.println(F("Welcome, You shall pass"));
        lcd.clear();
        lcd.home ();                   // go home
        lcd.print("*YOU ARE WELCOME");
        //        granted(300);         // Open the door lock for 300 ms
        goooo();
        myStepper.step(STEPS_PER_REVOLUTION); // step one revolution  in one direction:
        delay(500);
        myStepper.step(-STEPS_PER_REVOLUTION); // step one revolution in the other direction:
        delay(1000);
      }
      else {      // If not, show that the ID was not valid
        Serial.println(F("You shall not pass"));
        lcd.clear();
        lcd.home ();                   // go home
        lcd.print("*ACCESS DENIED");
        //        denied();
        whooop(); // Run Alarm on
      }
    }
  }
}
void testing123() {
  lcd.clear();
  lcd.home ();                   // go home
  lcd.print("Testing ...");
  lcd.setCursor ( 0, 1 );
  myStepper.step(STEPS_PER_REVOLUTION); // step one revolution  in one direction:
  lcd.print("1 ");
  delay(500);
  myStepper.step(-STEPS_PER_REVOLUTION); // step one revolution in the other direction:
  lcd.print("2 ");
  delay(1000);
  whooop(); // Run Alarm on
  lcd.print("3 ");
  lcd.setCursor ( 15, 1 );
  lcd.print (char(2));
  delay (2000);
  lcd.setCursor ( 15, 1 );
  lcd.print ( char(1));
  delay (2000);
  lcd.setCursor ( 15, 1 );
  lcd.print ( char(0));
  delay (2000);
  lcd.print("4 ");
  goooo();
  delay (2000);
  lcd.print("5 ");
}
void goooo() {
  for (i = 0; i < 255; i = i + 2)
  {
    analogWrite(greenLed, i);
    analogWrite(Buzz, i);
    delay(10);
  }
  for (i = 255; i > 1; i = i - 2)
  {
    analogWrite(greenLed, i);
    analogWrite(Buzz, i);
    delay(5);
  }
  for (i = 1; i <= 10; i++)
  {
    analogWrite(greenLed, 255);
    analogWrite(Buzz, 200);
    delay(100);
    analogWrite(greenLed, 0);
    analogWrite(Buzz, 25);
    delay(100);
  }
  //    pre_val = val;
}
void whooop() {
  for (int j = 0; j < 3; j++) {
    // Whoop up
    for (int hz = 440; hz < 1000; hz++) {
      int light = map(hz, 440, 1000, 0, 255);
      analogWrite(greenLed, light);
      tone(Buzz, hz, 30);
      delay(2);
    }
    noTone(Buzz);
    // Whoop down
    for (int hz = 1000; hz > 440; hz--) {
      int light = map(hz, 1000, 440 , 255, 0);
      analogWrite(greenLed, light);
      tone(Buzz, hz, 30);
      delay(2);
    }
    noTone(Buzz);
  }
}
void cycling() {
  digitalWrite(Buzz, OFF);  // Make sure buzzer LED is off
  digitalWrite(greenLed, ON);   // Make sure green LED is on
  delay(200);
  digitalWrite(Buzz, ON);  // Make sure buzzer LED is off
  digitalWrite(greenLed, OFF); // Make sure green LED is off
  delay(200);
  digitalWrite(Buzz, OFF);   // Make sure buzzer LED is on
  digitalWrite(greenLed, OFF);  // Make sure green LED is off
  delay(200);
}
bool monitorWipeButton(uint32_t interval) {
  uint32_t now = (uint32_t)millis();
  while ((uint32_t)millis() - now < interval)  {
    // check on every half a second
    if (((uint32_t)millis() % 500) == 0) {
      if (digitalRead(wipeB) != LOW)
        return false;
    }
  }
  return true;
}
///////////////////////////////////////// Get UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("Scanned PICC's UID:"));
  lcd.clear();
  lcd.home ();                   // go home
  lcd.print("Scanned UID:");
  lcd.setCursor ( 0, 1 );
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
    lcd.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}
void normalModeOn () {
  digitalWrite(Buzz, OFF);  // Make sure buzzer LED is off
  digitalWrite(greenLed, OFF);  // Make sure Green LED is off
}
///////////////////////////////////////// Check Bytes   ///////////////////////////////////
bool checkTwo ( byte a[], byte b[] ) {
  for ( uint8_t k = 0; k < 4; k++ ) {   // Loop 4 times
    if ( a[k] != b[k] ) {     // IF a != b then false, because: one fails, all fail
      return false;
    }
  }
  return true;
}
////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
bool isMaster( byte test[] ) {
  return checkTwo(test, masterCard);
}
///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT( byte find[] ) {
  uint8_t count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);                // Read an ID from EEPROM, it is stobuzzer in stobuzzerCard[4]
    if ( checkTwo( find, stobuzzerCard ) ) {   // Check to see if the stobuzzerCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
    }
  }
}
///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
bool findID( byte find[] ) {
  uint8_t count = EEPROM.read(0);     // Read the first Byte of EEPROM that
  for ( uint8_t i = 1; i < count; i++ ) {    // Loop once for each EEPROM entry
    readID(i);          // Read an ID from EEPROM, it is stobuzzer in stobuzzerCard[4]
    if ( checkTwo( find, stobuzzerCard ) ) {   // Check to see if the stobuzzerCard read from EEPROM
      return true;
    }
    else {    // If not, return false
    }
  }
  return false;
}
///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite();      // If not
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else {
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    successDelete();
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}
//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( uint8_t number ) {
  uint8_t start = (number * 4 ) + 2;    // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) {     // Loop 4 times to get the 4 Bytes
    stobuzzerCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
  }
}
///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = ( num * 4 ) + 6;  // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( uint8_t j = 0; j < 4; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    successWrite();
    Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else {
    failedWrite();
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}
/////////////////////////////////////////  Access Granted    ///////////////////////////////////
//void granted ( uint16_t setDelay) {
//  digitalWrite(Buzz, OFF);  // Turn off buzzer LED
//  digitalWrite(greenLed, ON);   // Turn on green LED
//  delay(1000);            // Hold green LED on for a second
//}
////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////// Access Denied  ///////////////////////////////////
//void denied() {
//  digitalWrite(greenLed, OFF);  // Make sure green LED is off
//  digitalWrite(Buzz, ON);   // Turn on buzzer LED
//  delay(1000);
//}
////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////// Write Failed to EEPROM   ////////////////////////
// Flashes the buzzer LED 3 times to indicate a failed write to EEPROM
void failedWrite() {
  digitalWrite(Buzz, OFF);  // Make sure buzzer is off
  digitalWrite(greenLed, OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(Buzz, ON);   // Make sure buzzer is on
  delay(200);
  digitalWrite(Buzz, OFF);  // Make sure buzzer is off
  delay(200);
  digitalWrite(Buzz, ON);   // Make sure buzzer is on
  delay(200);
  digitalWrite(Buzz, OFF);  // Make sure buzzer is off
  delay(200);
  digitalWrite(Buzz, ON);   // Make sure buzzer is on
  delay(200);
}
///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the Green LED & Buzzer 3 times to indicate a success delete to EEPROM
void successDelete() {
  digitalWrite(Buzz, OFF);  // Make sure buzzeris off
  digitalWrite(greenLed, OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(Buzz, ON);  // Make sure buzzer is on
  digitalWrite(greenLed, ON);  // Make sure green LED is on
  delay(200);
  digitalWrite(Buzz, OFF);  // Make sure buzzer is OFF
  digitalWrite(greenLed, OFF);   // Make sure green LED is off
  delay(200);
  digitalWrite(Buzz, ON);  // Make sure buzzer is on
  digitalWrite(greenLed, ON);  // Make sure green LED is on
  delay(200);
  digitalWrite(Buzz, OFF);  // Make sure buzzer is OFF
  digitalWrite(greenLed, OFF);   // Make sure green LED is off
  delay(200);
  digitalWrite(Buzz, ON);  // Make sure buzzer is on
  digitalWrite(greenLed, ON);  // Make sure green LED is on
  delay(200);
}
///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the green LED 3 times to indicate a successful write to EEPROM
void successWrite() {
  digitalWrite(Buzz, OFF);  // Make sure buzzer is off
  digitalWrite(greenLed, OFF);  // Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, ON);   // Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(greenLed, ON);   // Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(greenLed, ON);   // Make sure green LED is on
  delay(200);
}
