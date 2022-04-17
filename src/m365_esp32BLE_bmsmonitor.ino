/*
  based on example ESP32/BLE BLE_UART,   use "M365Tools" or M365Battery(cells don't work now, only sends 20bytes)
  works with M365Tools v1.2.8, v1.3.0 using 55AA protocol
  works with M365Tools V1.3.4 using 5AA5 protocol(uncomment define NBOTFORMAT)
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

//#define STICKCPLUS
//#define STICKC
#define ATOM

#ifdef STICKCPLUS
#include <M5StickCPlus.h>
#define XMAX 240
#define YMAX  135
#elif defined(STICKC)
#include <M5StickC.h>
#define XMAX  160
#define YMAX  80
#endif

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;


// BMS connection
// default: Serial2_TX(17pin)for transmit to BMS, Serial0_RX(regular) to receive, while Serial0_TX is used for debugging to USB UART.
#define SerialTX Serial2
#ifdef ATOM
#define SerialRX Serial2
#define RXD2  26
#define TXD2  32
#elif defined(STICKC)
#define SerialRX Serial2
#define RXD2  32
#define TXD2  33
#else
#define SerialRX Serial
//Serial2, RXD2/TXD2 16/17 are default for ESP32
#define RXD2 16
#define TXD2 17
#endif

uint8_t txTypeIdx = 0;
uint16_t delayIdx = 0;

byte packet[64];  // incoming packet from BMS
uint8_t packetSize = 0;
enum { UNKNOWN=0, XM=0x55, NB=0x5A };
byte packetType = UNKNOWN;
byte nBuf[64]; //for conversion to 5AA5

// requests to send to BMS
// 10: serial number
const byte buf_1010[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x10, 0x10, 0xB9, 0xFF}; //9bytes
const byte buf_1012[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x10, 0x12, 0xB7, 0xFF}; //9bytes
// 1b: number of charge cycles
const byte buf_1b[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x1B, 0x04, 0xBA, 0xFF}; //9bytes
// 20: manufacture date
const byte buf_20[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x20, 0x06, 0xB3, 0xFF}; //9bytes
// 30: MODE/STAT/CAPACITY /CHARGE   /CURRENT  /VOLTAGE  /TEMP 
const byte buf_30[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x30, 0x0C, 0x9D, 0xFF}; //9bytes
// 31: battery capacity (mAh), charge %, current in A /100, battery voltage, temperature
const byte buf_31[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x31, 0x0A, 0x9E, 0xFF}; //9bytes
// 40: cell voltages
const byte buf_40[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x40, 0x1E, 0x7B, 0xFF}; //9bytes

// responses to BLE app
// serial number
byte xmbuf_1012[]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //26 bytes
// charge cycles
byte xmbuf_1b04[]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //12 bytes
// manufacture date
byte xmbuf_2006[]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //14 bytes
// capacity, charge, current, voltage, temp
byte xmbuf_310a[]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //18 bytes
// cell voltages
byte xmbuf_401e[]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //38 bytes

/* Hardcoded Valid Examples
0x55 0xAA format:
byte xmbuf_1012[]= {0x55, 0xAA, 0x14, 0x25, 0x01, 0x10, 0x33, 0x4A, 0x43, 0x47, 0x4A, 0x31, 0x38, 0x41, 0x54, 0x44, 0x31, 0x31, 0x32, 0x35, 0x15, 0x01, 0x78, 0x1E, 0xAD, 0xFB}; //26 bytes
byte xmbuf_1b04[]= {0x55, 0xAA, 0x06, 0x25, 0x01, 0x1B, 0x16, 0x00, 0x2B, 0x00, 0x77, 0xFF}; //12 bytes
byte xmbuf_2006[]= {0x55, 0xAA, 0x08, 0x25, 0x01, 0x20, 0x3B, 0x24, 0x00, 0x00, 0x00, 0x00, 0x52, 0xFF}; //14 bytes
byte xmbuf_310a[]= {0x55, 0xAA, 0x0C, 0x25, 0x01, 0x31, 0x31, 0x1A, 0x55, 0x00, 0x00, 0x00, 0x78, 0x0F, 0x2F, 0x2F, 0x17, 0xFE}; //18 bytes
byte xmbuf_401e[]= {0x55, 0xAA, 0x20, 0x25, 0x01, 0x40, 0x6B, 0x0F, 0x6A, 0x0F, 0x6F, 0x0F, 0x6A, 0x0F, 0x6E, 0x0F, 0x84, 0x0F, 0x84, 0x0F, 0x87, 0x0F, 0x86, 0x0F, 0x86, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 0xFA}; //38 bytes
0x5A 0xA5 format: length(buf[2])->reduced by 2,  address(buf[3]) "0x25" -> "0x22 0x3E", so total size is increased by 1 byte. Needs new CRC. See convert5AA5() function.
byte nbbuf_1012[]= {0x5A, 0xA5, 0x12, 0x22, 0x3E, 0x01, 0x10, 0x33, 0x4A, 0x43, 0x47, 0x4A, 0x31, 0x38, 0x41, 0x54, 0x44, 0x31, 0x31, 0x32, 0x35, 0x15, 0x01, 0x78, 0x1E, 0x74, 0xFB}; //27 bytes
byte nbbuf_1b04[]= {0x5A, 0xA5, 0x04, 0x22, 0x3E, 0x01, 0x1B, 0x16, 0x00, 0x2B, 0x00, 0x3E, 0xFF}; //13 bytes                                                       
byte nbbuf_2006[]= {{0x5A, 0xA5, 0x06, 0x22, 0x3E, 0x01, 0x20, 0x3B, 0x24, 0x00, 0x00, 0x00, 0x00, 0x19, 0xFF}; //15 bytes
byte nbbuf_310a[]= {0x5A, 0xA5, 0x0A, 0x22, 0x3E, 0x01, 0x31, 0x31, 0x1A, 0x55, 0x00, 0x00, 0x00, 0x78, 0x0F, 0x2F, 0x2F, 0xDE, 0xFD}; //19 bytes
byte nbbuf_401e[]= {0x5A, 0xA5, 0x1E, 0x22, 0x3E, 0x01, 0x40, 0x6B, 0x0F, 0x6A, 0x0F, 0x6F, 0x0F, 0x6A, 0x0F, 0x6E, 0x0F, 0x84, 0x0F, 0x84, 0x0F, 0x87, 0x0F, 0x86, 0x0F, 0x86, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF3, 0xF9}; //39 bytes
*/

