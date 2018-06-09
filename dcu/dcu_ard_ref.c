// -------------------------------------------------------------------------
// DCU_ref - Reference file for a DCU implementation
//
// Reference file for the DCU implementations for any device that needs
// to be integrated in the architecture.
// It handles basic registration with the CCU. How these commands are
// passed onto the specific device (or handled directly by the DCU_ref)
// is up to the developper extending the program for its specific purpose.
//
// -------------------------------------------------------------------------
// Version History
// 1.0 - RvR - 18.05.2018 - Initial version
//
// -------------------------------------------------------------------------

// Includes
#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include  <stdbool.h>

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>


#include  "../includes/ccu-memtypes.h"

#define ZMQ_CCU_RU_CONNECT "tcp://localhost:5550"
#define ZMQ_CCU_SUB        "tcp://localhost:5552"
#define ZMQ_CCU_PUSH       "tcp://localhost:5551"

#define DCU_RETRY_MCU_REG 30


#define FALSE 0
#define TRUE 1

// Global variables
  int  dcu_id;
  char buffer [MAX_MSGSIZE];
  bool mcu_registered = 0;


  void *zmq_context;
  void *zmq_registrator;
  void *zmq_ccu_push;
  void *zmq_ccu_sub;

// some globals that can be changed at runtime
int MSG_TIMEOUT = 2500;			// in ms
int WAIT_MSG_TIMEOUT = 1700;		// in ms
int KEEP_ALIVE_FREQ = 15;		// in seconds - 0 is do not send keep alive
int KEEP_ALIVE_TIMEOUT = 30;		// in seconds - 0 is no timeout
int MAX_TRANSMISSION_ERRORS = 5;

// Message constants
const unsigned char MSG_START_CHAR = 50;
const unsigned char MSG_END_CHAR = 13;


/*#define MSGTYPE_OK 4
#define MSGTYPE_NOK 5
#define MSGTYPE_RESET 3
#define MSGTYPE_TRANSMISSION_ERROR 8
#define MSGTYPE_GENERIC_ERROR 9
#define MSGTYPE_SET_SERIAL_TIMEOUT 6
#define MSGTYPE_KEEPALIVE 2

#define MSGTYPE_HELLO 1
#define MSGTYPE_GET_CONFIG 11
#define MSGTYPE_GET_DEVICE_INFO 12
#define MSGTYPE_SET_DEVICE_INFO 16
#define MSGTYPE_NOT_REGISTERED 14
#define MSGTYPE_REGISTRATION_ID 15
#define MSGTYPE_SET_REG_ID 10

#define MSGTYPE_GET_DATETIME 20
#define MSGTYPE_PRINT_DATETIME 22
#define MSGTYPE_SET_DATETIME 21




#define MSGSUBTYPE_CRCERROR 6
#define MSGSUBTYPE_TIMEOUT 1
#define MSGSUBTYPE_TIMEOUT_ON_START 1
#define MSGSUBTYPE_INVALID_MSG 2
#define MSGSUBTYPE_MSG_OVERFLOW 3
#define MSGSUBTYPE_UNEXPECTED_INPUT 4
*/

// These settings can not be changed during runtime
#define REGISTRATION_BYTES 6
#define MAX_MSG_LENGTH 255

time_t last_sent_message_time, last_rcv_message_time;

/* Some global communication settings */

/* baudrate settings are defined in <asm/termbits.h>, which is
   included by <termios.h> */
#define BAUDRATE B9600

/* change this definition for the correct port */
#define MCU_DEVICE "/dev/rfcomm0"
#define _POSIX_SOURCE 1 /* POSIX compliant source */

struct message_struct {
  unsigned char type;
  int len;
  unsigned char vars[MAX_MSG_LENGTH];
};

// global variables declaration
unsigned char v_send_buffer[MAX_MSG_LENGTH];
int serial_fd;
struct termios newtio;
struct message_struct in_message;
unsigned char registration_id[REGISTRATION_BYTES];
int transmission_error = 0;


