// -------------------------------------------------------------------------
// Central Control Unit (CCU) - fifolib
//
// Library with standard functions for the interprocess communication
// between the various CCU components
//
// -------------------------------------------------------------------------
// Version History
// 1.0 - RvR - 18.05.2018 - Initial version
//
// -------------------------------------------------------------------------
#define _BSD_SOURCE

// Includes
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include <time.h>

#include  <sys/types.h>
#include  <sys/ipc.h>
#include  <sys/shm.h>
#include  <sys/select.h>


#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// ZeroMQ CCU sockets
#define ZMQ_DCU_PUB        "tcp://*:5552"
#define ZMQ_DCU_PULL       "tcp://*:5551"



// -------------------------------------------------------------------------
// Structs and typedefs
struct fifo_struct {
	int fd;
	char* filename;
};
typedef struct fifo_struct fifo_struct_t;

// FIFO file names
  char * fifo_db_filename = "/tmp/ccu-db";
  char * fifo_ru_filename = "/tmp/ccu-ru";
  char * fifo_re_filename = "/tmp/ccu-re";
  char * fifo_qh_filename = "/tmp/ccu-qh";

//  char * zmq_comm_pub_socket  = "ipc:///tmp/ccu-comm-pub";
//	char * zmq_pull_socket      = "ipc:////tmp/ccu-pull";
//	char * zmq_data_pub_socket  = "ipc:///tmp/ccu-data-pub";

// -------------------------------------------------------------------------
// Functions

// wait for the internal queue to be ready - used during initialization fase
fifo_struct_t* wait_fifo_ready( char* filename ) {
  fifo_struct_t* return_val = malloc(sizeof(fifo_struct_t));

// Creating the named file(FIFO)
  return_val->filename = filename;
  mkfifo(return_val->filename, 0666);
  return_val->fd = open(return_val->filename, O_RDONLY | O_NONBLOCK );
//  return_val->fd = open(filename, O_RDONLY | O_RSYNC | O_NONBLOCK );

// wait for the other side to send me a 'READY' message.
  char str[2];
  str[0]=0;
  while (str[0]==0) {
    read(return_val->fd, str, 2);
    if ( (str[0]!=MSG_READY) && (str[0]!=0) ) {
      printf( "unexpected reponse from CCU-DB - I was waiting for an 'CCU_READY message but got an %hhx\n", str[0] );
    }
    ccu_pause();	// just to force a context switch and avoid hogging the system
  }
  return (return_val);
}

// function to open the queue for writing and communicate that we are ready
// used by the CCU components for the queues that can be initiated by the component itself.
fifo_struct_t* send_fifo_ready( char* filename ) {
  fifo_struct_t* return_val = malloc(sizeof(fifo_struct_t));

  mkfifo(filename, 0666);

   return_val->fd = open(filename, O_WRONLY );
  return_val->filename = filename;

  char str[2];
  str[0] = MSG_READY;
  str[1] = 0;

  write(return_val->fd, str, 2);
  return (return_val);
}


// Functions to alter the PIPE direction
// be aware that the use of these functions must be synchronized
// between both ends of the pipe
void set_input( fifo_struct_t* fifo ) {
  close(fifo->fd);   fifo->fd = open(fifo->filename, O_RDONLY | O_NONBLOCK );
}
void set_output( fifo_struct_t* fifo ) {
  close(fifo->fd);   fifo->fd = open(fifo->filename, O_WRONLY );
}

// write a message to the internal queue - for queues that are by default in 'write mode'
void write_fifo( fifo_struct_t* fifo, char* msg, int size ) {
  write(fifo->fd, msg, size );
//  fifo->fd = open(fifo->filename, O_RDONLY | O_RSYNC | O_NONBLOCK );
}
// write a message to the internal queue - for queues that are by default in 'read mode'
// this is used to send the answer to an incoming message
void answer_fifo( fifo_struct_t* fifo, char* msg, int size ) {
  close(fifo->fd); fifo->fd = open(fifo->filename, O_WRONLY);
  write_fifo(fifo, msg, size );
	close(fifo->fd);   fifo->fd = open(fifo->filename, O_RDONLY );
	close(fifo->fd);   fifo->fd = open(fifo->filename, O_RDONLY | O_NONBLOCK );
//  fifo->fd = open(fifo->filename, O_RDONLY | O_RSYNC | O_NONBLOCK );
}

int get_fifo_msg( fifo_struct_t* fifo, char* fifo_in_msg ) {
  return (read(fifo->fd, fifo_in_msg, MAX_MSGSIZE));
}

int wait_fifo_msg( fifo_struct_t* fifo, char* fifo_in_msg ) {
  int return_val = 0;
	while (return_val <= 0)  return_val = get_fifo_msg(fifo, fifo_in_msg);
  return (return_val);
}

int wait_fifo_answer( fifo_struct_t* fifo, char* fifo_in_msg ) {
	close(fifo->fd);   fifo->fd = open(fifo->filename, O_RDONLY );
  int return_val = 0;
	return_val = wait_fifo_msg(fifo, fifo_in_msg);
	close(fifo->fd); fifo->fd = open(fifo->filename, O_WRONLY);
  return (return_val);
}

// close the internal fifo queue - used for finalization
void close_fifo( fifo_struct_t* fifo ) {
  close(fifo->fd);
  free(fifo);
}
