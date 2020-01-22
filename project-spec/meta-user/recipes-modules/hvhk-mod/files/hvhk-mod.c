/*================================ ZYNQBOARD ==================================
*	PROJECT:	ZYNQ3 v1:	 "ZynqBoard software (Xilinx Zynq-7000, Linux) "
*	FILE:		hvhk-mod.c
*	CONTENTS:	Kernel module. HVHK IP Core driver.
*				Provides control and monitoring for HVHK IP.
*	VERSION:	01.01  10.09.2019
*	AUTHOR:		Andrey Poroshin
*	UPDATES :
*	1) 01.01   10 September 2019 - Initial version
 ============================================================================== */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/delay.h>

// Standard module information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Poroshin Andrey");
MODULE_DESCRIPTION("hvhk-mod - control and monitoring for HVHK IP Core");

/******************************************************************************
*	Internal definitions
*******************************************************************************/
// This module name
#define DRIVER_NAME	"hvhk-mod"

// Created class name
#define CLASS_NAME	"hvhk-cls"

// Created sevice thread name
#define THR_SRV_NAME "hvhk-srv-thr"

// Delay times (ms)
#define DELAY_1MS			1
#define DELAY_5MS			5
#define DELAY_10MS			10
#define DELAY_100MS			100

// Number of hvhk channels
#define HV_NUM				9

// Hvhk channels bit mask
#define HV_NUM_BITMASK		0x1FF

// Number of hvhk interrupts: two interrupts for each channel
// One for Status pin, another for ON/OFF pin
#define HV_INT_NUM			(HV_NUM * 2)

// Hvhk interrupts bit mask
#define HV_INT_NUM_BITMASK	0x3FFFF

// Command code to set dac outputs
#define HV_DAC_CMD_SET_OUT	1

// Number of hvhk dac chips
#define HV_DAC_CHIP_NUM		3		

// Max value to set for dac
#define HV_DAC_VAL_MAX		4095

// HVHK transaction: number of attempts
// 	to check that transaction is completed
#define HV_TRAN_ATT_NUM		10

// HVHK transaction: delay between attempts
//	to check that transaction is completed (ms)
#define HV_TRAN_ATT_DELAY	DELAY_1MS

// HVHK Registers
#define REGW_CMD				0	// RW: command register
#define REGW_STATUS				1	// RO: status of the transfer
#define REGW_CONFIG				2	// RW: configuration register
#define REGW_DATAIN1			4	// RW: register for sending data to Expander or DAC
#define REGW_DATAOUT			5	// RO: register for receiving data from Expander
#define REGW_DATAIN2			6	// RW: register for sending data to DAC	(chip 2)
#define REGW_DATAIN3			7	// RW: register for sending data to DAC	(chip 3)

// HVHK command register bits
#define REGW_CMD_BIT_START		0	// Setting the bit to 1 for a shot time initiates transmission

// HWHK status register bits
#define REGW_STATUS_BIT_COMPL	0	// 1: transfer is completed, 0 - not completed

// HVHK configuration register bits
#define	REGW_CONFIG_BIT_DEST	0	// 1: transmit to expander, 0 - transmit to dac

// HVHK Expander registers
#define REGE_IODIR			0x00	// RW: GPIO direction register (1-input, 0-output)
#define REGE_IPOL			0x01	// RW: GPIO-I polarity for each pin (0 - normal, 1- inverted)
#define REGE_GPINTEN		0x02	// RW: GPIO interrupt enable for each pin (1-enabled)
#define REGE_DEFVAL			0x03	// RW: Default comparison value for interrupt generation
#define REGE_INTCON			0x04	// RW: GPIO interrupt control for each pin (1-pin vs.defval,0-pin change)
#define REGE_IOCON			0x05	// RW: I/O expander configuration
#define REGE_GPPU			0x06	// RW: GPIO pull-up resistor register for each pin (1-enable)
#define REGE_INTF			0x07	// RO: Interrupt flag register (1-pin caused interrupt, 0-no interrupt)
#define REGE_INTCAP			0x08	// RO: Interrupt capture register (GPIO data values at the interrupt)
#define REGE_GPIO			0x09	// RW: GPIO data values for each pin
#define REGE_OLAT			0x0A	// RW: Output latch register

// Expander configuration register bits
#define REGE_IOCON_BIT_INTPOL	1	// Polarity of the INT output pin (1 - active-high, 0 - Active-low)
#define REGE_IOCON_BIT_ODR		2	// INT pin as an open-drain output (1)
#define REGE_IOCON_BIT_HAEN		3	// Hardware Address enabled (1), disabled (0)
#define REGE_IOCON_BIT_DISSLW	4	// Slew rate disabled (1), enabled (0)
#define REGE_IOCON_BIT_SREAD	5	// (SEQOP) sequential operation disabled (1), enabled (0)

// Expander ON/OFF pins mask (for each expander)  00010101
#define EXP_PINS_ONOFF_MSK		0x15

// Expander used pins mask (for each expander)
#define EXP_PINS_USED_MSK		0x3F

// Expander used pins number (for each expander)
#define EXP_PINS_USED_NUM		6

// Number of attempts to turn the channel on
#define CHAN_TURNON_ATT_NUM		10

// Channel software timer expiration value (*10ms)
// (Timer to count time while Status or ON/OFF line is in LOW state)
#define CHAN_N_TRIES_RELEASE_MAX	100		// 1 second

// Channel max allowed number of interrupts
// (if too many interrupt requests were received from Status or ON/OFF, channel is turned off)
#define CHAN_MAX_INTERRUPTS		1000

// Sysfs file: transmitted message max length (b)
#define SYSFS_MSGTR_LEN_MAX	10

/******************************************************************************
*	Internal structures
*******************************************************************************/
// Module parameters structure
typedef struct MODULE_PARM_s {
	uint8_t plat_drv_registered;	// Flag: platform driver was registered (1)
	uint8_t dev_found;				// Flag: device (HVHK IP Core) was found (1)
	struct class *pclass;			// Pointer to the created class
} MODULE_PARM_t;

// HVHK parameters structure
typedef struct HV_PARM_s {
	uint8_t	flcr_cmddacsnd;			// Flag: file "commands to send to DAC" was created (1)
	uint8_t flcr_dacval[HV_NUM];	// Flag: file "dac value" for HV channel was created (1)
	uint8_t flcr_cmdchanon;			// Flag: file "command channel on" was created (1)
	uint8_t flcr_cmdchanoff;		// Flag: file "command channel off" was created (1)
	uint8_t flcr_chanstatus;		// Flag: file "channel status" was created (1)
	uint8_t io_base_mapped;			// Flag: base address mapped to the device (1)
	uint8_t io_mem_allocated;		// Flag: device IO memory allocated (1)
	uint8_t irq_allocated;			// Flag: device IRQ allocated (1)
	uint8_t hvmutex_initialized;	// Flag: hvhk access mutex initialized (1)
	uint8_t thr_srv_started;		// Flag: service thread started (1)
	unsigned long mem_start;		// IO memory start address
	unsigned long mem_end;			// IO memory end address
	uint32_t irq_num;				// IRQ number
	uint32_t __iomem *base_addr;	// Device base address
	uint32_t dac_values[HV_NUM];	// Digital value for each HV channel dac
	struct mutex hvmutex;			// HVHK access mutex
	struct task_struct *thr_srv;	// Service thread handle 
} HV_PARM_t;

// HVHK DAC channel identifiers
typedef enum HV_DAC_CHANNEL_e {
	HV_DAC_CHAN1,					// Channel 1
	HV_DAC_CHAN2,					// Channel 2
	HV_DAC_CHAN3					// Channel 3
} HV_DAC_CHANNEL_t;
#define HV_DAC_CHAN_NUM		(HV_DAC_CHAN3 + 1)		// Nmber of channels (for one chip)

// HVHK command to send to DAC
typedef union HV_DAC_CMD_u {
	struct {
		uint32_t reserved:	4;		// Not used
		uint32_t value:		12;		// Dac value to set
		uint32_t chan_msk:	3;		// Mask of the channel to use
		uint32_t reserved1:	1;		// Not used
		uint32_t chan_mark:	4;		// Channel mark
		uint32_t reserved2:	8;		// Not used
	} fields;
	uint32_t w;
} __attribute__((__packed__)) HV_DAC_CMD_t;
#define HV_DAC_CMD_CHAN_MARK	0x03	// Channel mark (must be written to the correspondent field)

// HVHK expander transaction parameters
typedef union HV_EXP_PAR_u {
	struct {
		uint32_t reg_data:	8;		// Data to be written to 8-bit expander register
		uint32_t reg_addr:	8;		// Register number inside expander
		uint32_t opcode:	8;		// Address of the expander (bit 0 is read/write bit)
		uint32_t reserved:	8;		// Not used
	} fields;
	uint32_t w;
} __attribute__((__packed__)) HV_EXP_PAR_t;
#define HV_EXP_OPCODE_RW_MSK	1	// Mask of RW bit in the expander address

// HVHK expander identifiers
typedef enum HV_EXP_e {
	HV_EXP1,						// Expander 0
	HV_EXP2,						// Expander 1
	HV_EXP3							// Expander 2
} HV_EXP_t;
#define HV_EXP_NUM	(HV_EXP3 + 1)	// Total number of expanders

// HVHK Channel control parameters (for all channels)
typedef struct HV_CHAN_CTRL_PAR_s {
	// Bit mask. Represents for each HV channel is it switched on by user or not.
	// LSB bit represents channel0, etc...
	uint32_t turned_on_user;

	// Bit mask. Represents for each HV channel is it successful.
	// The bit is set if both ON/OFF and Status were set to HIGH during the last channel turn on.
	// The bit is is cleared if:
	//		1. HVPS channel is turned off by user.
	//		2. Both ON/OFF and Status were not gone back to HIGH state after interrupt event
	//		3. HVPS channel has produced more than specified number of interrupts
	uint32_t working_successful;
	
	// Interrupt counters for each HV channel (Status pin interrupt, ON/OFF pin interrupt)
	// The values in this array are cleared if corresponding HV channels are turned off by user
	uint32_t n_interrupts[HV_INT_NUM];

	// Software timers to count time while Status or ON/OFF line is in LOW state
	// If the counter reaches threshold, corresponding channel is turned off
	uint32_t n_tries_to_release[HV_INT_NUM];

	// Bit mask. Represents pending interrupts for Status and ON/OFF line for each HV channel
	// The interrupt in pending when the correspondent line is in LOW state
	uint32_t interrupt_pending;
} HV_CHAN_CTRL_PAR_t;

/******************************************************************************
*	Internal functions
*******************************************************************************/
static int __init moduleInit(void);
static void __init moduleInitParm(void);
static int __init moduleCrCls(void);
static int __init moduleReg(void);
static void __exit moduleExit(void);
static void moduleFreeAll(void);
static void moduleUnreg(void);
static void moduleDestrCls(void);
static int hvProbe(struct platform_device *pdev);
static int hvProbeDevFound(void);
static void hvInitParm(void);
static int hvRemove(struct platform_device *pdev);
static int hvFilesCreate(void);
static void hvFilesRemove(void);
static int hvFlCmdDacSndCr(void);
static void hvFlCmdDacSndRm(void);
static ssize_t hvFlCmdDacSndSh(struct class *class, struct class_attribute *attr, char *buf);
static ssize_t hvFlCmdDacSndSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
static int hvFlDacValCr(uint32_t hv_idx);
static int hvFlDacValCrAll(void);
static void hvFlDacValRm(uint32_t hv_idx);
static void hvFlDacValRmAll(void);
static ssize_t hvFlDacValSh(struct class *class, struct class_attribute *attr,char *buf);
static ssize_t hvFlDacValSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
static int hvFlDacValNamePrc(struct class_attribute *attr, uint32_t *hv_idx);
static int hvFlCmdChanOnCr(void);
static void hvFlCmdChanOnRm(void);
static ssize_t hvFlCmdChanOnSh(struct class *class, struct class_attribute *attr,char *buf);
static ssize_t hvFlCmdChanOnSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
static int hvFlCmdChanOffCr(void);
static void hvFlCmdChanOffRm(void);
static ssize_t hvFlCmdChanOffSh(struct class *class, struct class_attribute *attr,char *buf);
static ssize_t hvFlCmdChanOffSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
static int hvFlChanStatusCr(void);
static void hvFlChanStatusRm(void);
static ssize_t hvFlChanStatusSh(struct class *class, struct class_attribute *attr,char *buf);
static ssize_t hvFlChanStatusSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
static ssize_t hvFlZeroSh(char *buf);
static int hvPlatInit(struct platform_device *pdev);
static int hvPlatInitGetIORes(struct platform_device *pdev);
static int hvPlatInitGetIOMem(struct platform_device *pdev);
static int hvPlatInitGetIOIrq(struct platform_device *pdev);
static int hvPlatInitAllocIO(struct device *dev);
static int hvPlatInitAllocMem(struct device *dev);
static int hvPlatInitAllocBase(struct device *dev);
static void hvPlatInitMutex(void);
static int hvPlatInitAllocIrq(struct device *dev);
static int hvPlatInitThrSrv(struct device *dev);
static int hvPlatThrSrv(void *data);
static irqreturn_t hvPlatIrqHndlTh(int irq_num, void *parm);
static irqreturn_t hvPlatIrqHndlBh(int irq_num, void *parm);
static uint32_t hvPlatRegRd(uint32_t regw);
static void hvPlatRegWr(uint32_t val, uint32_t regw);
static int hvPlatTran(void);
static void hvPlatFreeAll(void);
static void hvPlatFreeStopThrSrv(void);
static void hvPlatFreeIrq(void);
static void hvPlatFreeMutex(void);
static void hvPlatFreeBaseUnmap(void);
static void hvPlatFreeReleaseMem(void);
static void hvFreeAll(void);
static int hvDacExecCmd(uint32_t cmd1, uint32_t cmd2, uint32_t cmd3);
static uint32_t hvDacCreateCmd(uint32_t chip_idx, uint32_t chan_idx);
static void hvDacSetValues(void);
static int hvExpInit(void);
static void hvExpInitDisAddr(void);
static void hvExpInitRegs(void);
static void hvExpInitEnAddr(void);
static int hvExpInitChk(void);
static int hvExpSetReg(uint32_t opcode, uint32_t reg_addr, uint8_t reg_data);
static int hvExpGetReg(uint32_t opcode, uint32_t reg_addr, uint8_t *reg_data);
static uint32_t hvExpSetRegParCr(uint32_t opcode, uint32_t reg_addr, uint8_t reg_data);
static uint32_t hvExpGetRegParCr(uint32_t opcode, uint32_t reg_addr);
static int hvExpTran(uint32_t par);
static void hvExpEnInt(uint32_t opcode, uint8_t msk_en, uint8_t msk_dis);
static void hvMutexLock(void);
static void hvMutexUnlock(void);
static void hvChanService(void);
static void hvChanSrvLowTmrs(void);
static void hvChanSrvIntCnt(void);
static void hvChanSrvReEnInt(void);
static void hvChanIntHndl(void);
static void hvChanIntHndlExp(uint32_t exp_id);
static void hvChanListOff(uint32_t msk);
static void hvChanListOn(uint32_t msk);
static void hvChanOff(uint8_t khv);
static void hvChanParUOff(uint8_t khv);
static void hvChanParUOn(uint8_t khv);
static void hvChanParAOff(uint8_t khv);
static void hvChanParClrInt(uint8_t khv);
static void hvChanParClrCntTmr(uint8_t khv);
static void hvChanUOff(uint8_t khv);
static void hvChanAOff(uint8_t khv);
static void hvChanAOffByIdx(uint32_t int_idx);
static int hvChanOn(uint8_t khv);
static void hvChanUOn(uint8_t khv);
static void hvChanDisInt(uint8_t khv);
static void hvChanEnInt(uint8_t khv, uint8_t st_flg, uint8_t oo_flg);
static void hvChanClrPendInt(uint8_t khv, uint8_t st_flg, uint8_t oo_flg);
static void hvChanSetInt(uint8_t khv);
static void hvChanOOClrOut(uint8_t khv);
static void hvChanOOSetOut(uint8_t khv);
static void hvChanOOClr(uint8_t khv);
static void hvChanOOSet(uint8_t khv);
static uint8_t hvChanOOGet(uint8_t khv);
static void hvChanOOOut(uint8_t khv);
static void hvChanOOIn(uint8_t khv);
static uint8_t hvChanBitMaskSt(uint8_t cwin_exp);
static uint8_t hvChanBitMaskOO(uint8_t cwin_exp);
static uint32_t hvChanArrIdxSt(uint8_t khv);
static uint32_t hvChanArrIdxOO(uint8_t khv);
static uint8_t hvChanArrIdxToKhv(uint32_t idx);
static uint32_t hvChanIntMskOOSt(uint8_t khv);
static void hvChanGetPins(uint8_t khv, uint8_t *st_val, uint8_t *oo_val);
static uint32_t hvChanGetPinsAll(void);
static void hvChanGetPar(uint8_t khv, uint8_t *cwin_exp, uint8_t *exp_addr);