#define CRC7_POLY 0x91
unsigned char getCRC(unsigned char message[], unsigned char length)
{
  unsigned char i, j, crc = 0;
  for (i = 0; i < length; i++) {
    crc ^= message[i];
    for (j = 0; j < 8; j++) {
      if (crc & 1)
        crc ^= CRC7_POLY;
      crc >>= 1;
    }
  }
  return crc;
}

struct termios oldtio;

int setup_serial_comm() {
	int    ret_fd;
	int    c;
       /*
          Open modem device for reading and writing and not as controlling tty
          because we don't want to get killed if linenoise sends CTRL-C.
        */
        ret_fd = open(MCU_DEVICE, O_RDWR | O_NONBLOCK );
        if (ret_fd <0) {perror(MCU_DEVICE); exit(-1); }

        tcgetattr(ret_fd,&oldtio); /* save current serial port settings */
        bzero(&newtio, sizeof(newtio)); /* clear struct for new port settings */

        /*
          BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
          CRTSCTS : output hardware flow control (only used if the cable has
                    all necessary lines. See sect. 7 of Serial-HOWTO)
          CS8     : 8n1 (8bit,no parity,1 stopbit)
          CLOCAL  : local connection, no modem contol
          CREAD   : enable receiving characters
        */
        newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;

        /*
          IGNPAR  : ignore bytes with parity errors
          ICRNL   : map CR to NL (otherwise a CR input on the other computer
                    will not terminate input)
          otherwise make device raw (no other input processing)
        */
        newtio.c_iflag = 0;

        /*
         Raw output.
        */
        newtio.c_oflag = 0;

        /*
          ICANON  : enable canonical input
          disable all echo functionality, and don't send signals to calling program
        */
        newtio.c_lflag = 0;

        /*
          initialize all control characters
          default values can be found in /usr/include/termios.h, and are given
          in the comments, but we don't need them here
        */
        newtio.c_cc[VINTR]    = 0;     /* Ctrl-c */
        newtio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
        newtio.c_cc[VERASE]   = 0;     /* del */
        newtio.c_cc[VKILL]    = 0;     /* @ */
        newtio.c_cc[VEOF]     = 4;     /* Ctrl-d */
        newtio.c_cc[VTIME]    = 5;     /* inter-character timer unused */
        newtio.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrives */
        newtio.c_cc[VSWTC]    = 0;     /* '\0' */
        newtio.c_cc[VSTART]   = 0;     /* Ctrl-q */
        newtio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
        newtio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
        newtio.c_cc[VEOL]     = 0;     /* '\0' */
        newtio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
        newtio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
        newtio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
        newtio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
        newtio.c_cc[VEOL2]    = 0;     /* '\0' */

        /*
          now clean the modem line and activate the settings for the port
        */
        tcflush(ret_fd, TCIFLUSH);
        tcsetattr(ret_fd,TCSANOW,&newtio);
	unsigned char read_char;
	while (read(ret_fd, &read_char, 1)>0) {}
	return (ret_fd);
}

void submit_send_buffer() {
	write(serial_fd, &MSG_START_CHAR,1);
	write(serial_fd, v_send_buffer,(v_send_buffer[1]+3));
	write(serial_fd, &MSG_END_CHAR,1);

	// set the time of last sent message for keep alive counter
	struct timeval cur_time;
	gettimeofday (&cur_time, NULL);
	last_sent_message_time = cur_time.tv_sec;
}

void send_message( unsigned char p_msg_type, int p_len, unsigned char p_buf[MAX_MSG_LENGTH] ) {

	printf( "ready to send a command\n" );
	printf( " - Msg type  : %i \n", p_msg_type );
	printf( " - length    : %i \n", p_len );

	v_send_buffer[0] = p_msg_type;
	v_send_buffer[1] = p_len;

	for(int i=0; i<(p_len); i++)  {
		printf( " - variable  : %i \n", p_buf[i] );
		v_send_buffer[(i+2)] = p_buf[i];
	}
	v_send_buffer[(p_len+2)] = getCRC(v_send_buffer, (p_len+2));
	printf( " - CRC sum   : %i \n", v_send_buffer[(p_len+2)] );

	printf( "Full message is now  %02x ", MSG_START_CHAR );
	for(int i=0; i<(p_len+3); i++)  {
		printf( "%02x ", v_send_buffer[i] );
	}
	printf( "%02x ", MSG_END_CHAR );

	printf( "\n----------------------------------\n" );
	submit_send_buffer();
}
void send_control_message( unsigned char p_msg_type ) {
	unsigned char p_buf[1];
	send_message( p_msg_type, 0, p_buf );
}
void send_transmission_error( unsigned char p_error_type ) {
	unsigned char p_buf[1];
	transmission_error++;
	if ( transmission_error > MAX_TRANSMISSION_ERRORS ) {
		send_control_message( MSG_SAMP_RESET );
//		transmission_error = 0;
	} else {
		p_buf[0] = p_error_type;
		send_message( MSG_SAMP_TRANSM_ERR, 1, p_buf );
	}
}

