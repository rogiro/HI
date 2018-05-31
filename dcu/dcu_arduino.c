// Device Control Unit

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <zmq.h>


#define FALSE 0
#define TRUE 1

#define CRC_MAX_CRC_RESEND 5
#define REGISTRATION_BYTES 6

#define CCU_SERVER "tcp://localhost:5555"

/* Some global communication settings */

/* baudrate settings are defined in <asm/termbits.h>, which is
   included by <termios.h> */
#define BAUDRATE B9600

/* change this definition for the correct port */
#define MCUDEVICE "/dev/ttyACM0"
#define _POSIX_SOURCE 1 /* POSIX compliant source */


// Message constants
const unsigned char MSG_START_CHAR = 50;
const unsigned char MSG_END_CHAR = 10;

const unsigned char MSGTYPE_RESET = 3;
const unsigned char MSGTYPE_REGISTER = 1;
const unsigned char MSGTYPE_NOT_REGISTERED = 4;
const unsigned char MSGTYPE_REGISTER_PROCESSED = 2;
const unsigned char MSGTYPE_GET_CONFIG = 5;
const unsigned char MSGTYPE_LIST_DEVICES = 6;
const unsigned char MSGTYPE_ERROR = 9;
const unsigned char MSGTYPE_OK = 8;

const unsigned char MSGTYPE_HELLO = 1;
const unsigned char MSGTYPE_KEEPALIVE = 2;
const unsigned char MSGTYPE_SETREGID = 3;

const unsigned char MSGSUBTYPE_CRCERROR = 6;
const unsigned char MSGSUBTYPE_TIMEOUT = 1;
const unsigned char MSGSUBTYPE_INVALID_MSG = 2;
const unsigned char MSGSUBTYPE_MSG_OVERFLOW = 3;
const unsigned char MSGSUBTYPE_INCOMPL_MSG = 4;

struct message_struct {
  unsigned char type;
  int len;
  unsigned char vars[255];
};

unsigned char v_send_buffer[255];
int serial_fd;
struct termios newtio;
int crc_err_counter = 0;
struct message_struct in_message;
unsigned char registration_id[REGISTRATION_BYTES];

// code to integrate - this is the arduino version... we'll have to adapt it to C...
//byte CRC8(const byte *data, byte len) {
//  byte crc = 0x00;
//  while (len--) {
//    byte extract = *data++;
//    for (byte tempI = 8; tempI; tempI--) {
//      byte sum = (crc ^ extract) & 0x01;
//      crc >>= 1;
//      if (sum) {
//        crc ^= 0x8C;
//      }
//      extract >>= 1;
//    }
//  }
//  return crc;
//}

const unsigned char CRC7_POLY = 0x91;

unsigned char getCRC(unsigned char message[], unsigned char length)
{
  unsigned char i, j, crc = 0;

  for (i = 0; i < length; i++)
  {
    crc ^= message[i];
    for (j = 0; j < 8; j++)
    {
      if (crc & 1)
        crc ^= CRC7_POLY;
      crc >>= 1;
    }
  }
  return crc;
}

/*int main()
{
  // create a message array that has one extra byte to hold the CRC:
  unsigned char message[3] = {0x83, 0x01, 0x00};
  message[2] = getCRC(message, 2);
  // send this message to the Simple Motor Controller
}*/

struct termios oldtio;

int setup_serial_comm() {
	int    ret_fd;
	int    c;
       /*
          Open modem device for reading and writing and not as controlling tty
          because we don't want to get killed if linenoise sends CTRL-C.
        */
         ret_fd = open(MCUDEVICE, O_RDWR | O_NONBLOCK );
         if (ret_fd <0) {perror(MCUDEVICE); exit(-1); }

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
	return (ret_fd);
}

void zmq_request( char *p_req[], char *p_rep[] ) {
	char v_rep[255];
	printf ("starting request \n" );
	printf ("Connecting to CCU server… with a string of length %i \n", strlen(*p_req) );
	void *context = zmq_ctx_new ();
	void *requester = zmq_socket (context, ZMQ_REQ);
	zmq_connect (requester, CCU_SERVER);
	zmq_send (requester, *p_req, (strlen(*p_req)), 0);
        int res = zmq_recv (requester, v_rep, 255, 0);
	while (res == 0 ) { res = zmq_recv (requester, v_rep, 255, 0); }
	p_rep[0] = &v_rep[0];
	zmq_close (requester);
	zmq_ctx_destroy (context);
}

void submit_send_buffer() {
	write(serial_fd, &MSG_START_CHAR,1);
	write(serial_fd, v_send_buffer,(v_send_buffer[1]+3));
	write(serial_fd, &MSG_END_CHAR,1);
	//write(serial_fd, "\n" ,1);
}

