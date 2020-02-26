/*================================ ZYNQBOARD ==================================
*	PROJECT:	ZYNQ3 v1:	 "ZynqBoard software (Xilinx Zynq-7000, Linux) "
*	FILE:		dma-mod-intf.h
*	CONTENTS:	Header file. Provides ioctl interface between 
*				DMA-PROXY pseudo device and user application
*	VERSION:	01.01  30.01.2020
*	AUTHOR:		Andrey Poroshin
*	UPDATES :
*	1) 01.01   30 January 2020 - Initial version
 ============================================================================== */

#ifndef DMA_MOD_INTF__H
#define DMA_MOD_INTF__H

// DMA channels
typedef enum _DM_CH_e {
	_DM_CH_AXI_DMA_0,
	_DM_CH_AXI_DMA_SC
} _DM_CH_t;
#define _DM_CH_NUM		(_DM_CH_AXI_DMA_SC + 1)

// DMA channel names
#define _DM_CHN_AXI_DMA_0	"axi_dma_0"		// For _DM_CH_AXI_DMA_0 channel
#define	_DM_CHN_AXI_DMA_SC	"axi_dma_sc36"	// For _DM_CH_AXI_DMA_SC channel

// Size of one DMA transaction (for each DMA channel) (b)
#define _DM_AXI_DMA_0_TRSZ	(48*48*128)
#define _DM_AXI_DMA_SC_TRSZ	(48*48*4)

// DMA transaction result codes
typedef enum _DM_TRAN_RES_CODE_e {
	_DM_TRAN_RES_SUCCESS,		// Transaction was executed successfully
	_DM_TRAN_RES_TIMEOUT,		// Error: timeout
	_DM_TRAN_RES_ERROR			// Other error
} _DM_TRAN_RES_CODE_t;

// DMA transaction result structure (for user space application)
typedef struct _DM_TRAN_RESULT_s {
	uint32_t res_code;				// DMA transaction result code
} _DM_TRAN_RESULT_t;

// Ioctl call type (8-bit)
#define _DM_IOC_MAGIC    	'i'

// Ioctl function code (nr - sequence number) (8-bit)
#define _DM_IOC_NR_TRAN_RC	1	// Execute DMA data receive transation

// Ioctl "execute DMA data receive transaction" code (32-bit)
#define _DM_IOCTL_TRAN_RC	_IOR(_DM_IOC_MAGIC, \
									_DM_IOC_NR_TRAN_RC, \
									_DM_TRAN_RESULT_t)

#endif /* DMA_MOD_INTF__H */

