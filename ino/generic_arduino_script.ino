#include <EEPROM.h>


//CRC-8 - based on the CRC8 formulas by Dallas/Maxim
//code released under the therms of the GNU GPL 3.0 license
byte CRC8(const byte *data, byte len) {
  byte crc = 0x00;
  while (len--) {
    byte extract = *data++;
    for (byte tempI = 8; tempI; tempI--) {
      byte sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum) {
        crc ^= 0x8C;
      }
      extract >>= 1;
    }
  }
  return crc;
}

void setup() {
  // put your setup code here, to run once:
  int address = 0;
  byte value[16];
  boolean newdevice = true;

  // Initiate serial communication
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  // Read the registration ID from EEPROM
  for (int i = 0 ; i < 16; i++) {
    value[i] = EEPROM.read(address);
    if ( value[i] != 0 ) { newdevice = false; }
  }
  if (newdevice) {
    Serial.println( "unregistered device - no reg ID or device type has been set" );
  } else {
    Serial.println( "device has registration number" );
  }
}

void loop() {
   byte value;// = byte( 0x05 );
   byte str[] = {128, 128, 128, 128 };
  value = CRC8( &str[0], (byte) 5 );
}
