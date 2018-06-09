// -------------------------------------------------------------------------
// Central Control Unit (CCU) - Database
//
// The database process of the CCU will create persistence to the
// architecture. When started up, the CCU-DB is the first one to be
// activated, as it will create the shared memory blocks and populate
// it with the known devices and prepare for the DCU / MCU registration
// structures.
//
// -------------------------------------------------------------------------
// Version History
// 1.0 - RvR - 18.05.2018 - Initial version
//
// -------------------------------------------------------------------------

// Includes
#include <unistd.h>
#include <libpq-fe.h>


#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <stdbool.h>

#include  <sys/types.h>
#include  <sys/ipc.h>
#include  <sys/shm.h>
#include  <sys/select.h>


#include <fcntl.h>
#include <sys/stat.h>

#include  "../includes/ccu-memtypes-db.h"
#include  "../includes/ccu-fifolib.h"


#define _REG_HEADING_BYTES		50		// int4 representation of the first 4 bytes of the reg_id of the MCU when we set it
							// should be stored in the DB, and when installing and registering the installation
							// a unique int4 will be generated for each installation.

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

  init_mem_structures_db();
//  load_device_array();

  // connect to the DB
  PGconn *conn = PQconnectdb("user=ccudb dbname=ccu-db");
  if (PQstatus(conn) == CONNECTION_BAD) {
    fprintf(stderr, "Connection to database failed: %s\n",
    PQerrorMessage(conn));
    PQfinish(conn);
    exit(1);
  }

  // Select all devices to add to the shared memory
  PGresult *res = PQexec(conn, "SELECT id, device_name, devtype_id, mcu_id FROM devices");
  int rows = PQntuples(res);
  for(int i=0; i<rows; i++) {
    char* loaded_settings = (char*) malloc( MAX_DEVICE_PAYLOAD_SIZE );
    char* sql  = (char*) malloc( 4096 );

    // Select all parameters for each device
    sprintf(sql, "select param_name, value from device_params AS dp, device_config as dc where dc.devparam_id = dp.id and dc.device_id = %i", PQgetvalue(res, i, 0)  );
//printf( sql );
    PGresult *res2 = PQexec(conn, sql);
    free(sql);
    int rows2 = PQntuples(res2);
    int msg_size;

    // Add the parameters in the right struct element for each device type - Device specific (!)
    switch (atoi( PQgetvalue(res, i, 2))) {
      case 1 : ; // Internal clock
        msg_size = 0;
        break;

      case 2 : ; // Relay
        msg_size = sizeof(device_relay_t);
        device_relay_t* formated_settings = (device_relay_t*) loaded_settings;
        memset( formated_settings, 0, sizeof( device_relay_t ) );

        for(int j=0; j<rows2; j++) {
          if (strcmp( PQgetvalue(res2, j, 0), "subtype" ) == 0 ) {
            (*formated_settings).subtype = (char) atoi( PQgetvalue(res2, j, 1));
          }
          if (strcmp( PQgetvalue(res2, j, 0), "digital pin" ) == 0 ) {
            (*formated_settings).digital_pin = (char) atoi( PQgetvalue(res2, j, 1));
          }
        }
        break;

      default :
        msg_size = 0;
    }
//printf( "adding device with id %i and type %i that I just got from DB\n", atoi( PQgetvalue(res, i, 0)), atoi( PQgetvalue(res, i, 2)));
    int id   = atoi(PQgetvalue(res, i, 0));
    int type = atoi(PQgetvalue(res, i, 2));
    add_device_to_Shm( &id, &type, loaded_settings, &msg_size);
//    add_device_to_Shm( &id, &type, arguments, &msg_size);
    free(loaded_settings);
    PQclear(res2);
  }
  PQclear(res);

  // All devices added to the shared memory
  // (re)load the device array.
  load_device_array();

// Init Done
//sleep(1);


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
// End of the debug routine


// ---------------------- FROM THIS POINT WE STILL NEED TO CLEAN UP THE CODE
  // Communicate to the CCU that we are all set and ready
  fifo = wait_fifo_ready( fifo_db_filename );

