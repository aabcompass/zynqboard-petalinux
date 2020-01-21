/*================================ ZYNQBOARD ==================================
*	PROJECT:	ZYNQ3 v1:	 "ZynqBoard software (Xilinx Zynq-7000, Linux) "
*	FILE:		dataprov-mod-intf.h
*	CONTENTS:	Header file. Provides ioctl interface between 
*					DATA-PROVIDER kernel driver and user space application
*	VERSION:	01.01  10.12.2019
*	AUTHOR:		Andrey Poroshin
*	UPDATES :
*	1) 01.01   10 December 2019 - Initial version
 ============================================================================== */
#ifndef DATAPROV_MOD_INTF__H
#define DATAPROV_MOD_INTF__H

// Total number of DATA-PROVIDER registers
#define _DATAPROV_REGS_NUM		64

// The structure with 32-bit DATA-PROVIDER register value
typedef struct _DATAPROV_REG_s {
	uint32_t regw;					// Register number
	uint32_t val;					// Register value
} _DATAPROV_REG_t;

// Ioctl call type (8-bit)
#define _DATAPROV_IOC_MAGIC    'h'

// Ioctl function codes (nr - sequence numbers) (8-bit)
#define _DATAPROV_IOC_NR_RD	1		// Read register function
#define _DATAPROV_IOC_NR_WR	2		// Write register function

// Ioctl register read request code (32-bit)
#define _DATAPROV_IOCTL_REG_RD	_IOWR(_DATAPROV_IOC_MAGIC, \
										_DATAPROV_IOC_NR_RD, \
										_DATAPROV_REG_t)

// Ioctl register write request code (32-bit)
#define _DATAPROV_IOCTL_REG_WR	_IOW(_DATAPROV_IOC_MAGIC, \
										_DATAPROV_IOC_NR_WR, \
										_DATAPROV_REG_t)

#endif /* DATAPROV_MOD_INTF__H */