enum pkt_type {PKT_RX, PKT_TX, PKT_BLE_RX, PKT_BLE_TX};
const int font_width = 13;
const int font_height = 18;

void disp_packet(int pt, const byte* packet, int len) {
#ifdef STICKC
  int y, start;
  
  switch (pt) {
    case PKT_BLE_TX:
      M5.Lcd.setTextColor(BLUE);
      y = 3*font_height;
      start = 0;
      break;
    case PKT_RX:
      M5.Lcd.setTextColor(RED);
      y = 2*font_height;
      start = 5;
      break;
    case PKT_TX:
    default:
      M5.Lcd.setTextColor(GREEN);
      y = font_height;
      start = 0;
      break;
  }
  if (len > start+XMAX/(2*font_width)) len = start+XMAX/(2*font_width);
  M5.Lcd.fillRect(0, y, XMAX, font_height, BLACK);
  M5.Lcd.setCursor(0, y);
  for (int i = start; i < len; i++) {
    if (packet[i]<16) M5.Lcd.print("0");
    M5.Lcd.print(packet[i], HEX);
  }
#endif
}

struct {
  int capacity;
  int charge;
  int current;
  int voltage;
  int temp1;
  int temp2;
} parsed_state;
int max_current = 0;

// parse response packet for capacity, charge %, current, voltage, and temperature
void parse_31(const byte *pkt) {
//           LENG/ADDR/READ/CMND/CAPACITY /CHARGE   /CURRENT  /VOLTAGE  /TEMP    */
//{0x55,0xAA,0x0C,0x25,0x01,0x31,0xDE,0xAD,0xBE,0x00,0x10,0x00,0xFA,0xCE,0x2A,0x2B};
  int off;
  if (packetType == NB) off = 1;
  else off = 0;
  parsed_state.capacity = pkt[off+6] + (pkt[off+7] << 8);
  parsed_state.charge = pkt[off+8] + (pkt[off+9] << 8);
  parsed_state.current = pkt[off+10] + (pkt[off+11] << 8);
  if (parsed_state.current > max_current) max_current = parsed_state.current;
  parsed_state.voltage = pkt[off+12] + (pkt[off+13] << 8);
  parsed_state.temp1 = pkt[off+14];
  parsed_state.temp2 = pkt[off+15];
}