/******************************************************************************
*	Internal data
*******************************************************************************/
// Module parameters
static MODULE_PARM_t module_parm;

// HVHK parameters (for HVHK IP core)
static HV_PARM_t hv_parm;

// Show and store functions for the file: "commands to send to DAC"
#define cmddacsnd_show	hvFlCmdDacSndSh		// Show function
#define cmddacsnd_store hvFlCmdDacSndSt		// Store function

// Module class attribute: file "commands to send to DAC"
CLASS_ATTR_RW(cmddacsnd);

// Show functions for the files: "dac value"
#define dacval0_show	hvFlDacValSh
#define dacval1_show	hvFlDacValSh
#define dacval2_show	hvFlDacValSh
#define dacval3_show	hvFlDacValSh
#define dacval4_show	hvFlDacValSh
#define dacval5_show	hvFlDacValSh
#define dacval6_show	hvFlDacValSh
#define dacval7_show	hvFlDacValSh
#define dacval8_show	hvFlDacValSh

// Store functions for the files: "dac value"
#define dacval0_store	hvFlDacValSt
#define dacval1_store	hvFlDacValSt
#define dacval2_store	hvFlDacValSt
#define dacval3_store	hvFlDacValSt
#define dacval4_store	hvFlDacValSt
#define dacval5_store	hvFlDacValSt
#define dacval6_store	hvFlDacValSt
#define dacval7_store	hvFlDacValSt
#define dacval8_store	hvFlDacValSt

// Module class attribute: files "dac value" - for each dac
CLASS_ATTR_RW(dacval0);
CLASS_ATTR_RW(dacval1);
CLASS_ATTR_RW(dacval2);
CLASS_ATTR_RW(dacval3);
CLASS_ATTR_RW(dacval4);
CLASS_ATTR_RW(dacval5);
CLASS_ATTR_RW(dacval6);
CLASS_ATTR_RW(dacval7);
CLASS_ATTR_RW(dacval8);

// Array with pointers to "dac value" file attributes for each HV channel
static struct class_attribute	*dacval_file_attr[HV_NUM] = {
	&class_attr_dacval0,				// Attributes for the file "dac 0 value"
	&class_attr_dacval1,				// Attributes for the file "dac 1 value"
	&class_attr_dacval2,				// Attributes for the file "dac 2 value"
	&class_attr_dacval3,				// Attributes for the file "dac 3 value"
	&class_attr_dacval4,				// Attributes for the file "dac 4 value"
	&class_attr_dacval5,				// Attributes for the file "dac 5 value"
	&class_attr_dacval6,				// Attributes for the file "dac 6 value"
	&class_attr_dacval7,				// Attributes for the file "dac 7 value"
	&class_attr_dacval8,				// Attributes for the file "dac 8 value"
};

// Show and store functions for the file: "command channel on"
#define cmdchanon_show		hvFlCmdChanOnSh		// Show function
#define cmdchanon_store		hvFlCmdChanOnSt		// Store function

// Module class attribute: file "command channel on"
CLASS_ATTR_RW(cmdchanon);

// Show and store functions for the file:  "command channel off"
#define cmdchanoff_show		hvFlCmdChanOffSh	// Show function
#define cmdchanoff_store	hvFlCmdChanOffSt	// Store function

// Module class attribute: file "command channel off"
CLASS_ATTR_RW(cmdchanoff);

// Show and store functions for the file: "channel status"
#define chanstatus_show		hvFlChanStatusSh	// Show function
#define chanstatus_store	hvFlChanStatusSt	// Store function

// Module class attribute: file "channel status"
CLASS_ATTR_RW(chanstatus);

// List of platform driver compatible devices
static struct of_device_id plat_of_match[] = {
	{ .compatible = "xlnx,hv-hk-v1-0-1.0", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, plat_of_match);		// Make the list global for the kernel

// HVHK platform driver structure
static struct platform_driver plat_drv = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= plat_of_match,
	},
	.probe  = hvProbe,
	.remove = hvRemove,
};

// HVHK expander addresses
static uint32_t hv_exp_addr[HV_EXP_NUM] = {
	0x40,				// Index: HV_EXP1
	0x42,				// Index: HV_EXP2
	0x44,				// Index: HV_EXP3
};

// HVHK Channel control parameters (for all channels)
static volatile HV_CHAN_CTRL_PAR_t hv_chan_ctrl_par;

/******************************** moduleInit() ********************************
* Module initialization function
* It is called when the module is inserted into the Linux kernel
* Return value:
*	0  - Initialization done
*	<0 - Error code
*******************************************************************************/
static int __init moduleInit(void)
{
	int rc;

	printk(KERN_INFO "Poroshin: init %s \n", DRIVER_NAME);

	// Init local module parameters
	moduleInitParm();

	// Register class for the module
	rc = moduleCrCls();
	if(rc != 0) return rc;

	// Register platform driver for HVHK IP core
	rc = moduleReg();
	if(rc != 0)
		// Free all resurces associated with this module
		moduleFreeAll();

	// Return module initialization success/error code
	return rc;
}

/****************************** moduleInitParm() ******************************
* Module initialization: init local module parameters
* (This function must be called before all module initializations)
* Used variable:
*	(o)module_parm - module parameters
*******************************************************************************/
static void __init moduleInitParm(void)
{
	// Set initial values for the module parameters
	module_parm.plat_drv_registered = 0;
	module_parm.dev_found = 0;
	module_parm.pclass = NULL;
}

/******************************* moduleCrCls() ********************************
* Register class for the module
* The function is called when the module is initialized
* Used variable:
*	(o)module_parm - module parameters
* Return value:
*	0  - Success. The class was registered
*	<0 - Error code. Failed to register the class
*******************************************************************************/
static int __init moduleCrCls(void)
{
	struct class *pclass_tmp;

	// Register the class
	pclass_tmp = class_create(THIS_MODULE, CLASS_NAME);
	if(IS_ERR(pclass_tmp)){
		// Failed to register class
		printk(KERN_INFO "hvhk-mod: failed to register class \n");
	
		// Return error code
		return PTR_ERR(pclass_tmp);
	}
	
	// The class was created successfully
	// Set the pointer to the created class
	module_parm.pclass = pclass_tmp;

	// The class was registered successfully
	return 0;
}

/******************************** moduleReg() *********************************
* Register platform driver for HVHK IP core
* Used variable:
*	(i)plat_drv - HVHK platform driver structure
*	(o)module_parm - module parameters
* Return value:
*	0  - Platform driver was registered successfully
*	<0 - Error code
*******************************************************************************/
static int __init moduleReg(void)
{
	int rc;

	// Register platform driver
	rc = platform_driver_register(&plat_drv);
	if(rc != 0)	return rc;					// Can not register platform driver

	// Platform driver was registered successfully
	// Set correspondent flag in the parameters structure
	module_parm.plat_drv_registered = 1;

	return 0;
}

/******************************** moduleExit() ********************************
* Module exit function
* It is called when the module is removed from kernel.
*******************************************************************************/
static void __exit moduleExit(void)
{
	// Free all resurces associated with this module
	moduleFreeAll();

	printk(KERN_INFO "Poroshin: exit %s \n", DRIVER_NAME);
}

/****************************** moduleFreeAll() *******************************
* Free all resurces associated with this module
* The function is called from module exit function
* It is also called from module initialization function in case of errors
* Used variable:
*	(i)module_parm - module parameters
*******************************************************************************/
static void moduleFreeAll(void)
{
	// Unregister platform driver for HVHK IP core
	moduleUnreg();

	// Destroy registered class
	moduleDestrCls();
}

/******************************* moduleUnreg() ********************************
* Unregister platform driver for HVHK IP core
* The driver is unregistered only if it was registered previously
* Used variables:
*	(i)plat_drv - HVHK platform driver structure
*	(i)module_parm - module parameters
*******************************************************************************/
static void moduleUnreg(void)
{
	uint8_t registered;

	// Read the flag: platform driver was registered (1)
	registered = module_parm.plat_drv_registered;

	// Unregister platform driver only if it was registered
	if(registered)
		platform_driver_unregister(&plat_drv);
}

/****************************** moduleDestrCls() ******************************
* Destroy registered class
* The class is destroyed only if it was registered (created) previously
* Used variables:
*	(io)module_parm - module parameters
*******************************************************************************/
static void moduleDestrCls(void)
{
	struct class *pclass_tmp;

	// Read pointer to the created class
	pclass_tmp = module_parm.pclass;

	// Destroy the class if it was created
	if(pclass_tmp != NULL)
		class_destroy(pclass_tmp);

	// Clear the pointer in the module parameters structure
	module_parm.pclass = NULL;
}

/******************************* hvProbe(pdev) ********************************
* HVHK device probe function.
* The function is called when compatible with this driver platform device
*	(HVHK IP) was found
* Only one HVHK IP Core is supported
* Creates all needed files to control the device
* Initializes platform device (hvhk)
* Parameter:
*	(i)pdev - structure of the platform device to initialize
* Return value:
*	0  - Device was initialized successfully
*	<0 - Error code
*******************************************************************************/
static int hvProbe(struct platform_device *pdev)
{
	int rc;

	printk(KERN_INFO "Poroshin: hvProbe START \n");

	// Check that only one HVHK IP Core was found
	rc = hvProbeDevFound();
	if(rc != 0) return -1;			// Error: Only one HVHK IP Core is supported

	// Init local HVHK parameters
	hvInitParm();

	// Initialize HVHK platform device
	rc = hvPlatInit(pdev);
	if(rc != 0)	goto HV_PROBE_FAILED;

	// Create all needed files for user I/O in the /sys file subsystem
	rc = hvFilesCreate();
	if(rc != 0)	goto HV_PROBE_FAILED;

	// Device was initialized successfully
	return 0;

HV_PROBE_FAILED:
	// Free all resurces associated with HVHK
	hvFreeAll();

	// Return error code
	return rc;
}

/***************************** hvProbeDevFound() ******************************
* HVHK initialization: check that only one HVHK IP Core was found
* Used variable:
*	(io)module_parm - module parameters
* Return value:
*	0  - Success. First HVHK IP Core was found
*	-1 - Error. Only one HVHK IP Core is supported. 
*******************************************************************************/
static int hvProbeDevFound(void)
{
	uint32_t dev_found;

	// Read the flag: device (HVHK IP Core) was found (1), not found (0)
	dev_found = module_parm.dev_found;

	// Check if the device is not the first one
	if(dev_found) return -1;	// Error. Only one HVHK IP Core is supported.

	// Set the flag: device (HVHK IP Core) was found
	module_parm.dev_found = 1;

	// Success. First HVHK IP Core was found.
	return 0;
}

/******************************** hvInitParm() ********************************
* HVHK initialization: init local HVHK parameters
* (This function must be called before all HVHK initializations)
* Used variable:
*	(o)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvInitParm(void)
{
	uint32_t i;
	uint8_t *flcr_dacval;

	// Clear all flags in the parameters structure
	hv_parm.flcr_cmddacsnd = 0;
	hv_parm.flcr_cmdchanon = 0;
	hv_parm.flcr_cmdchanoff = 0;
	hv_parm.flcr_chanstatus = 0;
	hv_parm.io_base_mapped = 0;
	hv_parm.io_mem_allocated = 0;
	hv_parm.irq_allocated = 0;
	hv_parm.hvmutex_initialized = 0;
	hv_parm.thr_srv_started = 0;

	// Set the pointer to the array of "dac value file created" flags
	flcr_dacval = hv_parm.flcr_dacval;

	// Clear all "dac value file created" flags
	for(i = 0; i < HV_NUM; i++)
		flcr_dacval[i] = 0;		
}

/******************************* hvRemove(pdev) *******************************
* Platform device - HVHK remove function.
* The function is called when compatible platform device (HVHK IP)
*	was removed (or the driver module was removed from kernel)
* Parameter:
*	(i)pdev - structure of the platform device to remove
* Return value:
*	0  - The device was removed successfully
*******************************************************************************/
static int hvRemove(struct platform_device *pdev)
{
	printk(KERN_INFO "Poroshin: hvRemove EXECUTED \n");

	// Free all resurces associated with HVHK
	hvFreeAll();

	// The device was removed successfully
	return 0; 
}

