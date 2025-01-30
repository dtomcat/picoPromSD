#define XBOX_EEPROM_ADDRESS 0x54
#define XBOX_EEPROM_SIZE    256
#define GLED                16
#define RLED                17
#define BUTT                18 

extern "C" void flash_get_unique_id(uint8_t *p);

//SD card pin assignment
const int _MISO = 12;
const int _MOSI = 11;
const int _CS = 13;
const int _SCK = 10;
//End SD card pin assignment

const int logPrint = 1;         //Definitions for serialLog function
const int logPrintln = 2;       //Definitions for serialLog function
String deSerial;
const char ver[] = "2.4";       //Firmware Version constant 
bool noDelay = false;            //Used to remove delay that is used to allow time for a serial connection
uint8_t UniqueID[8];            //Holds PicoPromSD's Serial Number

//variables for Seagate Password routines
byte r1, r2, r3, r4, r5, r6, r7, r8, r9, r10;
char conRx;                     //General CHAR variable
bool verboseMode = false;       //Display all HDD terminal info?
bool ErrDet = false;            //Error Flag
bool isSlim = true;             //True if Slim, False if Rubber
String HDDPW = "";              //Holds HDD Password
String HDDSN = "";              //Holds HDD Serial Number
const int slim = 0;
const int rubber = 1;
//----------------------------------------

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "sha1.hpp"
#include "xbox.hpp"
#include "HMAC_SHA1.h"

File logFile;
bool isLogFile = false;
bool hddPassFile = false;
bool grabPassOnly = false;
String SN = "";

char pbEEPROM[XBOX_EEPROM_SIZE];
char deEEPROM[XBOX_EEPROM_SIZE];
byte driveData[40] = {0};
int xversion = -1;
int rlen = 0;