bool read_incoming_message() {
	int res = 0;
	int read_indx = 0;
	unsigned char p_buf[MAX_MSG_LENGTH];
	unsigned char read_char;
	bool read_complete = FALSE;
	bool return_value = TRUE;

	struct timeval start_time, cur_time;
	gettimeofday (&start_time, NULL);

	in_message.type = 0;
	res = read(serial_fd, &read_char, 1);
	if (res<0) {
//printf( "NO INPUT MESSAGE - %i\n", res);
		return(FALSE);
	} else {
//printf( "We have an input char - %i - %i\n", res, read_char );sleep(10);
		while ((read_char != MSG_START_CHAR) && (return_value)) {
			if ( ( read_char != 0 ) && ( read_char != 33 ) ) {
//				printf( "we got an INVALID start character %i\n", read_char );
			}
			res = read(serial_fd, &read_char, 1);

			gettimeofday (&cur_time, NULL);
			if ( (((cur_time.tv_sec - start_time.tv_sec)*1000
			           +cur_time.tv_usec/1000) - start_time.tv_usec/1000 ) > MSG_TIMEOUT ) {
				return_value = FALSE;
				//send_transmission_error(ERR_TYPE_TIMEOUT);
			}
		}
		while ((return_value) && !(read_complete)) {
//printf( "VALID start character\n" );
			res = read(serial_fd, &read_char, 1);
			if (res>=0) {
				if (read_indx < 2) {
//printf( "res = %i and next char = %i\n", res, read_char );
					p_buf[read_indx] = read_char;
					read_indx++;
				} else {
					if ( read_indx == (3+p_buf[1]) ) {
						if ( MSG_END_CHAR == read_char ) {
							read_complete = TRUE;
						} else {
							// invalid message
							send_transmission_error(ERR_TYPE_INV_MSG);
printf( "unexpected end char %i\n ", read_char );
							return_value = FALSE;
						}
					} else {
//printf( "next char = %i\n", read_char );
						p_buf[read_indx] = read_char;
						read_indx++;
					}
				}
			}
			gettimeofday (&cur_time, NULL);
			if ( (((cur_time.tv_sec - start_time.tv_sec)*1000
			           +cur_time.tv_usec/1000) - start_time.tv_usec/1000 ) > MSG_TIMEOUT ) {
				return_value = FALSE;
				send_transmission_error(ERR_TYPE_TIMEOUT);
			}
		}
		if (read_complete) {
//printf( "calculated CRC is %i \n", getCRC(p_buf, (read_indx-1)) );
			if ( p_buf[(read_indx-1)] != getCRC(p_buf, (read_indx-1)) ) {
				// CRC error
				send_transmission_error(ERR_TYPE_CRC_ERR);
			} else {
				// got a valid message - setting last rcv message for keep alive timeout
				gettimeofday (&cur_time, NULL);
				last_rcv_message_time = cur_time.tv_sec;

				// if the emssage is a transmission error, I will handle it here not return it
				if (p_buf[0] == MSG_SAMP_TRANSM_ERR) {
					submit_send_buffer();
					return_value = FALSE;
				} else {
					// Everything is OK - so I'll set the input message now
printf( "Got a valid input message - %i - len = %i\n", p_buf[0], p_buf[1] );
					in_message.type = p_buf[0];
					in_message.len  =  p_buf[1];
					if (p_buf[1] > 0) {
printf( "Message has a payload " );
						for (int i=0;i<p_buf[1];i++) {
printf( "- %i ", p_buf[(i+2)] );
							in_message.vars[i] = p_buf[(i+2)];
						}
printf( "\n" );
					}
					return_value = TRUE;
				}
			}
		}
	}
	return(return_value);
}