/****************************** hvFilesCreate() *******************************
* Create all needed files for user I/O in the /sys file subsystem
* Used variables:
*	(i)module_parm - module parameters
*	(o)hv_parm - HVHK parameters (for HVHK IP core)
* Return value:
*	0  - All files were created successfully
*	<0 - Error. Can not create all needed files
*******************************************************************************/
static int hvFilesCreate(void)
{
	int rc;

	// Create file: commands to transmit to the hvhk DAC
	rc = hvFlCmdDacSndCr();
	if(rc != 0) return rc;

	// Create all "dac value" files
	rc = hvFlDacValCrAll();
	if(rc != 0) return rc;

	// Create file: command "channel on" for hvhk channel
	rc = hvFlCmdChanOnCr();
	if(rc != 0) return rc;

	// Create file: command "channel off" for hvhk channel
	rc = hvFlCmdChanOffCr();
	if(rc != 0) return rc;
 
	// Create file "channel status" for hvhk channel
	return hvFlChanStatusCr();
}

/****************************** hvFilesRemove() *******************************
* Remove all user I/O files in the /sys file subsystem
* Each file is removed only if it was created previously
* Used variables:
*	(i)module_parm - module parameters
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvFilesRemove(void)
{
	// Remove file: "channel status" for hvhk channel
	hvFlChanStatusRm();

	// Remove file: command "channel off" for hvhk channel
	hvFlCmdChanOffRm();

	// Remove file: command "channel on" for hvhk channel
	hvFlCmdChanOnRm();

	// Remove all "dac value" files
	hvFlDacValRmAll();

	// Remove file: commands to transmit to the hvhk DAC
	hvFlCmdDacSndRm();
}

/***************************** hvFlCmdDacSndCr() ******************************
* Create file for user I/O in the /sys file subsystem
* File: commands to transmit to the hvhk DAC.
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_cmddacsnd - attributes of the created file
*	(o)hv_parm - HVHK parameters (for HVHK IP core)
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int hvFlCmdDacSndCr(void)
{
	int rc;
	struct class *pclass;

	// Read the pointer to the module class
	pclass = module_parm.pclass;

	// Create file
	rc = class_create_file(pclass, &class_attr_cmddacsnd);
	if(rc != 0) return rc;					// File was not created

	// The file was created successfully
	// Set the flag: file "commands to send to DAC" was created
	hv_parm.flcr_cmddacsnd = 1;

	return 0;
}

/***************************** hvFlCmdDacSndRm() ******************************
* Remove user I/O file in the /sys file subsystem
* File: commands to transmit to the hvhk DAC.
* The file is removed only if it was created previously.
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_cmddacsnd - attributes of the removed file
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvFlCmdDacSndRm(void)
{
	uint8_t *flcr_cmddacsnd;
	struct class *pclass;

	// Set the pointer to the flag: file "commands to send to DAC" was created
	flcr_cmddacsnd = &hv_parm.flcr_cmddacsnd;

	// The file is removed only if it was created previously
	if(*flcr_cmddacsnd){
		// Read the pointer to the module class
		pclass = module_parm.pclass;

		// Remove the file
		class_remove_file(pclass, &class_attr_cmddacsnd);

		// Clear the flag that indicates that the file was created
		*flcr_cmddacsnd = 0;
	}
}

/********************** hvFlCmdDacSndSh(class,attr,buf) ***********************
* Show function for the I/O file in the /sys file subsystem
* File: commands to transmit to the hvhk DAC.
* Parameters: 
*	(i)class - class of the file that was read by user
*	(i)attr - attributes of the file that was read by user
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t hvFlCmdDacSndSh(struct class *class, struct class_attribute *attr,char *buf)
{
	// Transmit message with zero value to user
	return hvFlZeroSh(buf);
}

/******************* hvFlCmdDacSndSt(class,attr,buf,count) ********************
* Store function for the I/O file in the /sys file subsystem
* File: commands to transmit to the hvhk DAC.
* Parameters: 
*	(i)class - class of the file that was written by user
*	(i)attr - attributes of the file that was written by user
*	(i)buf - buffer with user data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t hvFlCmdDacSndSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count)
{
	int rc;
	uint32_t received_val;

	// Lock hvhk access mutex
	hvMutexLock();

	// Read received value
	rc = sscanf(buf, "%d", &received_val);
	
	// Check received data
	if(rc == 1 && received_val == HV_DAC_CMD_SET_OUT) {
		// Valid command to set dac outputs was received
		// Set new values for all dac outputs
		hvDacSetValues();
	}

	// Unlock hvhk access mutex
	hvMutexUnlock();	

	// Return the number of bytes processed (all bytes were processed)
	return count;
}

/**************************** hvFlDacValCr(hv_idx) ****************************
* Create file for user I/O in the /sys file subsystem
* File: "dac value" for HV channel
* Used variables:
*	(i)module_parm - module parameters
*	(i)dacval_file_attr - array of "dac value" file attributes
*	(o)hv_parm - HVHK parameters (for HVHK IP core)
* Parameter:
*	(i)hv_idx - index of hvhk channel (not checked here)
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int hvFlDacValCr(uint32_t hv_idx)
{
	struct class *pclass;
	struct class_attribute	*file_attr;
	int rc;

	// Read the pointer to the module class
	pclass = module_parm.pclass;

	// Read the pointer to the file attributes for the current file
	file_attr = dacval_file_attr[hv_idx];

	// Create file
	rc = class_create_file(pclass, file_attr);
	if(rc != 0) return rc;				// File was not created

	// The file was created successfully
	// Set the flag: file "dac value" for the HV channel was created
	hv_parm.flcr_dacval[hv_idx] = 1;

	return 0;
}

/************************** hvFlDacValCrAll(hv_idx) ***************************
* Create files for user I/O in the /sys file subsystem
* All "dac value" files for HV channels are created here 
* Used variables:
*	(i)module_parm - module parameters
*	(i)dacval_file_attr - array of "dac value" file attributes
*	(o)hv_parm - HVHK parameters (for HVHK IP core)
* Return value:
*	0  - All files were created successfully 
*	<0 - Error. Can not create one or more files
*******************************************************************************/
static int hvFlDacValCrAll(void)
{
	uint32_t hv_idx;
	int rc;

	// Create files in a cycle
	for(hv_idx = 0; hv_idx < HV_NUM; hv_idx++){
		// Create one "dac value" file
		rc = hvFlDacValCr(hv_idx);

		// Stop file creation cycle in case of errors
		if(rc != 0) break;
	}

	// Return success/error code
	return rc;
}

/**************************** hvFlDacValRm(hv_idx) ****************************
* Remove user I/O file in the /sys file subsystem
* File: "dac value" for HV channel
* Used variables:	
*	(i)module_parm - module parameters
*	(i)dacval_file_attr - array of "dac value" file attributes
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
* Parameter:
*	(i)hv_idx - index of hvhk channel (not checked here)
*******************************************************************************/
static void hvFlDacValRm(uint32_t hv_idx)
{
	uint8_t *flcr_dacval;
	struct class *pclass;
	struct class_attribute	*file_attr;
	
	// Set the pointer to the flag: file "dac value" for HV channel was created
	flcr_dacval = &hv_parm.flcr_dacval[hv_idx];

	// The file is removed only if it was created previously
	if(*flcr_dacval) {
		// Read the pointer to the module class
		pclass = module_parm.pclass;

		// Read the pointer to the file attributes for the current file
		file_attr = dacval_file_attr[hv_idx];

		// Remove the file
		class_remove_file(pclass, file_attr);

		// Clear the flag that indicates that the file was created
		*flcr_dacval = 0;
	}
}

/***************************** hvFlDacValRmAll() ******************************
* Remove user I/O files in the /sys file subsystem
* All "dac value" files for HV channels are removed here
* Used variables:
*	(i)module_parm - module parameters
*	(i)dacval_file_attr - array of "dac value" file attributes
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvFlDacValRmAll(void)
{
	uint32_t hv_idx;

	// Remove files in a cycle
	for(hv_idx = 0; hv_idx < HV_NUM; hv_idx++){
		// Remove one "dac value" file
		hvFlDacValRm(hv_idx);
	}
}

/************************ hvFlDacValSh(class,attr,buf) ************************
* Show function for the I/O file in the /sys file subsystem
* File: one of the "dac value" files for HV channel
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
* Parameters: 
*	(i)class - class of the file that was read by user
*	(i)attr - attributes of the file that was read by user
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t hvFlDacValSh(struct class *class, struct class_attribute *attr,char *buf)
{
	ssize_t len;
	uint32_t hv_idx;
	uint32_t dacvalue;

	// Extract index of hv from the file name
	if(hvFlDacValNamePrc(attr, &hv_idx) < 0)
		return 0;			// File name is invalid, return nothing to user

	// Read the dac value for the corresponding dac channel
	dacvalue = hv_parm.dac_values[hv_idx];

	// Create message for the user
	len = snprintf(buf, SYSFS_MSGTR_LEN_MAX,"%.4d", dacvalue);
	buf[len] = 0;			// String must end with zero

	// Return the length of the message for the user
	// +1 for the zero at the end of a string
	return (len + 1);
}

/********************* hvFlDacValSt(class,attr,buf,count) *********************
* Store function for the I/O file in the /sys file subsystem
* File: one of the "dac value" files for HV channel
* Parameters: 
*	(i)class - class of the file that was written by user
*	(i)attr - attributes of the file that was written by user
*	(i)buf - buffer with user data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t hvFlDacValSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count)
{
	uint32_t hv_idx;
	int rc;
	uint32_t received_val;

	// Extract index of hv from the file name
	if(hvFlDacValNamePrc(attr, &hv_idx) < 0)
		return count;				// File name is invalid

	// Read received value
	rc = sscanf(buf, "%d", &received_val);

	// Check received data
	if(rc == 1 && received_val <= HV_DAC_VAL_MAX)
		// Received value is valid. Store it in the dac values array.
		hv_parm.dac_values[hv_idx] = received_val;

	// Return the number of bytes processed (all bytes were processed)
	return count;
}

/*********************** hvFlDacValNamePrc(attr,hv_idx) ***********************
* Process the name of the "dac value" file
* Extracts index of hv from the file name
* Parameters:
*	(i)attr - attributes of the file, contains file name
*	(o)hv_idx - index of the hv
* Return value:
*	-1 - Error. Can not read index of hv from the file name
*	0  - Success. The file name was processed, index of hv was extracted
*******************************************************************************/
static int hvFlDacValNamePrc(struct class_attribute *attr, uint32_t *hv_idx)
{
	uint32_t hv_idx_tmp;
	const char *fname;

	// Set the pointer to the file name
	fname = attr->attr.name;

	// Check that accessed file name is valid
	if(fname == NULL)	
		return -1;						// File name is invalid

	// Read corresponding index of hv 
	hv_idx_tmp = fname[6] - '0';

	// Check the index of hv
	if(hv_idx_tmp >= HV_NUM) 
		return -1;						// Hv index is invalid

	// Store index of the hv
	*hv_idx = hv_idx_tmp;

	// The file name was processed successfully
	return 0;
}

