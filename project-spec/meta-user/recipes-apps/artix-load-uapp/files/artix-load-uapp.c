/*================================ ZYNQBOARD ==================================
*	PROJECT:	ZYNQ3 v1:	 "ZynqBoard software (Xilinx Zynq-7000, Linux) "
*	FILE:		artix-uapp.c
*	CONTENTS:	User space application.
*				Provides firmware loading to artix.
*	VERSION:	01.01  10.10.2019
*	AUTHOR:		Andrey Poroshin
*	UPDATES :
*	1) 01.01   10 October 2019 - Initial version
 ============================================================================== */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include <errno.h>

/******************************************************************************
*	Internal definitions
*******************************************************************************/

// Max size of artix bitstream data (b)
#define BITSTR_BUF_SZ	10000000		// 10 MBytes

// Spi default mode (Mode 0: CPOL = 0 CPHA = 0)
#define SPI_DEFAULT_MODE	0

// Spi number of bits per word
#define SPI_BITS_PER_WORD		8

// Spi default max clock speed (Hz)
#define SPI_DEFAULT_SPEED	6250000

// Spi max message size to transmit at a moment (b)
#define SPI_MESSAGE_SZ_MAX	2048

/******************************************************************************
*	Internal structures
*******************************************************************************/

/******************************************************************************
*	Internal functions
*******************************************************************************/
static int init(void);
static int initBsFileOpen(void);
static int initSpi(void);
static int initSpiFileOpen(void);
static int initSpiSetPar(void);
static int initSpiSetParMd(void);
static int initSpiSetParSp(void);
static void taskFinalize(void);
static int bsFileRead(void);
static void bsFileClose(void);
static int spiTrData(void);
static int spiTrDataPart(uint8_t *buf, uint32_t len);
static void spiFileClose(void);

/******************************************************************************
*	Internal data
*******************************************************************************/
static FILE *file_bitstr = NULL;	// File descriptor of a file with bitstream data
static int fd_spi = -1;				// File descriptor of spi device

// The buffer to store artix bitstream data
static uint8_t bitstr_buf[BITSTR_BUF_SZ];

// Bitstream data length (b)
static uint32_t bitstr_len;

// Name of the file with artix bitstream data
static const char *fname_bitstr = "/run/media/mmcblk0p1/top_art1.bit";

// Name of the file for spi slave device
static const char *fname_spi_slave = "/dev/spidev0.0";

/******************************* main(argc,argv) ******************************
* Main function of the program
* Parameters:
*	(i)argc - Number of arguments. Not Used
*	(i)argv - Argument list. Not Used
* Return value:
*	always 0
*******************************************************************************/
int main(int argc, char *argv[])
{
	int rc;

	printf("artix-uapp: START \n");

	// Init the program
	if(init() < 0) goto FIN;

	printf("artix-uapp: initialized successfully \n");

	// Read artix bitstream data from file, store it in the local buffer
	if(bsFileRead() < 0) goto FIN;

	printf("artix-uapp: file was read \n");

	// Send bitstream data to artix
	if(spiTrData() < 0) goto FIN;

	printf("artix-uapp: data was transmitted to artix \n");

FIN:
	printf("artix-uapp: FINISHING \n");

	// Free task resources: close opened file descriptors
	taskFinalize();

	return 0;
}

/*********************************** init() ***********************************
* Initilization of the program
* Opens artix bitstream data file for reading
* Inits spi device file descriptor, set spi device parameters
* Used global variables:
*	(o)file_bitstr - file descriptor of a file with bitstream data
*	(o)fd_spi - file descriptor of spi device
* Return value:
*	-1	Initialization error
*	0	Ininitialization done
*******************************************************************************/
static int init(void)
{
	// Open artix bitstream data file for reading
	if(initBsFileOpen() < 0) return -1;

	// Init spi device file descriptor, set spi device parameters
	return initSpi();
}

/****************************** initBsFileOpen() ******************************
* Open artix bitstream data file for reading
* Used variables:
*	(i)fname_bitstr - name of the file with artix bitstream data
*	(o)file_bitstr - file descriptor of a file with bitstream data
* Return value:
*	-1  Error. Can not open the file
*	0   Success. The file was opened.
*******************************************************************************/
static int initBsFileOpen(void)
{
	FILE *file;

	// Open the file for reading
	file = fopen(fname_bitstr, "r");
	if(file == NULL) return -1;					// Can not open the file

	// Store the pointer to the file descriptor in the correspondent variable
	file_bitstr = file;

	// The file was opened successfully
	return 0;
}