// -------------------------------------------------------------------------
// Main LOOP
  int msg_size;
  char* in_msg  = malloc( MAX_MSGSIZE );
  char* out_msg = malloc( MAX_MSGSIZE );

  while (stay_alive) {
    msg_size = wait_fifo_msg (fifo, in_msg);
    if ( msg_size > 0 )  {
      //printf( "I got some message with id %i\n", ccu_in_message[0] );
      switch ((int)in_msg[sizeof(int)]) {
        case MSG_REG_MCU : ;
          printf( "I got a MCU registration request with payload %i.%i.%i.%i\n",
                  in_msg[1], in_msg[2], in_msg[3], in_msg[4] );
          msg_reg_mcu_t* in_msg_fmt = (msg_reg_mcu_t*) (char*)in_msg;

          int* reg_header = (unsigned int*) in_msg_fmt->mcu_reg_id;
          short* reg_tail = (unsigned short*) &(in_msg_fmt->mcu_reg_id[4]);
          if ( ( (*reg_header == -1 ) && (*reg_tail == -1 ) ) || ( (*reg_header == 0) && (*reg_tail == 0 ) ) ) {
            printf( "NEW DEVICE\n" );
            // generating a new REG ID
            *reg_header = _REG_HEADING_BYTES;
            char* sql = "select nextval('mcu_reg_id_sequ')";
            PGresult *res = PQexec(conn, sql);
            *reg_tail = atoi( PQgetvalue(res, 0, 0) );
//printf("DEBUG - registration tailing 2 bytes = %i\n", *reg_tail );
//printf("DEBUG REG ID = %i, %i, %i, %i, %i, %i, %i, %i\n", in_msg_fmt->mcu_reg_id[0],in_msg_fmt->mcu_reg_id[1],in_msg_fmt->mcu_reg_id[2],in_msg_fmt->mcu_reg_id[3],in_msg_fmt->mcu_reg_id[4],in_msg_fmt->mcu_reg_id[5]);
            PQclear(res);

            // we might go through the list to see if the id we generated is allready used and continue to loop it.
            // but for now I'll assume we did not run the new device routine 65535 times.

            in_msg_fmt->msg_type = MSG_SAMP_SET_REGID;
//            out_msg[0] = MSG_RU_SET_REGID;
//            memcpy( &(out_msg[1]), (void*) (in_msg+1), sizeof(msg_reg_mcu_t) );
//            out_msg[(sizeof(msg_reg_mcu_t)+1)] = 0;
            printf("DEBUG OUT MSG = %i, %i, %i, %i, %i, %i, %i, %i\n", in_msg[0],in_msg[1],in_msg[2],in_msg[3],in_msg[4],in_msg[5],in_msg[6],in_msg[7]);
            printf("STRUCT SIZE = %i, %i, %i, %i, %i, %i, %i, %i\n", sizeof(int), sizeof(unsigned char), sizeof(msg_reg_mcu_t), sizeof(msg_reg_dcu_t), sizeof(struct msg_reg_mcu ) );
            answer_fifo( fifo, in_msg, sizeof(msg_reg_mcu_t) );

            printf( "Response sent - %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i\n", (sizeof(msg_reg_mcu_t)+2), in_msg[1], in_msg[2],
                      in_msg[3], in_msg[4], in_msg[5], in_msg[6], in_msg[7], in_msg[8], in_msg[9], in_msg[10], in_msg[11], in_msg[12], in_msg[13], in_msg[14] );
          } else {

            char* sql  = (char*) malloc( 4096 );
            sprintf( sql, "select * from mcu where reg_id = '%i-%i-%i-%i-%i-%i'", in_msg_fmt->mcu_reg_id[0], in_msg_fmt->mcu_reg_id[1]
                      , in_msg_fmt->mcu_reg_id[2], in_msg_fmt->mcu_reg_id[3], in_msg_fmt->mcu_reg_id[4], in_msg_fmt->mcu_reg_id[5] );

            PGresult *res = PQexec(conn, sql);
            int rows = PQntuples(res);
            free(sql);
            PQclear(res);
            memcpy(out_msg,in_msg,sizeof(int));
            if     	( rows == 1 )  { out_msg[sizeof(int)] = MSG_OK;	} // I should trigger the QH to send device listto DCU and initialization commands to MCU
            else if	( rows == 0 )  { out_msg[sizeof(int)] = MSG_NOK;	} // Not known device -> should ask for Make / model and / or device list in eeprom
            else			{ out_msg[0] = MSG_ERROR;	} // strange case... we need to investigate what happened here.
            out_msg[1] = 0;

            answer_fifo( fifo, out_msg, 2 );
            printf( "Response sent - %i\n", out_msg[0] );
          }
        break;

        case MSG_RU_SHUTDOWN : ;
          stay_alive = 0;
          break;

        default : ;

      }
    }

  }
  close_fifo(fifo);
  PQfinish(conn);
  finalize_mem_structs();
  // sleep 2 sec to allow all other processes to detach from shared memory
  sleep(2);
  remove_shared_pages();
}