/***************************** hvFlCmdChanOnCr() ******************************
* Create file for user I/O in the /sys file subsystem
* File: Command "channel on" for hvhk channel
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_cmdchanon - attributes of the created file
*	(o)hv_parm - HVHK parameters (for HVHK IP core)
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int hvFlCmdChanOnCr(void)
{
	int rc;
	struct class *pclass;

	// Read the pointer to the module class
	pclass = module_parm.pclass;

	// Create file
	rc = class_create_file(pclass, &class_attr_cmdchanon);
	if(rc != 0) return rc;					// File was not created

	// The file was created successfully
	// Set the flag: file "command channel on" was created
	hv_parm.flcr_cmdchanon = 1;

	return 0;
}

/***************************** hvFlCmdChanOnRm() ******************************
* Remove user I/O file in the /sys file subsystem
* File: Command "channel on" for hvhk channel
* The file is removed only if it was created previously.
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_cmdchanon - attributes of the removed file
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvFlCmdChanOnRm(void)
{
	uint8_t *flcr_cmdchanon;
	struct class *pclass;

	// Set the pointer to the flag: file "command channel on" was created
	flcr_cmdchanon = &hv_parm.flcr_cmdchanon;

	// The file is removed only if it was created previously
	if(*flcr_cmdchanon){
		// Read the pointer to the module class
		pclass = module_parm.pclass;

		// Remove the file
		class_remove_file(pclass, &class_attr_cmdchanon);

		// Clear the flag that indicates that the file was created
		*flcr_cmdchanon = 0;
	}
}

/********************** hvFlCmdChanOnSh(class,attr,buf) ***********************
* Show function for the I/O file in the /sys file subsystem
* File: Command "channel on" for hvhk channel
* Parameters:
*	(i)class - class of the file that was read by user
*	(i)attr - attributes of the file that was read by user
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t hvFlCmdChanOnSh(struct class *class, struct class_attribute *attr,char *buf)
{
	// Transmit message with zero value to user
	return hvFlZeroSh(buf);
}

/******************* hvFlCmdChanOnSt(class,attr,buf,count) ********************
* Store function for the I/O file in the /sys file subsystem
* File: Command "channel on" for hvhk channel
* Parameters: 
*	(i)class - class of the file that was written by user
*	(i)attr - attributes of the file that was written by user
*	(i)buf - buffer with user data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t hvFlCmdChanOnSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count)
{
	int rc;
	uint32_t received_val;
	
	// Lock hvhk access mutex
	hvMutexLock();

	// Read received value
	rc = sscanf(buf, "%x", &received_val);

	// Check that the value was received
	if(rc == 1) {
		// Clear all unused bits in the received value
		received_val &= HV_NUM_BITMASK;

		// Turn on HVHK channels by list (bitmask)
		hvChanListOn(received_val);
	}

	// Unlock hvhk access mutex
	hvMutexUnlock();

	// Return the number of bytes processed (all bytes were processed)
	return count;
}

/***************************** hvFlCmdChanOffCr() *****************************
* Create file for user I/O in the /sys file subsystem
* File: Command "channel off" for hvhk channel
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_cmdchanoff - attributes of the created file
*	(o)hv_parm - HVHK parameters (for HVHK IP core)
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int hvFlCmdChanOffCr(void)
{
	int rc;
	struct class *pclass;

	// Read the pointer to the module class
	pclass = module_parm.pclass;

	// Create file
	rc = class_create_file(pclass, &class_attr_cmdchanoff);
	if(rc != 0) return rc;					// File was not created

	// The file was created successfully
	// Set the flag: file "command channel off" was created
	hv_parm.flcr_cmdchanoff = 1;

	return 0;
}

/***************************** hvFlCmdChanOffRm() *****************************
* Remove user I/O file in the /sys file subsystem
* File: Command "channel off" for hvhk channel
* The file is removed only if it was created previously.
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_cmdchanoff - attributes of the removed file
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvFlCmdChanOffRm(void)
{
	uint8_t *flcr_cmdchanoff;
	struct class *pclass;

	// Set the pointer to the flag: file "command channel off" was created
	flcr_cmdchanoff =  &hv_parm.flcr_cmdchanoff;

	// The file is removed only if it was created previously
	if(*flcr_cmdchanoff) {
		// Read the pointer to the module class
		pclass = module_parm.pclass;

		// Remove the file
		class_remove_file(pclass, &class_attr_cmdchanoff);

		// Clear the flag that indicates that the file was created
		*flcr_cmdchanoff = 0;
	}
}

/********************** hvFlCmdChanOffSh(class,attr,buf) **********************
* Show function for the I/O file in the /sys file subsystem
* File: Command "channel off" for hvhk channel
* Parameters:
*	(i)class - class of the file that was read by user
*	(i)attr - attributes of the file that was read by user
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t hvFlCmdChanOffSh(struct class *class, struct class_attribute *attr,char *buf)
{
	// Transmit message with zero value to user
	return hvFlZeroSh(buf);
}

/******************* hvFlCmdChanOffSt(class,attr,buf,count) *******************
* Store function for the I/O file in the /sys file subsystem
* File: Command "channel off" for hvhk channel
* Parameters: 
*	(i)class - class of the file that was written by user
*	(i)attr - attributes of the file that was written by user
*	(i)buf - buffer with user data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t hvFlCmdChanOffSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count)
{
	int rc;
	uint32_t received_val;

	// Lock hvhk access mutex
	hvMutexLock();	

	// Read received value
	rc = sscanf(buf, "%x", &received_val);

	// Check that the value was received
	if(rc == 1) {
		// Clear all unused bits in the received value
		received_val &= HV_NUM_BITMASK;

		// Turn off HVHK channels by list (bitmask)
		hvChanListOff(received_val);
	}

	// Unlock hvhk access mutex
	hvMutexUnlock();

	// Return the number of bytes processed (all bytes were processed)
	return count;
}

/***************************** hvFlChanStatusCr() *****************************
* Create file for user I/O in the /sys file subsystem
* File: "channel status" for hvhk channel
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_chanstatus - attributes of the created file
*	(o)hv_parm - HVHK parameters (for HVHK IP core)
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int hvFlChanStatusCr(void)
{
	int rc;
	struct class *pclass;

	// Read the pointer to the module class
	pclass = module_parm.pclass;

	// Create file
	rc = class_create_file(pclass, &class_attr_chanstatus);
	if(rc != 0) return rc;					// File was not created

	// The file was created successfully
	// Set the flag: file "channel status" was created
	hv_parm.flcr_chanstatus = 1;

	return 0;		
}

/***************************** hvFlChanStatusRm() *****************************
* Remove user I/O file in the /sys file subsystem
* File: "channel status" for hvhk channel
* The file is removed only if it was created previously.
* Used variables:
* 	(i)module_parm - module parameters
*	(i)class_attr_chanstatus - attributes of the removed file
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvFlChanStatusRm(void)
{
	uint8_t *flcr_chanstatus;
	struct class *pclass;

	// Set the pointer to the flag: file "channel status" was created
	flcr_chanstatus = &hv_parm.flcr_chanstatus;

	// The file is removed only if it was created previously
	if(*flcr_chanstatus){
		// Read the pointer to the module class
		pclass = module_parm.pclass;

		// Remove the file
		class_remove_file(pclass, &class_attr_chanstatus);

		// Clear the flag that indicates that the file was created
		*flcr_chanstatus = 0;
	}
}

/********************** hvFlChanStatusSh(class,attr,buf) **********************
* Show function for the I/O file in the /sys file subsystem
* File: "channel status" for hvhk channel
* The bitmask with current Status, ON/OFF pin values for all channels
*	is transmitted to user
* Parameters:
*	(i)class - class of the file that was read by user
*	(i)attr - attributes of the file that was read by user
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t hvFlChanStatusSh(struct class *class, struct class_attribute *attr,char *buf)
{
	ssize_t len;
	uint32_t bitmask;

	// Lock hvhk access mutex
	hvMutexLock();

	// Get the values of HVHK Status and ON/OFF pins - for all channels
	bitmask = hvChanGetPinsAll();

	// Create message for the user
	len = snprintf(buf, SYSFS_MSGTR_LEN_MAX,"%.8X", bitmask);
	buf[len] = 0;			// String must end with zero

	// Unlock hvhk access mutex
	hvMutexUnlock();

	// Return the length of the message for the user
	// +1 for the zero at the end of a string
	return (len + 1);
}

/******************* hvFlChanStatusSt(class,attr,buf,count) *******************
* Store function for the I/O file in the /sys file subsystem
* File: "channel status" for hvhk channel
* The function does not process user data
* Parameters: 
*	(i)class - class of the file that was written by user
*	(i)attr - attributes of the file that was written by user
*	(i)buf - buffer with user data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t hvFlChanStatusSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count)
{
	// Return the number of bytes processed (all bytes were processed)
	return count;
}

/****************************** hvFlZeroSh(buf) *******************************
* Show function for the I/O file in the /sys file subsystem
* Creates output with zero value
* The function can be called from any I/O file "show" function
* Parameter:
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t hvFlZeroSh(char *buf)
{
	ssize_t len;

	// Create message for the user
	len = snprintf(buf, SYSFS_MSGTR_LEN_MAX,"%d", 0);
	buf[len] = 0;			// String must end with zero

	// Return the length of the message for the user
	// +1 for the zero at the end of a string
	return (len + 1);
}

/****************************** hvPlatInit(pdev) ******************************
* Platform device - HVHK initialization function
* Allocates resources for the device
* Creates and starts service thread
* Parameter:
*	(io)pdev - structure of the platform device to initialize
* Return value:
*	0  - Platform device was initialized successfully
*	<0 - Error code
*******************************************************************************/
static int hvPlatInit(struct platform_device *pdev)
{
	int rc;
	struct device *dev;

	// Get device structure pointer for the platform device
	dev = &pdev->dev;

	// Init platform device io memory parameters and irq number
	rc = hvPlatInitGetIORes(pdev);
	if(rc != 0) return rc;				// Can not get device iospace resourses

	// Allocate device IO space resources
	rc = hvPlatInitAllocIO(dev);
	if(rc != 0) return rc;				// Can not allocate device iospace resourses

	// Initialize hvhk access mutex
	hvPlatInitMutex();

	// Initialize three expanders
	rc = hvExpInit();
	if(rc != 0) return rc;				// Expanders were not initialized

	// Allocate device IRQ
	rc = hvPlatInitAllocIrq(dev);
	if(rc != 0) return rc;				// Can not allocate IRQ
	
	// Create and start service thread
	return hvPlatInitThrSrv(dev);
}

/************************** hvPlatInitGetIORes(pdev) **************************
* Initialization of HVHK:
* 	Init platform device io memory parameters and irq number
* Used variable:
*	(o)hv_parm - HVHK parameters (for HVHK IP core)
* Parameter:
*	(i)pdev - platform device structure
* Return value:
*	0  - Success. The parameters were initialized
*	-ENODEV - Error. Can not get device resource
*******************************************************************************/
static int hvPlatInitGetIORes(struct platform_device *pdev)
{
	int rc;

	// Init platform device io memory parameters
	rc = hvPlatInitGetIOMem(pdev);
	if(rc != 0) return rc;				// Can not get device io memory resourses

	// Init platform device irq number
	return hvPlatInitGetIOIrq(pdev);
}

/************************** hvPlatInitGetIOMem(pdev) **************************
* Initialization of HVHK:
*	Init platform device io memory parameters
* Used variable:
*	(o)hv_parm - HVHK parameters (for HVHK IP core)
* Parameter:
*	(i)pdev - platform device structure
* Return value:
*	0  - Success. The parameters were initialized
*	-ENODEV - Error. Can not get device io memory resourses
*******************************************************************************/
static int hvPlatInitGetIOMem(struct platform_device *pdev)
{
	struct resource *r_mem; 			// IO mem resources
	struct device *dev;

	// Get device structure pointer for the platform device
	dev = &pdev->dev;

	// Get io memory parameters for the device
	r_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r_mem) {
		dev_err(dev, "can not get io memory parameters\n");
		return -ENODEV;
	}

	// Initialize platform device IO memory start/end address
	hv_parm.mem_start = r_mem -> start;
	hv_parm.mem_end = r_mem -> end;

	// Parameters were initialized successfully
	return 0;
}

/************************** hvPlatInitGetIOIrq(pdev) **************************
* Initialization of HVHK:
*	Init platform device irq number
* Used variable:
*	(o)hv_parm - HVHK parameters (for HVHK IP core)
* Parameter:
*	(i)pdev - platform device structure
* Return value:
*	0  - Success. IRQ number was initialized
*	-ENODEV - Error. Can not get device IRQ number
*******************************************************************************/
static int hvPlatInitGetIOIrq(struct platform_device *pdev)
{
	struct resource *r_irq;			// IRQ resource
	struct device *dev;

	printk(KERN_INFO "Poroshin: hvPlatInitGetIOIrq START!!!\n");

	// Get device structure pointer for the platform device
	dev = &pdev->dev;

	// Get irq resource for the device
	r_irq = platform_get_resource(pdev,IORESOURCE_IRQ, 0);
	if(!r_irq) {
		dev_err(dev, "can not get IRQ\n");
		return -ENODEV;
	}

	// Initialize platform device IRQ number
	hv_parm.irq_num = r_irq -> start;

	printk(KERN_INFO "Poroshin: hvPlatInitGetIOIrq FIN SUCCESS!!!\n");

	// IRQ number was initialized successfully
	return 0;
}

/*************************** hvPlatInitAllocIO(dev) ***************************
* Initialization of HVHK:
*	Allocate device IO memory resources, set base address pointer
* Used variable:
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
* Parameter:
*	(i)dev - pointer to the device structure of the device
* Return value:
*	0  - Success. Device IO resourses were allocated
* 	-EBUSY - Error. Can not allocate memory region
*	-EIO   - Error. Can not init device base address
*******************************************************************************/
static int hvPlatInitAllocIO(struct device *dev)
{
	int rc;

	// Allocate device IO memory resources
	rc = hvPlatInitAllocMem(dev);
	if(rc != 0) return rc;				// Can not lock memory region

	// Set device base address pointer
	return hvPlatInitAllocBase(dev);
}

/************************** hvPlatInitAllocMem(dev) ***************************
* Initialization of HVHK:
*	Allocate device IO memory resources
* Used variable:
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
* Parameter:
*	(i)dev - pointer to the device structure of the device
* Return value:
*	0  - Success. Device IO memory resourses were allocated
* 	-EBUSY - Error. Can not allocate memory region
*******************************************************************************/
static int hvPlatInitAllocMem(struct device *dev)
{
	unsigned long mem_start;
	unsigned long mem_end;
	unsigned long device_iomem_size;

	// Read device IO memory start/end addresses
	mem_start = hv_parm.mem_start;
	mem_end = hv_parm.mem_end;

	// Calculate device IO memory size to allocate
	device_iomem_size = mem_end - mem_start + 1;

	// Allocate device IO memory resources
	if (!request_mem_region(mem_start, device_iomem_size, DRIVER_NAME)) {
		dev_err(dev, "Can not lock memory region at %p\n", (void *)mem_start);
		return -EBUSY;
	}

	// Set flag: device IO memory allocated
	hv_parm.io_mem_allocated = 1;

	// Device IO memory resourses were allocated
	return 0;
}

/************************** hvPlatInitAllocBase(dev) **************************
* Initialization of HVHK:
*	Set device base address pointer
* Used variable:
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
* Parameter:
*	(i)dev - pointer to the device structure of the device
* Return value:
*	0  - Success. Device base address pointer was initialized
*	-EIO - Error. Can not init device base address pointer 
*******************************************************************************/
static int hvPlatInitAllocBase(struct device *dev)
{
	unsigned long mem_start;
	unsigned long mem_end;
	uint32_t __iomem *base_addr;
	unsigned long device_iomem_size;

	// Read device IO memory start/end addresses
	mem_start = hv_parm.mem_start;
	mem_end = hv_parm.mem_end;

	// Calculate device IO memory size
	device_iomem_size = mem_end - mem_start + 1;

	// Init device IO memory pointer
	base_addr = (uint32_t __iomem *)ioremap(mem_start, device_iomem_size);
	if(! base_addr)  {
		dev_err(dev, "Can not init device base address \n");
		return -EIO;
	}

	// Store base address in the device parameters structure
	hv_parm.base_addr = base_addr;

	printk(KERN_INFO "Poroshin: hvPlatInitAllocBase base_addr=%.8x \n", (uint32_t)base_addr);

	// Set flag: base address mapped to the device
	hv_parm.io_base_mapped = 1;

	// Device base address pointer was initialized successfully
	return 0;
}

