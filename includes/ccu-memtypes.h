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

#include  "hi_types.h"
#include  "hi_structs.h"


#define	SHARED_MEM_PAGE_SIZE		65535
#define	MAX_NR_SHARED_MEM_PAGES		20
#define	MAX_NR_OF_DEVICES		2048
#define	MAX_NR_OF_MCUS			512
#define	_REG_BYTES			6
#define	_MAKE_BYTES			14
#define MAX_DEVICE_PAYLOAD_SIZE		4096
#define _DCU_START_PORT			5551		// start of the port range on which the DCU will be listening







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