void setup() {
  pinMode(GLED, OUTPUT);
  pinMode(RLED, OUTPUT);
  pinMode(BUTT, INPUT_PULLDOWN);
  digitalWrite(GLED, HIGH); //Green LED on
  digitalWrite(RLED, HIGH); //Red LED on

  if (digitalRead(BUTT)){
    grabPassOnly = true;
    dance(5);
  }
  SPI1.setRX(_MISO);
  SPI1.setTX(_MOSI);
  SPI1.setSCK(_SCK);
  Serial.begin(115200);
  if (!noDelay){
    delay(5000);
  }
  if (!grabPassOnly){
    Wire.begin();
  }else{
    Serial.println("setting up serial2");
    Serial2.setTX(4);
    Serial2.setRX(5);
    Serial2.begin(9600);
  }
  flash_get_unique_id(UniqueID);          //Grab PicoPromSD's Serial Number and store it in UniqueID
  Serial.print("Initializing SD card...");
  //Initialize SD card then check for folder... 
  //if doesn't exist; create it
  if (!SD.begin(_CS, SPI1)) {
    delay(100);
    setError();
    serialLog(logPrintln, "*********************************************");
    serialLog(logPrintln, "SD Card Initialization Failed!");
    serialLog(logPrintln, "*********************************************");
    endProgram();
  }
  serialLog(logPrintln, "OK");
  serialLog(logPrintln, "Checking for \"epbackup\" folder...");
  if (!SD.exists("epbackup")){
    serialLog(logPrintln, "epbackup doesn't exist...  creating it");
    if (!SD.mkdir("epbackup")) {
      setError();
      serialLog(logPrintln, "*********************************************");
      serialLog(logPrintln, "Error creating epbackup folder");
      serialLog(logPrintln, "*********************************************");
      endProgram();
    }
  }
  serialLog(logPrintln, "OK");
  
  serialLog(logPrintln, "Checking for \"HDDPass\" folder...");
  if (!SD.exists("HDDPass")){
    serialLog(logPrintln, "HDDPass doesn't exist...  creating it");
    if (!SD.mkdir("HDDPass")) {
      setError();
      serialLog(logPrintln, "*********************************************");
      serialLog(logPrintln, "Error creating HDDPass folder");
      serialLog(logPrintln, "*********************************************");
      endProgram();
    }
  }
    serialLog(logPrintln, "OK");
  
  //*************************************************
  //Check for file that holds data needed for HDD Password
  //*************************************************
  if (SD.exists("epbackup/driveinfo.txt") && !grabPassOnly){
    serialLog(logPrintln, "Driveinfo.txt is present!  Will attempt to provide HDD password.");
    //Open file for read
    File HDDpass = SD.open("epbackup/driveinfo.txt", FILE_READ);
    if (HDDpass){
      rlen = HDDpass.available();
      byte driveDataTemp[rlen];
      int returnStatus = -1;
      returnStatus = HDDpass.read(driveDataTemp, sizeof(driveDataTemp));
      hddPassFile = true;
      for (int aa = 0; aa < sizeof(driveDataTemp); aa++){
        driveData[aa] = driveDataTemp[aa];
      }
    }
    HDDpass.close();
  }
  serialLog(logPrintln, "Attempting to create log file...");
  String logFileName = "";
  if (!grabPassOnly){
    logFileName = getFileName("output", "log");
  } else {
    logFileName = getFileNameHDD("output", "log");
  }
  logFile = SD.open(logFileName, FILE_WRITE);
  // if the file opened okay, write to it:
  if (logFile) {
    serialLog(logPrintln, "OK");
    serialLog(logPrintln, "All Further information will also appear in the LOG");
    isLogFile = true;
  } else {
    serialLog(logPrintln, "*********************************************");
    serialLog(logPrintln, "Error create log file! Attempting to continue with logging.");
    serialLog(logPrintln, "*********************************************");
    isLogFile = false;    
  }
  
  //**************************************************
  //Send everything to try to get logged from now on.
  //**************************************************
  printVer();
  if (grabPassOnly) {
    HDDPassGrab();
  }
  serialLog(logPrint, "Trying to communicate with Xbox EEPROM...");
  //Check for communication with EEPROM
  int returnStatus = -1;
  returnStatus = XboxI2C_DetectEEPROM(XBOX_EEPROM_ADDRESS);
  if (returnStatus == -1) {
    setError();
    serialLog(logPrintln, "");
    serialLog(logPrintln, "*********************************************");
    serialLog(logPrintln, "Error - EEPROM not detected.  Please check wires and ensure Xbox is on");
    serialLog(logPrintln, "*********************************************");
    endProgram();
  }
  serialLog(logPrintln, "OK");
  serialLog(logPrintln, "Attempting to get EEPROM data...");
  //Grab EEPROM and decrypt it
  String fname = "";
  returnStatus = XboxI2C_ReadEEPROM(XBOX_EEPROM_ADDRESS, pbEEPROM);
  if (returnStatus != -1) {
    serialLog(logPrintln, "EEPROM data successful... Attempting to decrypt");
    serialLog(logPrint, "CRC of EEPROM Data: "); String crc = checksumCalculator((uint8_t*) pbEEPROM, XBOX_EEPROM_SIZE);serialLog(logPrintln, crc);
    memcpy(deEEPROM, pbEEPROM, XBOX_EEPROM_SIZE);
    serialLog(logPrint, "CRC of copied EEPROM Data: "); crc = checksumCalculator((uint8_t*) deEEPROM, XBOX_EEPROM_SIZE); serialLog(logPrintln, crc);
    XboxCrypto *xbx = new XboxCrypto();
    if (xbx->decrypt((unsigned char *)deEEPROM) >= 0) {
       serialLog(logPrintln, "EEPROM Decrypt successful!");
       xversion = xbx->getVersion();
       SN = getSerialNum();
       //Get filename for txt file
       fname = getFileName(getSerialNum(), "txt", SN);
       bool fileResult = writeTXT(fname);
       if (!fileResult){
        serialLog(logPrintln, "Txt Not written");
       }
       serialLog(logPrint, "CRC of Decrypted EEPROM Data: "); crc = checksumCalculator((uint8_t*) deEEPROM, XBOX_EEPROM_SIZE); serialLog(logPrintln, crc);
       serialLog(logPrintln, "Xbox Version: " + xboxVerLookup(xversion));
       serialLog(logPrintln, "Online Key: " + getOnlineKey());
       serialLog(logPrintln, "HDD Key: " + getHDDkey());
       if (hddPassFile) serialLog(logPrintln, "HDD Pass: " + getHDDPassword());
       serialLog(logPrintln, "Confounder: " + getConfounder());
       serialLog(logPrintln, "Region: " + getRegion());
       serialLog(logPrintln, "MAC: " + getMAC());
       serialLog(logPrintln, "Serial: " + getSerialNum());
       serialLog(logPrintln, "Video Standard: " + getVideoStd());
       serialLog(logPrintln, "DVD Zone: " + getDVDRegion());
      }
      else {
        serialLog(logPrintln, "*********************************************");
        serialLog(logPrintln, "Error - Decryption Failed.  Continuing with save and write fuctions!");
        serialLog(logPrintln, "*********************************************");
      }
  }else
  {
    setError();
    serialLog(logPrintln, "*********************************************");
    serialLog(logPrintln, "Error - Failed to get EEPROM data.  Please check wires and ensure Xbox is on");
    serialLog(logPrintln, "*********************************************");
    endProgram();
  }
  

  
  //create eeprom.bin and read eeprom from xbox and
  //dump into eeprom.bin
  String filename;
  if (fname == ""){
    filename = getFileName("eeprom", "bin", SN);
  } else {
    filename = getFileName(getSerialNum(), "bin", SN);
  }
  serialLog(logPrintln, "Attempting to create: " + filename + " ...");
  File myFile = SD.open(filename, FILE_WRITE);
  // if the file opened okay, write to it:
  if (myFile) {
    returnStatus = myFile.write(pbEEPROM, XBOX_EEPROM_SIZE);
    if (!returnStatus) {
      setError();
      serialLog(logPrintln, "*********************************************");
      serialLog(logPrintln, "failed to write EEPROM Data to " + filename + ".  Please check SD card!");
      serialLog(logPrintln, "EEPROM is NOT backed up!!!!");
      serialLog(logPrintln, "*********************************************");
      myFile.close();
      endProgram();
    }
    // close the file:
    myFile.close();
    setOK();
    serialLog(logPrintln, "SUCCESS!");
  }
  else {
    setError();
    serialLog(logPrintln, "*********************************************");
    serialLog(logPrintln, "ERROR creating eeprom.bin file.  Please check SD card!");
    serialLog(logPrintln, "EEPROM is NOT backed up!!!!");
    serialLog(logPrintln, "*********************************************");
    endProgram();
  }
  //Clear EEPROM data from memory
  memset(pbEEPROM, 0, XBOX_EEPROM_SIZE); 
}