/***************************** hvPlatInitMutex() ******************************
* Initialize hvhk access mutex
* Used variable:
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvPlatInitMutex(void)
{
	struct mutex *hvmutex;

	// Set the pointer to the mutex structure
	hvmutex = &hv_parm.hvmutex;

	// Init the mutex structure
	mutex_init(hvmutex);

	// Set the flag: hvhk access mutex initialized
	hv_parm.hvmutex_initialized = 1;
}

/************************** hvPlatInitAllocIrq(dev) ***************************
* Initialization of HVHK:
*	Allocate device IRQ
* Used variable:
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
* Parameter:
*	(i)dev - pointer to the device structure of the device
* Return value:
*	0  - Success. Device IRQ was allocated
*	-EBUSY - Error. Can not allocate IRQ
*******************************************************************************/
static int hvPlatInitAllocIrq(struct device *dev)
{
	uint32_t irq_num;
	int rc;

	printk(KERN_INFO "Poroshin: hvPlatInitAllocIrq START!!!\n");

	// Read device IRQ number
	irq_num = hv_parm.irq_num;

	// Allocate device IRQ
	// (interrupt handling is divided into two parts:  top-half and bottom-half)
	rc = request_threaded_irq(irq_num,
		&hvPlatIrqHndlTh, &hvPlatIrqHndlBh,0, DRIVER_NAME, NULL);
	if(rc != 0) {
		dev_err(dev, "Can not allocate device irq \n");
		return -EBUSY;
	}

	// Set flag: device IRQ allocated
	hv_parm.irq_allocated = 1;

	printk(KERN_INFO "Poroshin: hvPlatInitAllocIrq FIN SUCCESS!!!\n");

	// Device IRQ was allocated successfully
	return 0;
}

/*************************** hvPlatInitThrSrv(dev) ****************************
* Initialization of HVHK:
*	Create and start service thread
* Used variable:
*	(o)hv_parm - HVHK parameters (for HVHK IP core)
* Parameter:
*	(i)dev - pointer to the device structure of the device
* Return value:
*	0  - Success. Service thread was created and started
*	-1 - Error. Can not create thread
*******************************************************************************/
static int hvPlatInitThrSrv(struct device *dev)
{
	int rc;
	struct task_struct *thr_srv;

	// Create service thread
	thr_srv = kthread_create(hvPlatThrSrv, NULL, THR_SRV_NAME);
	if(IS_ERR(thr_srv)) {
		// Can not create thread
		dev_err(dev, "Can not create service thread \n");

		// Return error code
		return -1;
	}

	// Start service thread
	wake_up_process(thr_srv);

	// Store service thread handle in device parameters
	hv_parm.thr_srv = thr_srv;

	// Set the flag: service thread was started
	hv_parm.thr_srv_started = 1;

	// Service thread was successfully created and started
	return 0;
}

/***************************** hvPlatThrSrv(data) *****************************
* Service thread main function
* Executes periodic HVHK service work
* Parameter:
*	(i)data - not used
* Return value:
*	always 0
*******************************************************************************/
static int hvPlatThrSrv(void *data)
{
	// The main cycle is executed until the thread is stopped
	while(!kthread_should_stop()){
		// Delay between periodic work activation
		mdelay(DELAY_10MS);

		// Do periodic work: call hvhk channel service routine
		hvChanService();
	}

	// Service thread was successfully finished
	return 0;
}

/*********************** hvPlatIrqHndlTh(irq_num,parm) ************************
* HVHK Interrupt handler (Top-Half)
* The function runs in interrupt context
* It is automatically called when interrupt line from expanders is rising up
* Parameters:
*	(i)irq_num - number of IRQ (not used)
*	(i)parm - not used, always null
* Return value:
*	IRQ_NONE - interrupt was not handled
*	IRQ_HANDLED - interrupt was handled successfully
*	IRQ_WAKE_THREAD - interrupt handler requests to wake the handler thread
*******************************************************************************/
static irqreturn_t hvPlatIrqHndlTh(int irq_num, void *parm)
{
	printk(KERN_INFO "Poroshin: hvPlatIrqHndlTh START!!!\n");

	// Disable an IRQ
	disable_irq_nosync(irq_num);

	// Interrupt handler requests to wake the handler thread (Bottom-Half)
	return IRQ_WAKE_THREAD;
}

/*********************** hvPlatIrqHndlBh(irq_num,parm) ************************
* HVHK Interrupt handler (Bottom-Half)
* The function runs in process context
* It is automatically called when interrupt line from expanders is rising up
* Parameters:
*	(i)irq_num - number of IRQ (not used)
*	(i)parm - not used, always null
* Return value:
*	IRQ_HANDLED - interrupt was handled successfully
*******************************************************************************/
static irqreturn_t hvPlatIrqHndlBh(int irq_num, void *parm)
{
	printk(KERN_INFO "Poroshin: hvPlatIrqHndlBh START!!!\n");

	//DEBUG!!!!
	//mdelay(1000);

	// Handle HVHK channel interrupt
	hvChanIntHndl();

	// Enable an IRQ
	enable_irq(irq_num);

	// Interrupt was handled successfully
	return IRQ_HANDLED;
}

/***************************** hvPlatRegRd(regw) ******************************
* Read 32-bit register value from the HVHK IP core.
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
* Parameter:
*	(i)regw - register number (not checked here, must be valid)
* Return value:
*	32-bit register value
*******************************************************************************/
static uint32_t hvPlatRegRd(uint32_t regw)
{
	uint32_t __iomem *base_addr;

	// Read device base address
	base_addr = hv_parm.base_addr;

	// Return 32-bit register value
	return ioread32(&base_addr[regw]);
}

/*************************** hvPlatRegWr(val,regw) ****************************
* Write 32-bit register value to the HVHK IP core.
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
* Parameters:
*	(i)val  - value to write
*	(i)regw - register number (not checked here, must be valid)
*******************************************************************************/
static void hvPlatRegWr(uint32_t val, uint32_t regw)
{
	uint32_t __iomem *base_addr;

	// Read device base address
	base_addr = hv_parm.base_addr;

	// Write 32-bit register value
	iowrite32(val,&base_addr[regw]);
}

/******************************** hvPlatTran() ********************************
* Execute HVHK IP core data exchange transaction with dac or expander
* - Initiates transaction
* - Waits until transaction is finished
* - Checks the result
* Parameters of the transaction must be written to the HVHK IP core
*	registers before calling this function.
* Return value:
*	-1 - Error. Data exchange transaction failed
*	0  - Success. The transaction was executed
*******************************************************************************/
static int hvPlatTran(void)
{
	uint32_t i;
	uint32_t regw_status_val;

	// Start data transmission
	hvPlatRegWr(BIT_MASK(REGW_CMD_BIT_START),  REGW_CMD);
	hvPlatRegWr(0,  REGW_CMD);

	// Loop in a cycle until operation is completed
	for(i = 0; i < HV_TRAN_ATT_NUM; i++){
		// Give some time to execute operation
		mdelay(HV_TRAN_ATT_DELAY);

		// Read status of the operation
		regw_status_val = hvPlatRegRd(REGW_STATUS);

		// Check if operation is completed
		if(regw_status_val & BIT_MASK(REGW_STATUS_BIT_COMPL))
			return 0;			// The transaction was executed successfully
	}

	// Error. Data exchange transaction failed
	return -1;
}

/****************************** hvPlatFreeAll() *******************************
* Free all resources allocated for the HVHK platform device
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvPlatFreeAll(void)
{
	// Stop service thread
	hvPlatFreeStopThrSrv();

	// Release allocated IRQ
	hvPlatFreeIrq();

	// Destroy hvhk access mutex
	hvPlatFreeMutex();

	// Unmap device base address
	hvPlatFreeBaseUnmap();

	// Release allocated device IO memory
	hvPlatFreeReleaseMem();
}

/*************************** hvPlatFreeStopThrSrv() ***************************
* Stop service thread
* The function blocks until the tread is finished
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvPlatFreeStopThrSrv(void)
{
	uint32_t thr_srv_started;
	struct task_struct *thr_srv;

	// Read service thread handle and "thread started" flag
	thr_srv_started = hv_parm.thr_srv_started;
	thr_srv = hv_parm.thr_srv;

	// Stop service thread. Block until the tread is finished
	if(thr_srv_started) kthread_stop(thr_srv);
}

/****************************** hvPlatFreeIrq() *******************************
* Release allocated IRQ
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvPlatFreeIrq(void)
{
	uint32_t irq_allocated;
	uint32_t irq_num;

	// Read IRQ number and "IRQ allocated" flag
	irq_allocated = hv_parm.irq_allocated;
	irq_num = hv_parm.irq_num;

	// Release allocated IRQ
	if(irq_allocated) free_irq(irq_num, &hv_parm);
}

/***************************** hvPlatFreeMutex() ******************************
* Destroy hvhk access mutex
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvPlatFreeMutex(void)
{
	uint32_t hvmutex_initialized;
	struct mutex *hvmutex;

	// Set pointer to hvhk access mutex structure, read "mutex initialized" flag
	hvmutex_initialized = hv_parm.hvmutex_initialized;
	hvmutex = &hv_parm.hvmutex;

	// Destroy hvhk access mutex
	if(hvmutex_initialized) mutex_destroy(hvmutex);
}

/*************************** hvPlatFreeBaseUnmap() ****************************
* Unmap device base address
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvPlatFreeBaseUnmap(void)
{
	uint32_t io_base_mapped;
	uint32_t __iomem *base_addr;

	// Read device base address and "base address mapped" flag
	io_base_mapped = hv_parm.io_base_mapped;
	base_addr = hv_parm.base_addr;

	// Unmap device base address if needed
	if(io_base_mapped) iounmap(base_addr);
}

/*************************** hvPlatFreeReleaseMem() ***************************
* Release allocated device IO memory
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvPlatFreeReleaseMem(void)
{
	uint32_t io_mem_allocated;
	unsigned long mem_start;
	unsigned long mem_end;
	unsigned long device_iomem_size;

	// Read platform device IO memory parameters
	io_mem_allocated = hv_parm.io_mem_allocated;
	mem_start = hv_parm.mem_start;
	mem_end = hv_parm.mem_end;

	// Calculate device IO memory size to release;
	device_iomem_size = mem_end - mem_start + 1;

	// Release allocated device IO memory
	if(io_mem_allocated) release_mem_region(mem_start, device_iomem_size);
}

/******************************** hvFreeAll() *********************************
* Free all resurces associated with HVHK
* The function is called from HVHK remove function
* It is also called from HVHK probe function in case of errors
* Used variable:
*	(io)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvFreeAll(void)
{
	// Remove all user I/O files in the /sys file subsystem
	hvFilesRemove();

	// Free all resources allocated for the platform device
	hvPlatFreeAll();
}

/************************ hvDacExecCmd(cmd1,cmd2,cmd3) ************************
* Execute command for each dac.
* Sets dac values for all three dac chips
* One, two or three outputs are set on each dac chip
* Access to dac is performed via HVHK IP core
* Parameters:
*	(i)cmd1 - command to set chip 1 outputs
*	(i)cmd2 - command to set chip 2 outputs
*	(i)cmd3 - command to set chip 3 outputs
* Return value:
*	-1 - Error. Operation was not executed
*	0  - Success. Dac commands were executed
*******************************************************************************/
static int hvDacExecCmd(uint32_t cmd1, uint32_t cmd2, uint32_t cmd3)
{
	// Store the commands to send to chips 1,2,3.
	hvPlatRegWr(cmd1, REGW_DATAIN1);
	hvPlatRegWr(cmd2, REGW_DATAIN2);
	hvPlatRegWr(cmd3, REGW_DATAIN3);

	// Transmit data to dac, not to expander
	hvPlatRegWr(0, REGW_CONFIG);

	// Execute HVHK IP core data exchange transaction
	return hvPlatTran();
}

/********************* hvDacCreateCmd(chip_idx,chan_idx) **********************
* Create a command for dac chip to set outputs
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
* Parameters:
*	(i)chip_idx - index of a dac chip (not checked here)
*	(i)chan_idx - index of a channel (not checked here)
* Return value:
*	32-bit command code to set outputs
*******************************************************************************/
static uint32_t hvDacCreateCmd(uint32_t chip_idx, uint32_t chan_idx)
{
	uint32_t dac_idx;
	uint32_t dacvalue;
	HV_DAC_CMD_t cmd;

	// Calculate the index of the dac output
	dac_idx = chip_idx*HV_DAC_CHAN_NUM + chan_idx;

	// Read the dac value for the corresponding dac channel
	dacvalue = hv_parm.dac_values[dac_idx];

	// Create a command word
	cmd.w = 0;
	cmd.fields.value = dacvalue;
	cmd.fields.chan_msk = BIT_MASK(chan_idx);
	cmd.fields.chan_mark = HV_DAC_CMD_CHAN_MARK;

	// Return created command as 32-bit word
	return cmd.w;
}

/****************************** hvDacSetValues() ******************************
* Set new values to the dac outputs
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvDacSetValues(void)
{
	uint32_t cmd[HV_DAC_CHIP_NUM];
	uint32_t chip_idx;
	uint32_t chan_idx;

	// Set outputs in all three chips for each channel
	for(chan_idx = 0; chan_idx < HV_DAC_CHAN_NUM; chan_idx++){
		// Create command to set dac outputs for each chip
		for(chip_idx = 0; chip_idx < HV_DAC_CHIP_NUM; chip_idx++)
			// Create a command for dac chip
			cmd[chip_idx] = hvDacCreateCmd(chip_idx, chan_idx);
		
		// Execute command for the current dac channel
		// The operation is performed with each dac chip
		hvDacExecCmd(cmd[0],cmd[1],cmd[2]);
	}
}

/******************************** hvExpInit() *********************************
* Initialize three expanders
* Access to the each expander is performed via HVHK IP core
* All three expanders are initialized similarly.
* ON/OFF pins are configured as outputs
* Status pins are configured as inputs
* Interrupts are disabled
* Used variable:
*	(i)hv_exp_addr - hvhk expander addresses array
* Return value:
*	-1 - Error. Expanders were not initialized
*	0  - Success. Expanders were initialized
*******************************************************************************/
static int hvExpInit(void)
{
	printk(KERN_INFO "Poroshin: hvExpInit START!!!\n");
	
	// Disable hardware address in each expander
	hvExpInitDisAddr();

	// Initialize registers in all expanders
	hvExpInitRegs();

	// Enable hardware address in all expanders
	hvExpInitEnAddr();

	// Check whether expanders are connected and powered
	return hvExpInitChk();
}

