/*================================ ZYNQBOARD ==================================
*	PROJECT:	ZYNQ3 v1:	 "ZynqBoard software (Xilinx Zynq-7000, Linux) "
*	FILE:		dma-uapp.c
*	CONTENTS:	User space application.
*				Receives data from several DMA channels, stores received data.
*				Kernel dma proxy driver is used to access DMA channels.
*	VERSION:	01.01  07.02.2020
*	AUTHOR:		Andrey Poroshin
*	UPDATES :
*	1) 01.01   07 February 2020 - Initial version
 ============================================================================== */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "dma-mod-intf.h"

/******************************************************************************
*	Internal definitions
*******************************************************************************/

/******************************************************************************
*	Internal structures
*******************************************************************************/

// DMA channel data receive/store thread parameters
typedef struct THR_PARAMS_s {
	pthread_t	id;				// Thread ID
	uint32_t	created;		// Flag: the thread was created and started (1)
} THR_PARAMS_t;

// DMA channel data receive/store operation parameters
typedef struct CHRC_PARAMS_s {
	uint32_t	ch_idx;			// DMA channel index
	FILE 		*file_store;	// File to store the data
	int 		proxy_fd;		// DMA proxy character device file descriptor
	uint8_t		*kernel_buf;	// Pointer to the DMA channel data buffer
	uint32_t	kernel_buf_sz;	// Kernel buffer size (b)
} CHRC_PARAMS_t;

/******************************************************************************
*	Internal functions
*******************************************************************************/
static int thrStart(uint32_t thr_idx);
static void thrWaitFin(uint32_t thr_idx);
static void *thrMain(void *arg);
static void chRcMain(uint32_t ch_idx);
static int chRcInit(CHRC_PARAMS_t *params);
static void chRcInitParams(uint32_t ch_idx);
static int chRcFlDtOpen(CHRC_PARAMS_t *params);
static int chRcFlDtWrite(CHRC_PARAMS_t *params);
static void chRcFlDtClose(CHRC_PARAMS_t *params);
static int chRcFlProxyOpen(CHRC_PARAMS_t *params);
static void chRcFlProxyClose(CHRC_PARAMS_t *params);
static int chRcMemMap(CHRC_PARAMS_t *params);
static void chRcMemUnmap(CHRC_PARAMS_t *params);
static int chRcDataCycle(CHRC_PARAMS_t *params);
static void chRcDataClrBuf(CHRC_PARAMS_t *params);
static int chRcDataTran(CHRC_PARAMS_t *params);
static void chRcDataPrint(CHRC_PARAMS_t *params);
static void chRcFinalize(CHRC_PARAMS_t *params);

/******************************************************************************
*	Internal data
*******************************************************************************/

// DMA channel data receive/store thread parameters - for each thread
static THR_PARAMS_t thr_params[_DM_CH_NUM];

// DMA channel data receive/store operation parameters - for each thread
static CHRC_PARAMS_t chrc_params[_DM_CH_NUM];

// DMA channel names
static const char	*dm_ch_name[_DM_CH_NUM] = {
	_DM_CHN_AXI_DMA_0,			// Index - _DM_CH_AXI_DMA_0
	_DM_CHN_AXI_DMA_SC			// Index - _DM_CH_AXI_DMA_SC
};

// DMA proxy character device names
static const char 	*chrc_proxy_name[_DM_CH_NUM] = {
	"/dev/"_DM_CHN_AXI_DMA_0,	// Index - _DM_CH_AXI_DMA_0
	"/dev/"_DM_CHN_AXI_DMA_SC	// Index - _DM_CH_AXI_DMA_SC
};

// DMA channel kernel buffer sizes (b)
static const uint32_t chrc_kbuf_sz[_DM_CH_NUM] = {
	_DM_AXI_DMA_0_TRSZ,			// Index - _DM_CH_AXI_DMA_0
	_DM_AXI_DMA_SC_TRSZ			// Index - _DM_CH_AXI_DMA_SC
};

