/*
  based on example ESP32/BLE BLE_UART,   use "M655Tools" or M365Battery(cells don't work now, only sends 20bytes)
  works with M365Tools v1.2.8, v1.3.0 using 55AA protocol
  works with M365Tools V1.3.4 using 5AA5 protocol(uncomment define NBOTFORMAT)
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;


//BMS connectrion
//I am using Serial2_TX(17pin)for transmit to BMS,  Serial0_RX(regular) to receive, while Serial0_TX is used for debugging to USB UART.
#define SerialTX Serial2
#define SerialRX Serial
//Serial2, RXD2/TXD2 16/17 are default for ESP32
//#define RXD2 16
//#define TXD2 17

uint8_t txTypeIdx = 0;
uint16_t delayIdx = 0;

byte packet[64];
uint8_t packetSize = 0;
byte nBuf[64]; //for conversion to 5AA5

//Requests to send to BMS
byte buf_1010[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x10, 0x10, 0xB9, 0xFF}; //9bytes
byte buf_1012[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x10, 0x12, 0xB7, 0xFF}; //9bytes
byte buf_1b[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x1B, 0x04, 0xBA, 0xFF}; //9bytes
byte buf_20[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x20, 0x06, 0xB3, 0xFF}; //9bytes
byte buf_30[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x30, 0x0C, 0x9D, 0xFF}; //9bytes
byte buf_31[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x31, 0x0A, 0x9E, 0xFF}; //9bytes
byte buf_40[]= {0x55, 0xAA, 0x03, 0x22, 0x01, 0x40, 0x1E, 0x7B, 0xFF}; //9bytes

byte xmbuf_1012[]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //26 bytes
byte xmbuf_1b04[]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //12 bytes
byte xmbuf_2006[]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //14 bytes
byte xmbuf_310a[]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; //18 bytes
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

size_t convert5AA5(byte *buf, size_t size, byte *bufNew){
  bufNew[0] = 0x5A;
  bufNew[1] = 0xA5;
  bufNew[2] = buf[2]-2;
  bufNew[3] = 0x22;
  bufNew[4] = 0x3E;
  memcpy(bufNew+5, buf+4, size-6); //skip first 4 bytes and last 2 bytes
  fillCRC(bufNew, size+1);
  return size+1;  
}

template<class T>
bool readPacket(T& pserial, unsigned long maxDelayMs=30){
  packetSize = 0;
  unsigned long startMs = millis();
  while(pserial.available() > 0 || millis()-startMs<maxDelayMs){
    if(pserial.available() > 0){
      byte incomingByte = pserial.read();
      if(packetSize==0){ //preambula 1st byte        
        if(incomingByte==0x55){
          packet[0] = 0x55;
          packetSize = 1;
        }
      }else if (packetSize==1){ //preambula 2nd byte
        if(incomingByte==0xAA){
          packet[1] = 0xAA;
          packetSize = 2;
        }else{
          packetSize = 0;
        }
      }else{        
        packet[packetSize] = incomingByte;
        packetSize++;
        if(packetSize>8 && packet[2]+6==packetSize) //Reached the end of the packet according to packet[2] byte
          break;
        if(packetSize>=40)
          break;
      }
    }
  }
  while(pserial.available()) pserial.read(); //flush the remaining of the serial buffer, something is off.

  //check integrity of the packet
  //minimal size of the packet is 9 with payload 1byte, packet[2] is size of the packet minus 2(preambula)+2(bLen + type)+2(CRC)
  if(packetSize > 8 && packetSize<40 && packet[2]+6==packetSize){    
    if(checkCRC(packet, packetSize)){
      //CRC is good
      return true;
    }
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

void TxPacket(byte *buf, size_t bsize, uint8_t fType){
  //breaks packet if needed to 20 bytes chunks
  if(bsize>40)return; //we don't expect more than 2 packets for M365 protocol

  if(fType==2){ //Convert to 5AA5 format for BLE app
    bsize = convert5AA5(buf, bsize, nBuf);
    buf = nBuf;
  }

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

      uint8_t fType = 0;
      uint8_t reqId;
      if(rxValue[0]==0x55 && rxValue[1]==0xAA){
        fType = 1;
        reqId = rxValue[5];
      }
      if(rxValue[0]==0x5A && rxValue[1]==0xA5){
        fType = 2;
        reqId = rxValue[6];        
      }
      
      if(fType == 0) return;

      //I hijacked some request which are not 1:1, but it seems it works, M365Tools still process responses no matter what it requested
      if(reqId==0x1A) {
        if(checkCRC(xmbuf_1012, 26)){
          TxPacket(xmbuf_1012, 26, fType);
        }
      }
      if(reqId==0x67) {
        if(checkCRC(xmbuf_1b04, 12)){
          TxPacket(xmbuf_1b04, 12, fType);
        }
      }
      if(reqId==0x72) { 
        if(checkCRC(xmbuf_2006, 14)){
          TxPacket(xmbuf_2006, 14, fType);
        }
      }
      if(reqId==0x30) {
        if(checkCRC(xmbuf_310a, 18)){
          TxPacket(xmbuf_310a, 18, fType);
        }
      }
      if(reqId==0x40) {
        if(checkCRC(xmbuf_401e, 38)){
          TxPacket(xmbuf_401e, 38, fType);
        }
      }

    }
  }
};


void setup() {
  Serial.begin(115200);
  Serial2.begin(115200); //, SERIAL_8N1, RXD2, TXD2
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

void loop() {

  //make sure nothing in transmit or received buffer for BMS Serial
  SerialTX.flush();
  while(SerialRX.available()) SerialRX.read();
  
  //Send request to BMS, alternate request based on txTypeIdx iterator, 0x30 is sent 3 times more frequent than others
  uint8_t txType = txTypeIdx%9;   
  switch(txType){
    case 0:
    case 3:
    case 6:
      SerialTX.write(buf_30,9);
      break;
    case 1:
      SerialTX.write(buf_1010,9);
      break;
    case 2:
      SerialTX.write(buf_1012,9);
      break;
    case 4:
      SerialTX.write(buf_1b,9);
      break;
    case 5:
      SerialTX.write(buf_20,9);
      break;
    case 7:
      SerialTX.write(buf_31,9);
      break;
    case 8:
      SerialTX.write(buf_40,9);
      break;    
    default:
      break;
  }

  //Read reply from BMS
  bool isGood = readPacket(SerialRX); //checkCRC(packet, packetSize) is returned
  
  //Parse packet consistency(length and CRC) and copy to the corresponding xmbuf
  if(packetSize<40 && packet[2]+6==packetSize){
    //valid size of the packet, packet[2](payload + 2 bytes for mod_type and command) and doesn't include preambula, CRC, bLen, Destination)
    if(checkCRC(packet, packetSize)){
      //integrity and CRC are good
      if(packet[5]==0x10 && packet[2]==0x14) //1012
        memcpy(xmbuf_1012, packet, packetSize);
      if(packet[5]==0x1B && packet[2]==0x06) //1b04
        memcpy(xmbuf_1b04, packet, packetSize);
      if(packet[5]==0x20 && packet[2]==0x08) //2006
        memcpy(xmbuf_2006, packet, packetSize);
      if(packet[5]==0x31 && packet[2]==0x0C) //310a
        memcpy(xmbuf_310a, packet, packetSize);
      if(packet[5]==0x40 && packet[2]==0x20) //401e
        memcpy(xmbuf_401e, packet, packetSize);
    }
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