/***************************** hvExpInitDisAddr() *****************************
* Expander initialization:
* 	Disable hardware address in each expander.
* 	Set each expander I/O configuration register
* Used variable:
*	(i)hv_exp_addr - hvhk expander addresses array
*******************************************************************************/
static void hvExpInitDisAddr(void)
{
	uint32_t exp_id;
	uint32_t exp_addr;

	// Cycle on each expander
	for(exp_id = 0; exp_id < HV_EXP_NUM; exp_id++){
		// Get expander address
		exp_addr = hv_exp_addr[exp_id];

		// Set expander I/O configuration register
		hvExpSetReg(exp_addr, REGE_IOCON,
			BIT_MASK(REGE_IOCON_BIT_SREAD)  | \
			BIT_MASK(REGE_IOCON_BIT_DISSLW) | \
			BIT_MASK(REGE_IOCON_BIT_ODR));
	}
}

/****************************** hvExpInitRegs() *******************************
* Expander initialization:
*	Initialize expander registers
* To init all three expanders hardware address in each expander must be 
*	disabled before calling this function.
* Used variable:
*	(i)hv_exp_addr - hvhk expander addresses array
*******************************************************************************/
static void hvExpInitRegs(void)
{
	uint32_t exp_addr;

	// Use first expander address to work with all three expanders
	exp_addr = hv_exp_addr[HV_EXP1];

	// Set to normal GPIO polarity (for each expander)
	hvExpSetReg(exp_addr, REGE_IPOL, 0);
	
	// Disable interrupts
	hvExpSetReg(exp_addr, REGE_GPINTEN, 0);

	// Default values (not used when interrupts are disabled)
	hvExpSetReg(exp_addr, REGE_DEFVAL, 0);

	// Interrupt control (not used when interrupts are disabled)
	hvExpSetReg(exp_addr, REGE_INTCON, 0);

	// GPIO data register - outputs are set to zero, channels disabled
	hvExpSetReg(exp_addr, REGE_GPIO, 0);

	// Output latch register - outputs are set to zero, channels disabled
	hvExpSetReg(exp_addr, REGE_OLAT, 0);

	// Set GPIO direction (ON/OFF pins are configured as outputs)
	hvExpSetReg(exp_addr, REGE_IODIR, ~EXP_PINS_ONOFF_MSK & EXP_PINS_USED_MSK);

	// GPIO pull-up resistor: no pull up
	hvExpSetReg(exp_addr, REGE_GPPU, 0);
}

/***************************** hvExpInitEnAddr() ******************************
* Expander initialization:
*	Enable hardware address in each expander.
*	Set each expander I/O configuration register
* Used variable:
*	(i)hv_exp_addr - hvhk expander addresses array
*******************************************************************************/
static void hvExpInitEnAddr(void)
{
	uint32_t exp_addr;

	// Use first expander address to work with all three expanders
	exp_addr = hv_exp_addr[HV_EXP1];

	// Enable hardware address in all expanders
	hvExpSetReg(exp_addr, REGE_IOCON,
		BIT_MASK(REGE_IOCON_BIT_SREAD)  | \
		BIT_MASK(REGE_IOCON_BIT_DISSLW) | \
		BIT_MASK(REGE_IOCON_BIT_HAEN)	| \
		BIT_MASK(REGE_IOCON_BIT_ODR));
}

/******************************* hvExpInitChk() *******************************
* Expander initialization:
*	Check whether expanders are connected and powered
* The GPIO direction register value from first expander is read and compared 
*	with the default value
* Used variable:
*	(i)hv_exp_addr - hvhk expander addresses array
* Return value:
*	-1 - Error. Received value is not valid
*	0  - Success. Received value equals to the default value
*******************************************************************************/
static int hvExpInitChk(void)
{
	uint32_t exp_addr;
	uint8_t reg_iodir_val;

	printk(KERN_INFO "Poroshin: hvExpInitChk START!!!\n");

	// Use first expander address for the operation
	exp_addr = hv_exp_addr[HV_EXP1];

	// Read the value of "GPIO direction register"
	hvExpGetReg(exp_addr, REGE_IODIR, &reg_iodir_val);

	// Check the value
	if(reg_iodir_val != (~EXP_PINS_ONOFF_MSK & EXP_PINS_USED_MSK)) {
		printk(KERN_INFO "Poroshin: hvExpInitChk FAILED!!!\n");

		return -1;				// Invalid value was received
	}

	printk(KERN_INFO "Poroshin: hvExpInitChk SUCCESS!!!\n");

	// Valid value was received
	return 0;
}

/******************* hvExpSetReg(opcode,reg_addr,reg_data) ********************
* Set the register in the expander
* Access to the expander is performed via HVHK IP core
* Parameters:
*	(i)opcode - address of the expander (bit 0 is always zero)
*	(i)reg_addr - register number inside expander
*	(i)reg_data - data to be written to 8-bit register
* Return value:
*	-1 - Error. Operation was not executed
*	0  - Success. The data was written to the register
*******************************************************************************/
static int hvExpSetReg(uint32_t opcode, uint32_t reg_addr, uint8_t reg_data)
{
	uint32_t par;

	// Create 32-bit expander transaction parameters word
	par = hvExpSetRegParCr(opcode,reg_addr,reg_data);

	// Execute HVHK IP core data exchange transaction with expander
	return hvExpTran(par);
}

/******************* hvExpGetReg(opcode,reg_addr,reg_data) ********************
* Get expander register value
* Access to the expander is performed via HVHK IP core
* Parameters:
*	(i)opcode - address of the expander (bit 0 is always zero)
*	(i)reg_addr - register number inside expander
*	(o)reg_data - received expander register value
* Return value:
*	-1 - Error. Operation was not executed
*	0  - Success. Register value was received
*******************************************************************************/
static int hvExpGetReg(uint32_t opcode, uint32_t reg_addr, uint8_t *reg_data)
{
	uint32_t par;
	int rc;

	// Create 32-bit expander transaction parameters word
	par = hvExpGetRegParCr(opcode,reg_addr);

	// Execute HVHK IP core data exchange transaction with expander
	rc = hvExpTran(par);
	if(rc != 0) return rc;

	// Store received register value
	*reg_data = hvPlatRegRd(REGW_DATAOUT);

	// Register value was received successfully
	return 0;
}

/***************** hvExpSetRegParCr(opcode,reg_addr,reg_data) *****************
* Create HVHK expander "set register" transaction parameters word
* Parameters:
*	(i)opcode - address of the expander (bit 0 is always zero)
*	(i)reg_addr - register number inside expander
*	(i)reg_data - data to be written to 8-bit register
* Return value:
*	32-bit expander transaction parameters word
*******************************************************************************/
static uint32_t hvExpSetRegParCr(uint32_t opcode, uint32_t reg_addr, uint8_t reg_data)
{
	HV_EXP_PAR_t par;

	// Create expander transaction parameters
	par.w = 0;
	par.fields.reg_data = reg_data;
	par.fields.reg_addr = reg_addr;
	par.fields.opcode = opcode;

	// Return 32-bit expander transaction parameters word
	return par.w;
}

/********************* hvExpGetRegParCr(opcode,reg_addr) **********************
* Create HVHK expander "get register" transaction parameters word
* Parameters:
*	(i)opcode - address of the expander (bit 0 is always zero)
*	(i)reg_addr - register number inside expander
* Return value:
*	32-bit expander transaction parameters word
*******************************************************************************/
static uint32_t hvExpGetRegParCr(uint32_t opcode, uint32_t reg_addr)
{
	HV_EXP_PAR_t par;

	// Create expander transaction parameters
	par.w = 0;
	par.fields.reg_addr = reg_addr;
	par.fields.opcode = opcode | HV_EXP_OPCODE_RW_MSK;	// Read operation
	
	// Return 32-bit expander transaction parameters word
	return par.w;
}

/******************************* hvExpTran(par) *******************************
* Execute HVHK IP core data exchange transaction with expander
* Parameter:
*	(i)par - 32-bit expander transaction parameters word
* Return value:
*	-1 - Error. Data exchange transaction failed
*	0  - Success. The transaction was executed
*******************************************************************************/
static int hvExpTran(uint32_t par)
{
	// Write expander transaction parameters into HVHK IP core
	hvPlatRegWr(par, REGW_DATAIN1);

	// Transmit to expander, not to dac
	hvPlatRegWr(BIT_MASK(REGW_CONFIG_BIT_DEST), REGW_CONFIG);

	// Execute HVHK IP core data exchange transaction
	return hvPlatTran();
}

/********************* hvExpEnInt(opcode,msk_en,msk_dis) **********************
* Enable (and disable) expander input pin interrupts
* Parameters:
*	(i)opcode - address of the expander (bit 0 is always zero)
*	(i)msk_en - bit mask for interrupt enabling
*	(i)msk_dis - bit mask for interrupt disabling
*******************************************************************************/
static void hvExpEnInt(uint32_t opcode, uint8_t msk_en, uint8_t msk_dis)
{
	uint8_t defval_val;
	uint8_t intcon_val;
	uint8_t gpinten_val;

	// Read register current values
	hvExpGetReg(opcode, REGE_DEFVAL, &defval_val);
	hvExpGetReg(opcode, REGE_INTCON, &intcon_val);
	hvExpGetReg(opcode, REGE_GPINTEN, &gpinten_val);

	// Update register values (interrupt enabling)
	defval_val |= msk_en;
	intcon_val |= msk_en;
	gpinten_val |= msk_en;

	// Update register values (interrupt disabling)
	defval_val &= ~msk_dis;
	intcon_val &= ~msk_dis;
	gpinten_val &= ~msk_dis;

	// Store new values of the registers
	hvExpSetReg(opcode, REGE_DEFVAL, defval_val);
	hvExpSetReg(opcode, REGE_INTCON, intcon_val);
	hvExpSetReg(opcode, REGE_GPINTEN, gpinten_val);
}

/******************************* hvMutexLock() ********************************
* Lock hvhk access mutex
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvMutexLock(void)
{
	struct mutex *hvmutex;

	// Set the pointer to the mutex structure
	hvmutex = &hv_parm.hvmutex;

	// Lock the mutex
	mutex_lock(hvmutex);
}

/****************************** hvMutexUnlock() *******************************
* Unlock hvhk access mutex
* Used variable:
*	(i)hv_parm - HVHK parameters (for HVHK IP core)
*******************************************************************************/
static void hvMutexUnlock(void)
{
	struct mutex *hvmutex;

	// Set the pointer to the mutex structure
	hvmutex = &hv_parm.hvmutex;

	// Unlock the mutex
	mutex_unlock(hvmutex);
}

/****************************** hvChanService() *******************************
* HVHK channel service routine
* This function is periodically called from a service thread
* Call of this function is independent of interrupt services
* Checks all HVHK channels
* Blocks HVHK channel if:
*	- Channel ON/OFF or Status pin is in LOW state for a long time
*	- Too many interrupt requests from the channel were generated since
*		the channel was turned ON 
*		(ON/OFF or Status pin interrupt counters are checked)
* Used variable:
*	(io)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
*******************************************************************************/
static void hvChanService(void)
{
	// Lock hvhk access mutex
	hvMutexLock();

	// Turn off HVHK channels if the Channel ON/OFF 
	// or Status pin is in LOW state for a long time
	hvChanSrvLowTmrs();

	// Turn off HVHK channels if too many interrupt requests
	// from the channel were generated since the channel was turned ON 
	hvChanSrvIntCnt();

	//	Reenable interrupt mode for successfully working channels
	hvChanSrvReEnInt();

	// Unlock hvhk access mutex
	hvMutexUnlock();	
}

/***************************** hvChanSrvLowTmrs() *****************************
* HVHK channel service:
*	Turns off HVHK channel if the Channel ON/OFF or Status pin is in
*	LOW state for a long time
* Software timers to count time while line is in LOW state are updated here
* Used variable:
*	(io)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
*******************************************************************************/
static void hvChanSrvLowTmrs(void)
{
	uint32_t *pint_pending;
	uint32_t *n_tries_to_release;
	uint32_t *psoft_timer;
	uint32_t int_idx;
	uint32_t int_bit_msk;

	// Set the pointer to the interrupt pending mask
	pint_pending = &hv_chan_ctrl_par.interrupt_pending;

	// Set the pointer to "software timers" array
	n_tries_to_release = hv_chan_ctrl_par.n_tries_to_release;

	// Check pin LOW state timers cycle
	for(int_idx = 0; int_idx < HV_INT_NUM; int_idx++){
		// Create bit mask of the interrupt
		int_bit_msk = BIT_MASK(int_idx);

		// Set the pointer to the software timer 
		// (that counts to time ON/OFF or Status pin LOW state)
		psoft_timer = &n_tries_to_release[int_idx];

		// Update soft timers to count time ON/OFF or Status pin LOW state
		if(*pint_pending & int_bit_msk) {
			// DEBUG!!!!
			printk(KERN_INFO "!");

			// Increment corresponding soft timer
			(*psoft_timer)++;

			// Check if the timer is expired
			if(*psoft_timer > CHAN_N_TRIES_RELEASE_MAX) {
				printk(KERN_INFO "DEBUG: ! HV channel turn off: soft timer expired \n");

				// Timer was expired. Turn off the channel (automatically)
				hvChanAOffByIdx(int_idx);
			}
		}
		else
			// There is no interrupt from the channel.
			// Reset corresponding timer
			*psoft_timer = 0;
	}
}

/***************************** hvChanSrvIntCnt() ******************************
* HVHK channel service:
*	Turns off HVHK channel if too many interrupt requests from the channel
*	were generated since the channel was turned ON 
* Used variable:
*	(io)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
*******************************************************************************/
static void hvChanSrvIntCnt(void)
{
	uint32_t *n_tries_to_release;
	uint32_t *n_interrupts;
	uint32_t *psoft_timer;
	uint32_t *pint_counter;
	uint32_t int_idx;

	// Set the pointer to "software timers" array
	n_tries_to_release = hv_chan_ctrl_par.n_tries_to_release;

	// Set the pointer to "interrupt counters" array
	n_interrupts = hv_chan_ctrl_par.n_interrupts;

	// Check interrupt request counter cycle
	for(int_idx = 0; int_idx < HV_INT_NUM; int_idx++){
		// Set the pointer to the software timer 
		// (that counts to time ON/OFF or Status pin LOW state)
		psoft_timer = &n_tries_to_release[int_idx];

		// Set the pointer to the interrupt counter
		pint_counter = &n_interrupts[int_idx];

		// Update interrupt counters for the triggered interrupts
		// (If interrupt triggered, soft timer just started counting)
		if(*psoft_timer == 1) {
			// DEBUG!!!!
			printk(KERN_INFO "I");

			// Increment corresponding interrupt counter
			(*pint_counter)++;

			// Check the number of interrupt requests
			if(*pint_counter > CHAN_MAX_INTERRUPTS) {
				printk(KERN_INFO "DEBUG: I HV channel turn off: too many interrupts \n");

				// Too many channel interrupts. Turn off the channel (automatically)
				hvChanAOffByIdx(int_idx);
			}
		}
	}
}