void loop(){
  int returnStatus = -1;
  int buttonState = digitalRead(BUTT); //Button Status
  serialLog(logPrint, "Write EEPROM button State: ");serialLog(logPrintln, (String)buttonState);
  //Check if eeprom.bin in writeep folder to write to EEPROM
  //If so, let user know it's ready to write (blinking Green LED)
  //if not, halt program
  if (SD.exists("writeep/eeprom.bin"))
  {
    serialLog(logPrintln, "eeprom.bin found in writeep folder!");
    File ckFile = SD.open("writeep/eeprom.bin", FILE_READ);
    serialLog(logPrint, "writeep/eeprom.bin size = ");
    serialLog(logPrintln, String(ckFile.size()));
    memset(pbEEPROM, 0, XBOX_EEPROM_SIZE);
    memset(deEEPROM, 0, XBOX_EEPROM_SIZE);
    if (ckFile.size() != 256)
    {
      serialLog(logPrintln, "*********************************************");
      serialLog(logPrintln, "writeep/eeprom.bin is wrong file size. Write cannot be performed");
      serialLog(logPrintln, "*********************************************");
      endProgramError();
    }
    serialLog(logPrintln, "attempting to read writeep/eeprom.bin and decrypt");
    returnStatus = ckFile.read((uint8_t*) pbEEPROM, XBOX_EEPROM_SIZE);
    if (returnStatus){
      memcpy(deEEPROM, pbEEPROM, XBOX_EEPROM_SIZE);
      XboxCrypto *xbx2 = new XboxCrypto();
      if (xbx2->decrypt((unsigned char *)deEEPROM) >= 0) {
       serialLog(logPrintln, "EEPROM Decrypted successfully!");
      } else {
        serialLog(logPrintln, "*********************************************");
        serialLog(logPrintln, "writeep/eeprom.bin was not able to be decrypted. For safety, write cannot be performed");
        serialLog(logPrintln, "*********************************************");
        endProgramError();
      }
    } else {
      serialLog(logPrintln, "*********************************************");
      serialLog(logPrintln, "writeep/eeprom.bin was not able to be read. Write cannot be performed");
      serialLog(logPrintln, "*********************************************");
      endProgramError();
    }
    ckFile.close();
    serialLog(logPrintln, "All checks passed, waiting for button press to write EEPROM...");
    while (!buttonState) {
      digitalWrite(GLED, !digitalRead(GLED));
      buttonState = digitalRead(BUTT);
      delay(100);
    }
    digitalWrite(GLED, HIGH);
    digitalWrite(RLED, HIGH);
    
    //Write bin to EEPROM
    memset(pbEEPROM, 0, XBOX_EEPROM_SIZE);
    File myFile = SD.open("writeep/eeprom.bin", FILE_READ);
    returnStatus = myFile.read((uint8_t*) pbEEPROM, XBOX_EEPROM_SIZE);
    serialLog(logPrint, "CRC ToEEPROM: ");
    String crc = checksumCalculator((uint8_t*) pbEEPROM, XBOX_EEPROM_SIZE);
    serialLog(logPrintln, crc);
    //Check if file read
    if (returnStatus) {
      serialLog(logPrintln, "EEPROM data read from SD card.  Attempting to write to EEPROM...");
      returnStatus = XboxI2C_WriteEEPROM(XBOX_EEPROM_ADDRESS, pbEEPROM);
      //Check if write was successful
      if (returnStatus != -1) {
        myFile.close();
        serialLog(logPrintln, "Written Successfully!  Erasing eeprom.bin");
        //After successful write... remove file
        SD.remove("writeep/eeprom.bin");
        serialLog(logPrintln, "eeprom.bin has been erased from writeep folder.");
        setOK();
        serialLog(logPrintln, "*********************************************");
        serialLog(logPrintln, "Backup and Write processes completed successfully");
        serialLog(logPrintln, "Restart module to run again!");
        serialLog(logPrintln, "*********************************************");
        endProgram();
      } else {
        serialLog(logPrintln, "*********************************************");
        serialLog(logPrintln, "Error writing eeprom.bin to EEPROM!  Check wires and try again.");
        serialLog(logPrintln, "*********************************************");
        setError();
        myFile.close();
        endProgram();
      }
    } else {
      setError();
      serialLog(logPrintln, "*********************************************");
      serialLog(logPrintln, "Error Reading eeprom.bin from SD card!");
      serialLog(logPrintln, "*********************************************");
      myFile.close();
      while(1);
    }
  } else {
      serialLog(logPrintln, "*********************************************");
      serialLog(logPrintln, "Write files not present... Program ended!");
      serialLog(logPrintln, "Restart module to try again!");
      serialLog(logPrintln, "*********************************************");
    endProgram();
  }
}

