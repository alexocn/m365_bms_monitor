/*  
  based on example ESP32/BLE BLE_UART,   use "M655Tools" or M365Battery(cells don't work now, only receives 20bytes)
  works with BMS version 1.1.5 which uses 55AA protocol
  works with M365Tools v1.2.8, v1.3.0 using 55AA protocol
  works with M365Tools V1.3.4 using 5AA5 protocol(uncomment define NBOTFORMAT)  
  I used it for testing BLE android apps to determine which protocols they use and spoofed BMS messages(no actual connection to BMS)
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

uint8_t txTypeIdx = 0;

//Use New format (M365 Tools v1.3.2 or later)
//define NBOTFORMAT

#ifndef NBOTFORMAT
byte xmbuf_1012[]= {0x55, 0xAA, 0x14, 0x25, 0x01, 0x10, 0x33, 0x4A, 0x43, 0x47, 0x4A, 0x31, 0x38, 0x41, 0x54, 0x44, 0x31, 0x31, 0x32, 0x35, 0x15, 0x01, 0x78, 0x1E, 0xAD, 0xFB}; //26 bytes
byte xmbuf_1b04[]= {0x55, 0xAA, 0x06, 0x25, 0x01, 0x1B, 0x16, 0x00, 0x2B, 0x00, 0x77, 0xFF}; //12 bytes
byte xmbuf_2006[]= {0x55, 0xAA, 0x08, 0x25, 0x01, 0x20, 0x3B, 0x24, 0x00, 0x00, 0x00, 0x00, 0x52, 0xFF}; //14 bytes
byte xmbuf_310a[]= {0x55, 0xAA, 0x0C, 0x25, 0x01, 0x31, 0x31, 0x1A, 0x55, 0x00, 0x00, 0x00, 0x78, 0x0F, 0x2F, 0x2F, 0x17, 0xFE}; //18 bytes
byte xmbuf_401e[]= {0x55, 0xAA, 0x20, 0x25, 0x01, 0x40, 0x6B, 0x0F, 0x6A, 0x0F, 0x6F, 0x0F, 0x6A, 0x0F, 0x6E, 0x0F, 0x84, 0x0F, 0x84, 0x0F, 0x87, 0x0F, 0x86, 0x0F, 0x86, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 0xFA}; //38 bytes
#else
byte nbbuf_1012[]= {0x5A, 0xA5, 0x12, 0x22, 0x3E, 0x01, 0x10, 0x33, 0x4A, 0x43, 0x47, 0x4A, 0x31, 0x38, 0x41, 0x54, 0x44, 0x31, 0x31, 0x32, 0x35, 0x15, 0x01, 0x78, 0x1E, 0x74, 0xFB}; //27 bytes
byte nbbuf_1b04[]= {0x5A, 0xA5, 0x04, 0x22, 0x3E, 0x01, 0x1B, 0x16, 0x00, 0x2B, 0x00, 0x3E, 0xFF}; //13 bytes                                                       
byte nbbuf_2006[]= {0x5A, 0xA5, 0x06, 0x22, 0x3E, 0x01, 0x20, 0x3B, 0x24, 0x19, 0x00, 0x00, 0x00, 0x00, 0xFF}; //15 bytes
byte nbbuf_310a[]= {0x5A, 0xA5, 0x0A, 0x22, 0x3E, 0x01, 0x31, 0x31, 0x1A, 0x55, 0x00, 0x00, 0x00, 0x78, 0x0F, 0x2F, 0x2F, 0xDE, 0xFD}; //19 bytes
byte nbbuf_401e[]= {0x5A, 0xA5, 0x1E, 0x22, 0x3E, 0x01, 0x40, 0x6B, 0x0F, 0x6A, 0x0F, 0x6F, 0x0F, 0x6A, 0x0F, 0x6E, 0x0F, 0x84, 0x0F, 0x84, 0x0F, 0x87, 0x0F, 0x86, 0x0F, 0x86, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF3, 0xF9}; //39 bytes
#endif

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

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {

        Serial.print("RX:");
        for (int i = 0; i < rxValue.length(); i++){
          if(rxValue[i]<16) Serial.print('0');
          Serial.print(rxValue[i], HEX);          
        }
        Serial.println();

#ifndef NBOTFORMAT
        //I hijacked some request which are not 1:1, but it seems it works, M365Tools still process responses no matter what it requested
        if(rxValue[5]==0x1A) {
          pTxCharacteristic->setValue(xmbuf_1012, 20); //26
          pTxCharacteristic->notify();
          for(uint16_t idx=0; idx<1000; ++idx) ++txTypeIdx; //delay
          pTxCharacteristic->setValue(xmbuf_1012+20, 6); 
          pTxCharacteristic->notify();
        }
        if(rxValue[5]==0x67) {pTxCharacteristic->setValue(xmbuf_1b04, 12); pTxCharacteristic->notify(); }        
        if(rxValue[5]==0x72) {pTxCharacteristic->setValue(xmbuf_2006, 14); pTxCharacteristic->notify(); }
        if(rxValue[5]==0x30) {pTxCharacteristic->setValue(xmbuf_310a, 18); pTxCharacteristic->notify(); }        
        if(rxValue[5]==0x40) {
          pTxCharacteristic->setValue(xmbuf_401e, 20); //38
          pTxCharacteristic->notify();
          for(uint16_t idx=0; idx<1000; ++idx) ++txTypeIdx; //delay
          pTxCharacteristic->setValue(xmbuf_401e+20, 18);
          pTxCharacteristic->notify();
        }

        
        //m365battery app
        if(rxValue[5]==0x10&&rxValue[6]==0x12) {pTxCharacteristic->setValue(xmbuf_1012, 26); pTxCharacteristic->notify(); }
        if(rxValue[5]==0x1B&&rxValue[6]==0x04) {pTxCharacteristic->setValue(xmbuf_1b04, 12); pTxCharacteristic->notify(); }        
        if(rxValue[5]==0x20&&rxValue[6]==0x02) {pTxCharacteristic->setValue(xmbuf_2006, 14); pTxCharacteristic->notify(); }
        if(rxValue[5]==0x31&&rxValue[6]==0x0A) {pTxCharacteristic->setValue(xmbuf_310a, 18); pTxCharacteristic->notify(); }
        //if(rxValue[5]==0x40&&rxValue[6]==0x1E) {pTxCharacteristic->setValue(xmbuf_401e, 38); pTxCharacteristic->notify(); Serial.println("\tCELLS"); }
        if(rxValue[5]==0x40&&rxValue[6]==0x1E) {
          //pTxCharacteristic->setValue(xmbuf_401e, 38);
          pTxCharacteristic->setValue(xmbuf_401e, 20); //38
          pTxCharacteristic->notify();
          for(uint16_t idx=0; idx<1000; ++idx) ++txTypeIdx; //delay
          pTxCharacteristic->setValue(xmbuf_401e+20, 18);
          pTxCharacteristic->notify();
          Serial.println("XM_CELLS");
        }
#else
        //I hijacked some request which are not 1:1, but it seems it works, M365Tools still process responses no matter what it requested
        if(rxValue[6]==0x10) {
          pTxCharacteristic->setValue(nbbuf_1012, 20); //27
          pTxCharacteristic->notify();
          for(uint16_t idx=0; idx<1000; ++idx) ++txTypeIdx; //delay
          pTxCharacteristic->setValue(nbbuf_1012+20, 7);
          pTxCharacteristic->notify(); Serial.println("\tNB_Serial");
        }
        if(rxValue[6]==0x1A) {pTxCharacteristic->setValue(nbbuf_1b04, 13); pTxCharacteristic->notify(); Serial.println("\tNB_CYCLES"); }
        if(rxValue[6]==0x72) {pTxCharacteristic->setValue(nbbuf_2006, 15); pTxCharacteristic->notify(); Serial.println("\tNB_ManDate"); }
        if(rxValue[6]==0x30) {pTxCharacteristic->setValue(nbbuf_310a, 19); pTxCharacteristic->notify(); Serial.println("\tNB_BATTMAIN"); }
        if(rxValue[6]==0x40) {        
          pTxCharacteristic->setValue(nbbuf_401e, 20); //39
          pTxCharacteristic->notify();
          for(uint16_t idx=0; idx<1000; ++idx) ++txTypeIdx; //delay
          pTxCharacteristic->setValue(nbbuf_401e+20, 19);
          pTxCharacteristic->notify(); Serial.println("\tNB_CELLS");
        }
#endif 
        
        
      }
    }
};


void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  //BLEDevice::setMTU(64); //doesn't work without doing the same on the client side
  BLEDevice::init("MiScooter6560"); //"Mi Electric Scooter", "Xiaomi M365", "MiScooter6560"

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY); //CALE, consider READ property 'BLECharacteristic::PROPERTY_READ |'
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

  if (deviceConnected) {
    //pTxCharacteristic->setValue(xmbuf_1012, 20);
    //for(uint16_t idx=0; idx<1000; ++idx) ++txTypeIdx; //delay
    //pTxCharacteristic->setValue(xmbuf_1012+20, 6);
    //pTxCharacteristic->notify();
    
    txTypeIdx++;
    delay(200); // bluetooth stack will go into congestion, if too many packets are sent
  }

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
    Serial.println("CALE::connecting");
    oldDeviceConnected = deviceConnected;
  }

}
