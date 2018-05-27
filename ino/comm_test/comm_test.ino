#include <EEPROM.h>
#include <SoftwareSerial.h>

SoftwareSerial mySerial(12, 11); // RX, TX

// Some general configuration data for the Serial communication
const long SerialTimeout      = 500;
const long MessageTimeout     = 100;
const long poolingDelay       = 500;
const long poolingResetDelay  = 10000;
const byte CRCretries         = 5;
const byte registration_bytes = 6;

// message structure
struct message_struct {
  byte type;
  byte len;
  byte vars[200];
};

// the communcation buffers and structured data
byte incomingBuffer[255];
byte outgoingBuffer[255];
message_struct incomingMsg;
// message_struct outgoingMsg; // NOT NEEDED, I can directly create the output string

// Message constants
const byte MSG_START_CHAR = 50;
const byte MSG_END_CHAR = 10;

const byte MSGTYPE_RESET = 3;
const byte MSGTYPE_REGISTER = 1;
const byte MSGTYPE_NOT_REGISTERED = 4;
const byte MSGTYPE_REGISTER_PROCESSED = 2;
const byte MSGTYPE_GET_CONFIG = 5;
const byte MSGTYPE_LIST_DEVICES = 6;
const byte MSGTYPE_ERROR = 9;
const byte MSGTYPE_OK = 8;

const byte MSGTYPE_HELLO = 1;
const byte MSGTYPE_KEEPALIVE = 2;
const byte MSGTYPE_SETREGID = 3;

const byte MSGSUBTYPE_CRCERROR = 6;
const byte MSGSUBTYPE_TIMEOUT = 1;
const byte MSGSUBTYPE_INVALID_MSG = 2;
const byte MSGSUBTYPE_MSG_OVERFLOW = 3;
const byte MSGSUBTYPE_INCOMPL_MSG = 4;


// Other Global variables
boolean registered_to_dcu = false;      // boolean to keep track if the MCU has succesfully registered to DCU
byte v_CRCretries = CRCretries;         // Keep track of the number of CRC error messages we recieved
byte interrupt_arr[255];                // List of all interrupts the DCU asks us to process.
byte interrupt_num = 0;                 // Number of registered interrupts (offset of the interrupt array)

// Function to calculate the CRC8 value of a string
byte CRC8(const byte *data, byte len) {
  byte crc = 0x00;
  for (byte i = 0; i < len; i++)
  {
    byte extract = *data++;
    crc ^= extract;
    for (byte j = 0; j < 8; j++)
    {
      if (crc & 1)
        crc ^= 0x91;
      crc >>= 1;
    }
  }
  return crc;
}
/*byte CRC8(const byte *data, byte len) {
  byte crc = 0x00;
  while (len--) {
    byte extract = *data++;
    for (byte tempI = 0; tempI<8; tempI++) {
      byte sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum) {
        crc ^= 0x8C;
      }
      extract >>= 1;
    }
  }
  return crc;
}*/

// Send the outgoing messages using the type. length and variables array
void send_message( byte type, byte len, const byte *vars) {
  byte i = 0;
  outgoingBuffer[i] = MSG_START_CHAR;        i++;
  outgoingBuffer[i] = type;                  i++;
  outgoingBuffer[i] = len;                   i++;

  if (len > 0) {
    for (byte j = 0; j<len; j++) {
      byte extract = *vars++;
      outgoingBuffer[i] = extract;           i++;
    }
  }
  outgoingBuffer[i] = CRC8( &outgoingBuffer[1], (len+2) ); i++;
  outgoingBuffer[i] = MSG_END_CHAR;
  
  // outgoing buffer is ready - we can send it
  for (byte j = 0; j<=i; j++) {
    mySerial.write(outgoingBuffer[j]);
Serial.println(outgoingBuffer[j]);
  }
}

// Got an error message from DCU - last available message needs to be resent.
void resend_last_message() {
  for (byte i = 0; i<(outgoingBuffer[2]+4); i++) {
    mySerial.write(outgoingBuffer[i]);
  }
}

// Set the incoming messages type. length and variables array using the incoming buffer
void set_incoming_message()  {
  incomingMsg.type = incomingBuffer[0];
  incomingMsg.len  = incomingBuffer[1];
  if (incomingMsg.len > 0) {
    for (byte i = 0; i<incomingMsg.len; i++) {
      incomingMsg.vars[i] = incomingBuffer[(2+i)];
    }
  }  
}