/********************************* initSpi() **********************************
* Init spi device file descriptor, set spi device parameters
* Used variables:
*	(i)fname_spi_slave - name of the file for spi slave device
*	(o)fd_spi - file descriptor of spi device
* Return value:
*	-1 Error. Can not open device or set parameters of the device
*	0  Success. Spi device was initialized
*******************************************************************************/
static int initSpi(void)
{
	// Open spi character device file
	if(initSpiFileOpen() < 0) return -1;			// Can not open device file

	// Set spi device parameters
	return initSpiSetPar();
}

/***************************** initSpiFileOpen() ******************************
* Open spi character device file
* Used variables:
*	(i)fname_spi_slave - name of the file for spi slave device
*	(o)fd_spi - file descriptor of spi device
* Return value:
*	-1 Error. Can not open the file
*	0  Success. Spi character device file was opened
*******************************************************************************/
static int initSpiFileOpen(void)
{
	int fd_tmp;

	// Open the file for reading and writing
	fd_tmp = open(fname_spi_slave, O_RDWR);
	if(fd_tmp < 0) return -1;					// Can not open the file

	// Store file descriptor of spi device in correspondent variable
	fd_spi = fd_tmp;

	// The file was opened successfully
	return 0;
}

/****************************** initSpiSetPar() *******************************
* Set the parameters of the spi device:
* 	SPI mode, SPI max clock speed(Hz).
* Used variable:
*	(i)fd_spi - file descriptor of spi device
* Return value:
*	-1 Error. Can not set parameters of the device
*	0  Success. Spi device parameters were set
*******************************************************************************/
static int initSpiSetPar(void)
{
	// Set the mode of the spi device
	if(initSpiSetParMd() < 0)
		return -1;					// Can not set Spi mode

	// Set max clock speed for spi device
	return initSpiSetParSp();
}

/***************************** initSpiSetParMd() ******************************
* Set the mode of the spi device
* Spi mode defines clock polarity,phase etc.
* Used variable:
*	(i)fd_spi - file descriptor of spi device
* Return value:
*	-1 Error: Spi mode was not changed
*	0  Spi mode was changed successfully
*******************************************************************************/
static int initSpiSetParMd(void)
{
	uint8_t mode;

	// Use default spi mode
	mode = SPI_DEFAULT_MODE;

	// Make kernel call to change Spi mode
	if(ioctl(fd_spi, SPI_IOC_WR_MODE, &mode) < 0)
		return -1;	// Spi mode was not changed

	// Spi mode was changed successfully
	return 0;
}

/***************************** initSpiSetParSp() ******************************
* Set max clock speed for spi device
* Used variable:
*	(i)fd_spi - file descriptor of spi device
* Return value:
*	-1 Error: Spi max clock speed was not set
*	0  Spi device max clock speed was set successfully
*******************************************************************************/
static int initSpiSetParSp(void)
{
	uint32_t speed;

	// Write max clock speed to the local variable
	speed = SPI_DEFAULT_SPEED;

	if(ioctl(fd_spi,SPI_IOC_WR_MAX_SPEED_HZ,&speed) < 0)
		return -1;	// Spi clock speed was not set

	// Spi device max clock speed was set successfully
	return 0;
}



/******************************* taskFinalize() *******************************
* Free task resources: close opened file descriptors
* The descriptors are closed only if they were opened before
* Used variables:
*	(io)file_bitstr - file descriptor of a file with bitstream data
*	(io)fd_spi - file descriptor of spi device
*******************************************************************************/
static void taskFinalize(void)
{
	// Close bitstream data file descriptor
	bsFileClose();

	// Close spi device file descriptor
	spiFileClose();
}

