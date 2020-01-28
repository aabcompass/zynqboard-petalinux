/*================================ ZYNQBOARD ==================================
*	PROJECT:	ZYNQ3 v1:	 "ZynqBoard software (Xilinx Zynq-7000, Linux) "
*	FILE:		scurve-adder-mod-intf.h
*	CONTENTS:	Header file. Provides ioctl interface between 
*					Common peripheral kernel driver and user space application
*	VERSION:	01.01  10.12.2019
*	AUTHOR:		Andrey Poroshin
*	UPDATES :
*	1) 01.01   10 December 2019 - Initial version
 ============================================================================== */
#ifndef SCURVE_ADDER_MOD_INTF__H
#define SCURVE_ADDER_MOD_INTF__H

// Total number of DATA-PROVIDER registers
#define _PERIPH_REGS_NUM		64

// The structure with 32-bit DATA-PROVIDER register value
typedef struct _PERIPH_REG_s {
	uint32_t regw;					// Register number
	uint32_t val;					// Register value
} _PERIPH_REG_t;

// Ioctl call type (8-bit)
#define _PERIPH_IOC_MAGIC    'h'

// Ioctl function codes (nr - sequence numbers) (8-bit)
#define _PERIPH_IOC_NR_RD	1		// Read register function
#define _PERIPH_IOC_NR_WR	2		// Write register function

// Ioctl register read request code (32-bit)
#define _PERIPH_IOCTL_REG_RD	_IOWR(_PERIPH_IOC_MAGIC, \
										_PERIPH_IOC_NR_RD, \
										_PERIPH_REG_t)

// Ioctl register write request code (32-bit)
#define _PERIPH_IOCTL_REG_WR	_IOW(_PERIPH_IOC_MAGIC, \
										_PERIPH_IOC_NR_WR, \
										_PERIPH_REG_t)

#endif /* SCURVE_ADDER_INTF__H */