bool wait_for_in_message( int p_timeout ) {	// wait for a message for p_timeout ms - used for flow control
	bool return_value = FALSE;
	bool timeout = FALSE;
	struct timeval start_time, cur_time;
	gettimeofday (&start_time, NULL);
	if (p_timeout == 0 ) { p_timeout = WAIT_MSG_TIMEOUT; }
	while ( (!(return_value)) && (!(timeout)) ) {
//		printf( "waiting for input message\n" );
usleep( 500000);
		return_value = read_incoming_message();
		if ( !(return_value) ) {
			gettimeofday (&cur_time, NULL);

//printf( "CUR TIME : sec %i usec %i \n", cur_time.tv_sec, cur_time.tv_usec );
//printf( "LST TIME : sec %i usec %i \n", start_time.tv_sec, start_time.tv_usec );
			if ( (((cur_time.tv_sec - start_time.tv_sec)*1000
			           +cur_time.tv_usec/1000) - start_time.tv_usec/1000 ) > p_timeout ) {
				timeout = TRUE;
//				printf( "Timeout waiting for input message\n" );
			}
		}
	}
	return(return_value);
}
bool flush_in_buffer() {	// Flushes all messages in the buffer - last message is kept
				// return true if there is a message (incomingMsg set to last read msg
	int last_msg_type = 0;

	while ( read_incoming_message() ) {
		last_msg_type = in_message.type;
	}
	in_message.type = last_msg_type;
	return (last_msg_type == 0);
}


void main() {


  // Set up connection to CCU-RU
  printf ("Connecting CCU Registration Unit\n");
  zmq_context = zmq_ctx_new ();
  zmq_registrator = zmq_socket (zmq_context, ZMQ_REQ);
  zmq_connect (zmq_registrator, ZMQ_CCU_RU_CONNECT);

  // Sending the DCU registration message
  // to announce ourselves and set up the communication channels
  printf ("Sending DCU registration message\n");
  buffer[0] = MSG_RU_REG_DCU;
  buffer[1] = 0;
  zmq_send (zmq_registrator, buffer, 2, 0);
  zmq_recv (zmq_registrator, buffer, MAX_MSGSIZE, 0);
  printf ("Received DCU configuration %i %i %i %i %i %i %i %i %i\n", buffer[0], buffer[1],
            buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8] );

  if (buffer[0] == MSG_RU_SET_DCU ) {
    printf( "OK - got a SET_DCU message back\n" );
    msg_set_dcu_t* dcu_settings = (msg_set_dcu_t*) ((char*)buffer+1);
    printf( " - DCU id   = %i\n", dcu_settings->dcu_id );
    printf( " - DCU ip   = %i.%i.%i.%i\n", dcu_settings->dcu_ip[0], dcu_settings->dcu_ip[1], dcu_settings->dcu_ip[2], dcu_settings->dcu_ip[3] );
    printf( " - DCU port = %i\n", dcu_settings->dcu_port );
    //dcu_id = dcu_settings->dcu_id;

    // Set the global variables and init the ZMQ queues
    dcu_id  = dcu_settings->dcu_id;
    printf( "set the DCU ID to %i\n", dcu_id );

//    char* zmq_input_bind = malloc(30);

    zmq_ccu_push = zmq_socket (zmq_context, ZMQ_PUSH);
//    sprintf( zmq_input_bind, "tcp://*:%i", (int)dcu_settings->dcu_port);
//    printf( "Binding the output queue on %s \n", zmq_input_bind );
    int zmq_rc = zmq_connect (zmq_ccu_push, ZMQ_CCU_PUSH);
    assert (zmq_rc == 0);
    printf( "zeromq push to ccu has been set up\n");

    zmq_ccu_sub = zmq_socket (zmq_context, ZMQ_SUB);
//    sprintf( zmq_input_bind, "tcp://*:%i", (int)(dcu_settings->dcu_port+1));
//    printf( "Binding the input queue on %s \n", zmq_input_bind );
    zmq_rc = zmq_connect (zmq_ccu_sub, ZMQ_CCU_SUB);
    assert (zmq_rc == 0);
    zmq_setsockopt( zmq_ccu_sub, ZMQ_SUBSCRIBE, &dcu_id, sizeof(int));
//    zmq_setsockopt( zmq_ccu_sub, ZMQ_SUBSCRIBE, 0, 0);
    printf( "zeromq subscribe to ccu has been set up\n");

//    free(zmq_input_bind);
    zmq_close (zmq_registrator);
  } else {
    printf("ERROR - Did not get a valid registration message for the DCU - exiting\n");
    zmq_close (zmq_registrator);
    exit(1);
  }

  serial_fd = setup_serial_comm();
	bool STOP = FALSE;
	bool SETUP_MODE = TRUE;

	/* Wait for 1.5 sec to enable the MCU to get online */
