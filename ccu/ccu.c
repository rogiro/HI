// -------------------------------------------------------------------------
// Central Control Unit (CCU) - Backend script
//
// this will put together the different parts of the CCU architecture
// and serve as message router between the different components.
//
// -------------------------------------------------------------------------
// Version History
// 1.0 - RvR - 18.05.2018 - Initial version
//
// -------------------------------------------------------------------------

// Includes
#include  <zmq.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <assert.h>
#include  <sys/types.h>
#include  <sys/ipc.h>
#include  <sys/shm.h>
#include  <sys/select.h>
#include  <stdbool.h>


#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>


#include  "ccu-memtypes.h"
#include  "ccu-fifolib.h"


// Global variables
  fifo_struct_t* fifo_db;
  fifo_struct_t* fifo_ru;
  fifo_struct_t* fifo_re;
  fifo_struct_t* fifo_qh;

  bool stay_alive = 1;


// -------------------------------------------------------------------------
// Main
void  main()
{
// variable declaration
  char in_msg  [MAX_MSGSIZE];
  char out_msg [MAX_MSGSIZE];

  printf( "Initializing internal communications\n" );

// Set up the internal ZeroMQ queues for publishing commands
// and pulling data from the DCU devices that will be used by the
// Queue handler child processes.
  void *zmq_context = zmq_ctx_new ();
  void *zmq_dcu_publisher = zmq_socket (zmq_context, ZMQ_PUB);
  int zmq_rc = zmq_bind (zmq_dcu_publisher, zmq_comm_pub_socket);
  assert (zmq_rc == 0);
  printf( " - DCU Publish socket initialized\n");
  void *zmq_pull = zmq_socket (zmq_context, ZMQ_PULL);
  zmq_rc = zmq_bind (zmq_pull, zmq_pull_socket);
  assert (zmq_rc == 0);
  printf( " - Command Pull socket initialized\n");
  void *zmq_data_publisher = zmq_socket (zmq_context, ZMQ_PUB);
  zmq_rc = zmq_bind (zmq_data_publisher, zmq_data_pub_socket);
  assert (zmq_rc == 0);
  printf( " - Data Publish socket initialized\n");


// I could also fork the processes here - to see if this is what we'd want
// But for now I'll be launching them in seperate shells to keep better control
  fifo_db = send_fifo_ready( fifo_db_filename ); printf( " - Database fifo ready\n" );
  fifo_ru = wait_fifo_ready( fifo_ru_filename ); printf( " - Registration Unit fifo ready\n" );
  fifo_qh = send_fifo_ready( fifo_qh_filename ); printf( " - Queue Handler fifo ready\n" );
//	wait_for_fifo_ready( &fd_qh ); printf( " - got the CCU-QH process ready message\n" );
//	wait_for_fifo_ready( &fd_re ); printf( " - got the CCU-RE process ready message\n" );

// Initialize the shared memory structures
printf("entering memtypes\n");
  init_mem_structures();
printf("exiting memtypes\n");


// debug routines used to dump the memory structures - will be moved to some header file
// testing the shared mem structures
  for (int i=0 ; i<get_nr_of_devices() ; i++) {
    device_header_t* v_device_header = (device_header_t*) device_array[i];
    if ( (*v_device_header).device_type == 1 ) {
      printf("device %i is an internal clock\n", i);
    }
    if ( v_device_header->device_type == 2 ) {
      printf("device %i is a relay\n", i);
      device_relay_t* v_device_relay = (device_relay_t*)  (((char*)device_array[i]) + sizeof(device_header_t));
      printf( "relay subtype = %i and pin = %i\n", (*v_device_relay).subtype, (*v_device_relay).digital_pin );
    }
  }
  printf( "Actual number of registered devices is %i\n", get_nr_of_devices() );


// -------------------------------------------------------------------------
// Main LOOP
  int in_read_char;
  while (stay_alive) {

  // we need to loop through the queues and see if there are any messages to process or route.

  // starting with the registration unit
    in_msg[0] = 0;
    in_read_char = get_fifo_msg(fifo_ru, in_msg );
    //if (in_read_char <= 0) { printf("got a RU \n" ); }
    if (in_read_char > 0) { printf("got a RU message of type %i-%i\n", in_msg[0],in_read_char);
    //printf( "in message first char = %i\n", in_msg[0]); sleep(1);
      switch ((int)in_msg[0]) {

        case MSG_RU_REG_DCU : ;
      // Register a new DCU
      // will need to set up the communication channel for the new DCU and set MCU details to 0
        msg_reg_dcu_t* in_ip = (msg_reg_dcu_t*)(((char*)in_msg)+1);
        printf( "I got a DCU registration request from IP %i.%i.%i.%i\n",
				in_ip->ip_address[0], in_ip->ip_address[1], in_ip->ip_address[2], in_ip->ip_address[3] );
        // Build up the answer
        mcu_stack_element_t* mcu_el = (mcu_stack_element_t*)(((char*)out_msg)+1);

        // Set the start values from which we will look for valid values
        mcu_el->dcu_id = 1;
        mcu_el->dcu_port    = _DCU_START_PORT;
        mcu_el->status = MCU_STATUS_INIT;

        // Set the IP address.
        memcpy( mcu_el->dcu_ip, in_ip->ip_address, 4 );

        for (int i=0;i<MAX_NR_OF_MCUS;i++) {
          // find the max dcu_id used for now to set the new one to max+1
          if (mcu_stack_ptr[i].dcu_id >= mcu_el->dcu_id )              { mcu_el->dcu_id = mcu_stack_ptr[i].dcu_id+1; }
          // find an unused ip/port combination for the DCU queue and the QH queue
          if ( mcu_stack_ptr[i].dcu_id > 0 ) {
            if (  (mcu_el->dcu_ip[0] == mcu_stack_ptr[i].dcu_ip[0])
               && (mcu_el->dcu_ip[1] == mcu_stack_ptr[i].dcu_ip[1])
               && (mcu_el->dcu_ip[2] == mcu_stack_ptr[i].dcu_ip[2])
               && (mcu_el->dcu_ip[3] == mcu_stack_ptr[i].dcu_ip[3])
               && (mcu_el->dcu_port  <= mcu_stack_ptr[i].dcu_port )  ) { mcu_el->dcu_port = mcu_stack_ptr[i].dcu_port + 2; }
          }
        }
        // init all MCU specific parts of the struct to "0"
        for (int i=0; i<_REG_BYTES; i++)  { mcu_el->reg_id[i] = 0; }
        for (int i=0; i<_MAKE_BYTES; i++) { mcu_el->make[i] = 0; }

        printf( "got dcu_id %i and ip %i.%i.%i.%i and port %i\n", mcu_el->dcu_id, mcu_el->dcu_ip[0], mcu_el->dcu_ip[1], mcu_el->dcu_ip[2], mcu_el->dcu_ip[3], mcu_el->dcu_port );

        // Set the MCU in the shared memory
	      int i=0;
	      while ( (i<MAX_NR_OF_MCUS) && (mcu_stack_ptr[i].dcu_id >= 0) ) { i++; }
        if (i == MAX_NR_OF_MCUS ) {
          printf("ERROR - Cannot register thie DCU - no more space let in array\n"); }
	      else {
          memcpy( &(mcu_stack_ptr[i]), mcu_el, sizeof(mcu_stack_element_t) );
        }
        out_msg[0] = MSG_RU_SET_DCU;
       // memcpy( &(out_msg[1]), &mcu_el, sizeof(mcu_stack_element_t) );
        int msg_size = (int) (sizeof(mcu_stack_element_t)+2);
        // Send the message back to the DCU
        answer_fifo(fifo_ru, out_msg, msg_size );

        // send the message also to the QH so we can fork a processes
        // and communicate with the DCU through ZMQ queues
        printf( "Informing Queue handler of the existance of a new DCU\n" );
        printf( "msg details - %i %i %i %i %i %i %i %i %i\n", out_msg[0], out_msg[1], out_msg[2], out_msg[3], out_msg[4], out_msg[5], out_msg[6], out_msg[7], out_msg[8]);
        write_fifo( fifo_qh, out_msg, msg_size );
        wait_fifo_answer( fifo_qh, in_msg );
        if (in_msg[0] == MSG_CCU_OK) {
          printf( "Queue handler has been instructed to set up communication with new DCU\n" );
        } else {
          printf( "Queue handler has returned an Error when instructed to set up communication with new DCU\n" );
        }

        printf( "dumping the mcu_stack_ptr array\n");
        for (int i=0;i<MAX_NR_OF_MCUS;i++) {
          if ( mcu_stack_ptr[i].dcu_id > 0 ) {
            printf(" - dcu_id = %i - DCU IP/Port = %i.%i.%i.%i/%i\n",
		                        mcu_stack_ptr[i].dcu_id,
                            mcu_stack_ptr[i].dcu_ip[0], mcu_stack_ptr[i].dcu_ip[1],
                            mcu_stack_ptr[i].dcu_ip[2], mcu_stack_ptr[i].dcu_ip[3], mcu_stack_ptr[i].dcu_port );
          }
        }

        break;

      case MSG_RU_REG_MCU : ;
        printf( "I got a MCU registration request of length %i with payload %i.%i.%i.%i\n",
                  in_read_char, in_msg[5], in_msg[6], in_msg[7], in_msg[8] );

        // route the message to the DB and return the DB answer 'as-is'
        write_fifo(fifo_db, in_msg, in_read_char );
        in_read_char = wait_fifo_answer( fifo_db, in_msg );

        printf( "I got a MCU registration answer of length %i with payload %i.%i.%i.%i.%i.%i\n",
                in_read_char, in_msg[6], in_msg[7], in_msg[8], in_msg[9], in_msg[10], in_msg[11] );
      	answer_fifo(fifo_ru, in_msg, in_read_char );

        break;


      case MSG_RU_SHUTDOWN : ;
        write_fifo(fifo_db, in_msg, in_read_char );
        char* kill_msg = malloc( 20 );
        memset((void*)kill_msg, 0, 2);
        write_fifo(fifo_qh, kill_msg, 10 );

        stay_alive = 0;

        break;

//      default :
      }

      ccu_pause();	// force a context switch and avoid hogging the system
    }
  }


// -------------------------------------------------------------------------
// Finalization
  // Close the FIFOs and finalize the memory structures
  close_fifo (fifo_qh);
  close_fifo (fifo_ru);
  close_fifo (fifo_db);

  finalize_mem_structs();

}
