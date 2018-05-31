// -------------------------------------------------------------------------
// Central Control Unit (CCU) - memtypes
//
// Header with the memory structures that are shared between different
// CCU components and the basic functions to access and manipulate these
// components.
//
// -------------------------------------------------------------------------
// Version History
// 1.0 - RvR - 18.05.2018 - Initial version
//
// -------------------------------------------------------------------------
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <xmmintrin.h>
#include  <sys/types.h>
#include  <sys/ipc.h>
#include  <sys/shm.h>


#define	SHARED_MEM_PAGE_SIZE		65535
#define	MAX_NR_SHARED_MEM_PAGES		20
#define	MAX_NR_OF_DEVICES		2048
#define	MAX_NR_OF_MCUS			512
#define	_REG_BYTES			6
#define	_MAKE_BYTES			14
#define MAX_DEVICE_PAYLOAD_SIZE		4096
#define _DCU_START_PORT			5551		// start of the port range on which the DCU will be listening
#define _REG_HEADING_BYTES		50		// int4 representation of the first 4 bytes of the reg_id of the MCU when we set it
							// should be stored in the DB, and when installing and registering the installation
							// a unique int4 will be generated for each installation.

// status of devices
#define	STATUS_NOT_READY		-1
#define	STATUS_DELETED			-2
#define	STATUS_INIT			1
#define	STATUS_READY			2
#define	STATUS_BUSY			3

#define MAX_MSGSIZE 1024

#define MSG_RU_REG_DCU 1
#define MSG_RU_REG_MCU 2
#define MSG_RU_MCU_MAKE 3
#define MSG_RU_SET_DCU 4
#define MSG_RU_SET_REGID 6
#define MSG_RU_GET_DEVLIST 7
#define MSG_CCU_READY 11
#define MSG_CCU_OK 12
#define MSG_CCU_NOK 13
#define MSG_CCU_ERR 14
#define MSG_RU_SHUTDOWN 99


#define MCU_STATUS_INIT 0
#define MCU_STATUS_READY 1
#define MCU_STATUS_OFFLINE 3



struct dev_stack_element {
  short	page_nr;
  int	address_offset;
};
struct mcu_stack_element {
  int	  dcu_id;
  char  status;
  char  reg_id[_REG_BYTES];
  char  make[14];
  char  dcu_ip[4];
  int	  dcu_port;
};

struct device_header {
  int	device_id;
  short	device_type;
  char	status;
  int	mcu_id;
};
struct device_relay {
  char	subtype;
  char	digital_pin;
  long	timer_value;
  int	last_value;
};
struct device_dht11_dht22 {
  char	subtype;
  char	digital_pin;
  long	timer_value;
  int	last_temp;
  int	last_hum;
};

typedef struct dev_stack_element	dev_stack_element_t;
typedef struct mcu_stack_element	mcu_stack_element_t;
typedef struct device_header		device_header_t;
typedef struct device_relay		device_relay_t;
typedef struct device_dht11_dht22	device_dht11_dht22_t;


// some specific message types - we might get this in a seperate include file
struct msg_reg_dcu {
	char ip_address[4];
};
struct msg_reg_mcu {
  int  dcu_id;
  char msg_type;
	unsigned char mcu_reg_id[_REG_BYTES];
};
struct msg_mcu_make {
  int  dcu_id;
  char msg_type;
	char manuf[4];
	char model[6];
	char version[4];
};
struct msg_set_dcu {
	int  dcu_id;
  char dcu_ip[4];
  int  dcu_port;
};
struct msg_unknown_mcu {
  int  dcu_id;
  char msg_type;
	char mcu_reg_id[_REG_BYTES];
};

typedef struct msg_reg_dcu	msg_reg_dcu_t;
typedef struct msg_reg_mcu	msg_reg_mcu_t;
typedef struct msg_mcu_make	msg_mcu_make_t;
typedef struct msg_set_dcu	msg_set_dcu_t;
typedef struct msg_unknown_mcu	msg_unknown_mcu_t;





void**			page_start_address;

int			shmem_device_page[MAX_NR_SHARED_MEM_PAGES];
int			shmem_control_page;