void disp_battery_state() {
#ifdef STICKCPLUS
  int y1 = YMAX - font_height*3;
  int y2 = YMAX - font_height*2;
  int y3 = YMAX - font_height;
  
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(0, y1);
  M5.Lcd.fillRect(0, y1, XMAX, font_height*3, BLACK);
  M5.Lcd.print("Amax: ");
  M5.Lcd.print(max_current);
  M5.Lcd.setCursor(0, y2);
  M5.Lcd.print("V:");
  M5.Lcd.print(parsed_state.voltage);
  M5.Lcd.print(" A:");
  M5.Lcd.print(parsed_state.current);
  M5.Lcd.setCursor(0, y3);
  M5.Lcd.print("C:");
  M5.Lcd.print(parsed_state.charge);
  M5.Lcd.print("% T:");
  M5.Lcd.print(parsed_state.temp1);
  M5.Lcd.print(",");
  M5.Lcd.print(parsed_state.temp2);
#endif
}

void printBytes(byte *buf, size_t size){
  for(size_t idx=0; idx<size; ++idx){
    if(idx!=0) Serial.print(' ');
    Serial.print(buf[idx], HEX);
  }
  Serial.println();
}
void fillCRC(byte *buf, size_t bsize){
  //minimal size of the packet is 9 for 55AA packet
  if(!(bsize > 8 && bsize<40)) return;
  
  uint16_t CRC = 0xFFFF;
  for(size_t idx=2; idx<bsize-2; ++idx){
    CRC -= (uint16_t)buf[idx];
  }
  buf[bsize-2]=(uint8_t)CRC; //lowbyte
  buf[bsize-1]=(uint8_t)(CRC>>8); //hibyte
}

bool checkCRC(byte *buf, size_t bsize){
  //check integrity of the packet
  //minimal size of the 55AA packet is 9 with payload 1byte, buf[2] is size of the packet minus 6 == 2(preambula)+2(bLen + Dest)+2(CRC), includes payload +2bytes(modType and command)
  //for 5AA5 packet min size is 10 and buf[2]+9==bsize
  if(!(bsize > 8 && bsize<40)) return false;
  if(!( (buf[0]==0x55&&buf[2]+6==bsize) || (buf[0]==0x5A&&buf[2]+9==bsize) )) return false;
  
  
  uint16_t CRC = 0xFFFF;
  for(size_t idx=2; idx<bsize-2; ++idx){
    CRC -= (uint16_t)buf[idx];
  }
  bool isGood = buf[bsize-2]==(uint8_t)CRC && buf[bsize-1]==(uint8_t)(CRC>>8);
  return isGood;
}

size_t convert5AA5(const byte *buf, size_t size, byte *bufNew, bool toBMS=false){
  bufNew[0] = 0x5A;
  bufNew[1] = 0xA5;
  bufNew[2] = buf[2]-2;
  if (toBMS) {
    bufNew[3] = 0x3E;
    bufNew[4] = 0x22;
  }
  else {
    bufNew[3] = 0x22;
    bufNew[4] = 0x3E;
  }
  memcpy(bufNew+5, buf+4, size-6); //skip first 4 bytes and last 2 bytes
  fillCRC(bufNew, size+1);
  return size+1;  
}

