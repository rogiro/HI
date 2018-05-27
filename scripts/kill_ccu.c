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


#include  "ccu-memtypes.h"

#define ZMQ_CCU_RU_CONNECT "tcp://localhost:5550"


// Global variables
  char buffer [MAX_MSGSIZE];

  void *zmq_context;
  void *zmq_requester;



// -------------------------------------------------------------------------
// Main
int main (void)
{
  // Set up connection to CCU-RU
  printf ("Connecting CCU Registration Unit\n");
  zmq_context = zmq_ctx_new ();
  zmq_requester = zmq_socket (zmq_context, ZMQ_REQ);
  zmq_connect (zmq_requester, ZMQ_CCU_RU_CONNECT);

  // Sending the DCU registration message
  // to announce ourselves and set up the communication channels
  printf ("Sending CCU shutdown message\n");
  buffer[0] = MSG_RU_SHUTDOWN;
  buffer[1] = 0;
  zmq_send (zmq_requester, buffer, 2, 0);
  zmq_recv (zmq_requester, buffer, MAX_MSGSIZE, 0);
  printf( "shutdown processed\n" );

  zmq_close (zmq_requester);
  zmq_ctx_destroy (zmq_context);
}