/***************************** hvChanSrvReEnInt() *****************************
* HVHK channel service:
* Reenable interrupt mode for successfully working channels
* (function reenables interrupts)
* Used variable:
*	(io)hv_chan_ctrl_par - HVHK channel control parameters (for all channels) 
*******************************************************************************/
static void hvChanSrvReEnInt(void)
{
	uint32_t *pint_pending;
	uint32_t *pwrk_succ;
	uint32_t int_bit_msk;
	uint32_t khv_bit_msk;
	uint8_t khv;

	// Set the pointer to the interrupt pending mask
	pint_pending = &hv_chan_ctrl_par.interrupt_pending;

	// Set the pointer to the "working successful" channel mask
	pwrk_succ = &hv_chan_ctrl_par.working_successful;

	// Reenable interrupts cycle
	for(khv = 0; khv < HV_NUM; khv++){
		// Create bit mask of the correspondent channel
		khv_bit_msk = BIT_MASK(khv);

		// Check that the channel is working
		if(*pwrk_succ & khv_bit_msk) {
			// Create bitmask with both ON/OFF and Status pins
			int_bit_msk = hvChanIntMskOOSt(khv);

			// Interrupts are reenabled only for channels with pending interrupt
			if(*pint_pending & int_bit_msk){
				// Set interrupt mode for the channel with pending interrupt,
				// clear/set interrupt pending bit
				hvChanSetInt(khv);
			}
		}
	}
}

/****************************** hvChanIntHndl() *******************************
* HVHK channel interrupt handling routine
* The function runs in process context in a separate thread
* HVHK interrupt is disabled when this function executes
* Checks all ON/OFF, Status pin interrupt signals for all channels
* 	- Disables interrupt on each active interrupt line
*	- Sets "interrupt pending" flags for each active interrupt line
* Used variables:
*	(i)hv_exp_addr - hvhk expander addresses array
*	(o)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
*******************************************************************************/
static void hvChanIntHndl(void)
{
	uint32_t exp_id;

	// Lock hvhk access mutex
	hvMutexLock();

	printk(KERN_INFO "H");

	// Cycle on each expander
	for(exp_id = 0; exp_id < HV_EXP_NUM; exp_id++){
		// Handle active interrupts of the expander
		hvChanIntHndlExp(exp_id);
	}

	// Unlock hvhk access mutex
	hvMutexUnlock();
}

/************************** hvChanIntHndlExp(exp_id) **************************
* HVHK channel interrupt handling: 
* Handle active interrupts of one of the expanders
* 	- Disables interrupt on each active interrupt line
*	- Sets "interrupt pending" flags for each active interrupt line
* Used variables:
*	(i)hv_exp_addr - hvhk expander addresses array
*	(o)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
* Parameter:
*	exp_id - hvhk expander identifier
*******************************************************************************/
static void hvChanIntHndlExp(uint32_t exp_id)
{
	uint32_t exp_addr;
	uint8_t intf_val;
	uint8_t intcap_val;
	uint8_t gpinten_val;

	// Get expander address
	exp_addr = hv_exp_addr[exp_id];

	// Read active interrupts mask
	hvExpGetReg(exp_addr, REGE_INTF, &intf_val);

	// Read GPIO data values at the time the interrupt occured
	hvExpGetReg(exp_addr, REGE_INTCAP, &intcap_val);

	// Read interrupt enable register
	hvExpGetReg(exp_addr, REGE_GPINTEN, &gpinten_val);

	// Process expander interrupts 
	if(intf_val) {
		// Disable interrupt on each active interrupt line
		gpinten_val &= ~intf_val;

		// Store updated register value
		hvExpSetReg(exp_addr, REGE_GPINTEN, gpinten_val);

		// Update "interrupt pending" flags bit mask
		hv_chan_ctrl_par.interrupt_pending |= 
			((uint32_t)intf_val << EXP_PINS_USED_NUM * exp_id);
	}
}

/***************************** hvChanListOff(msk) *****************************
* Turn off HVHK channels by list
* Used variable:
*	(o)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
* Parameter:
*	(i)msk - bitmask represents the list of channels to turn off
*******************************************************************************/
static void hvChanListOff(uint32_t msk)
{
	uint8_t khv;
	uint32_t khv_bit_msk;

	// Turn off channel cycle
	for(khv = 0; khv < HV_NUM; khv++){
		// Create bit mask of the correspondent channel
		khv_bit_msk = BIT_MASK(khv);

		// Check if the channel has to be turned off
		if(msk & khv_bit_msk){
			// Turn off the channel (by user)
			hvChanUOff(khv);
		}
	}
}

/***************************** hvChanListOn(msk) ******************************
* Turn on HVHK channels by list
* Interrupt mode is set for the turned on channel
* Used variable:
*	(o)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
* Parameter:
*	(i)msk - bitmask represents the list of channels to turn on
*******************************************************************************/
static void hvChanListOn(uint32_t msk)
{
	uint8_t khv;
	uint32_t khv_bit_msk;

	// Turn on channel cycle
	for(khv = 0; khv < HV_NUM; khv++){
		// Create bit mask of the correspondent channel
		khv_bit_msk = BIT_MASK(khv);

		// Check if the channel has to be turned on
		if(msk & khv_bit_msk){
			// Turn on the channel (by user)
			hvChanUOn(khv);

			// Delay for some time before enabling interrupts
			mdelay(DELAY_100MS);

			// Set interrupt mode for the channel
			hvChanSetInt(khv);
		}
	}
}

/******************************* hvChanOff(khv) *******************************
* Turn off HVHK channel
* The interrupts are disabled for the correspondent ON/OFF and Status pins
* The ON/OFF pin is configured as output, the value is set to zero.
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanOff(uint8_t khv)
{
	// Disable HVHK channel interrupts for ON/OFF and Status pins
	hvChanDisInt(khv);

	// Configure HVHK channel ON/OFF pin as output and clear it
	hvChanOOClrOut(khv);

	// Give some time for a channel to turn off
	mdelay(DELAY_10MS);
}

/***************************** hvChanParUOff(khv) *****************************
* Reset parameters for the turned off HVHK channel (by user)
* Used variable:
*	(o)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanParUOff(uint8_t khv)
{
	uint32_t khv_bit_msk;

	// Create bit mask of the channel
	khv_bit_msk = BIT_MASK(khv);

	// Clear "turned on by user" bit
	hv_chan_ctrl_par.turned_on_user &= ~khv_bit_msk;

	// Other parameters reset is performed in
	// "automatically" channel parameters reset function
	hvChanParAOff(khv);
}

/***************************** hvChanParUOn(khv) ******************************
* Set parameters for the turned on channel (by user)
* Used variable:
*	(o)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanParUOn(uint8_t khv)
{
	uint32_t khv_bit_msk;

	// Create bit mask of the channel
	khv_bit_msk = BIT_MASK(khv);

	// Set "turned on" and "working successfull" bits
	hv_chan_ctrl_par.turned_on_user |= khv_bit_msk;
	hv_chan_ctrl_par.working_successful |= khv_bit_msk;

	// Clear channel interrupt parameters
	hvChanParClrInt(khv);
}

/***************************** hvChanParAOff(khv) *****************************
* Reset parameters for the turned off HVHK channel (automatically)
* Used variable:
*	(o)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanParAOff(uint8_t khv)
{
	uint32_t khv_bit_msk;

	// Create bit mask of the channel
	khv_bit_msk = BIT_MASK(khv);

	// Clear "working successfull" bit
	hv_chan_ctrl_par.working_successful &= ~khv_bit_msk;

	// Clear channel interrupt parameters
	hvChanParClrInt(khv);
}

/**************************** hvChanParClrInt(khv) ****************************
* Clear channel interrupt parameters
* Interrupt pending bits, interrupt counters, soft timers for
*	Status and ON/OFF pins are cleared here
* Used variable:
*	(o)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanParClrInt(uint8_t khv)
{
	uint32_t int_bit_msk_oost;

	// Create bitmask with both ON/OFF and Status pins
	int_bit_msk_oost = hvChanIntMskOOSt(khv);

	// Clear "interrupt pending" bits for ON/OFF and Status
	hv_chan_ctrl_par.interrupt_pending &= ~int_bit_msk_oost;

	// Clear channel interrupt counters and soft timers for Status and ON/OFF pins
	hvChanParClrCntTmr(khv);
}

/************************** hvChanParClrCntTmr(khv) ***************************
* Clear channel interrupt counters and soft timers for Status and ON/OFF pins
* Used variable:
*	(o)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanParClrCntTmr(uint8_t khv)
{
	uint32_t *n_interrupts;
	uint32_t *n_tries_to_release;
	uint32_t idx_oo;
	uint32_t idx_st;

	// Set pointers to "interrupt counters" and "software timers" arrays
	n_interrupts = hv_chan_ctrl_par.n_interrupts;
	n_tries_to_release = hv_chan_ctrl_par.n_tries_to_release;

	// Get indexes in arrays for ON/OFF pin and Status pin
	idx_oo = hvChanArrIdxOO(khv);
	idx_st = hvChanArrIdxSt(khv);

	// Clear interrupt counters
	n_interrupts[idx_oo] = 0;
	n_interrupts[idx_st] = 0;

	// Reset software timers
	n_tries_to_release[idx_oo] = 0;
	n_tries_to_release[idx_st] = 0;
}

/****************************** hvChanUOff(khv) *******************************
* Turn off HVHK channel (by user)
* Turns off the chanel, resets cannel parameters
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanUOff(uint8_t khv)
{
	printk(KERN_INFO "DEBUG: hvChanUOff Start khv=%d \n", khv);

	// Turn off HVHK channel
	hvChanOff(khv);

	// Reset parameters for the turned off HVHK channel (user turned off)
	hvChanParUOff(khv);
}

/****************************** hvChanAOff(khv) *******************************
* Turn off HVHK channel (automatically)
* Turns off the chanel, resets cannel parameters
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanAOff(uint8_t khv)
{

	printk(KERN_INFO "DEBUG: hvChanAOff Start khv=%d \n", khv);

	// Turn off HVHK channel
	hvChanOff(khv);

	// Reset parameters for the turned off HVHK channel (automatically turned off)
	hvChanParAOff(khv);
}

/************************** hvChanAOffByIdx(int_idx) **************************
* Turn off HVHK channel (automatically)
* Turns off the chanel, resets cannel parameters
* Parameter:
*	(i)int_idx - index of Status or ON/OFF interrupt associated with the channel 
*******************************************************************************/
static void hvChanAOffByIdx(uint32_t int_idx)
{
	uint8_t khv;

	// Get hvhk channel number by the index of the interrupt
	khv = hvChanArrIdxToKhv(int_idx);

	// Turn off HVHK channel (automatically)
	hvChanAOff(khv);
}

/******************************* hvChanOn(khv) ********************************
* Turn on HVHK channel
* The channel is turned off for a short time to discharge capacitor
* The interrupts are disabled for the correspondent ON/OFF and Status pins
* The ON/OFF pin is configured as output, the value=1 is transmitted for a 
*	short time.
* Than ON/OFF pin is configured as input.
* Parameter:
*	(i)khv - HVHK channel number
* Return value:
*	-1 - Error. Can not turn on HVHK channel
*	0  - Success. HVHK channel was turned on
*******************************************************************************/
static int hvChanOn(uint8_t khv)
{
	uint32_t i;
	uint32_t onoff_bit_val;

	// Turn off the channel for a short time to discharge capacitor
	hvChanOff(khv);

	// Make attempts to turn ON the channel in a cycle
	for(i = 0; i < CHAN_TURNON_ATT_NUM; i++){
		// Configure HVHK channel ON/OFF pin as output and set it to one
		hvChanOOSetOut(khv);

		// Give some time for the channel to turn on
		mdelay(DELAY_10MS);

		// Configure HVHK channel ON/OFF pin as an input
		hvChanOOIn(khv);

		// Wait for some time before reading ON/OFF pin
		mdelay(DELAY_5MS);

		// Read the input
		onoff_bit_val = hvChanOOGet(khv);

		// Check if the channel is ON
		if(onoff_bit_val) return 0;			// HVHK channel was turned on successfully
	}

	// Error. Can not turn on HVHK channel after several attempts
	return -1;
}

/******************************* hvChanUOn(khv) *******************************
* Turn on HVHK channel (by user)
* Turns on the channel, sets parameters for the turned on channel
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanUOn(uint8_t khv)
{
	printk(KERN_INFO "DEBUG: hvChanUOn Start khv=%d \n", khv);

	// Turn on HVHK channel
	hvChanOn(khv);

	// Set parameters for the turned on channel
	hvChanParUOn(khv);
}

/***************************** hvChanDisInt(khv) ******************************
* Disable HVHK channel interrupts for ON/OFF and Status pins
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanDisInt(uint8_t khv)
{
	hvChanEnInt(khv, 0, 0);
}

/*********************** hvChanEnInt(khv,st_flg,oo_flg) ***********************
* Enable/Disable HVHK channel interrupts for ON/OFF and Status pins
* Parameters:
*	(i)khv - HVHK channel number
*	(i)st_flg - flag: enable interrupt for Status pin (1) disable (0)
*	(i)oo_flg - flag: enable interrupt for ON/OFF pin (1) disable (0)
*******************************************************************************/
static void hvChanEnInt(uint8_t khv, uint8_t st_flg, uint8_t oo_flg)
{
	uint8_t cwin_exp;
	uint8_t exp_addr;
	uint8_t bitmsk_st;
	uint8_t bitmsk_oo;
	uint8_t bitmsk_en;
	uint8_t bitmsk_dis;

	printk(KERN_INFO "DEBUG: hvChanEnInt Start khv=%d st_flg=%d oo_flg=%d \n", khv, st_flg, oo_flg);

	// Get HVHK channel parameters
	hvChanGetPar(khv, &cwin_exp, &exp_addr);

	// Create bitmasks for Status and ON/OFF bits
	bitmsk_st = hvChanBitMaskSt(cwin_exp);
	bitmsk_oo = hvChanBitMaskOO(cwin_exp);

	// Initial bitmask values for enabling/disabling interrupts
	bitmsk_en = 0;
	bitmsk_dis = 0;

	// Create the bitmasks for interrupt enabling and disabling
	// (Status bit, ON/OFF bit or both)
	if(st_flg)
		bitmsk_en  |= bitmsk_st;
	else
		bitmsk_dis |= bitmsk_st;

	if(oo_flg)
		bitmsk_en  |= bitmsk_oo;
	else
		bitmsk_dis |= bitmsk_oo;

	// Enable (and disable) expander pin interrupts
	hvExpEnInt(exp_addr, bitmsk_en, bitmsk_dis);
}