template<class T>
bool readPacket(T& pserial, unsigned long maxDelayMs=30){
  packetSize = 0;
  int sizeOff = 6;
  unsigned long startMs = millis();
  while(pserial.available() > 0 || millis()-startMs<maxDelayMs){
    if(pserial.available() > 0){
      byte incomingByte = pserial.read();
      if(packetSize==0){ //preambula 1st byte        
        if(incomingByte==0x55 || incomingByte==0x5A){
          packet[0] = incomingByte;
          packetSize = 1;
          packetType = incomingByte;
          sizeOff = (packetType == 0x55) ? 6 : 9;
        }
      }else if (packetSize==1){ //preambula 2nd byte
        if(incomingByte==0xAA || incomingByte==0xA5){
          packet[1] = incomingByte;
          packetSize = 2;
        }else{
          packetSize = 0;
        }
      }else{        
        packet[packetSize] = incomingByte;
        packetSize++;
        if(packetSize>8 && packet[2]+sizeOff==packetSize) //Reached the end of the packet according to packet[2] byte
          break;
        if(packetSize>=40)
          break;
      }
    }
  }
  while(pserial.available()) pserial.read(); //flush the remaining of the serial buffer, something is off.

  //check integrity of the packet
  //XM:minimal size of the packet is 9 with payload 1byte, packet[2] is size of the packet minus 2(preambula)+2(bLen + type)+2(CRC)
  //NB:minimal size is 10, packet[2] is size - 4
  if(packetSize > 8 && packetSize<40 && packet[2]+sizeOff==packetSize){    
    return checkCRC(packet, packetSize);
  }
  //packetSize = 0;
  return false;
}



// UART service UUID, has to be those specific for M365 protocol
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

byte *lastBleTxBuf = NULL;
size_t lastBleTxSize = 0;

void TxPacket(byte *buf, size_t bsize, uint8_t fType){  
  //breaks packet if needed to 20 bytes chunks
  if(bsize>40)return; //we don't expect more than 2 packets for M365 protocol

  if(fType==2){ //Convert to 5AA5 format for BLE app
    if (packetType==XM) {
      bsize = convert5AA5(buf, bsize, nBuf);
      buf = nBuf;
    }
    else {  // the BMS is using NB, so just swap source and destination
      buf[3] = 0x22;
      buf[4] = 0x3E;
    }
  }
 
  lastBleTxBuf = buf;
  lastBleTxSize = bsize;

  while(bsize>0){
    if(bsize<=20){
      //send everything
      pTxCharacteristic->setValue(buf, bsize);
      pTxCharacteristic->notify();
      bsize = 0;
    }else{
      //send next 20 bytes
      pTxCharacteristic->setValue(buf, 20);
      pTxCharacteristic->notify();
      for(uint16_t idx=0; idx<1000; ++idx) ++delayIdx; //delay
      buf += 20;
      bsize -= 20;
    }    
  }
}

class MyCallbacks: public BLECharacteristicCallbacks {  
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 6) {

      uint8_t fType;
      uint8_t reqId;
      uint8_t lenOff;
      
      if(rxValue[0]==0x55 && rxValue[1]==0xAA){
        fType = 1;
        reqId = rxValue[5];
        lenOff = 0;
      }
      else if(rxValue[0]==0x5A && rxValue[1]==0xA5){
        fType = 2;
        reqId = rxValue[6];
        lenOff = 1;     
      }
      else return;

      //I hijacked some request which are not 1:1, but it seems it works, M365Tools still process responses no matter what it requested
      if(reqId==0x1A) {
        if(checkCRC(xmbuf_1012, 26+lenOff)){
          TxPacket(xmbuf_1012, 26+lenOff, fType);
        }
      }
      else if(reqId==0x67) {
        if(checkCRC(xmbuf_1b04, 12+lenOff)){
          TxPacket(xmbuf_1b04, 12+lenOff, fType);
        }
      }
      else if(reqId==0x72) { 
        if(checkCRC(xmbuf_2006, 14+lenOff)){
          TxPacket(xmbuf_2006, 14+lenOff, fType);
        }
      }
      else if(reqId==0x30) {
        if(checkCRC(xmbuf_310a, 18+lenOff)){
          TxPacket(xmbuf_310a, 18+lenOff, fType);
        }
      }
      else if(reqId==0x40) {
        if(checkCRC(xmbuf_401e, 38+lenOff)){
          TxPacket(xmbuf_401e, 38+lenOff, fType);
        }
      }

    }
  }
};


