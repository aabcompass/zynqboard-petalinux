/*================================ ZYNQBOARD ==================================
*	PROJECT:	ZYNQ3 v1:	 "ZynqBoard software (Xilinx Zynq-7000, Linux) "
*	FILE:		scurve-adder-uapp.c
*	CONTENTS:	User space application.
*				SCURVE ADDER register access example.
*	VERSION:	01.01  11.12.2019
*	AUTHOR:		Andrey Poroshin
*	UPDATES :
*	1) 01.01   11 December 2019 - Initial version
 ============================================================================== */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdlib.h>    /* for exit */
#include <getopt.h>


#include "scurve-adder-mod-intf.h"

/******************************************************************************
*	Internal definitions
*******************************************************************************/
#define REGW_SCURVE_ADDER_FLAGS		0
#define REGW_SCURVE_ADDER_ADDS		4

/******************************************************************************
*	Internal structures
*******************************************************************************/

/******************************************************************************
*	Internal functions
*******************************************************************************/
static int initCdevFileOpen(void);
static int cdevFileRegRd(uint32_t regw, uint32_t *val);
static int cdevFileRegWr(uint32_t regw, uint32_t val);
static void cdevFileClose(void);
//static void dpTest(int);
static int sub_getopt(int argc, char **argv);
static void dpCmdRun(int);
static void dpCmdSetFrames(uint32_t param);
static void dpCmdPrintHelp();

/******************************************************************************
*	Internal data
*******************************************************************************/
// File descriptor of the character device
static int fd_cdev = -1;

// Character device file name
static const char *fname_cdev = "/dev/scurve-adder-dev";

/******************************* main(argc,argv) ******************************
* Main function of the program
* Parameters:
*	(i)argc - Number of arguments.
*	(i)argv - Argument list.
* Return value:
*	always 0
*******************************************************************************/
int main(int argc, char *argv[])
{
	uint32_t val;

	printf("Scurve adder control utility: built " __DATE__ " -- " __TIME__"\n");
	// Open character device
	if(initCdevFileOpen() < 0) goto FIN;


	sub_getopt (argc, argv);

FIN:
	printf("scurve-adder-uapp: FINISHING! \n");

	// Close character device file descriptor
	cdevFileClose();

	return 0;
}

/***************************** initCdevFileOpen() *****************************
* Open character device file
* Used variables:
*	(i)fname_cdev - character device file name
*	(o)fd_cdev - file descriptor of the character device
* Return value:
*	-1  Error. Can not open the file
*	0   Success. The file was opened
*******************************************************************************/
static int initCdevFileOpen(void)
{
	int fd_tmp;

	// Open the file for reading and writing
	fd_tmp = open(fname_cdev, O_RDWR);
	if(fd_tmp < 0) return -1;					// Can not open the file

	// Store file descriptor of the character device
	fd_cdev = fd_tmp;

	// The file was opened successfully
	return 0;
}

/************************** cdevFileRegRd(regw,val) ***************************
* Read DATA-PROVIDER register
* Used variable:
*	(i)fd_cdev - file descriptor of the character device
* Parameters:
*	(i)regw - register number
*	(o)val - register value
* Return value:
*	-1 Error. Can not read register value
*	0  Success. The register value was read
*******************************************************************************/
static int cdevFileRegRd(uint32_t regw, uint32_t *val)
{
	int rc;
	_PERIPH_REG_t reg;

	// Set the register number for the operation
	reg.regw = regw;

	// Execute "read register" operation
	rc = ioctl(fd_cdev,_PERIPH_IOCTL_REG_RD, &reg);
	if(rc != 0) return -1;					// Can not read register value

	// Write register value to the user variable
	*val = reg.val;

	// The register value was read successfully
	return 0;
}

/************************** cdevFileRegWr(regw,val) ***************************
* Write DATA-PROVIDER register
* Used variable:
*	(i)fd_cdev - file descriptor of the character device
* Parameters:
*	(i)regw - register number
*	(i)val - register value
* Return value:
*	-1 Error. Can not write new register value
*	0  Success. The register value was updated
*******************************************************************************/
static int cdevFileRegWr(uint32_t regw, uint32_t val)
{
	int rc;
	_PERIPH_REG_t reg;

	// Set register number and new value for the operation
	reg.regw = regw;
	reg.val = val;

	// Execute "write register" operation
	rc = ioctl(fd_cdev,_PERIPH_IOCTL_REG_WR,&reg);
	if(rc != 0) return -1;					// Can not write register value

	// The register value was updated successfully
	return 0;
}