// shortcut to send some messages without having to bother about variables and so on
// control message
void send_control_message(byte msg_type){
  send_message( msg_type, 0, {} );
}
// Generic error message (without variables)
void send_error_message(byte error_type){
  byte errorBuffer[1] = {error_type};
  send_message( MSGTYPE_ERROR, 1, &errorBuffer[0] );
}
// Overloaded function to enable also error message sending with some variables array
void send_error_message(byte error_type, const byte *vars, byte var_len){
  byte errorBuffer[(1+var_len)];
  errorBuffer[0] = error_type;
  byte i = 0;
  while (var_len--) {
    i++;
    byte extract = *vars++;
    errorBuffer[i] = extract;
  }
  send_message( MSGTYPE_ERROR, 1, &errorBuffer[0] );
}

// Read an incoming message - do some flow control for CRC , valid string and timeout checking
// Non blocking - if no message is available, returns an empty message in the incoming message struct
// but if at least 1 character is available on the serial RX, we will wait for full message (or timeout)
void read_message() {
  unsigned long timeout_counter = millis();
  byte first_char = 0;
  incomingMsg.type = 0;
  if (mySerial.available() > 0) {
    
    // Wait for a message start character
    first_char = mySerial.read();
    while ( (first_char != MSG_START_CHAR ) && ( millis()-timeout_counter>MessageTimeout ) ) { first_char = mySerial.read(); }
    if ( first_char == MSG_START_CHAR ) {

Serial.println("ZZZ");
Serial.println("X");
      byte len = mySerial.readBytesUntil( MSG_END_CHAR, incomingBuffer, 255 );
      if ( len > 0 ) {

Serial.println(char(incomingBuffer[0]) );
Serial.println(char(incomingBuffer[1]) );
Serial.println(char(incomingBuffer[2]) );
delay(1000);
Serial.println(char(incomingBuffer[3]) );
Serial.println(char(len));
Serial.println( char(incomingBuffer[1]+3) );

      // check lenght
        while (((incomingBuffer[1]+3)>len) && (millis()-timeout_counter<MessageTimeout))
        {
//Serial.println("XXX");
          byte tmp_inBuf[255];
          byte len2 = mySerial.readBytesUntil( (char) MSG_END_CHAR, tmp_inBuf, 255 );
          if ((len2+len)>255) {send_error_message(MSGSUBTYPE_MSG_OVERFLOW);}
          else {
            for (byte i=0; i<len2; i++) {
              incomingBuffer[len] = tmp_inBuf[i]; len++;
            }
          }
        }
//Serial.println(char(incomingBuffer[0]) );
//Serial.println(char(incomingBuffer[1]) );
//Serial.println(char(incomingBuffer[2]) );
//Serial.println(char(incomingBuffer[3]) );

        if (((incomingBuffer[1]+3) != len) ) {
          // message not complete - send an invalid message error
//Serial.println(char(5));
//Serial.println(char(9));
//Serial.println( (int) incomingBuffer[0] );
//Serial.println( (int) len );
//Serial.println( (incomingBuffer[1]+3) );
//delay(1000);    //Serial.println("XX");
          if (millis()-timeout_counter>MessageTimeout)  { send_error_message(MSGSUBTYPE_INCOMPL_MSG); }
          else                                          { send_error_message(MSGSUBTYPE_INVALID_MSG, &incomingBuffer[0],20); }
        } else {

//Serial.println(char(7));
//Serial.println(char(7));
//delay(1000);    //Serial.println("XX");
          // got a complete message - check the CRC
          byte crc = CRC8( &incomingBuffer[0], len-1 );
          if (crc != incomingBuffer[len-1]) {
             // CRC error in incoming message
            send_error_message( MSGSUBTYPE_CRCERROR );
          } else {
            // if message is a CRC error message, resend the message
            if (( incomingBuffer[0] == MSGTYPE_ERROR ) && ( incomingBuffer[2] == MSGSUBTYPE_CRCERROR )) {
              v_CRCretries--;
              if (v_CRCretries>0) {
                resend_last_message();
              } else {
//Serial.println('XXXX');
                send_error_message( MSGSUBTYPE_TIMEOUT );
              }
            }
//Serial.println('XXXX');
            set_incoming_message();
//Serial.println( char(incomingMsg.type) );
//Serial.println( char(incomingMsg.len) );
            v_CRCretries = CRCretries;
          }
        }
      }
    }
  }
}