//	usleep (1500000);

	/* Main Loop */
	while (STOP==FALSE) {

		while ( SETUP_MODE ) {
			flush_in_buffer();
			/* Send a HELLO message */
			if (!(in_message.type) ) {
				send_control_message( MSG_SAMP_HELLO );
				wait_for_in_message(10000);
			}
			if ( in_message.type == MSG_SAMP_RESET ) {
				// expected to get this...
				// no action needed as we will send another greeting
printf( "Got a RESET message but I allready sent an HELLO\n" );
			}
			if ( in_message.type == MSG_SAMP_SET_REGID ) {
				printf( "got a request to register\n" );
				// check with CCU if device is registered  ToDo
        msg_reg_mcu_t* out_msg = (msg_reg_mcu_t*) (char*)buffer;
        out_msg->dcu_id = dcu_id;
        out_msg->msg_type = MSG_REG_MCU;
        for (int i=0;i<in_message.len;i++) {
					out_msg->mcu_reg_id[i] = in_message.vars[i];
				}
        int rc = zmq_recv (zmq_ccu_sub, buffer, MAX_MSGSIZE, ZMQ_NOBLOCK);
        int retry_counter = DCU_RETRY_MCU_REG;
        while (rc == -1) {
          retry_counter++;
          if ( retry_counter > DCU_RETRY_MCU_REG ) {
            // retry sending the reg id to the CCU
            printf ( "Sending a REG MCU message\n" );
            zmq_send (zmq_ccu_push, buffer, sizeof(msg_reg_mcu_t), 0);
            printf ( "Waiting reponse for REG MCU message\n" );
            retry_counter = 0;
          }
          sleep(1);
          rc = zmq_recv (zmq_ccu_sub, buffer, MAX_MSGSIZE, ZMQ_NOBLOCK);

        }
        printf( "Received buffer character %i %i %i %i %i %i %i %i %i %i %i %i\n", buffer[0]
             , buffer[1],  buffer[2],  buffer[3],  buffer[4],  buffer[5],  buffer[6],  buffer[7]
             ,  buffer[8],  buffer[9],  buffer[10],  buffer[11]);
        mcu_registered = 1;

				if (mcu_registered) {
					send_control_message( MSG_OK );
				} else  {
          // We need to check all valid answers here
          // and send an unexpected message error if needed

		//			send_control_message( MSGTYPE_GET_CONFIG );
				}
				SETUP_MODE = FALSE;
			} else {
				printf( "no answer from HELLO - I'll try again sending\n" );
				serial_fd = setup_serial_comm();
			}
		}

		if (wait_for_in_message(0)) {
//printf( "got a valid message of type %i\n", in_message.type );

			if ( in_message.type == MSG_OK ) {
				printf( "got an OK\n" );
			}
			if ( in_message.type == MSG_NOK ) {
				printf( "got an NOK\n" );
			}
			if ( in_message.type == MSG_SAMP_GET_REGID ) {
				printf( "MCU is sending me its registration id - I will set it, even if not in setup mode\n" );
				for (int i=0;i<in_message.len;i++) {
				registration_id[i] = in_message.vars[i];
				}
				// check with CCU if device is registered  ToDo

				if (TRUE) {
					send_control_message( MSG_OK );
				} else  {
          // We need to check all expected messages
          // and send an unexpected message error if needed


	//				send_control_message( MSGTYPE_GET_CONFIG );
				}
			}
			if ( in_message.type == MSG_ERROR ) {
				printf( "got an ERROR\n" );
			}
			if ( in_message.type == MSG_SAMP_RESET ) {
				printf( "got a RESET REQUEST - will enter in SETUP_MODE\n" );
				SETUP_MODE = TRUE;
			}
		}

		// KEEP ALIVE functions
		// see if I need to send a keep alive, and if I got a message lately
		// Check last message sent time to see if I need to send a keep alive
		struct timeval cur_time;
		gettimeofday (&cur_time, NULL);
//		printf( "checking time keep alive timeout - cur = %i , last = %i - sent =  %i\n", cur_time.tv_sec, last_sent_message_time, last_rcv_message_time );
		if ( ( ( cur_time.tv_sec - last_sent_message_time ) > KEEP_ALIVE_FREQ ) && ( KEEP_ALIVE_FREQ > 0 ) ) {
//			printf( "keep alive freq reached - we will send a keep alive message\n" );
			send_control_message( MSG_SAMP_KEEPALIVE );
		}
		// Check if last received message is younger than the set timeout
		if ( ( ( cur_time.tv_sec - last_rcv_message_time ) > KEEP_ALIVE_TIMEOUT ) && ( KEEP_ALIVE_TIMEOUT > 0 )) {
			// I will just reset the comm channel - no need to reset device
			printf( "communcation timeout - resetting the channel\n" );
			serial_fd = setup_serial_comm();
			last_rcv_message_time = cur_time.tv_sec;
		}

/*
		// I will now scan stdin to see if we want to inject some command here
		// used for debugging for now -- will need to be connected to api
		char inp_command[64];
		if (fgets(inp_command, sizeof inp_command, stdin) ) {
			printf( "got a manually entered command : %i\n", inp_command[0] );
			if (inp_command[0] == 100) {
printf( "Gettting the date of the MCU - NOK in case date has not been initialized\n" );
				send_control_message( MSGTYPE_GET_DATETIME );
			}
			if (inp_command[0] == 68 ) {
				time_t t = time(NULL);
				struct tm tm = *localtime(&t);
				//sprintf( in_message.vars,
				//	"%04d%02d%02d%02d%02d%02d",
				//	(tm.tm_year+1900),
				//	(tm.tm_mon+1),
				//	tm.tm_mday,
				//	tm.tm_hour,
				//	tm.tm_min,
				//	tm.tm_sec );
printf( "year to set is = ");
printf( "%i", (unsigned short)(tm.tm_year+1900) );
printf( "\n" );
				in_message.vars[0] = (unsigned short)(tm.tm_year+1900) / 256;
				in_message.vars[1] = (unsigned short)(tm.tm_year+1900);
				in_message.vars[2] = (unsigned char)(tm.tm_mon+1);
				in_message.vars[3] = (unsigned char)tm.tm_mday;
				in_message.vars[4] = (unsigned char)tm.tm_hour;
				in_message.vars[5] = (unsigned char)tm.tm_min;
				in_message.vars[6] = (unsigned char)tm.tm_sec;
//printf( "Setting the date of the MCU to %s\n", in_message.vars );
				send_message( MSGTYPE_SET_DATETIME, 7, in_message.vars );
			}
			if (inp_command[0] == 114 ) {
				//sprintf( in_message.vars, "%s%s%s%s%s%s", "\0\0\0\0\0\0" );
				in_message.vars[0] = 0;
				in_message.vars[1] = 0;
				in_message.vars[2] = 0;
				in_message.vars[3] = 0;
				in_message.vars[4] = 0;
				in_message.vars[5] = 0;
				send_message( MSGTYPE_SET_REG_ID, REGISTRATION_BYTES, in_message.vars );
			}
			if (inp_command[0] == 120) {
				STOP = TRUE;
			}
		}
*/
        }

	/* Finalize the program */

        /* restore the old port settings */
        tcsetattr(serial_fd, TCSANOW, &oldtio);
}