/****************************** cdevFileClose() *******************************
* Close character device file descriptor
* File descriptor is closed only if it was opened
* Used variable:
*	(io)fd_cdev - file descriptor of the character device
*******************************************************************************/
static void cdevFileClose(void)
{
	// If the file was opened - close it
	if(fd_cdev >= 0) close(fd_cdev);

	// Set file descriptor value: file is closed
	fd_cdev = -1;
}

/********************************** dpTest() **********************************
* Perform DATA-PROVIDER register read/write test
* Character device must be opened before calling this function
* Used variable:
*	(i)fd_cdev - file descriptor of the character device
*******************************************************************************/
//static void dpTest(void)
//{
//	uint32_t val;
//	uint32_t regw;
//
//	// Set register number for "read register" operation
//	regw = 5;
//
//	printf("Reading register %d... \n",regw);
//	if(cdevFileRegRd(regw,&val) < 0){
//		printf("Error: Can not read register value\n");
//		return;
//	}
//	printf("Received value = %d \n", val);
//
//	// Set register number for "write register" operation, set the value to write
//	regw = 7;
//	val = 777;
//
//	printf("Writing new value==%d to the register %d... \n",val,regw);
//	if(cdevFileRegWr(regw,val) < 0) {
//		printf("Error: Can not write register value\n");
//		return;
//	}
//	printf("The value was written successfully! \n");
//
//	printf("Test success! \n");
//}

static void saCmdInit()
{
	// Enable auto restart
	cdevFileRegWr(REGW_SCURVE_ADDER_FLAGS, 0x80);
	// Start
	cdevFileRegWr(REGW_SCURVE_ADDER_FLAGS, 0x81);

   	printf("Started\n");
}

static void saCmdSetNAcc(uint32_t param)
{
	cdevFileRegWr(REGW_SCURVE_ADDER_FLAGS, 0x0);
	cdevFileRegWr(REGW_SCURVE_ADDER_ADDS, param);
   	printf("Set N_ADDS to %d\n", param);
   	saCmdInit();
}



int
sub_getopt (int argc, char **argv) {
    int c;
    //int digit_optind = 0;
    int ret, adds_int;
    uint32_t reg;

    while (1) {
        //int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
           // {"init", 0, 0, 'i'},
            {"adds", 1, 0, 'a'},
            {"help", 0, 0, 'h'},
             {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "a:h",
                 long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 0:
            printf ("Parameter %s", long_options[option_index].name);
            if (optarg)
                printf (" with arg. %s", optarg);
            printf ("\n");
            break;
//
//        case 'i':
//        	saCmdInit();
//        	break;

        case 'a':
            //printf ("Parameter frames (f): `%s'\n", optarg);
           // printf("optarg = %s\n", optarg);
            ret = sscanf(optarg, "%d ",  &adds_int);
            //printf("sprintf returned %d\n", ret);
            //printf("frames_int = %d\n", frames_int);
            if(adds_int > 0)
            {
            	saCmdSetNAcc(adds_int);
            }
            else
            {
            	printf("Parameter must be positive\n");
            }
            break;


        case 'h':
        	dpCmdPrintHelp();
        	break;


        default:
            //printf ("?? getopt возвратило код символа 0%o ??\n", c);
        	printf ("BAD OPTION\n", c);
        }
    }

    if (optind < argc) {
        //printf ("элементы ARGV, не параметры: ");
    	printf ("No such params: ");
        while (optind < argc)
            printf ("%s ", argv[optind++]);
        printf ("\n");
    }
}

static void dpCmdPrintHelp()
{
    printf("NAME\n");
    printf("\t scurve-adder-uapp - scurve adder management utility\n");
    printf("\n");

    printf("SYNOPSYS\n");
    printf("\t scurve-adder-uapp [OPTIONS] ...\n");
    printf("\n");

    printf("DESCRIPTION\n");
    printf("\t Controls data flow. \n");
    printf("\n");

    printf("\t -a, --adds=[1..65535]\n");
    printf("\t\t Set number of additions and start.\n");
    printf("\n");

    printf("\t -h, --help\n");
    printf("\t\t print this page.\n");
    printf("\n");

    printf("AUTHOR\n");
    printf("\t Alexander Belov. SINP MSU\n");
    printf("\n");

}