/******************************* main(argc,argv) ******************************
* Main function of the application
* Parameters:
*	(i)argc - Number of arguments (not used)
*	(i)argv - Argument list (not used)
* Return value:
*	Always zero
*******************************************************************************/
int main(int argc, char *argv[])
{
	uint32_t thr_idx;

	printf("dma-uapp: Starting Threads \n");

	// Start threads cycle: one thread for one DMA channel
	for(thr_idx = 0; thr_idx < _DM_CH_NUM; thr_idx++)
		thrStart(thr_idx);					// Start data receive/store thread

	printf("dma-uapp: Threads were started \n");

	// Wait until all threads are finished
	for(thr_idx = 0; thr_idx < _DM_CH_NUM; thr_idx++)
		thrWaitFin(thr_idx);				// Block, wait until the thread is finished

	
	printf("dma-uapp: All threads were finished \n");

	// Application is finished successfully
	return 0;
}

/***************************** thrStart(thr_idx) ******************************
* Start data receive/store thread for one DMA channel
* Used variable:
*	(o)thr_params - thread parameters
* Parameter:
*	(i)thr_idx - index of the thread to start (not checked here)
* Return value:
*	 0 Thread was started successfully
*	-1 Error, the thread was not started
*******************************************************************************/
static int thrStart(uint32_t thr_idx)
{
	THR_PARAMS_t *thr_param;
	pthread_t id;
	int rc;

	// Set the pointer to the thread parameters structure
	thr_param = &thr_params[thr_idx];

	// Create a thread
	rc = pthread_create(&id, NULL, thrMain, (void *)thr_idx);
	if(rc != 0) return -1;					// Can not create thread

	// Store created thread ID in the thread parameters structure
	thr_param -> id = id;

	// Set the flag: the thread was created and started
	thr_param -> created = 1;

	// Thread was started successfully
	return 0;
}

/**************************** thrWaitFin(thr_idx) *****************************
* Wait until thread is finished
* The operation is performed only if the thread was started
* Used variable:
*	(i)thr_params - thread parameters
* Parameter:
*	(i)thr_idx - index of the thread
*******************************************************************************/
static void thrWaitFin(uint32_t thr_idx)
{
	THR_PARAMS_t *thr_param;
	pthread_t id;
	uint32_t created;

	// Set the pointer to the thread parameters structure
	thr_param = &thr_params[thr_idx];

	// Read thread parameters: thread ID and "created" flag
	id = thr_param -> id;
	created = thr_param -> created;

	// Wait until the thread is finished only if the thread was created and started
	if(created) pthread_join(id, NULL);

	// The thread is finished
}

/******************************** thrMain(arg) ********************************
* Main function of DMA channel data receive thread
* Parameter:
*	(i)arg - index of the thread (i.e. DMA channel ID)
* Return value:
*	NULL Data receiving is finished, the thread is finished
*******************************************************************************/
static void *thrMain(void *arg)
{
	uint32_t thr_idx;

	// The argument of the function contains thread index aka DMA channel ID
	thr_idx = (uint32_t)arg;

	// Execute channel data receive operation
	chRcMain(thr_idx);

	// Data receiving is finished, the thread is finished
	return NULL;
}

/****************************** chRcMain(ch_idx) ******************************
* Channel data receive operation: main function
* Inits DMA channel data receiving
* Receives DMA channel data, stores the data in a file
* Used variable:
*	(o)chrc_params - channel data operation parameters
* Parameter:
*	(i)ch_idx - DMA channel index
*******************************************************************************/
static void chRcMain(uint32_t ch_idx)
{
	CHRC_PARAMS_t *params;
	int rc;

	// Init DMA channel operation parameters
	chRcInitParams(ch_idx);
	
	// Set the pointer to the DMA channel operation parameters
	params = &chrc_params[ch_idx];

	// Init DMA channel data receiving
	rc = chRcInit(params);
	if(rc < 0) goto CHRC_FIN;

	// Perform dma receive operation in a cycle
	chRcDataCycle(params);

CHRC_FIN:
	printf("dma-uapp: Data receiving finished ch_idx=%d \n", ch_idx);

	// Free all resources allocated for the channel
	chRcFinalize(params);
}