// wait for (timeout) secs until a message is available - used for flow control only
void wait_for_message( int timeout = 0 ) {
  unsigned long start_time = millis();
  incomingMsg.type = 0;
  while ((incomingMsg.type==0) && ((timeout==0) || (millis()-start_time<timeout)))  {
    read_message();
  }
}

// General setup routine
void setup() {
  // Start the serial communication
  Serial.begin(9600);
  while (!Serial) { ; }
  Serial.setTimeout( SerialTimeout );
  mySerial.begin(9600);

  v_CRCretries = CRCretries;
// Serial.println( "test" );
  // Read the  registration ID saved in the EEPROM - if available  else the device is NEW
  boolean newdevice = true;
  byte reg_id[registration_bytes];
  int address = 0;
  for (int i = 0 ; i < registration_bytes; i++) {
    reg_id[i] = EEPROM.read(address);
    if ( reg_id[i] != 0 ) { newdevice = false; Serial.println("blank device"); }     // device is considered 'blank' if the first 6 bytes are '0'
  }

  // Let's wait 2 seconds if the DCU sends us a greeting
  wait_for_message( 2000 );
  while ( incomingMsg.type != MSGTYPE_HELLO ) {
Serial.println("No HELLO message - sending device reset messagef");
    // we did not receive an HELLO message. maybe the DCU thinks we are in 'running mode' and not 'setup mode'
    // Let's inform the DCU we have been resetted.
    // this is also the way to inform the DCU of our existence for new devices
    send_control_message( MSGTYPE_RESET );
    wait_for_message(200);
    if (incomingMsg.type==0) {
      delay( poolingResetDelay );  // delay the sending of the next reset, hoping we'll get a reaction from the DCU
      wait_for_message(2000);    // wait for next message from the DCU - uptill 10 seconds.
    }
  }
  if ( !newdevice ) {
    // we have a reestration ID so let's communciate this to the DCU
    send_message( MSGTYPE_REGISTER, registration_bytes, &reg_id[0] );
    wait_for_message();
    if ( incomingMsg.type == MSGTYPE_OK ) {
      // we are a registered device - the DCU knows us
      registered_to_dcu = true;
      // which means we'll ask for the configuration from the DCU
      // regarding the interrupts and thresholds the DCU wants to be notified about
      // Note: we will not explicitly handle the configs here
      //       these are standard messages from the DCU to set the interrupt data
      //       and are handled by the main loop, so we will only nbotify the DCU to 
      //       reload the active configuration by re-setting all interrupt data.
      send_control_message( MSGTYPE_GET_CONFIG );
    } // else - we are not known to the DCU - so we cannot do anything yet
      // in the main loop we'll wait for the registration of the device
  } else {
    // New device, so we'll notify the DCU that a new MCU is present
    send_control_message( MSGTYPE_NOT_REGISTERED );
    wait_for_message();
    if ( incomingMsg.type != MSGTYPE_OK ) {
      // something seems seriously wrong here - we do not get good communication with DCU
      // we might want to reset the device... todo
    };
  }
}

void loop() {
  // see if there is a message fromt he DCU that we need to process
//  read_message();
//  Serial.write( incomingMsg.type );
//  delay(2000);
  while (incomingMsg.type != 0) {
    //Serial.write( incomingMsg.type );
    // we got a message - we'll need to do something here...
    // DCU asks for device information - connected sensors and other components
    if (incomingMsg.type == MSGTYPE_LIST_DEVICES ) {
      //Serial.println( "XXXXX");
      send_control_message( MSGTYPE_OK );
    }
    
    // New registration of the device
    
    if (registered_to_dcu) {
      // set interrupt data
    
      // get a sensor reading
    
    }
    // Read the next incoming data string
    read_message();    
  }
  // now loop through the interrupts and see if there are any actions to take
  if (interrupt_num>0) {
    for (byte i = 0; i<interrupt_num; i++) {
    }
  }
}