//********************************************
//**********    END of MAIN CODE    **********
//**********    CUSTOM FUNCTIONS    ********** 
//********************************************

// Sets LEDs to indicate a bad result
void setError() {
  digitalWrite(RLED, HIGH); //Red LED on
  digitalWrite(GLED, LOW);  //Green LED off
}

// Sets LEDs to busy state
void setBusy() {
  digitalWrite(RLED, HIGH); //Red LED on
  digitalWrite(GLED, HIGH);  //Green LED on
}

// Sets LEDs to indicate Good result
void setOK() {
  digitalWrite(RLED, LOW); //Red LED off
  digitalWrite(GLED, HIGH);  //Green LED on
}

// Alternating LEDs
void dance(int times) {
  for (int t = 0; t < times; t++){
    digitalWrite(RLED, LOW);
    digitalWrite(GLED, HIGH);
    delay(500);
    digitalWrite(RLED, HIGH);
    digitalWrite(GLED, LOW);
    delay(500);
  }
  digitalWrite(RLED, LOW);
  digitalWrite(GLED, LOW);
}

// prints Header into each Log
void printVer() {
  serialLog(logPrintln,"      PicoPromSD");
  serialLog(logPrintln,"======================");
  serialLog(logPrint,"==   Version: ");
  serialLog(logPrint, ver);
  serialLog(logPrintln, "   ==");
  serialLog(logPrint,"== ");
  printPSN();
  serialLog(logPrintln, " ==");
  serialLog(logPrintln,"======================");
}

// Generates checksum
String checksumCalculator(uint8_t * data, uint16_t length)
{
   char buffers[16];
   String result;
   uint16_t curr_crc = 0x0000;
   uint8_t sum1 = (uint8_t) curr_crc;
   uint8_t sum2 = (uint8_t) (curr_crc >> 8);
   int index;
   for(index = 0; index < length; index = index+1)
   {
      sum1 = (sum1 + data[index]) % 255;
      sum2 = (sum2 + sum1) % 255;
   }
   uint16_t crcRaw = (sum2 << 8) | sum1;
   sprintf(buffers, "%04X", crcRaw);
   result = buffers;
   return result;
}

// Returns xbox Version (1.0, 1.1-1.4, or 1.6)
String xboxVerLookup(int ver){
  String result;
  switch(ver){
    case 0:
      result = "1.0";
      break;
    case 1:
      result = "1.1 - 1.4";
      break;
    case 2:
      result = "1.6";
      break;
    default:
      result = "INVALID";
      break;
  }
  return result;
}

// Returns eeprom Region
String getRegion(){
  String result;
  unsigned char char1 = deEEPROM[44];
  unsigned char char4 = deEEPROM[47];
  switch(char1){
    case ((char) 0x00):
      if (char4 == (char) 0x80) result = "Manufacturing Plant"; else result = "INVALID 00";
      break;
    case ((char) 0x01):
      result = "North America";
      break;
    case ((char) 0x02):
      result = "Japan";
      break;
    case ((char) 0x04):
      result = "Europe/Australia";
      break;
    default:
      result = "INVALID";
      break;
  }
  return result;
}

// Returns eeprom Video Standard
String getVideoStd(){
  String result;
  unsigned char char2 = deEEPROM[89];
  unsigned char char3 = deEEPROM[90];
  switch(char2){
    case ((char) 0x00):
      if (char3 == (char) 0x00) result = "Not Set (INVALID)"; else result = "INVALID 00";
      break;
    case ((char) 0x01):
      if (char3 == (char) 0x40) result = "NTSC-M"; else result = "INVALID 01";
      break;
    case ((char) 0x02):
      if (char3 == (char) 0x40) result = "NTSC-J"; else result = "INVALID 02";
      break;
    case ((char) 0x03):
      if (char3 == (char) 0x80) result = "PAL-I"; else result = "INVALID 03";
      break;
    case ((char) 0x04):
      if (char3 == (char) 0x40) result = "PAL-M"; else result = "INVALID 04";
      break;
    default:
      result = "INVALID (no match)";
      break;
  }
  return result;
}

