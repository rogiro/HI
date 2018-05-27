// -------------------------------------------------------------------------
// Central Control Unit (CCU) - Queue Handler
//
// The Queue Handler will listen on a dedicatrd FIFO pipe
// and fork a new listner instance for each DCU it gets info about.
// The child process will bind in PUSH and PULL to 2 queues defined
// by the DCU and act as a router to delivier these messages to
// the CCU backend (using again a PUSH / PULL method).
// The Queue handler will add some metadata (mostly the DCU_ID) to
// incoming messages or strip these for outgoing messages.
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

#include  "ccu-memtypes.h"
#include  "ccu-fifolib.h"

// -------------------------------------------------------------------------
// Global variables
fifo_struct_t* fifo;
bool is_parent = 1;
bool stay_alive = 1;
//int mcu_index;
char* in_msg;
mcu_stack_element_t* in_msg_fmt;

void *zmq_context;
void *zmq_ccu_comm_sub;
void *zmq_ccu_push;
void *zmq_dcu_push;
void *zmq_dcu_pull;


// -------------------------------------------------------------------------
// Main
int main (void)
{
// -------------------------------------------------------------------------
// Init
  // Communicate to the CCU that we are all set and ready
  fifo = wait_fifo_ready( fifo_qh_filename );

  // Initialize the shared memory structures
  init_mem_structures();
printf("init done...\n");

  in_msg = malloc( MAX_MSGSIZE );
  int msg_size;
  do {
    //mcu_index = -1;
    printf("going to listen to incoming message over the fifo\n");
    msg_size = wait_fifo_msg (fifo, in_msg);
    printf("we might have a message - the wait is over\n");
    if ( msg_size > 0 )  {
      in_msg_fmt = (mcu_stack_element_t*) ((char*)in_msg+1);
      if (in_msg_fmt->dcu_id == 0)  stay_alive = 0;
      else {
        /*for (int i=0;i<MAX_NR_OF_MCUS;i++) {
          if (mcu_stack_ptr[i].dcu_id == in_msg_fmt->dcu_id) {
            // I got the right pointer to the MCU/DCU in shared memory
            mcu_index = i;
            i = MAX_NR_OF_MCUS; // I could also just break out instead of doing this.
          }
        }
        if (mcu_index >= 0) {*/
          // So I will now fork a new process to communicate with this dcu
          int pid = -1;
          if ((pid = fork()) < 0) {
            printf( "ERROR FORKING THE QUEUE HANDLER\n");
            char * out_msg = malloc( MAX_MSGSIZE );
            out_msg[0] = MSG_CCU_ERR;
            out_msg[1] = 0;
            answer_fifo( fifo, out_msg, 2 );
            free (out_msg);
          }
          if (pid == 0 ) {
            printf("child process with dcu_id = %i\n", in_msg_fmt->dcu_id );
            is_parent = 0;
          } else {
            printf("parent process with dcu_id = %i\n", in_msg_fmt->dcu_id );
            char * out_msg = malloc( MAX_MSGSIZE );
            out_msg[0] = MSG_CCU_OK;
            out_msg[1] = 0;
            answer_fifo( fifo, out_msg, 2 );
            free (out_msg);
          }
        //}
      }
    }
    ccu_pause();
  } while ((is_parent) && (stay_alive));
  close_fifo (fifo);


if (stay_alive) {
  // -------------------------------------------------------------------------
  // Queue Handler child process
  // this is the actual HANDLER

  // Connect to the internal ZMQ queues (comm publisher and CC Upull queue)
  printf( "Initializing the communication of a new QH child process\n" );
  zmq_context = zmq_ctx_new ();

  void *zmq_ccu_comm_sub = zmq_socket (zmq_context, ZMQ_SUB);
  int zmq_rc = zmq_bind (zmq_ccu_comm_sub, zmq_comm_pub_socket);
  assert (zmq_rc == 0);
  printf( " - Subscribing to the CCU Publish socket\n");
  void *zmq_ccu_push = zmq_socket (zmq_context, ZMQ_PUSH);
  zmq_rc = zmq_bind (zmq_ccu_push, zmq_pull_socket);
  assert (zmq_rc == 0);
  printf( " - Connecting to the CCU Pull socket\n");


    // Wait for the DCU to become READY
//    while ( mcu_stack_ptr[mcu_index].status != MCU_STATUS_READY ) sleep(1);

    // connect to the ZMQ queues of the DCU
    char* connect_string = malloc(50);

    zmq_dcu_pull = zmq_socket (zmq_context, ZMQ_PULL);
    sprintf( connect_string, "tcp://%i.%i.%i.%i:%i", in_msg_fmt->dcu_ip[0], in_msg_fmt->dcu_ip[1],
             in_msg_fmt->dcu_ip[2], in_msg_fmt->dcu_ip[3], in_msg_fmt->dcu_port);
    zmq_connect (zmq_dcu_pull, connect_string );
    printf( " - Connected the DCU PULL socket to %s\n", connect_string );

    zmq_dcu_push = zmq_socket (zmq_context, ZMQ_PUSH);
    sprintf( connect_string, "tcp://%i.%i.%i.%i:%i", in_msg_fmt->dcu_ip[0],in_msg_fmt->dcu_ip[1],
             in_msg_fmt->dcu_ip[2], in_msg_fmt->dcu_ip[3], (in_msg_fmt->dcu_port+1));
    zmq_connect (zmq_dcu_push, connect_string );
    printf( " - Connected the DCU PUSH socket to %s\n", connect_string );
    free(connect_string);

    // all has been set up - we will do some debugging here...
    char* buffer = malloc(50);
    zmq_recv (zmq_dcu_pull, buffer, MAX_MSGSIZE, 0);
    printf( "Received buffer character %i\n", buffer[0] );
    buffer[0] = 9;
    buffer[1] = 0;
    zmq_send (zmq_dcu_push, buffer, 2, 0);

  }

  free(in_msg);

// -------------------------------------------------------------------------
// Finalization

  zmq_close (zmq_ccu_comm_sub);
  zmq_close (zmq_ccu_push);
  zmq_close (zmq_dcu_pull);
  zmq_close (zmq_dcu_push);
  zmq_ctx_destroy (zmq_context);
  finalize_mem_structs();

}
