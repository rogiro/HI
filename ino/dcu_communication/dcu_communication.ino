#include <EEPROM.h>
#include <SoftwareSerial.h>


// Some general configuration data for the Serial communication
const long SerialTimeout        = 500;
const long StartMsgTimeout      = 500;
const long MessageTimeout       = 1500;
//const long poolingDelay       = 500;
const long poolingResetDelay    = 10000;
const byte max_trans_retries    = 5;
const byte registration_bytes   = 6;
const byte max_msg_length       = 255;
const long baudrate             = 9600;


// message structure
struct message_struct {
  byte type;
  byte len;
  byte vars[max_msg_length];
};

// the communcation buffers and structured data
byte outgoingBuffer[(max_msg_length)];
message_struct incomingMsg;

// Message constants
const byte MSG_START_CHAR = 50;
const byte MSG_END_CHAR = 10;

// Message Types
// Generic messages
#define MSG_OK          4
#define MSG_NOK         5
#define MSG_READY       6
#define MSG_ERROR       9

// Reg Unit messages
#define MSG_RU_REG_DCU 91
#define MSG_RU_SET_DCU 92
#define MSG_RU_SHUTDOWN 99

// CCU messages
#define MSG_REG_MCU           21
#define MSG_GET_DEV_DETAILS   22
#define MSG_SET_DEV_DETAILS   23

// Error subtypes
#define ERR_TYPE_CRC_ERR    4
#define ERR_TYPE_OVERFLOW   5
#define ERR_TYPE_UNEXP_INP  6
#define ERR_TYPE_INV_MSG    7
#define ERR_TYPE_TIMEOUT    8

// SAMP messages
#define MSG_SAMP_HELLO        101
#define MSG_SAMP_RESET        102
#define MSG_SAMP_GET_REGID    103
#define MSG_SAMP_SET_REGID    104
#define MSG_SAMP_GET_MODEL    105
#define MSG_SAMP_SET_MODEL    106
#define MSG_SAMP_GET_DEVICE   107
#define MSG_SAMP_SET_DEVICE   108
#define MSG_SAMP_KEEPALIVE    109
#define MSG_SAMP_TRANSM_ERR   110
/*const byte MSGTYPE_OK = 4;
const byte MSGTYPE_NOK = 5;
const byte MSGTYPE_RESET = 3;
const byte MSGTYPE_TRANSMISSION_ERROR = 8;
const byte MSGTYPE_GENERIC_ERROR = 9;
const byte MSGTYPE_SET_SERIAL_TIMEOUT = 6;
const byte MSGTYPE_KEEPALIVE = 2;

const byte MSGTYPE_HELLO = 1;
const byte MSGTYPE_GET_DEVICES = 11;
const byte MSGTYPE_DEVICE_INFO = 12;
const byte MSGTYPE_NOT_REGISTERED = 14;
const byte MSGTYPE_REGISTRATION_ID = 15;
const byte MSGTYPE_SET_REG_ID = 13;

const byte MSGTYPE_GET_DATETIME = 20;
const byte MSGTYPE_PRINT_DATETIME = 22;
const byte MSGTYPE_SET_DATETIME = 21;

const byte MSGTYPE_READ_PIN = 30;
const byte MSGTYPE_PRINT_PIN = 31;
const byte MSGTYPE_SET_INTERUPT_BELOW = 32;
const byte MSGTYPE_SET_INTERUPT_ABOVE = 33;
const byte MSGTYPE_SET_INTERUPT_FREQ = 34;
const byte MSGTYPE_RETURN_VALUE = 35;
const byte MSGTYPE_INTERUPT_FIRED = 36;


const byte MSGSUBTYPE_CRCERROR = 6;
const byte MSGSUBTYPE_TIMEOUT = 1;
const byte MSGSUBTYPE_TIMEOUT_ON_START = 5;
const byte MSGSUBTYPE_INVALID_MSG = 2;
const byte MSGSUBTYPE_MSG_OVERFLOW = 3;
// const byte MSGSUBTYPE_INCOMPL_MSG = 4;
*/


// Other Global variables
boolean registered_to_dcu = false;      // boolean to keep track if the MCU has succesfully registered to DCU
byte v_transmission_errors = 0;         // Keep track of the number of transmission error messages we received

SoftwareSerial btSerial(12,11);