/****************************** chRcInit(params) ******************************
* Initialize DMA channel data receiving
*	Opens file for writing received data
*	Opens DMA proxy character device
*	Maps the kernel buffer memory into user space
* Parameter: 
*	(o)params - DMA channel data operation parameters
* Return value:
*	 0 Success. Initialization done
*	-1 Initialization failed
*******************************************************************************/
static int chRcInit(CHRC_PARAMS_t *params)
{
	int rc;

	// Open file for writing received data
	rc = chRcFlDtOpen(params);
	if(rc < 0) return rc;				// Can not open the file

	// Open DMA proxy character device
	rc = chRcFlProxyOpen(params);
	if(rc < 0) return rc;				// Can not open the file

	// Map the kernel buffer memory into user space
	return chRcMemMap(params);
}

/*************************** chRcInitParams(ch_idx) ***************************
* Initialize DMA channel operation parameters structure
* This function must be called before all channel initializations
* Used variable:
*	(o)chrc_params - channel data operation parameters
* Parameter:
*	(i)ch_idx - DMA channel index (not checked here)
*******************************************************************************/
static void chRcInitParams(uint32_t ch_idx)
{
	CHRC_PARAMS_t *params;

	// Set the pointer to the DMA channel operation parameters
	params = &chrc_params[ch_idx];

	// Init channel index in the parameters structure
	params -> ch_idx = ch_idx;

	// DMA proxy character device file descriptor is not initialized
	params -> proxy_fd = -1;

	// Init kernel buffer size for the channel
	params -> kernel_buf_sz = chrc_kbuf_sz[ch_idx];
}

/**************************** chRcFlDtOpen(params) ****************************
* Open file for writing received data
* Used variable:
*	(i)dm_ch_name - DMA channel names
* Parameter:
*	(io)params - DMA channel data operation parameters
* Return value:
*	 0 Success. The file was opened
*	-1 Error. Can not open the file
*******************************************************************************/
static int chRcFlDtOpen(CHRC_PARAMS_t *params)
{
	uint32_t ch_idx;
	const char	*fname;
	FILE *file;

	// Get DMA channel index
	ch_idx = params -> ch_idx;
	
	// Set the pointer to the file name
	fname = dm_ch_name[ch_idx];

	// Open the file for writing
	file = fopen(fname,"wb");
	if(file == NULL) return -1;			// Can not open the file

	// Set file structure pointer in DMA channel parameters
	params -> file_store = file;

	// The file was opened successfully
	return 0;
}

/*************************** chRcFlDtWrite(params) ****************************
* Write received data into the file
* Parameter:
*	(i)params - DMA channel data operation parameters
* Return value:
*	 0 Success. Data was written to the file
*	-1 Error. Data was not written to the file
*******************************************************************************/
static int chRcFlDtWrite(CHRC_PARAMS_t *params)
{
	uint8_t *kernel_buf;
	uint32_t kbuf_size;
	FILE *file;
	int sz;

	// Get the pointer to the mapped kernel buffer, read buffer size
	kernel_buf = params -> kernel_buf;
	kbuf_size = params -> kernel_buf_sz;

	// Get the pointer to the file structure
	file = params -> file_store;

	// Write the data from buffer to the file
	sz = fwrite(kernel_buf, 1, kbuf_size, file);
	if(sz != kbuf_size) return -1;			// Data was not written to the file

	// Force writing from the buffer to the file
	fflush(file);

	// Data was successfully written to the file
	return 0;
}

/*************************** chRcFlDtClose(params) ****************************
* Close local file with received data
* The file is closed only if it was opened before
* Parameter:
*	(io)params - DMA channel data operation parameters
*******************************************************************************/
static void chRcFlDtClose(CHRC_PARAMS_t *params)
{
	FILE *file;

	// Read opened file structure pointer
	file = params -> file_store;

	// Close the file only if it was opened
	if(file != NULL) fclose(file);

	// Clear file structure pointer in DMA channel parameters
	params -> file_store = NULL;
}

