// -------------------------------------------------------------------------
// Central Control Unit (CCU) - memtypes-db
//
// Header with the memory structures tfunctions needed by the CCU-DB
// to initialy populate the shared memory structures and to close them
// gracefully.
//
// -------------------------------------------------------------------------
// Version History
// 1.0 - RvR - 18.05.2018 - Initial version
//
// -------------------------------------------------------------------------
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>

#include  <sys/types.h>
#include  <sys/ipc.h>
#include  <sys/shm.h>

#include  "ccu-memtypes.h"

void**			page_free_address;


void create_Shm_Device_page(int index) {
	key_t		ShmKEY;
	ShmKEY = ftok(".", (index+2));
	printf( "Create a new shared page - %i\n", index );
	if (shmem_device_page[index] == 0) {
		shmem_device_page[index] = shmget(ShmKEY, SHARED_MEM_PAGE_SIZE, IPC_CREAT | 0666);
		page_start_address[index] = (void *) shmat( shmem_device_page[index], NULL, 0 );
		page_free_address[index] = page_start_address[index];
	} else {
		printf( "ERROR - not adding the page %i - it seems to exist allready\n", index );
	}
}


int get_Shm_page_for_device(int size) {
	int ret_value = -1;
	int index = 0;
	//printf( "Looking for a device page for a device of size %i\n", size );
	if (size <= SHARED_MEM_PAGE_SIZE) {
		while ( (ret_value < 0) && (index < MAX_NR_SHARED_MEM_PAGES) ) {
			if (shmem_device_page[index] == 0) {
				create_Shm_Device_page(index);
				ret_value = index;
			} else {
				//printf("looking for page - i got %i bytes if i use this page nr %i wich is %p - %p\n"
				//		, ((page_free_address[index]-page_start_address[index])+size)
				//		, index
				//		, page_free_address[index]
				//		, page_start_address[index] );
				if (((page_free_address[index]-page_start_address[index])+size)<=SHARED_MEM_PAGE_SIZE ) {
					ret_value = index;
				} else {
					index++;
				}
			}
		}
	}
//	printf( "Found a device page for the device : %i\n", ret_value );
	return (ret_value);
}

int set_new_device(dev_stack_element_t* reference) {
	int ret_value = get_nr_of_devices();
//	printf( "pushing device on stack - device_id = %i\n", ret_value );

	if ( (ret_value < 0) || (ret_value >=  MAX_NR_OF_DEVICES) ) {
		printf("ERROR - unable to push the device, get_nr_of_devices did not return a valid value\n");
	} else {
		device_stack_ptr[ret_value].page_nr		= (*reference).page_nr;
		device_stack_ptr[ret_value].address_offset	= (*reference).address_offset;
	}
	return (ret_value);
}


void add_device_to_Shm(int* device_id, int* device_type, void *devPointer, int* size) {
	device_header_t*	new_device;
//	printf("adding device (size %i + %i bytes) in page %i\n", sizeof(device_header_t), *size, index);
//printf( "Device Pointer : ");
//for (int j=0; j<(sizeof(device_relay_t)); j++) { printf("%i - ", *(((char*)devPointer)+j) ); } printf("\n");

	int index = get_Shm_page_for_device( *size+sizeof(device_header_t) );
	if (index<0) {
		printf("ERROR - did not found a valid page - device NOT added\n");
		return;
	}

	new_device = (device_header_t*) page_free_address[index];
	(*new_device).device_type	= *device_type;
	(*new_device).status		= STATUS_NOT_READY;  // initializing
//	printf("launching the memcopy for %i bytes from %p to dest %p\n", *size, devPointer, page_free_address[index]+sizeof(device_header_t) );
	memcpy( page_free_address[index]+sizeof(device_header_t), devPointer, *size );

	dev_stack_element_t	tmp_stack_element;
	tmp_stack_element.page_nr = index;
	tmp_stack_element.address_offset = (page_free_address[index] - page_start_address[index]);
	(*new_device).device_id = *device_id;
//printf( "attached device of type %i and status %i and id %i\n", (*new_device).device_type, (*new_device).status, (*new_device).device_id );
//printf( "Bytes dump : "); for (int j=0; j<((*size)+sizeof(device_header_t)); j++) { printf(" %hhx - ", *(((char*)new_device)+j) ); } printf("\n");

	set_new_device((dev_stack_element_t*) &tmp_stack_element); 
//	printf(" - set new device %i on the stack\n", new_device->device_id);
	printf(" -> add device %i in page %i (dev size %i bytes)\n", (*new_device).device_id, index, sizeof(device_header_t) + *size);
	page_free_address[index] = page_free_address[index] + sizeof(device_header_t) + *size;
}

// INIT NEEDS TO SET -1 TO ALL PAGE_NR OF THE DEVICES IN THE SHARED MEMORY WHEN CREATING THE CONTROL MEMORY PAGE
void init_mem_structures_db() {
	key_t          ShmKEY;

  // INIT the shared DEVICE LIST array - contains pointers to all known devices
//	printf( "initilize Device pointer stack of size - %i - element size %i", MAX_NR_OF_DEVICES * sizeof(dev_stack_element_t), sizeof(dev_stack_element_t) );
	ShmKEY = ftok(".", 0);
	shmem_control_page = shmget(ShmKEY, ((sizeof(dev_stack_element_t)*MAX_NR_OF_DEVICES)+(sizeof(mcu_stack_element_t)*MAX_NR_OF_MCUS)), IPC_CREAT | 0666);
	if ( shmem_control_page < 0) {
		printf("*** shmget error (CCU) ***\n");
		exit(1);
	}
//	printf( "Setting to NULL all pointers in the stack\n" );
	device_stack_ptr = shmat(shmem_control_page, NULL, 0);
	mcu_stack_ptr = (mcu_stack_element_t*) ((char*) device_stack_ptr + (sizeof(dev_stack_element_t)*MAX_NR_OF_DEVICES));

	for (int i=0 ; i < (MAX_NR_OF_DEVICES) ; i++) {
		device_stack_ptr[i].page_nr = -1;
	}
	for (int i=0 ; i < (MAX_NR_OF_MCUS) ; i++) {
		mcu_stack_ptr[i].dcu_id = -1;
	}



  // INIT the shared MCU LIST array - contains all the found MCU and its connection details


  // INIT the shared stack containing all the devices
	page_start_address = (void*) malloc(MAX_NR_SHARED_MEM_PAGES * sizeof(int*));
	for (int i=0; i<MAX_NR_SHARED_MEM_PAGES; i++) {
		shmem_device_page[i]	= 0;
		page_start_address[i]	= NULL;
	};
	page_free_address = (void*) malloc(MAX_NR_SHARED_MEM_PAGES * sizeof(int*));
	for (int i=0; i<MAX_NR_SHARED_MEM_PAGES; i++) {
		page_free_address[i]	= NULL;
	};
	printf( "---- INIT Done ----\n" );
}

void remove_shared_pages() {
  // FINALIZE remove all shared memory segments - we need to finalize the other CCU processes first

//	printf("--- Finalization of CCU - clearing all structures ----\n");
	for (int i=0 ; i<MAX_NR_SHARED_MEM_PAGES ; i++) {
		if ( shmem_device_page[i] != 0 ) {
			shmctl(shmem_device_page[i], IPC_RMID, NULL);	// only the CCU-DB should remove the pages.
			//printf("Server has detached and removed its shared memory of the device segment %i\n",i);
		}
	}

	shmctl(shmem_control_page, IPC_RMID, NULL);	// only the CCU-DB should remove the pages.
	printf("Server has removed its shared memory segments\n");
}
