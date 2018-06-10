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
