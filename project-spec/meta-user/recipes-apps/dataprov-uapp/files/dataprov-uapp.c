/*================================ ZYNQBOARD ==================================
*	PROJECT:	ZYNQ3 v1:	 "ZynqBoard software (Xilinx Zynq-7000, Linux) "
*	FILE:		dataprov-uapp.c
*	CONTENTS:	User space application.
*				DATA-PROVIDER register access example.
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


#include "dataprov-mod-intf.h"

/******************************************************************************
*	Internal definitions
*******************************************************************************/

#define REGW_DATAPROV_FLAGS		0
#define REGW_DATAPROV_N_FRAMES	3
#define REGW_DATAPROV_FLAGS2	10
#define REGR_STATUS				16

//REGW_DATAPROV_FLAGS
#define BIT_START_SIG			0
#define BIT_RUN					1
//REGW_DATAPROV_FLAGS2
#define BIT_INFINITE			0
#define BIT_GTU_1US				1
#define BIT_RUN_DATACONV		2

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
static int cmd_run = -1;

// Character device file name
static const char *fname_cdev = "/dev/dataprov-dev";

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

	printf("Data provider control utility: built " __DATE__ " -- " __TIME__"\n");
	// Open character device
	if(initCdevFileOpen() < 0) goto FIN;

	// Perform DATA-PROVIDER register read/write test
	//dpTest();
	// Read commands from command line and execute

	sub_getopt (argc, argv);
	// Run postponed cmd
	if(cmd_run == 1 || cmd_run == 0)
	{
		dpCmdRun(cmd_run);
	}

FIN:
	printf("dataprov-uapp: FINISHING! \n");

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
	_DATAPROV_REG_t reg;

	// Set the register number for the operation
	reg.regw = regw;

	// Execute "read register" operation
	rc = ioctl(fd_cdev,_DATAPROV_IOCTL_REG_RD, &reg);
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
	_DATAPROV_REG_t reg;

	// Set register number and new value for the operation
	reg.regw = regw;
	reg.val = val;

	// Execute "write register" operation
	rc = ioctl(fd_cdev,_DATAPROV_IOCTL_REG_WR,&reg);
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

static void dpCmdRun(int param)
{
	uint32_t reg;
	if(param == 1)
    {
		cdevFileRegRd(REGW_DATAPROV_FLAGS2, &reg);
		cdevFileRegWr(REGW_DATAPROV_FLAGS2, reg | (1<<BIT_RUN_DATACONV));

    	cdevFileRegWr(REGW_DATAPROV_FLAGS, (1<<BIT_START_SIG) | (1<<BIT_RUN));
    	cdevFileRegWr(REGW_DATAPROV_FLAGS, (1<<BIT_RUN));
    	printf("Started\n");
    }
    else
    {
    	cdevFileRegWr(REGW_DATAPROV_FLAGS, 0);
    	printf("Stopped\n");
    }
}

static void dpCmdSetFrames(uint32_t param)
{
   	cdevFileRegWr(REGW_DATAPROV_N_FRAMES, param);
}

static void dpCmdSetInfinite(uint32_t param)
{
	uint32_t reg;
	cdevFileRegRd(REGW_DATAPROV_FLAGS2, &reg);
	if(param == 1)
	{
		cdevFileRegWr(REGW_DATAPROV_FLAGS2, reg | (1<<BIT_INFINITE));
	}
	else
	{
		cdevFileRegWr(REGW_DATAPROV_FLAGS2, reg & ~(1<<BIT_INFINITE));
	}
}

static void dpCmdSetGtu1us(uint32_t param)
{
	uint32_t reg;
	cdevFileRegRd(REGW_DATAPROV_FLAGS2, &reg);
	if(param == 1)
	{
		cdevFileRegWr(REGW_DATAPROV_FLAGS2, reg | (1<<BIT_GTU_1US));
	}
	else
	{
		cdevFileRegWr(REGW_DATAPROV_FLAGS2, reg & ~(1<<BIT_GTU_1US));
	}
}

