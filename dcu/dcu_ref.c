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


#include  "../includes/ccu-memtypes.h"

#define ZMQ_CCU_RU_CONNECT "tcp://localhost:5550"
#define ZMQ_CCU_SUB        "tcp://localhost:5552"
#define ZMQ_CCU_PUSH       "tcp://localhost:5551"

// number of seconds to wait for an answer when registering the MCU
#define DCU_RETRY_MCU_REG 30

//#define ZMQ_CCU_QH_CONNECT "tcp://localhost:5551" // should get this from the CCU-RU when registering the DCU


// Global variables
  int  dcu_id;
  char buffer [MAX_MSGSIZE];

  void *zmq_context;
  void *zmq_registrator;
  void *zmq_ccu_push;
  void *zmq_ccu_sub;



// -------------------------------------------------------------------------
// Main
int main (void)
{
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

//
// For the MCU/DCU developper to add...
// we should be handshaking with MCU now to bring it up, and get it's REG_ID
//
sleep (1);


  bool mcu_registered = 0;
  // next message - we'll send the REG_ID
  while (! mcu_registered) {

    msg_reg_mcu_t* out_msg = (msg_reg_mcu_t*) (char*)buffer;
    out_msg->dcu_id = dcu_id;
    out_msg->msg_type = MSG_REG_MCU;

//
// For the MCU/DCU developper to add...
// we should have a way to store and retrieve the REG_ID bytes - we will hardcode them here as example
//
    out_msg->mcu_reg_id[0] = 0; 	out_msg->mcu_reg_id[1] = 0; 	out_msg->mcu_reg_id[2] = 0;
    out_msg->mcu_reg_id[3] = 0; 	out_msg->mcu_reg_id[4] = 0; 	out_msg->mcu_reg_id[5] = 0;


/*  zmq_recv (zmq_responder, buffer, MAX_MSGSIZE, 0);
  printf ("Received Answer for registration MCU message %i %i\n", buffer[0], buffer[1] );

  if ( buffer[0] == MSG_RU_SET_REGID ) {
    printf( "setting the reg id of the device\n" );
    msg_reg_mcu_t* in_reg_id = (msg_reg_mcu_t*) ((char*)buffer+1);
    printf( "NEW reg id = %i-%i-%i-%i-%i-%i\n", in_reg_id->mcu_reg_id[0], in_reg_id->mcu_reg_id[1], in_reg_id->mcu_reg_id[2],
              in_reg_id->mcu_reg_id[3], in_reg_id->mcu_reg_id[4], in_reg_id->mcu_reg_id[5] );

//
// For the MCU/DCU developper to add...
// we should have persistence of the received REG ID so the CCU recognizes us next time
//

  } else if ( buffer[0] == MSG_CCU_OK ) {
    printf( "seems a known device\n" );
    printf( "We should get the device info and a new reg id through zmq shortly\n" );

  } else if ( buffer[0] == MSG_CCU_NOK ) {
    printf( "CCU says it does not know us - we will send the Make / model info\n" );
    buffer[0] = MSG_RU_MCU_MAKE;
    buffer[1] = 0;			// Note - these bytes should contain the 14 make/model bytes.
    // msg_mcu_make_t* out_msg = (msg_mcu_make_t*) (buffer+1);
    // buffer[1+sizeof(msg_mcu_make_t)] = 0;
    zmq_send (zmq_registrator, buffer, (sizeof(msg_mcu_make_t)+1), 0);
    zmq_recv (zmq_registrator, buffer, MAX_MSGSIZE, 0);
    printf ("Received Answer for registration MCU message %i %i\n", buffer[0], buffer[1] );
    if ( buffer[0] == MSG_CCU_NOK ) {
      printf( "The CCU tells us it cannot download the details of internet\n" );
      printf( "We should send the device details on the queue\n" );
    }
  }
*/
  // Registration has been succesfully completed.
  // we will drop the connection with the CCU-RU


  // all has been set up - we will do some debugging here...
  //buffer[0] = 15;
  //buffer[1] = 0;
  //zmq_send (zmq_ccu_push, buffer, 2, 0);
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
  }

//
// still to add the loop part in which we will listen to MCU and CCU and pass on the messages
//
  zmq_close (zmq_ccu_push);
  zmq_close (zmq_ccu_sub);
  zmq_ctx_destroy (zmq_context);
}