/************************** chRcFlProxyOpen(params) ***************************
* Open DMA proxy character device
* Used variable:
*	(i)chrc_proxy_name - DMA proxy character device names
* Parameter:
*	(io)params - DMA channel data operation parameters
* Return value:
*	 0 Success. The file was opened
*	-1 Error. Can not open the file
*******************************************************************************/
static int chRcFlProxyOpen(CHRC_PARAMS_t *params)
{
	uint32_t ch_idx;
	int proxy_fd;
	const char	*fname;

	// Get DMA channel index
	ch_idx = params -> ch_idx;

	// Set the pointer to the file name
	fname = chrc_proxy_name[ch_idx];

	// Open DMA proxy character device
	proxy_fd = open(fname, O_RDWR);
	if(proxy_fd < 0) {
		printf("dma-uapp: can not open DMA proxy character device: %s \n", fname);

		// The device was not opened
		return -1;
	}

	// Store file descriptor in DMA channel parameters
	params -> proxy_fd = proxy_fd;

	// The file was opened successfully
	return 0;
}

/************************** chRcFlProxyClose(params) **************************
* Close DMA proxy character device file
* The file is closed only if it was opened before
* Parameter:
*	(io)params - DMA channel data operation parameters
*******************************************************************************/
static void chRcFlProxyClose(CHRC_PARAMS_t *params)
{
	int proxy_fd;
	
	// Read DMA proxy character device file descriptor
	proxy_fd = params -> proxy_fd;

	// Close the file only if it was opened
	if(proxy_fd >= 0) close(proxy_fd);

	// Clear file descriptor in DMA channel parameters
	params -> proxy_fd = -1;
}

/***************************** chRcMemMap(params) *****************************
* Map the kernel buffer memory into user space
* Parameter:
*	(io)params - DMA channel data operation parameters
* Return value:
*	 0 The memory was mapped successfully
*	-1 Memory mapping failed
*******************************************************************************/
static int chRcMemMap(CHRC_PARAMS_t *params)
{
	uint32_t ch_idx;
	int proxy_fd;
	uint32_t kbuf_size;
	uint8_t	*buf;

	// Read DMA channel parameters
	ch_idx = params -> ch_idx;
	proxy_fd = params -> proxy_fd;
	kbuf_size = params -> kernel_buf_sz;

	// Map the kernel buffer memory into user space
	buf = (uint8_t	*)mmap(NULL, kbuf_size, 
				PROT_READ | PROT_WRITE,
				MAP_SHARED, proxy_fd, 0);

	// Check memory mapping result
	if(buf == MAP_FAILED) {
		printf("dma-uapp: Failed to map kernel memory, ch_idx=%d \n",ch_idx);

		// Memory mapping failed
		return -1;
	}

	// Store mapped memory pointer in DMA channel parameters
	params -> kernel_buf = buf;

	// The memory was mapped successfully
	return 0;		
}

/**************************** chRcMemUnmap(params) ****************************
* Unmap kernel buffer memory from user space
* The memory is unmapped only if it was mapped before
* Parameter:
*	(io)params - DMA channel data operation parameters
*******************************************************************************/
static void chRcMemUnmap(CHRC_PARAMS_t *params)
{
	uint8_t *kernel_buf;
	uint32_t kbuf_size;
	
	// Read DMA channel parameters
	kernel_buf = params -> kernel_buf;
	kbuf_size = params -> kernel_buf_sz;

	// Unmap kernel buffer memory if it was mapped
	if(kernel_buf != NULL)
		munmap(kernel_buf, kbuf_size);

	// Clear the pointer to the kernel buffer
	params -> kernel_buf = NULL;
}