//byte interrupt_arr[255];                // List of all interrupts the DCU asks us to process.
//byte interrupt_num = 0;                 // Number of registered interrupts (offset of the interrupt array)

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

// Submit the last message that has been prepared and is in the buffer.
void submit_outgoing_buffer() {
Serial.print( "Sending a message - ");
  btSerial.write(MSG_START_CHAR);
  for (byte i = 0; i<(outgoingBuffer[1]+3); i++) {
Serial.print( (int) outgoingBuffer[i] );
Serial.print( " - " );
    btSerial.write( outgoingBuffer[i]);
  }
Serial.println( "End Char" );
  btSerial.write(MSG_END_CHAR);
}

// Prepare and send the outgoing message using the type. length and variables array
void send_message( byte type, byte len, const byte *vars) {
  byte i = 0;
  outgoingBuffer[i] = type;                  i++;
  outgoingBuffer[i] = len;                   i++;

  if (len > 0) {
    for (byte j = 0; j<len; j++) {
      byte extract = *vars++;
      outgoingBuffer[i] = extract;           i++;
    }
  }
  outgoingBuffer[i] = CRC8( &outgoingBuffer[0], (len+2) );

  // outgoing buffer is ready - we can send it
  submit_outgoing_buffer();
}

// shortcut to send some messages without having to bother about variables and so on
// control message
void send_control_message(byte msg_type){
  send_message( msg_type, 0, {} );
}
// Generic error message (without variables)
void send_transmission_error(byte error_type){
  byte errorBuffer[1] = {error_type};
  send_message( MSG_SAMP_TRANSM_ERR, 1, &errorBuffer[0] );
  v_transmission_errors++;
  if ( v_transmission_errors > max_trans_retries ) {
    send_control_message( MSG_SAMP_RESET );
    v_transmission_errors = 0;
  }
}
// Send a generic error messagee with some variables array
// Attention: never tested this function...
void send_generic_error(byte error_type, const byte *vars, byte var_len){
  byte errorBuffer[(1+var_len)];
  errorBuffer[0] = error_type;
  byte i = 0;
  while (var_len--) {
    i++;
    byte extract = *vars++;
    errorBuffer[i] = extract;
  }
  send_message( MSG_ERROR, 1, &errorBuffer[0] );
}

