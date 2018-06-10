// -------------------------------------------------------------------------
// HI structs
//
// All general structures
//
// -------------------------------------------------------------------------
// Version History
// 1.0 - RvR - 18.05.2018 - Initial version
//
// -------------------------------------------------------------------------


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
