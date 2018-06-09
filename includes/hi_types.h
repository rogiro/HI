// -------------------------------------------------------------------------
// HI types
//
// All general definitions of message codes, error codes and statuses
// for all components of the architecture (CCU, DCU and MCU)
//
// -------------------------------------------------------------------------
// Version History
// 1.0 - RvR - 18.05.2018 - Initial version
//
// -------------------------------------------------------------------------


#define	_REG_BYTES			6
#define	_MAKE_BYTES			14

// status of devices
#define	STATUS_NOT_READY		-1
#define	STATUS_DELETED			-2
#define	STATUS_INIT		     	1
#define	STATUS_READY  			2
#define	STATUS_BUSY		     	3
#define	STATUS_OFFLINE     	4

#define MAX_MSGSIZE 1024

// Message Types
// Generic messages
#define MSG_OK          4
#define MSG_NOK         5
#define MSG_READY       6
#define MSG_ERROR       9

// Reg Unit messages
#define MSG_RU_REG_DCU 91
#define MSG_RU_SET_DCU 92
#define MSG_RU_SHUTDOWN 99

// CCU messages
#define MSG_REG_MCU           21
#define MSG_GET_DEV_DETAILS   22
#define MSG_SET_DEV_DETAILS   23

// Error subtypes
#define ERR_TYPE_CRC_ERR    4
#define ERR_TYPE_OVERFLOW   5
#define ERR_TYPE_UNEXP_INP  6
#define ERR_TYPE_INV_MSG    7
#define ERR_TYPE_TIMEOUT    8

// SAMP messages
#define MSG_SAMP_HELLO        101
#define MSG_SAMP_RESET        102
#define MSG_SAMP_GET_REGID    103
#define MSG_SAMP_SET_REGID    104
#define MSG_SAMP_GET_MODEL    105
#define MSG_SAMP_SET_MODEL    106
#define MSG_SAMP_GET_DEVICE   107
#define MSG_SAMP_SET_DEVICE   108
#define MSG_SAMP_KEEPALIVE    109
#define MSG_SAMP_TRANSM_ERR   110



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