static uint32_t dpCmdGetStatus()
{
	uint32_t reg, ret;
	ret = cdevFileRegRd(REGR_STATUS, &reg);
	if(ret == 0)
		return reg;
	else
		printf("cdevFileRegRd returned -1\n");
}


int
sub_getopt (int argc, char **argv) {
    int c;
    //int digit_optind = 0;
    int ret, frames_int;
    uint32_t reg;

    while (1) {
        //int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"run", 1, 0, 'r'}, //postponed
            {"frames", 1, 0, 'f'},
			{"gtu1us", 1, 0, 'g'},
            {"help", 0, 0, 'h'},
            {"status", 0, 0, 's'},
           // {"write", 1, 0, 0},
	       // {"read", 1, 0, 0},
	       // {"value", 1, 0, '?'},

            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "r:f:g:sh",
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

        case 'r':
            //printf ("Parameter run (r) `%s'\n", optarg);
            if(optarg[0] == '1')
            {
            	cmd_run = 1;
            }
            else if (optarg[0] == '0')
            {
            	cmd_run = 0;
            }
            else
            {
            	cmd_run = -1;
            	printf ("Unsupported value: `%s'\n", optarg);
            }
            break;

        case 'f':
            //printf ("Parameter frames (f): `%s'\n", optarg);
           // printf("optarg = %s\n", optarg);
            ret = sscanf(optarg, "%d ",  &frames_int);
            //printf("sprintf returned %d\n", ret);
            //printf("frames_int = %d\n", frames_int);
            if(frames_int > 0)
            {
            	dpCmdSetFrames(frames_int);
            	dpCmdSetInfinite(0);
            }
            else if(frames_int == 0)
            {
            	dpCmdSetInfinite(1);
            }
            break;

        case 'g':
           // printf ("Parameter gtu1us (g) `%s'\n", optarg);
            dpCmdSetGtu1us(optarg[0]-'0');
            break;

        case 's':
            //printf ("Parameter gtu1us (g) `%s'\n", optarg);
        	reg = dpCmdGetStatus();
        	printf("status = 0x%08X\n", reg);
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
    printf("\t dataprov-uapp - data provider management utility\n");
    printf("\n");

    printf("SYNOPSYS\n");
    printf("\t dataprov-uapp [OPTIONS] ...\n");
    printf("\n");

    printf("DESCRIPTION\n");
    printf("\t Controls data flow. \n");
    printf("\n");

    printf("\t -r, --run=[0|1]\n");
    printf("\t\t start (1) or stop(0) data flow.\n");
    printf("\n");

    printf("\t -f, --frames=[0..4294967295]\n");
    printf("\t\t specify number of frames. 0 - generate until stop command.\n");
    printf("\t\t If number of frames is not specified, previous value will be taken.\n");

    printf("\n");

    printf("\t -g, --gtu1us=[0|1]\n");
    printf("\t\t set GTU period. 0 - 2.5 us. 1 - 1 us\n");
    printf("\t\t Important! GTU period can be changed only before first start.\n");
    printf("\n");

    printf("\t -s, --status\n");
    printf("\t\t get status.  1 - run, 0 - stopped.\n");
    printf("\n");

    printf("\t -h, --help\n");
    printf("\t\t print this page.\n");
    printf("\n");

    printf("EXAMPLES\n");
    printf("\t Start data provider:\n");
    printf("\t\t dataprov-uapp -r 1\n\n");
    printf("\t Start data provider for 163840 frames:\n");
    printf("\t\t dataprov-uapp -r 1 -f 163840\n\n");
    printf("\t Stop data provider:\n");
    printf("\t\t dataprov-uapp -r 0\n\n");
    printf("\n");

    printf("AUTHOR\n");
    printf("\t Alexander Belov. SINP MSU\n");
    printf("\n");

}