// Returns eeprom DVD region
String getDVDRegion(){
  String result;
  unsigned char char1 = deEEPROM[188];
  switch(char1){
    case ((char) 0x00):
      result = "None";
      break;
    case ((char) 0x01):
      result = "Region 1";
      break;
    case ((char) 0x02):
      result = "Region 2";
      break;
    case ((char) 0x03):
      result = "Region 3";
      break;
    case ((char) 0x04):
      result = "Region 4";
      break;
    case ((char) 0x05):
      result = "Region 5";
      break;
    case ((char) 0x06):
      result = "Region 6";
      break;
    default:
      result = "INVALID";
      break;
  }
  return result;
}

// Returns eeprom Serial Number
String getSerialNum(){
  String result;
  for (int i = 52; i <= 63; i++){
    result = result + deEEPROM[i];
  }
  return result;   
}

// Returns eeprom HDD Key
String getHDDkey(){
   char buffers[5];
   String result;
   for (int i = 28; i <= 43; i++){
    sprintf(buffers, "%02X", deEEPROM[i]);
    result = result + buffers;
   }  
   return result;
}

// Calculates the HDD Password based on driveinfo.txt and eeprom hdKey
String getHDDPassword(){
  char buffers[5];                                      //Local Variable for converting HEX HASH to string
  byte HDKey[16];                                       //Local Variable for HDKey
  String result;                                        //Local Variable to return results
  
  //Grab HDKey and store in local variable
  for (int i = 28; i <= 43; i++){
    HDKey[i-28] = deEEPROM[i];
  }  
  byte digest[20];                                      //Local Variable to store HEX HASH
  byte buff[rlen];                                      //Local Variable to store driveData
  
  //Grab Drive Data from Global variable.  Need to do this since Global is larger than we need.
  for(int j=0; j < rlen; j++){
    buff[j] = driveData[j];
  }
  
  CHMAC_SHA1 HMAC_SHA1;
  HMAC_SHA1.HMAC_SHA1(buff, sizeof(buff), HDKey, sizeof(HDKey), digest) ;
  
  //Convert Hex Hash to String
  for (int k = 0; k <= 19; k++){
    sprintf(buffers, "%02X", digest[k]);
    result = result + buffers;
  }
  return result;
}

// Returns eeprom Online Key
String getOnlineKey(){
   char buffers[5];
   String result;
   for (int i = 72; i <= 87; i++){
    sprintf(buffers, "%02X", deEEPROM[i]);
    result = result + buffers;
   }
   return result;
}

// Returns eeprom Confounder
String getConfounder(){
   char buffers[5];
   String result;
   for (int i = 20; i <= 27; i++){
    sprintf(buffers, "%02X", deEEPROM[i]);
    result = result + buffers;
   }
   return result;
}

// Returns eeprom MAC
String getMAC(){
   char buffers[5];
   String result;
   for (int i = 64; i <= 69; i++){
    sprintf(buffers, "%02X", deEEPROM[i]);
    result = result + buffers;
    if (i < 69){
      result = result + "-";
    }
   }
   return result;
}

// Used to get the file name for Hdd password (taking into account multiple scans with unique file names each time)
String getFileNameHDD(String fSerial, String extension){
  int ext = 1;
  String filename;
  filename = "HDDPass/" + fSerial + "." + extension;
  while (SD.exists(filename))
  {
    filename = "HDDPass/" + fSerial + "_" + String(ext++) + "." + extension;
    delay(10);
  }
  return filename;
}

// Used to get the file name for Hdd password using Serial number as folder(taking into account multiple scans with unique file names each time)
String getFileNameHDD(String fSerial, String extension, String Ser){
  int ext = 1;
  String filename;
  filename = "HDDPass/" + Ser + "/" + fSerial + "." + extension;
  while (SD.exists(filename))
  {
    filename = "HDDPass/" + Ser + "/" + fSerial + "_" + String(ext++) + "." + extension;
    delay(10);
  }
  return filename;
}

// Used to get the file name (taking into account multiple scans with unique file names each time)
String getFileName(String fSerial, String extension){
  int ext = 1;
  String filename;
  filename = "epbackup/" + fSerial + "." + extension;
  while (SD.exists(filename))
  {
    filename = "epbackup/" + fSerial + "_" + String(ext++) + "." + extension;
    delay(10);
  }
  return filename;
}