dev_stack_element_t*	device_stack_ptr;
mcu_stack_element_t*	mcu_stack_ptr;

device_header_t*	device_array[MAX_NR_OF_DEVICES];


void ccu_pause() {
//  asm volatile("" ::: "memory");
  _mm_pause();
}

void add_Shm_Device_page(int index) {
	key_t		ShmKEY;
	ShmKEY = ftok(".", (index+2));
	printf( "Attaching a new shared page - %i\n", index );
	if (shmem_device_page[index] == 0) {
		shmem_device_page[index] = shmget(ShmKEY, SHARED_MEM_PAGE_SIZE, 0666);
		page_start_address[index] = (void *) shmat( shmem_device_page[index], NULL, 0 );
	} else {
		printf( "WARNING - not attaching the page %i - it seems to be allready attached\n", index );
	}
}

int get_nr_of_devices() {
	int ret_value = -1;
	for (int i=0 ; i < (MAX_NR_OF_DEVICES) ; i++) {
		if ( device_stack_ptr[i].page_nr == -1 ) {
			ret_value = i;
			i = MAX_NR_OF_DEVICES;
		}
	}
	return (ret_value);
}


void load_device_array() {
	for (int i=0 ; i<MAX_NR_OF_DEVICES ; i++) {
		if (device_stack_ptr[i].page_nr >= 0) {
			if ( shmem_device_page[device_stack_ptr[i].page_nr] == 0 )	{ add_Shm_Device_page(device_stack_ptr[i].page_nr); }
//printf("adding device %i pointing to address %p\n", i, (page_start_address[device_stack_ptr[i].page_nr] + device_stack_ptr[i].address_offset) );
			device_array[i] = (device_header_t*) (page_start_address[device_stack_ptr[i].page_nr] + device_stack_ptr[i].address_offset);
		} else {
			device_array[i] = NULL;
		}
	}
}


// INIT NEEDS TO SET -1 TO ALL PAGE_NR OF THE DEVICES IN THE SHARED MEMORY WHEN CREATING THE CONTROL MEMORY PAGE
void init_mem_structures() {
	key_t          ShmKEY;

  // INIT the shared DEVICE LIST array - contains pointers to all known devices
	//printf( "initilize Device pointer stack of size - %i - element size %i", MAX_NR_OF_DEVICES * sizeof(dev_stack_element_t), sizeof(dev_stack_element_t) );
	ShmKEY = ftok(".", 0);
	shmem_control_page = shmget(ShmKEY, ((sizeof(dev_stack_element_t)*MAX_NR_OF_DEVICES)+(sizeof(mcu_stack_element_t)*MAX_NR_OF_MCUS)), 0666);
	if ( shmem_control_page < 0) {
		printf("*** shmget error (CCU) ***\n");
		exit(1);
	}
	device_stack_ptr = shmat(shmem_control_page, NULL, 0);
	mcu_stack_ptr = (mcu_stack_element_t*) ((char*) device_stack_ptr + (sizeof(dev_stack_element_t)*MAX_NR_OF_DEVICES));

  // INIT the shared MCU LIST array - contains all the found MCU and its connection details


  // INIT the shared stack containing all the devices
	page_start_address = (void*) malloc(MAX_NR_SHARED_MEM_PAGES * sizeof(int*));
	for (int i=0; i<MAX_NR_SHARED_MEM_PAGES; i++) {
		shmem_device_page[i]	= 0;
		page_start_address[i]	= NULL;
	};
	// not actual adding the first page. pages will be created when needed during devices loading
	load_device_array();
	printf( "---- INIT Done ----\n" );
}

void finalize_mem_structs() {
  // FINALIZE detaching all shared memory segments

	printf("--- Finalization of CCU - clearing all structures ----\n");
	for (int i=0 ; i<MAX_NR_SHARED_MEM_PAGES ; i++) {
		if ( shmem_device_page[i] != 0 ) {
			shmdt((void *) page_start_address[i]);
		}
	}

	shmdt((void *) device_stack_ptr);	// Will need to clear other structures present in the control page
	printf("Server has detached its shared memory segments\n");
}
