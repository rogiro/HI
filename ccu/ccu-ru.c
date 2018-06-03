// -------------------------------------------------------------------------
// Central Control Unit (CCU) - Registration Unit
//
// The Registration Unit will expose the registration APIs through
// a 0mq queue. The device control unit (DCU) can register itself,
// initialize the CCU communication channels and register it's MCU
// through calls to the RU queues.
//
// -------------------------------------------------------------------------
// Version History
// 1.0 - RvR - 18.05.2018 - Initial version
//
// -------------------------------------------------------------------------

// Includes
#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <stdbool.h>

#include <fcntl.h>
#include <sys/stat.h>

#include  "../includes/ccu-memtypes.h"
#include  "../includes/ccu-fifolib.h"

#define ZMQ_INPUT_BIND "tcp://*:5550"

// -------------------------------------------------------------------------
// Global variables
fifo_struct_t* fifo;
bool stay_alive = 1;


// -------------------------------------------------------------------------
// Main
int main (void)
{
// -------------------------------------------------------------------------
// Init

  // Init the ZeroMQ message queue for recieving registration requests
  printf( "zeromq init starting\n");
  //  Socket to talk to clients
  void *zmq_context = zmq_ctx_new ();
  void *zmq_responder = zmq_socket (zmq_context, ZMQ_REP);
  int zmq_rc = zmq_bind (zmq_responder, ZMQ_INPUT_BIND);
  assert (zmq_rc == 0);
  printf( "zeromq init done\n");

  // Communicate to the CCU that we are all set and ready
  fifo = send_fifo_ready( fifo_ru_filename );

  // Initialize the shared memory structures
  //init_mem_structures();


// -------------------------------------------------------------------------
// Main LOOP
  while (stay_alive) {

    char* zmq_in_msg;
    char  zmq_out_msg  [MAX_MSGSIZE];
    char  fifo_in_msg  [MAX_MSGSIZE];
    char  fifo_out_msg [MAX_MSGSIZE];
    int   bytes_read;

    zmq_msg_t zmq_in_msg_meta;
    zmq_msg_init (&zmq_in_msg_meta);
    int rc = zmq_msg_recv (&zmq_in_msg_meta, zmq_responder, 0);

//    printf("got a message consisting of %i bytes\n", rc );
    if (rc != -1) {
      zmq_in_msg = zmq_msg_data(&zmq_in_msg_meta);


      switch ((int)zmq_in_msg[0]) {

        case MSG_RU_REG_DCU : ;
          // In case we receive a DCU registration request on the 0MQ input
          // get originating IP address
          const char * ip_address = zmq_msg_gets (&zmq_in_msg_meta, "Peer-Address");
          char *ip, *token, *to_free;
          to_free = ip = strdup( ip_address );
          printf( "Got a DCU registration message from IP %s\n", ip_address );

          // create and send the message to the CCU for DCU registration
          fifo_out_msg[0] = MSG_RU_REG_DCU;
          msg_reg_dcu_t* fifo_out_msg_fmt = (msg_reg_dcu_t*) ((char*)fifo_out_msg+1);
          int i=0;
          while ((token = strsep(&ip, "."))) { fifo_out_msg_fmt->ip_address[i] = (char) atoi(token); i++; }
          free(to_free);
          fifo_out_msg[i+1] = 0;

          printf( " sending the DCU registration request to the CCU %i %i\n", fifo_out_msg[0], i );
          write_fifo(fifo, fifo_out_msg, (i+2));

          // Now wait for the answer of the CCU
          printf( "Waiting for a response from the CCU\n" );
          bytes_read = wait_fifo_answer( fifo, fifo_in_msg );
          printf( "Got the following response from the CCU: %i %i %i %i %i\n", bytes_read, fifo_in_msg[0], fifo_in_msg[1], fifo_in_msg[7], fifo_in_msg[8] );

          // Set the element in the shared mem.
          msg_set_dcu_t* out_msg = (msg_set_dcu_t*) ((char*)zmq_out_msg+1);
          mcu_stack_element_t* inp_msg = (mcu_stack_element_t*) ((char*)fifo_in_msg+1);
          out_msg->dcu_id   = inp_msg->dcu_id;
          out_msg->dcu_port = inp_msg->dcu_port;
          memcpy( out_msg->dcu_ip, fifo_out_msg_fmt->ip_address, 4 );
          printf( "setting port to %i and message length is %i\n", out_msg->dcu_port, sizeof(msg_set_dcu_t)+1 );
          zmq_out_msg[0] = MSG_RU_SET_DCU;

          zmq_send (zmq_responder, zmq_out_msg, sizeof(msg_set_dcu_t)+1, 0);

          break;

/*        case MSG_RU_REG_MCU : ;

          msg_reg_mcu_t* inp_msg1 = (msg_reg_mcu_t*) ((char*)zmq_in_msg+1);
          printf( "got a REG_MCU request with reg id %i %i %i %i %i %i\n", (inp_msg1->mcu_reg_id)[0], (inp_msg1->mcu_reg_id)[1]
                    , (inp_msg1->mcu_reg_id)[2], (inp_msg1->mcu_reg_id)[3], (inp_msg1->mcu_reg_id)[4], (inp_msg1->mcu_reg_id)[5]);

          // send the REG ID to the CCU to see if we know it
          write_fifo(fifo, zmq_in_msg, sizeof(msg_reg_mcu_t)+1);
          printf( "Waiting for a response from the CCU\n" );
          bytes_read = wait_fifo_answer( fifo, fifo_in_msg );

          printf( "Got the following response from the CCU: %i %i\n", fifo_in_msg[0], fifo_in_msg[1] );
          zmq_send (zmq_responder, fifo_in_msg, bytes_read, 0);

// add in shared mem the reg_id to the dcu
// and ask a feedback to CCU if the MCU (reg_id) is known

          break; */

  /*      case MSG_RU_MCU_MAKE : ;

          msg_mcu_make_t* inp_msg2 = (msg_mcu_make_t*) (zmq_in_msg+1);
          // will have to check if we can download the details of the MCU
          // however, I still need to clear out the format (json?) and precise content of the downloaded info
          // so I will just allways respond a NOK message - disabling the functionality.
          zmq_out_msg[0] = MSG_CCU_NOK;
          zmq_out_msg[1] = 0;
          zmq_send (zmq_responder, zmq_out_msg, 2, 0);

          break;

*/

          case MSG_RU_SHUTDOWN : ;
            write_fifo(fifo, zmq_in_msg, 1);
            stay_alive = 0;
            zmq_out_msg[0] = MSG_OK;
            zmq_out_msg[1] = 0;
            zmq_send (zmq_responder, zmq_out_msg, 2, 0);

            break;
        default :
          printf( "ERROR - got a unknown message with MSG_TYPE = %i\n", (int)zmq_in_msg[0] );

      }
      zmq_msg_close (&zmq_in_msg_meta);
    }
  }


// -------------------------------------------------------------------------
// Finalization
  printf( "Closing the registration queue\n");
  close_fifo (fifo);
  zmq_close (zmq_responder);
  zmq_ctx_destroy (zmq_context);

}