// Used to get the file name while using Serial number as folder(taking into account multiple scans with unique file names each time)
String getFileName(String fSerial, String extension, String Ser){
  int ext = 1;
  String filename;
  filename = "epbackup/" + Ser + "/" + fSerial + "." + extension;
  while (SD.exists(filename))
  {
    filename = "epbackup/" + Ser + "/" + fSerial + "_" + String(ext++) + "." + extension;
    delay(10);
  }
  return filename;
}

// Write plain text eeprom data to txt file
bool writeTXT(String filename)
{
  File txtFile = SD.open(filename, FILE_WRITE);
  if (txtFile)
  {
    txtFile.println("Serial: " + getSerialNum());
    txtFile.println("Xbox Version: " + xboxVerLookup(xversion));
    txtFile.println("Online Key: " + getOnlineKey());
    txtFile.println("HDD Key: " + getHDDkey());
    if (hddPassFile) txtFile.println("HDD Pass: " + getHDDPassword());
    txtFile.println("Confounder: " + getConfounder());
    txtFile.println("Region: " + getRegion());
    txtFile.println("MAC: " + getMAC());
    txtFile.println("Video Standard: " + getVideoStd());
    txtFile.print("DVD Zone: " + getDVDRegion());
    txtFile.close();
    return true;
  }else
  {
    serialLog(logPrintln, "Error creating " + filename);
    txtFile.close();
    return false;
  }
}

// Print Digital Serial Number
void printPSN(){
  for (size_t i = 0; i < sizeof(UniqueID); i++)
  {
    if (UniqueID[i] < 0x10)
      serialLog(logPrint, "0");
    serialLogHex(logPrint,UniqueID[i]);
  }
}

// Logging in Hex
void serialLogHex(int cmd, uint8_t text){
  switch (cmd){
    case (1):
      if (isLogFile) logFile.print(text, HEX);
      Serial.print(text, HEX);
      
      break;
    case (2):
      if (isLogFile) logFile.println(text, HEX);
      Serial.println(text, HEX);
      
      break;
    default:
      if (isLogFile) logFile.println(text, HEX);
      Serial.println(text, HEX);
      break;
  }
}

// Logging
void serialLog(int cmd, String text){
  switch (cmd){
    case (1):
      if (isLogFile) logFile.print(text);
      Serial.print(text);
      
      break;
    case (2):
      if (isLogFile) logFile.println(text);
      Serial.println(text);
      
      break;
    default:
      if (isLogFile) logFile.println(text);
      Serial.println(text);
      break;
  }
}

void endProgramError(){
  if (isLogFile){ 
    Serial.println("closing log file...");
    logFile.close();
  }
  digitalWrite(GLED, LOW);
  while(1){
    digitalWrite(RLED, !digitalRead(RLED));
    delay(100);
  }
}

void endProgram(){
  if (isLogFile){ 
    Serial.println("closing log file...");
    logFile.close();
  }
  while(1);
}

//***********************************************
//**** CODE FOR SEAGATE PASSWORD GRABBER  *******
//***********************************************
void HDDPassGrab(){
  bool runLoop = true;
  serialLog(logPrintln, "Seagate Password Grab Routine");
  int lCount = 0;
  int ledState = LOW;
  int busy = false;
  setOK();
  if (verboseMode){
    serialLog(logPrintln, "**Verbose Mode Active!  This information is only shown on Terminal Output!**");
  }
  while (runLoop){
    if (Serial2.available()) {          //Reading HDD looking for "PSlv" to ensure ready for commands
      if (!busy){
        busy = true;
        setBusy();
      }
      r1 = r2;
      r2 = r3;
      r3 = r4;
      r4 = Serial2.read();
      conRx = (char) r4;
      if (verboseMode){
        Serial.write(conRx);
      }
      if(r1 == 50 && r2 == 53 && r3 == 54 && r4 == 107){ //256k for buffer size to determine which seagate
        isSlim = false;
      }
      if(r1 == 69 && r2 == 114 && r3 == 114 && r4 == 61){ //Err=
        ErrDet = true;
      }
      if(r1 == 69 && r2 == 114 && r3 == 114 && r4 == 33){ //Err!
        ErrDet = true;
      }
      if(r1 == 101 && r2 == 114 && r3 == 114 && r4 == 33){ //err!
        ErrDet = true;
      }
      if(r4 == 10 && ErrDet){
        serialLog(logPrintln, "***An ERROR has been detected in the drive***\n***The drive may be bad and the data obtained may be incorrect***");
        ErrDet = false;
      }
      if((r1 == 80 && r2 == 83 && r3 == 108 && r4 == 118) || (r1 == 77 && r2 == 115 && r3 == 116 && r4 == 114) || (r1 == 108 && r2 == 97 && r3 == 118 && r4 == 101) || (r1 == 115 && r2 == 116 && r3 == 101 && r4 == 114)){ //PSlv / Mstr (slim) or lave / ster (RJ)  
        serialLog(logPrintln,"");
        runLoop = false;
        if (isSlim){
          serialLog(logPrintln, "Slim HDD is Ready!");
          prepSlim();
        } else {
          serialLog(logPrintln, "Jacketed HDD is Ready!");
          prepRJ();  
        }
      }   
    }
 }
}