/*************************** chRcDataCycle(params) ****************************
* Dma receive cycle
* In the cycle:
*	- clears kernel buffer before data receiving
*	- performs single dma receive operation
*	- prints received data
*	- stores received data in the file
* The function returns only in case of errors
* Parameter:
*	(i)params - DMA channel data operation parameters
* Return value:
*	-1 DMA receive operation failed
*******************************************************************************/
static int chRcDataCycle(CHRC_PARAMS_t *params)
{
	int rc;

	// DMA receive infinite cycle
	while(1){
		// Clear kernel buffer before data receiving
		chRcDataClrBuf(params);

		// Perform single DMA receive transaction
		rc = chRcDataTran(params);
		if(rc < 0) break;		// DMA receive transaction failed

		// Print received data
		chRcDataPrint(params);

		// Write received data into the file
		rc = chRcFlDtWrite(params);
		if(rc < 0) break;		// Can not write to the file
	}

	// DMA receive operation failed
	return -1;
}

/*************************** chRcDataClrBuf(params) ***************************
* Clear kernel buffer before DMA data receiving
* Parameter:
*	(i)params - DMA channel data operation parameters
*******************************************************************************/
static void chRcDataClrBuf(CHRC_PARAMS_t *params)
{
	uint8_t *kernel_buf;
	uint32_t kbuf_size;
	uint32_t i;

	// Get the pointer to the mapped kernel buffer, read buffer size
	kernel_buf = params -> kernel_buf;
	kbuf_size = params -> kernel_buf_sz;

	// Clear the buffer in a cycle
	for(i = 0; i < kbuf_size; i++)
		kernel_buf[i] = 0;
}

/**************************** chRcDataTran(params) ****************************
* Perform single DMA receive transaction
* This function can block.
* Parameter:
*	(io)params - DMA channel data operation parameters
* Return value:
*	 0 Success. DMA receive transaction was executed
*	-1 DMA receive transaction failed
*******************************************************************************/
static int chRcDataTran(CHRC_PARAMS_t *params)
{
	uint32_t ch_idx;
	int proxy_fd;
	_DM_TRAN_RESULT_t res;
	uint32_t res_code;
	int rc;

	// Read DMA channel parameters
	ch_idx = params -> ch_idx;
	proxy_fd = params -> proxy_fd;

	// Execute DMA receive transaction
	// (the function can block here)
	rc = ioctl(proxy_fd, _DM_IOCTL_TRAN_RC, &res);
	if(rc != 0) return -1;				// Can not execute DMA transaction

	// Read DMA transaction result code
	res_code = res.res_code;

	// Check the result code
	if(res_code != _DM_TRAN_RES_SUCCESS){
		printf("dma-uapp: DMA transaction failed, ch_idx=%d res_code=%d \n",
			ch_idx, res_code);

		// DMA receive transaction failed
		return -1;
	}

	// DMA receive transaction was executed successfully
	return 0;
}

/*************************** chRcDataPrint(params) ****************************
* Print received data
* Parameter:
*	(i)params - DMA channel data operation parameters
*******************************************************************************/
static void chRcDataPrint(CHRC_PARAMS_t *params)
{
	uint32_t ch_idx;
	uint8_t *kernel_buf;
	uint32_t kbuf_size;

	// Get DMA channel index
	ch_idx = params -> ch_idx;

	// Get the pointer to the mapped kernel buffer, read buffer size
	kernel_buf = params -> kernel_buf;
	kbuf_size = params -> kernel_buf_sz;

	// Print received data
	printf("Received length=%.8x ch_idx=%d \n", kbuf_size, ch_idx);
	/*for(i = 0; i < kbuf_size; i++){
		if(i % 0x10 == 0) printf("\n");
		printf("%.2x ", kernel_buf[i]);
	}
	printf("\n End of the buffer \n");*/

	// Flush output buffer
	fflush(stdout);
}

/**************************** chRcFinalize(params) ****************************
* Free all resources allocated for the channel
* Opened file descriptors are closed here
* Parameter:
*	(io)params - DMA channel data operation parameters
*******************************************************************************/
static void chRcFinalize(CHRC_PARAMS_t *params)
{
	// Unmap kernel buffer memory from user space
	chRcMemUnmap(params);

	// Close DMA proxy character device
	chRcFlProxyClose(params);

	// Close local file with received data
	chRcFlDtClose(params);
}