// Read an incoming message - do some flow control for CRC , valid string and timeout checking
// Non blocking - if no message is available, returns an empty message in the incoming message struct
// but if at least 1 character is available on the serial RX, we will wait for full message (or timeout)
boolean read_message() {
  byte incomingBuffer[max_msg_length];  // should be moved into the read funcs.
  unsigned long timeout_counter = millis();
  byte in_char = 0;
  byte in_counter = 0;
  boolean return_value = true;
  boolean read_complete = false;

//  incomingMsg.type = 0;
  if ( !(btSerial.available() > 0) )  { return_value = false; }
  else                                { in_char = btSerial.read(); }

  while ((return_value) && (in_char != MSG_START_CHAR) ) {
    in_char = btSerial.read();
    if ( millis()-timeout_counter>StartMsgTimeout ) {
      // No valid start char in the timeframe we have... interupting comm
      send_transmission_error(ERR_TYPE_TIMEOUT);
      return_value = false;
    }
  }
if ( return_value ) { Serial.println( "Seems we have a valid start char" ); }
//else                { Serial.println( "No valid start char" ); }
  while ( (return_value) && (!read_complete) ) {
    if (btSerial.available() > 0) {
      in_char = btSerial.read();
      if ( in_counter < 2 ) {
        incomingBuffer[in_counter] = in_char;
        in_counter++;
      } else {
        if ( incomingBuffer[1] + 3 == in_counter ) {
            if ( in_char == MSG_END_CHAR ) {
              read_complete = true;
            } else {
Serial.println( "Message is invalid" );
              send_transmission_error(ERR_TYPE_INV_MSG);
              return_value = false;
            }
        } else {
          incomingBuffer[in_counter] = in_char;
          in_counter++;
        }
      }
    }
    if ( millis()-timeout_counter>MessageTimeout ) {
      // Timeout before getting the full message
      send_transmission_error(ERR_TYPE_TIMEOUT);
      return_value = false;
    }
  }
  if ( read_complete ) {
Serial.println( "Got a full message - have to check its CRC" );
    byte crc = CRC8( &incomingBuffer[0], in_counter-1 );
    if (crc != incomingBuffer[in_counter-1]) {
      // CRC error in incoming message
      send_transmission_error( ERR_TYPE_CRC_ERR );
      return_value = false;
    } else {
      v_transmission_errors = 0;
      if ( incomingBuffer[0] == MSG_SAMP_TRANSM_ERR ) {
        submit_outgoing_buffer();
        return_value = false;
      } else {
        incomingMsg.type = incomingBuffer[0];
        incomingMsg.len  = incomingBuffer[1];

Serial.print( "Got a valid message: ");
Serial.print( (int) incomingMsg.type );
Serial.print( " - ");
Serial.print( (int) incomingMsg.len );

        if (incomingMsg.len > 0) {
          for (byte i = 0; i<incomingMsg.len; i++) {
            incomingMsg.vars[i] = incomingBuffer[(2+i)];
Serial.print( " - ");
Serial.print( (int) incomingMsg.vars[i] );
          }
        }
      }
    }
  }
  delay(500);
  return (return_value);
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

  btSerial.begin(baudrate);
  //while (!btSerial) { ; }
  btSerial.setTimeout( SerialTimeout );

  v_transmission_errors = 0;
// Serial.println( "test" );
  // Read the  registration ID saved in the EEPROM - if available  else the device is NEW
  boolean newdevice = true;
  byte reg_id[registration_bytes];
  int address = 0;
  for (int i = 0 ; i < registration_bytes; i++) {
    reg_id[i] = EEPROM.read(address);
    if ( reg_id[i] != 0 ) { newdevice = false; }     // device is considered 'blank' if the first 6 bytes are '0'
  }

  // Let's wait at most 2 seconds if the DCU sends us a greeting
  wait_for_message( 2000 );
  while ( incomingMsg.type != MSG_SAMP_HELLO ) {
    // we did not receive an HELLO message. maybe the DCU thinks we are in 'running mode' and not 'setup mode'
    // Let's inform the DCU we have been resetted.
    // this is also the way to inform the DCU of our existence for new devices
    send_control_message( MSG_SAMP_RESET );
    wait_for_message(200);
    if (incomingMsg.type==0) {
      delay( poolingResetDelay );  // delay the sending of the next reset, hoping we'll get a reaction from the DCU
      wait_for_message(2000);    // wait for next message from the DCU - uptill 10 seconds.
    }
  }
  if ( !newdevice ) {
    // we have a reestration ID so let's communciate this to the DCU
    send_message( MSG_SAMP_GET_REGID, registration_bytes, &reg_id[0] );
    wait_for_message();
    if ( incomingMsg.type == MSG_OK ) {
      // we are a registered device - the DCU knows us
      registered_to_dcu = true;
      // which means we'll ask for the configuration from the DCU
      // regarding the interrupts and thresholds the DCU wants to be notified about
      // Note: we will not explicitly handle the configs here
      //       these are standard messages from the DCU to set the interrupt data
      //       and are handled by the main loop, so we will only nbotify the DCU to
      //       reload the active configuration by re-setting all interrupt data.
      // send_control_message( MSGTYPE_GET_CONFIG );
    } // else - we are not known to the DCU - so we cannot do anything yet
      // in the main loop we'll wait for the registration of the device
  } else {
    // We need to check all expected messge types where
    // and in case we get an unexpected message we should send an error

    // New device, so we'll notify the DCU that a new MCU is present
//    send_control_message( MSGTYPE_NOT_REGISTERED );
//    wait_for_message();
//    if ( incomingMsg.type != MSG_OK ) {
      // something seems seriously wrong here - we do not get good communication with DCU
      // we might want to reset the device... todo
//    };
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
//    if (incomingMsg.type == MSGTYPE_GET_DEVICES ) {
      //Serial.println( "XXXXX");
//      send_control_message( MSG_OK );
//    }

    // New registration of the device

    if (registered_to_dcu) {
      // set interrupt data

      // get a sensor reading

    }
    // Read the next incoming data string
    read_message();
  }
  // now loop through the interrupts and see if there are any actions to take
//  if (interrupt_num>0) {
//    for (byte i = 0; i<interrupt_num; i++) {
//    }
//  }
}