void setup() {
  Serial.begin(115200);
#ifdef STICKC
  M5.begin(true,true,true);

  M5.Lcd.setRotation(3);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(ORANGE);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("MiScooter6560");
  M5.Lcd.setTextSize(2);
#endif

  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  
  // Create the BLE Device
  BLEDevice::init("MiScooter6560"); //"Mi Electric Scooter", "Xiaomi M365", "MiScooter6560"

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

/*  read last received packet (global)
 *  and update cache (also global)
 */
void updateResponseCache() {
  //Parse packet consistency(length and CRC) and copy to the corresponding xmbuf
  int lenDiff, pktLenDiff, cmdOff;
  if (packetType == XM) {
    lenDiff = 0;
    pktLenDiff = 6;
    cmdOff = 5;
  }
  else {
    lenDiff = -2;
    pktLenDiff = 9;
    cmdOff = 6;
  }
  if (packetSize<40 && packet[2]+pktLenDiff==packetSize){
    //valid size of the packet, packet[2](payload + 2 bytes for mod_type and command) and doesn't include preambula, CRC, bLen, Destination)
    //NB: packet[2] doesn't include CRC in length, packet size adds byte (source)
      if(packet[cmdOff]==0x10 && packet[2]==0x14+lenDiff) //1012
        memcpy(xmbuf_1012, packet, packetSize);
      else if(packet[cmdOff]==0x1B && packet[2]==0x06+lenDiff) //1b04
        memcpy(xmbuf_1b04, packet, packetSize);
      else if(packet[cmdOff]==0x20 && packet[2]==0x08+lenDiff) //2006
        memcpy(xmbuf_2006, packet, packetSize);
      else if(packet[cmdOff]==0x31 && packet[2]==0x0C+lenDiff) { //310a
        memcpy(xmbuf_310a, packet, packetSize);
        parse_31(xmbuf_310a);
        disp_battery_state();
      }
      else if(packet[cmdOff]==0x40 && packet[2]==0x20+lenDiff) //401e
        memcpy(xmbuf_401e, packet, packetSize);
  }
}

byte lastPacketType = NB;

// rotate through a series of BMS requests, fill buffers with returned values, which will be sent to BLE app upon request
void loop() {
  //flush tx and rx buffers for BMS Serial
  SerialTX.flush();
  while (SerialRX.available()) SerialRX.read();
  
  //Send request to BMS, alternate request based on txTypeIdx iterator, 0x30 is sent 3 times more frequent than others
  const byte *pBuf;
  int len;
  uint8_t txType = txTypeIdx%9;   
  switch(txType){
    case 0:
    case 3:
    case 6:
      pBuf = buf_30;
      len = 9;      
      break;
    case 1:
      pBuf = buf_1010;
      len = 9;
      break;
    case 2:
      pBuf = buf_1012;
      len = 9;
      break;
    case 4:
      pBuf = buf_1b;
      len = 9;
      break;
    case 5:
      pBuf = buf_20;
      len = 9;
      break;
    case 7:
      pBuf = buf_31;
      len = 9;
      break;
    case 8:
      pBuf = buf_40;
      len = 9;
      break;    
    default:
      pBuf = NULL;
      len = 0;
      break;
  }
  if (pBuf && len) {
    switch (packetType) {
      case UNKNOWN: // alternate packet types we send till we get one back we recognize
        if (lastPacketType == NB) {
          lastPacketType = XM;
        }
        else {
          len = convert5AA5(pBuf, len, nBuf, true);
          pBuf = nBuf;
          lastPacketType = NB;
        }
        break;
      case XM:
        break;
      case NB:
        len = convert5AA5(pBuf, len, nBuf, true);
        pBuf = nBuf;
        break;
    }
    SerialTX.write(pBuf, len);
    disp_packet(PKT_TX, pBuf, len);
  }

  //Read reply from BMS
  bool isGood = readPacket(SerialRX); //checkCRC(packet, packetSize) is returned
  if (isGood && packetSize) {
    updateResponseCache();
    disp_packet(PKT_RX, packet, packetSize);
  }

  if (lastBleTxBuf && lastBleTxSize) {
    disp_packet(PKT_BLE_TX, lastBleTxBuf, lastBleTxSize);
    lastBleTxBuf = NULL;
    lastBleTxSize = 0;
  }

  txTypeIdx++;
  delay(200); // bluetooth stack will go into congestion, if too many packets are sent

  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    Serial.println("connecting");
    oldDeviceConnected = deviceConnected;
  }

}