void prepSlim(){
  Serial2.write(26); //CTRL + Z
  delay(1000);
  while(Serial2.available()){
    r5 = r6;
    r6 = Serial2.read();
  }
  if (r5 == 84 && r6 == 62){            //looking for "T>"
    
    
    //******************************
    //Grab HDD Serial Number
    //******************************
    Serial2.print("%");
    int sBUFFER_SIZE = 31;
    char sbuf[sBUFFER_SIZE];
    int srlen = Serial2.readBytes(sbuf, sBUFFER_SIZE);
    HDDSN = "";
    serialLog(logPrint, "Serial Number: ");
    for (int i = 23; i < srlen; i++){
      serialLog(logPrint, (String) sbuf[i]);
      HDDSN += (String) sbuf[i];
    }
    serialLog(logPrintln, "");
    Serial2.println("");
    delay(500);
    while(Serial2.available()){
      conRx = Serial2.read();
    }
    //*******************************
    
    serialLog(logPrintln, "Command Sucessful");
    Serial2.println("/2");
    delay(1000);
    while(Serial2.available()){
      r7 = r8;
      r8 = Serial2.read();
    }

    if(r7 == 50 && r8 == 62){           //Looking for "2>" to ensure in the right menus
      serialLog(logPrintln, "Final Commands being sent");
      Serial2.println("S006b");
      delay(250);
      while(Serial2.available()){
        conRx = Serial2.read();
      }
      Serial2.println("R20,01");
      delay(250);
      while(Serial2.available()){
        conRx = Serial2.read();
      }
      Serial2.println("C0,570");
      delay(250);
      while(Serial2.available()){
        conRx = Serial2.read();
      }
      Serial2.println("B570");
      int BUFFER_SIZE = 296;
      char buf[BUFFER_SIZE];
      int rlen = Serial2.readBytes(buf, BUFFER_SIZE);
      HDDPW = "";
      serialLog(logPrint, "HDD Password: ");
      for (int i = 252; i < rlen; i++){
        serialLog(logPrint, (String) buf[i]);
        HDDPW += (String) buf[i];
      }
      serialLog(logPrintln, "");
      String fnme = getFileNameHDD(HDDSN, "txt", HDDSN);
      bool fileResult = writePass(fnme, slim, HDDSN, HDDPW);
      if (!fileResult){
        serialLog(logPrintln, "Txt Not Written");
      }
      setOK();
      endProgram();
    } else {
      serialLog(logPrintln, "HDD didn't enter the Second menu?");
      setError();
      endProgram();
    }
  } else {
    serialLog(logPrintln, "HDD doesn't appear to enter a state that accepts commands? (Bad HDD?)");
    setError();
    endProgram();
  }
}

void prepRJ(){
  delay(5000);
  Serial2.write(26);
  delay(500);
  Serial2.write(18);                    //CTRL + R
  delay(1000);
  while(Serial2.available()){Serial2.read();}
  while(Serial2.available()){
    r5 = r6;
    r6 = Serial2.read();
  }
  if (r5 == 84 && r6 == 62 || true){    //looking for "T>"
    //******************************
    //Grab HDD Serial Number
    //******************************
    Serial2.print("%");
    int sBUFFER_SIZE = 31;
    char sbuf[sBUFFER_SIZE];
    int srlen = Serial2.readBytes(sbuf, sBUFFER_SIZE);
    HDDSN = "";
    serialLog(logPrint, "Serial Number: ");
    for (int i = 23; i < srlen; i++){
      serialLog(logPrint, (String) sbuf[i]);
      HDDSN += (String) sbuf[i];
    }
    serialLog(logPrintln, "");
    Serial2.println("");
    delay(500);
    while(Serial2.available()){
      conRx = Serial2.read();
    }
    //*******************************
    
    serialLog(logPrintln, "Command Successful");
    Serial2.println("G1");
    delay(1000);
    while(Serial2.available()){
      r7 = r8;
      r8 = Serial2.read();
    }
    if (r7 == 84 && r8 == 62){          //looking for "T>"
      serialLog(logPrintln, "Command Successful");
      Serial2.println("/2");
      delay(1000);
      while(Serial2.available()){
        r9 = r10;
        r10 = Serial2.read();
      }
      if(r9 == 50 && r10 == 62){        //Looking for "2>" to ensure in the right menus
        serialLog(logPrintln, "Final Commands being sent");
        Serial2.println("r,5,10");
        delay(250);
        while(Serial2.available()){
          r10 = Serial2.read();
          if (r10 == 67) {              // "C" meaning command isn't going to work
            serialLog(logPrintln, "**Command Failed! Cycle power to HDD and try again!**");
            setError();
            endProgram();
          }
        }
        Serial2.println("C0,010");
        delay(250);
        while(Serial2.available()){
          conRx = Serial2.read();
        }
        Serial2.println("B010");
        int BUFFER_SIZE = 288;
        char buf[BUFFER_SIZE];
        int rlen = Serial2.readBytes(buf, BUFFER_SIZE);
        HDDPW = "";
        serialLog(logPrint, "HDD Password: ");
        for (int i = 244; i < rlen; i++){
          serialLog(logPrint, (String) buf[i]);
          HDDPW += (String) buf[i];
        }
        serialLog(logPrintln, "");
        String fnme = getFileNameHDD(HDDSN, "txt", HDDSN);
        bool fileResult = writePass(fnme, rubber, HDDSN, HDDPW);
        if (!fileResult){
          serialLog(logPrintln, "Txt Not Written");
        }
        setOK();
        endProgram();
      } else {
        serialLog(logPrintln, "HDD didn't enter the Second menu?");
        setError();
        endProgram();
      }
    } else {
      serialLog(logPrintln, "HDD didn't enter Diag menu?");
      setError();
      endProgram();
    }
  } else {
    serialLog(logPrintln, "HDD doesn't appear to enter a state that accepts commands? (Bad HDD?)");
    setError();
    endProgram();
  }
}