void send_message( unsigned char p_msg_type, unsigned char p_buf[255], int p_len ) {
//	unsigned char v_crc_sum;

//	printf( "ready to send a command\n" );
//	printf( " - Msg type  : %i \n", p_msg_type );
//	printf( " - length    : %i \n", p_len );

	v_send_buffer[0] = p_msg_type;
	v_send_buffer[1] = p_len;

	for(int i=0; i<(p_len); i++)  {
//		printf( " - variable  : %i \n", p_buf[i] );
		v_send_buffer[(i+2)] = p_buf[i];
	}
	v_send_buffer[(p_len+2)] = getCRC(v_send_buffer, (p_len+2));
//	printf( " - CRC sum   : %i \n", v_send_buffer[(p_len+2)] );

	printf( "Sending message %02x ", MSG_START_CHAR );
	for(int i=0; i<(p_len+3); i++)  {
		printf( "%02x ", v_send_buffer[i] );
	}
	printf( "%02x \n", MSG_END_CHAR );

//	printf( "----------------------------------\n" );
	submit_send_buffer();
}

int read_incoming_message( unsigned char p_buf[255] ) {
	int res;
        /* read blocks program execution until a line terminating character is
           input, even if more than 255 chars are input. If the number
           of characters read is smaller than the number of chars available,
           subsequent reads will return the remaining chars. res will be set
           to the actual number of characters actually read */
	res = read(serial_fd, p_buf, 1);
	if (res>0) {
		while ((p_buf[0] != MSG_START_CHAR) && (res>0)) {
			printf( "we got an INVALID start character %i\n", p_buf[0] );
			res = read(serial_fd, p_buf, 1);
		}
//		printf( "we got a valid start character\n" );
//sleep(2);
		res = read(serial_fd, p_buf, 255);
		for(int i=0; i<res; i++)  {
			printf( "Got char num : %i \n", p_buf[i] );
		}
		// check message length and crc
		printf( "Length is %i \n", res );
		if ((p_buf[1]+3) > res) { printf( "incomplete message\n" ); return (0); }
		unsigned char v_crc = getCRC(p_buf, (res-2));
		printf( "calculated CRC is %i \n", v_crc );
		if (p_buf[(res-2)] != v_crc) {
			if (crc_err_counter < CRC_MAX_CRC_RESEND) {
				printf( "CRC error - need to resend the message\n" );
				crc_err_counter++;
				submit_send_buffer();
			} else {
				printf( "Too much consecutive CRC errors - send a Communication Reset message\n" );
			}
			return (0);
		}
		crc_err_counter = 0;
		return (TRUE);
	} else {
		// no message has been received
		return (0);
	}
}
void parse_incoming_message( unsigned char p_buf[255] ) {
	in_message.type = p_buf[0];
	in_message.len  =  p_buf[1];
	if (p_buf[1] > 0) {
		for (int i=0;i<p_buf[1];i++) {
			in_message.vars[i] = p_buf[(i+2)];
		}
	 }
}

void main() {
	int res;
        serial_fd = setup_serial_comm();
	unsigned char in_buf[255];
	unsigned char out_buf[255];

	char *ccu_in_buf[255];
	char *ccu_out_buf[255];


	volatile int STOP=FALSE;

	/* Wait for 1.5 sec to enable the MCU to get online */
	usleep (1500000);

	/* Send a HELLO message */
	send_message( MSGTYPE_HELLO, out_buf, 0 );

	/* Main Loop */
	while (STOP==FALSE) {
		in_buf[0] = 0;
		if (read_incoming_message( in_buf )) {
			parse_incoming_message( in_buf );
			printf( "got a valid message of type %i\n", in_buf[0] );
			if ( in_message.type == MSGTYPE_OK ) {
				printf( "got an OK\n" );
			}
			if ( in_message.type == MSGTYPE_REGISTER ) {
				printf( "got a request to register\n" );
				ccu_out_buf[0] = "Registration request for ID ";
				for (int i=0;i<in_message.len;i++) {
					registration_id[i] = in_message.vars[i];
					//ccu_out_buf[0][28+(3*i)] = in_message.vars[i];
				}
				//ccu_out_buf[0][29+(in_message.len*3)] = 0;
				// check with CCU if device is registered  ToDo
				zmq_request( ccu_out_buf, ccu_in_buf );
				printf( "CCU response is %s\n", ccu_in_buf[0] );

				if (FALSE) {
					send_message( MSGTYPE_OK, out_buf, 0 );
				} else  {
					send_message( MSGTYPE_LIST_DEVICES, out_buf, 0 );
				}
			}
			if ( in_message.type == MSGTYPE_ERROR ) {
				printf( "got an ERROR\n" );
			}
		} else { in_message.type = 0; }

//	           res = read(fd,in_buf,255);
//	           in_buf[res]=0;             /* set end of string, so we can printf */
//		   for(int i=0; i<res; i++)  {
//			printf( "Got char num : %i \n", in_buf[i] );
//		}

//		if ((in_buf[0]==50)&&(in_buf[1]==3)&&(in_buf[2]==0)&&(in_buf[3]==62)&&(in_buf[4]==10)) {
//			printf( "Got a RESET message - We will resend the HELLO message\n" );
//			send_message( fd, MSGTYPE_HELLO, out_buf, 0 );
//		}
        }


	/* Finalize the program */

        /* restore the old port settings */
        tcsetattr(serial_fd, TCSANOW, &oldtio);
}