/******************************** bsFileRead() ********************************
* Read artix bitstream data from file, store it in the local buffer
* Used variables:
*	(i)file_bitstr - file descriptor of a file with bitstream data
*	(o)bitstr_buf - the buffer where bitstream data is stored
*	(o)bitstr_len - bitstream data length (b)
* Return value:
*	-1 Error. Artix bitstream data was not read
*	0  Success. The data was read
*******************************************************************************/
static int bsFileRead(void)
{
	uint32_t i;
	int c;

	// Read the buffer from file in a cycle
	for(i = 0; i < BITSTR_BUF_SZ; i++){
		// Read one character
		c = fgetc(file_bitstr);

		// Check for the end of file
		if(c == EOF) {
			// End of file was reached
			// Check if the data was received
			if(i > 0) {
				// Set the length of bitstream data
				bitstr_len = i;

				// The data was read successfully
				return 0;
			}

			// Stop reading of the file
			break;
		}

		// Next character was read. Store it in the buffer
		bitstr_buf[i] = (uint8_t)c;
	}

	// Artix bitstream data was not read
	return -1;
}

/******************************* bsFileClose() ********************************
* Close bitstream data file descriptor
* File descriptor is closed only if it was opened
* Used variable:
*	(io)file_bitstr - file descriptor of a file with bitstream data
*******************************************************************************/
static void bsFileClose(void)
{
	// If the file was opened - close it
	if(file_bitstr != NULL) fclose(file_bitstr);

	// Set file descriptor value: file is closed
	file_bitstr = NULL;
}

/******************************** spiTrData() *********************************
* Send bitstream data to artix
* Used variables:
*	(i)bitstr_buf - the buffer where bitstream data is stored
*	(i)bitstr_len - bitstream data length (b)
*	(i)fd_spi - file descriptor of spi device
* Return value:
*	-1 Error. Spi data exchange transaction failed
*	0  Success. Spi bitstream data was transmitted
*******************************************************************************/
static int spiTrData(void)
{
	uint8_t *buf_ptr;
	uint32_t sz_left;
	uint32_t sz_part;
	int rc;

	// Set buffer pointer and "left size" variables
	buf_ptr = bitstr_buf;
	sz_left = bitstr_len;

	// Send the data in a cycle
	while(sz_left > 0){
		// Calculate the size of the part of the message
		if(sz_left > SPI_MESSAGE_SZ_MAX)
			sz_part = SPI_MESSAGE_SZ_MAX;
		else
			sz_part = sz_left;

		// Send small portion of data
		rc = spiTrDataPart(buf_ptr, sz_part);
		if(rc < 0) return -1;			// Transaction failed

		// Update buffer pointer and left size
		buf_ptr += sz_part;
		sz_left -= sz_part;
	}

	// Spi bitstream data was transmitted successfully
	return 0;
}

/*************************** spiTrDataPart(buf,len) ***************************
* Send small portion of data via spi to artix
* Only small portions of data can be processed by spidev driver
* Used variable:
*	(i)fd_spi - file descriptor of spi device
* Parameters:
*	(i)buf - data buffer to transmit
*	(i)len - length of the message to transmitt
* Return value:
*	-1 Error. Spi data exchange transaction failed
*	0  Success. Data from the buffer was transmitted
*******************************************************************************/
static int spiTrDataPart(uint8_t *buf, uint32_t len)
{
	struct spi_ioc_transfer tr;
	int rc;

	// Clear spi transfer structure
	memset(&tr, 0, sizeof(tr));

	// Set spi transfer structure parameters
	tr.tx_buf = (unsigned long)buf;
	tr.rx_buf = (unsigned long)NULL;
	tr.len = len;
	tr.speed_hz = SPI_DEFAULT_SPEED;
	tr.bits_per_word = SPI_BITS_PER_WORD;

	// Execute SPI transaction
	rc = ioctl(fd_spi, SPI_IOC_MESSAGE(1), &tr);
	if(rc < 0){
		printf("artix-uapp: spiTrDataPart Transaction failed!!! \n");
		printf("artix-uapp: ioctl returned %d \n", rc);
		printf("artix-uapp: errno=%d  comment: %s \n",errno, strerror(errno));

		// Transaction failed
		return -1;
	}

	// Data from the buffer was transmitted successfully
	return 0;
}

/******************************* spiFileClose() *******************************
* Close spi device file descriptor
* File descriptor is closed only if it was opened
* Used variable:
*	(io)fd_spi - file descriptor of spi device
*******************************************************************************/
static void spiFileClose(void)
{
	// If the file was opened - close it
	if(fd_spi >= 0) close(fd_spi);

	// Set file descriptor value: file is closed
	fd_spi = -1;
}