bool writePass(String filename,int drive, String ser, String Password)
{
  File txtFile = SD.open(filename, FILE_WRITE);
  if (txtFile)
  {
    String vDrive = "";
    switch (drive){
      case slim:
        vDrive = "Slim";
        break;
      case rubber:
        vDrive = "Rubber Jacket";
        break;
      default:
        vDrive = "Unknown";
        break; 
    }
    txtFile.println("Drive Type: " + vDrive);
    txtFile.println("Serial Number: " + ser);
    txtFile.println("HDD Password: " + Password);
    txtFile.close();
    return true;
  }else
  {
    serialLog(logPrintln, "Error creating " + filename);
    txtFile.close();
    return false;
  }
}

//***********************************************
//**** FOLLOWING CODE WRITTEN BY RYZEE119 *******
//***********************************************


//Read the EEPROM
//bAddress: I2C address of EEPROM
//pbBuffer: Pointer the receiver buffer (256 bytes minimum)
//Returns: -1 on error, 0 on success.
int XboxI2C_ReadEEPROM(char bAddress, char *pbBuffer)
{
  //Some input sanity
  if (bAddress < 0 || bAddress > 128)
    return -1;
  if (pbBuffer == 0)
    return -1;

  memset(pbBuffer, 0, XBOX_EEPROM_SIZE);

  // Read the EEPROM buffer from the chip
  int add = 0;
  Wire.beginTransmission(bAddress);
  Wire.write(add);
  if (Wire.endTransmission(false) == 0) {
    while (add < XBOX_EEPROM_SIZE) {
      Wire.requestFrom(bAddress, 1);
      if (Wire.available()) {
        pbBuffer[add] = Wire.read();
      } else {
        return -1;
      }
      add++;
    }
  } else {
    return -1;
  }

  // Successfully read the EEPROM chip.
  return 0;
}

//Write the EEPROM
//bAddress: I2C address of EEPROM
//pbBuffer: Pointer the transmit data buffer (256 bytes minimum)
//Returns: -1 on error, 0 on success.
int XboxI2C_WriteEEPROM(char bAddress, char *pbBuffer)
{
  char commandBuffer[2];
  int i;

  //Some input sanity
  if (bAddress < 0 || bAddress > 128)
    return -1;
  if (pbBuffer == 0)
    return -1;

  //Loop through the buffer to write.
  for (i = 0; i < XBOX_EEPROM_SIZE; i++)
  {
    Wire.beginTransmission(bAddress);
    // Set the target address and data for the current byte.
    commandBuffer[0] = (char)i;
    commandBuffer[1] = pbBuffer[i];

    // Write the data to the chip.
    Wire.write(commandBuffer, 2);

    if (Wire.endTransmission() != 0) {
      return -1;
    }

    //Wait before writing the next byte.
    delay(10);
  }

  //Successfully wrote the buffer to the EEPROM chip.
  return 0;
}

//Requests an ACK from the EEPROM to detect if it's present
//bAddress: I2C address of EEPROM
//Returns: -1 on error, 0 on success.
int XboxI2C_DetectEEPROM(char bAddress)
{
  Wire.beginTransmission(bAddress);
  if (Wire.endTransmission(true) != 0) {
    return -1;
  }
  //I2C device at specified address detected
  return 0;
}