/******************** hvChanClrPendInt(khv,st_flg,oo_flg) *********************
* Clear/Set pending interrupts
* Used variable:
*	(o)hv_chan_ctrl_par - HVHK channel control parameters (for all channels)
* Parameters:
*	(i)khv - HVHK channel number
*	(i)st_flg - flag: clear interrupt pending bit for Status (1) set the bit (0)
*	(i)oo_flg - flag: clear interrupt pending bit for ON/OFF (1) set the bit (0)
*******************************************************************************/
static void hvChanClrPendInt(uint8_t khv, uint8_t st_flg, uint8_t oo_flg)
{
	uint32_t *pint_pending;
	uint32_t bitmsk_oo;
	uint32_t bitmsk_st;
	uint32_t bitmsk_set;
	uint32_t bitmsk_clr;
	
	// Set the pointer to the interrupt pending mask
	pint_pending = &hv_chan_ctrl_par.interrupt_pending;

	// Create bitmask for ON/OFF and Status for the channel
	bitmsk_oo = BIT_MASK(hvChanArrIdxOO(khv));
	bitmsk_st = BIT_MASK(hvChanArrIdxSt(khv));

	// Initial bitmask values for clearing/setting pending interrupts
	bitmsk_set = 0;
	bitmsk_clr = 0;

	// Create the bitmasks for clearing/setting pending interrupts
	if(st_flg)
		bitmsk_clr |= bitmsk_st;
	else
		bitmsk_set |= bitmsk_st;

	if(oo_flg)
		bitmsk_clr |= bitmsk_oo;
	else
		bitmsk_set |= bitmsk_oo;
	
	// Clear/Set pending interrupts
	*pint_pending &= ~bitmsk_clr;
	*pint_pending |= bitmsk_set;
}

/***************************** hvChanSetInt(khv) ******************************
* Set interrupt mode for HVHK channel
* If Status pin is 0, interrupt is disabled, interrupt pending bit is set 
* If Status pin is 1, interrupt is enabled, interrupt pending bit is cleared
* If ON/OFF pin is 0, interrupt is disabled, interrupt pending bit is set
* If ON/OFF pin is 1, interrupt is enabled, interrupt pending bit is cleared
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanSetInt(uint8_t khv)
{
	uint8_t st_val;
	uint8_t oo_val;

	printk(KERN_INFO "DEBUG: hvChanSetInt Start khv=%d \n", khv);

	// Get the values of HVHK channel Status and ON/OFF pins
	hvChanGetPins(khv, &st_val, &oo_val);

	// Enable/Disable channel interrupts for ON/OFF and Status pins
	hvChanEnInt(khv, st_val, oo_val);

	// Clear/Set interrupt pending bits for ON/OFF and Status pins
	hvChanClrPendInt(khv, st_val, oo_val);
}

/**************************** hvChanOOClrOut(khv) *****************************
* Configure HVHK channel ON/OFF pin as output and clear it
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanOOClrOut(uint8_t khv)
{
	// Clear HVHK channel ON/OFF pin
	hvChanOOClr(khv);

	// Configure HVHK channel ON/OFF pin as output
	hvChanOOOut(khv);
}

/**************************** hvChanOOSetOut(khv) *****************************
* Configure HVHK channel ON/OFF pin as output and set it to one
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanOOSetOut(uint8_t khv)
{
	// Set HVHK channel ON/OFF pin
	hvChanOOSet(khv);

	// Configure HVHK channel ON/OFF pin as output
	hvChanOOOut(khv);
}

/****************************** hvChanOOClr(khv) ******************************
* Clear HVHK channel ON/OFF pin output
* The correspondent GPIO bit value is set to zero.
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanOOClr(uint8_t khv)
{
	uint8_t cwin_exp;
	uint8_t exp_addr;
	uint8_t gpio_val;

	// Get HVHK channel parameters
	hvChanGetPar(khv, &cwin_exp, &exp_addr);

	// Get GPIO data register
	hvExpGetReg(exp_addr, REGE_GPIO, &gpio_val);

	// Clear ON/OFF pin value
	gpio_val &= ~hvChanBitMaskOO(cwin_exp);

	// Set new value for GPIO data register
	hvExpSetReg(exp_addr, REGE_GPIO, gpio_val);
}

/****************************** hvChanOOSet(khv) ******************************
* Set HVHK channel ON/OFF pin output
* The correspondent GPIO bit value is set to one.
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanOOSet(uint8_t khv)
{
	uint8_t cwin_exp;
	uint8_t exp_addr;
	uint8_t gpio_val;

	// Get HVHK channel parameters
	hvChanGetPar(khv, &cwin_exp, &exp_addr);

	// Get GPIO data register
	hvExpGetReg(exp_addr, REGE_GPIO, &gpio_val);

	// Set ON/OFF pin to one
	gpio_val |= hvChanBitMaskOO(cwin_exp);

	// Set new value for GPIO data register
	hvExpSetReg(exp_addr, REGE_GPIO, gpio_val);
}

/****************************** hvChanOOGet(khv) ******************************
* Get the value of HVHK channel ON/OFF pin
* Parameter:
*	(i)khv - HVHK channel number
* Return value:
*	HVHK channel ON/OFF pin value 0 or 1
*******************************************************************************/
static uint8_t hvChanOOGet(uint8_t khv)
{
	uint8_t st_val;
	uint8_t oo_val;

	// Get the values of HVHK channel Status and ON/OFF pins
	hvChanGetPins(khv, &st_val, &oo_val);

	// Return ON/OFF pin value
	return oo_val;
}

/****************************** hvChanOOOut(khv) ******************************
* Configure HVHK channel ON/OFF pin as output
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanOOOut(uint8_t khv)
{
	uint8_t cwin_exp;
	uint8_t exp_addr;
	uint8_t iodir_val;

	// Get HVHK channel parameters
	hvChanGetPar(khv, &cwin_exp, &exp_addr);

	// Get GPIO direction register
	hvExpGetReg(exp_addr, REGE_IODIR, &iodir_val);

	// Configure the pin as output
	iodir_val &= ~hvChanBitMaskOO(cwin_exp);

	// Set new value for GPIO direction register
	hvExpSetReg(exp_addr, REGE_IODIR, iodir_val);
}

/****************************** hvChanOOIn(khv) *******************************
* Configure HVHK channel ON/OFF pin as input
* Parameter:
*	(i)khv - HVHK channel number
*******************************************************************************/
static void hvChanOOIn(uint8_t khv)
{
	uint8_t cwin_exp;
	uint8_t exp_addr;
	uint8_t iodir_val;

	// Get HVHK channel parameters
	hvChanGetPar(khv, &cwin_exp, &exp_addr);

	// Get GPIO direction register
	hvExpGetReg(exp_addr, REGE_IODIR, &iodir_val);

	// Configure the pin as input
	iodir_val |= hvChanBitMaskOO(cwin_exp);

	// Set new value for GPIO direction register
	hvExpSetReg(exp_addr, REGE_IODIR, iodir_val);
}

/************************* hvChanBitMaskSt(cwin_exp) **************************
* Get bit mask of the Status pin of expander
* Parameter:
*	(i)cwin_exp - channel ID within expander
* Return value:
*	8-bit mask with correspondent bit set to 1
*******************************************************************************/
static uint8_t hvChanBitMaskSt(uint8_t cwin_exp)
{
	return (uint8_t)(BIT_MASK(cwin_exp * 2 + 1));
}

/************************* hvChanBitMaskOO(cwin_exp) **************************
* Get bit mask of the ON/OFF pin of expander
* Parameter:
*	(i)cwin_exp - channel ID within expander
* Return value:
*	8-bit mask with correspondent bit set to 1
*******************************************************************************/
static uint8_t hvChanBitMaskOO(uint8_t cwin_exp)
{
	return (uint8_t)(BIT_MASK(cwin_exp * 2));
}

/**************************** hvChanArrIdxSt(khv) *****************************
* Get array index of the Status pin interrupt
* The index is used in "interrupt counters" and "software timers" arrays
* Parameter:
*	(i)khv - HVHK channel number
* Return value:
*	index in the array
*******************************************************************************/
static uint32_t hvChanArrIdxSt(uint8_t khv)
{
	return (2*khv + 1);
}

/**************************** hvChanArrIdxOO(khv) *****************************
* Get array index of the ON/OFF pin interrupt
* The index is used in "interrupt counters" and "software timers" arrays
* Parameter:
*	(i)khv - HVHK channel number
* Return value:
*	index in the array
*******************************************************************************/
static uint32_t hvChanArrIdxOO(uint8_t khv)
{
	return (2*khv);
}

/*************************** hvChanArrIdxToKhv(idx) ***************************
* Get hvhk channel number by the index in 
*	"interrupt counters" or "software timers" arrays
* Parameter:
*	(i)idx - index in the array
* Return value:
*	hvhk channel number
*******************************************************************************/
static uint8_t hvChanArrIdxToKhv(uint32_t idx)
{
	return ((uint8_t)(idx/2));
}

/*************************** hvChanIntMskOOSt(khv) ****************************
* Get Status and ON/OFF bitmask for HVHK channel
* The bitmask is used to update pending interrupts
* Parameter:
*	(i)khv - HVHK channel number
* Return value:
*	32-bit mask with Status and ON/OFF bits set
*******************************************************************************/
static uint32_t hvChanIntMskOOSt(uint8_t khv)
{
	uint32_t int_bit_msk_oo;
	uint32_t int_bit_msk_st;
	uint32_t int_bit_msk_oost;

	// Create bitmask for ON/OFF and Status for the channel
	int_bit_msk_oo = BIT_MASK(hvChanArrIdxOO(khv));
	int_bit_msk_st = BIT_MASK(hvChanArrIdxSt(khv));

	// Create bitmask with both Status and ON/OFF bits set
	int_bit_msk_oost = int_bit_msk_oo | int_bit_msk_st;

	// Return 32-bit mask with Status and ON/OFF bits set
	return int_bit_msk_oost;
}

/********************** hvChanGetPins(khv,st_val,oo_val) **********************
* Get the values of HVHK channel Status and ON/OFF pins
* Parameters:
*	(i)khv - HVHK channel number
*	(o)st_val - Status pin value (0 or 1)
*	(o)oo_val - ON/OFF pin value (0 or 1)
*******************************************************************************/
static void hvChanGetPins(uint8_t khv, uint8_t *st_val, uint8_t *oo_val)
{
	uint8_t cwin_exp;
	uint8_t exp_addr;
	uint8_t gpio_val;
	uint8_t bitmskst;
	uint8_t bitmskoo;

	// Get HVHK channel parameters
	hvChanGetPar(khv, &cwin_exp, &exp_addr);

	// Get GPIO data register
	hvExpGetReg(exp_addr, REGE_GPIO, &gpio_val);

	// Create masks for Status and ON/OFF bits
	bitmskst = hvChanBitMaskSt(cwin_exp);
	bitmskoo = hvChanBitMaskOO(cwin_exp);

	// Set Status pin value
	if(gpio_val & bitmskst)
		*st_val = 1;
	else
		*st_val = 0;

	// Set ON/OFF pin value
	if(gpio_val & bitmskoo) 
		*oo_val = 1;
	else
		*oo_val = 0;
}

/***************************** hvChanGetPinsAll() *****************************
* Get the values of HVHK Status and ON/OFF pins - for all channels
* Return value:
*	bitmask with Status and ON/OFF pin values for all channels
*******************************************************************************/
static uint32_t hvChanGetPinsAll(void)
{
	uint8_t khv;
	uint32_t bitmask;
	uint8_t st_val;
	uint8_t oo_val;

	// Set initial value for the bitmask
	bitmask = 0;

	// Read Status and ON/OFF pin values for each channel in a cycle
	for(khv = 0; khv < HV_NUM; khv++){
		// Get Status and ON/OFF pin values for the channel
		hvChanGetPins(khv, &st_val, &oo_val);

		// Store Status and ON/OFF pin values in the bitmask
		if(oo_val) bitmask |= BIT_MASK(hvChanArrIdxOO(khv));
		if(st_val) bitmask |= BIT_MASK(hvChanArrIdxSt(khv));
	}

	// Return the bitmask with Status and ON/OFF pin values
	return bitmask;
}

/******************** hvChanGetPar(khv,cwin_exp,exp_addr) *********************
* Get HVHK channel parameters
* Used variable:
*	(i)hv_exp_addr - hvhk expander addresses array
* Parameters:
*	(i)khv - HVHK channel number (not checked)
*	(o)cwin_exp - channel ID within expander
*	(o)exp_addr - expander address
*******************************************************************************/
static void hvChanGetPar(uint8_t khv, uint8_t *cwin_exp, uint8_t *exp_addr)
{
	uint8_t exp_idx;

	// Calculate expander index
	exp_idx = khv / HV_EXP_NUM;

	// Calculate channel ID within expander
	*cwin_exp = khv % HV_EXP_NUM;

	// Get expander address
	*exp_addr = hv_exp_addr[exp_idx];
}

/******************************************************************************
* A module must use the "module_init" "module_exit" macros from linux/init.h,
*	which identify the initialization function at insertion time, 
*	the cleanup function for the module removal
*******************************************************************************/
module_init(moduleInit);
module_exit(moduleExit);

