/*================================ ZYNQBOARD ==================================
*	PROJECT:	ZYNQ3 v1:	 "ZynqBoard software (Xilinx Zynq-7000, Linux) "
*	FILE:		spaciroc-mod.c
*	CONTENTS:	Kernel module. SPACIROC3_SC IP Core driver.
*				Provides interface to set up spaciroc parameters
*	VERSION:	01.01  23.10.2019
*	AUTHOR:		Andrey Poroshin
*	UPDATES :
*	1) 01.01   23 October 2019 - Initial version
 ============================================================================== */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>

// Standard module information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Poroshin Andrey");
MODULE_DESCRIPTION("spaciroc-mod - spaciroc parametes control");

/******************************************************************************
*	Internal definitions
*******************************************************************************/
// This module name
#define DRIVER_NAME	"spaciroc-mod"

// Created class name
#define CLASS_NAME	"spaciroc-cls"

// Delay times (ms)
#define DELAY_5MS			5
#define DELAY_10MS			10

// SPACIROC3_SC registers
#define REGW_CONTROLREG				0	// W: Control (command) register
#define REGW_RESETREG				1	// W: Reset register
#define REGW_CONFIG					3	// W: Configuration register
#define REGW_GENERALREG_0			8	// W: Same data parameters storage, register 0
#define REGW_GENERALREG_1			9	// W: Same data parameters storage, register 1
#define REGW_GENERALREG_2			10	// W: Same data parameters storage, register 2
#define REGW_GENERALREG_3			11	// W: Same data parameters storage, register 3
#define REGW_GENERALREG_4			12	// W: Same data parameters storage, register 4
#define REGW_GENERALREG_5			13	// W: Same data parameters storage, register 5

// SPACIROC3_SC control register bits
#define REGW_CONTROLREG_BIT_START	0	// Setting the bit to 1 for a shot time initiates transmission

// SPACIROC3_SC reset register bits
#define REGW_RESETREG_BIT_RESET		0	// Setting the bit to 1 for a shot time initiates reset

// SPACIROC3_SC config register bits
#define REGW_CONFIG_BIT_IS_SAME		0	// Flag: same data transmission (1) individual data (0)
#define REGW_CONFIG_BIT_USER_LED	1	// Flag: turn on the LED indicator (1)
#define REGW_CONFIG_BIT_SEL_DIN		2	// This bit must be set during transmission

// Same data parameters to load to to all SPACIROCs. Initial values
#define SAME_INI_MISC_REG0			0x0FA20007	// Miscellaneous register 0
#define SAME_INI_X2_TST_MSK_DAC		0x00C000C0	// Dac test mask, 2 pixels
#define SAME_INI_MISC_REG1			0x00000000	// Miscellaneous register 1
#define SAME_INI_X4_GAIN			0x00000000	// Gain, 4 pixels
#define SAME_INI_X4_DAC_7B_SUB		0x00000000	// Dac 7b_sub, 32 pixels
#define SAME_INI_MISC_REG2			0x00000000	// Miscellaneous register 2

// Max time needed to load data to spacirocs (ms)
#define SP_LOAD_TIME_MAX			DELAY_10MS

// Sysfs file: transmitted message max length (b)
#define SYSFS_MSGTR_LEN_MAX	10

/******************************************************************************
*	Internal structures
*******************************************************************************/
// Module parameters structure
typedef struct MODULE_PARM_s {
	uint8_t plat_drv_registered;	// Flag: platform driver was registered (1)
	uint8_t dev_found;				// Flag: device (SPACIROC3_SC IP core) was found (1)
	struct class *pclass;			// Pointer to the created class
} MODULE_PARM_t;

// SPACIROC3_SC parameters structure
typedef struct SP_PARM_s {
	uint8_t flcr_cmd_load_data;		// Flag: file "commands to load data" was created (1)
	uint8_t flcr_same_misc_reg0;	// Flag: file "same data, miscellaneous register 0" was created (1)
	uint8_t flcr_same_x2_tst_msk_dac;//Flag: file "same data, dac test mask 2 pixels" was created (1)
	uint8_t flcr_same_misc_reg1;	// Flag: file "same data, miscellaneous register 1" was created (1)
	uint8_t flcr_same_x4_gain;		// Flag: file "same data, gain, 4 pixels" was created (1)
	uint8_t flcr_same_x4_dac_7b_sub;// Flag: file "same data, dac 7b_sub, 32 pixels" was created (1)
	uint8_t flcr_same_misc_reg2;	// Flag: file "same data, miscellaneous register 2" was created (1)
	uint8_t io_base_mapped;			// Flag: base address mapped to the device (1)
	uint8_t io_mem_allocated;		// Flag: device IO memory allocated (1)
	unsigned long mem_start;		// IO memory start address
	unsigned long mem_end;			// IO memory end address
	uint32_t __iomem *base_addr;	// Device base address

	// Same data parameters to load to to all SPACIROCs
	uint32_t misc_reg0;				// Miscellaneous register 0
	uint32_t x2_tst_msk_dac;		// Dac test mask, 2 pixels
	uint32_t misc_reg1;				// Miscellaneous register 1
	uint32_t x4_gain;				// Gain, 4 pixels
	uint32_t x4_dac_7b_sub;			// Dac 7b_sub, 32 pixels
	uint32_t misc_reg2;				// Miscellaneous register 2
} SP_PARM_t;

// Received commands from user space application
typedef enum USER_CMD_e {
	UCMD_LOAD_SAME_DATA=0,			// Load same data to all spacirocs
	UCMD_LOAD_IND_DATA=1			// Load individual data to all spacirocs
} USER_CMD_t;

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
static int spProbe(struct platform_device *pdev);
static int spProbeDevFound(void);
static void spInitParm(void);
static void spInitParmFlg(void);
static void spInitParmSameData(void);
static int spRemove(struct platform_device *pdev);
static int spFilesCreate(void);
static void spFilesRemove(void);
static int spFlCmdLoadDataCr(void);
static void spFlCmdLoadDataRm(void);
static ssize_t spFlCmdLoadDataSh(struct class *class, struct class_attribute *attr,char *buf);
static ssize_t spFlCmdLoadDataSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
static int spFlSMiscReg0Cr(void);
static void spFlSMiscReg0Rm(void);
static ssize_t spFlSMiscReg0Sh(struct class *class, struct class_attribute *attr,char *buf);
static ssize_t spFlSMiscReg0St(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
static int spFlSX2TstMskDacCr(void);
static void spFlSX2TstMskDacRm(void);
static ssize_t spFlSX2TstMskDacSh(struct class *class, struct class_attribute *attr,char *buf);
static ssize_t spFlSX2TstMskDacSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
static int spFlSMiscReg1Cr(void);
static void spFlSMiscReg1Rm(void);
static ssize_t spFlSMiscReg1Sh(struct class *class, struct class_attribute *attr,char *buf);
static ssize_t spFlSMiscReg1St(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
static int spFlSX4GainCr(void);
static void spFlSX4GainRm(void);
static ssize_t spFlSX4GainSh(struct class *class, struct class_attribute *attr,char *buf);
static ssize_t spFlSX4GainSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
static int spFlSX4Dac7bSubCr(void);
static void spFlSX4Dac7bSubRm(void);
static ssize_t spFlSX4Dac7bSubSh(struct class *class, struct class_attribute *attr,char *buf);
static ssize_t spFlSX4Dac7bSubSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
static int spFlSMiscReg2Cr(void);
static void spFlSMiscReg2Rm(void);
static ssize_t spFlSMiscReg2Sh(struct class *class, struct class_attribute *attr,char *buf);
static ssize_t spFlSMiscReg2St(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count);
static int spFlCr(uint8_t *flcr, const struct class_attribute *attr);
static void spFlRm(uint8_t *flcr, const struct class_attribute *attr);
static ssize_t spFlShVal(uint32_t val, char *buf);
static ssize_t spFlShZero(char *buf);
static ssize_t spFlStVal(uint32_t *val, const char *buf, size_t count);
static int spPlatInit(struct platform_device *pdev);
static int spPlatInitGetIOMem(struct platform_device *pdev);
static int spPlatInitAllocIO(struct device *dev);
static int spPlatInitAllocMem(struct device *dev);
static int spPlatInitAllocBase(struct device *dev);
static void spPlatInitRst(void);
static void spPlatTran(void);
static void spPlatTranStart(void);
static void spPlatRegWr(uint32_t val, uint32_t regw);
static void spPlatFreeAll(void);
static void spPlatFreeBaseUnmap(void);
static void spPlatFreeReleaseMem(void);
static void spCmdLoadSameData(void);
static void spCmdLoadIndData(void);
static void spFreeAll(void);

/******************************************************************************
*	Internal data
*******************************************************************************/
// Module parameters
static MODULE_PARM_t module_parm;

// SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
static SP_PARM_t sp_parm;

// Show and store functions for the file: "commands to load data"
#define cmd_load_data_show			spFlCmdLoadDataSh
#define cmd_load_data_store			spFlCmdLoadDataSt

// Module class attribute: file "commands to load data"
CLASS_ATTR_RW(cmd_load_data);

// Show and store functions for the file: "same data, miscellaneous register 0"
#define same_misc_reg0_show			spFlSMiscReg0Sh
#define same_misc_reg0_store		spFlSMiscReg0St

// Module class attribute: file "same data, miscellaneous register 0"
CLASS_ATTR_RW(same_misc_reg0);

// Show and store functions for the file: "same data, dac test mask 2 pixels"
#define same_x2_tst_msk_dac_show	spFlSX2TstMskDacSh
#define same_x2_tst_msk_dac_store	spFlSX2TstMskDacSt

// Module class attribute: file "same data, dac test mask 2 pixels"
CLASS_ATTR_RW(same_x2_tst_msk_dac);

// Show and store functions for the file: "same data, miscellaneous register 1"
#define same_misc_reg1_show			spFlSMiscReg1Sh
#define same_misc_reg1_store		spFlSMiscReg1St

// Module class attribute: file "same data, miscellaneous register 1"
CLASS_ATTR_RW(same_misc_reg1);

// Show and store functions for the file "same data, gain, 4 pixels"
#define same_x4_gain_show			spFlSX4GainSh
#define same_x4_gain_store			spFlSX4GainSt

// Module class attribute: file "same data, gain, 4 pixels"
CLASS_ATTR_RW(same_x4_gain);

// Show and store functions for the file "same data, dac 7b_sub, 32 pixels"
#define same_x4_dac_7b_sub_show		spFlSX4Dac7bSubSh
#define same_x4_dac_7b_sub_store	spFlSX4Dac7bSubSt

// Module class attribute: file "same data, dac 7b_sub, 32 pixels"
CLASS_ATTR_RW(same_x4_dac_7b_sub);

// Show and store functions for the file: "same data, miscellaneous register 2"
#define same_misc_reg2_show			spFlSMiscReg2Sh
#define same_misc_reg2_store		spFlSMiscReg2St

// Module class attribute: file "same data, miscellaneous register 2"
CLASS_ATTR_RW(same_misc_reg2);

// List of platform driver compatible devices
static struct of_device_id plat_of_match[] = {
	{ .compatible = "xlnx,spaciroc3-sc-1.0", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, plat_of_match);		// Make the list global for the kernel

// SPACIROC3_SC platform driver structure
static struct platform_driver plat_drv = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= plat_of_match,
	},
	.probe  = spProbe,
	.remove = spRemove,
};

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

	// Register platform driver for SPACIROC3_SC IP Core
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
		printk(KERN_INFO "spaciroc-mod: failed to register class \n");

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
* Register platform driver for SPACIROC3_SC IP core
* Used variables:
*	(i)plat_drv - SPACIROC3_SC platform driver structure
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
	// Unregister platform driver for SPACIROC3_SC IP core
	moduleUnreg();

	// Destroy registered class
	moduleDestrCls();
}

/******************************* moduleUnreg() ********************************
* Unregister platform driver for SPACIROC3_SC IP core
* The driver is unregistered only if it was registered previously
* Used variables:
*	(i)plat_drv - SPACIROC3_SC platform driver structure
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

/******************************* spProbe(pdev) ********************************
* SPACIROC3_SC device probe function.
* The function is called when compatible with this driver platform device
*	(SPACIROC3_SC IP) was found
* Only one SPACIROC3_SC IP core is supported
* Creates all needed files to control the device
* Initializes platform device (spaciroc3_sc) - reset is performed
* Parameter:
*	(i)pdev - structure of the platform device to initialize
* Return value:
*	0  - Device was initialized successfully
*	<0 - Error code
*******************************************************************************/
static int spProbe(struct platform_device *pdev)
{
	int rc;

	printk(KERN_INFO "Poroshin: spProbe START \n");
	
	// Check that only one SPACIROC3_SC IP core was found
	rc = spProbeDevFound();
	if(rc != 0) return -1;		// Error: Only one SPACIROC3_SC IP core is supported

	// Init local SPACIROC3_SC parameters
	spInitParm();

	// Initialize SPACIROC3_SC platform device
	rc = spPlatInit(pdev);
	if(rc != 0)	goto SP_PROBE_FAILED;

	// Create all needed files for user I/O in the /sys file subsystem
	rc = spFilesCreate();
	if(rc != 0)	goto SP_PROBE_FAILED;

	// Device was initialized successfully
	return 0;

SP_PROBE_FAILED:
	// Free all resurces associated with SPACIROC3_SC
	spFreeAll();

	// Return error code
	return rc;
}

/***************************** spProbeDevFound() ******************************
* SPACIROC3_SC initialization: check that only one SPACIROC3_SC IP core was found
* Used variable:
*	(io)module_parm - module parameters
* Return value:
*	0  - Success. First SPACIROC3_SC IP core was found
*	-1 - Error. Only one SPACIROC3_SC IP core is supported
*******************************************************************************/
static int spProbeDevFound(void)
{
	uint32_t dev_found;

	// Read the flag: device (SPACIROC3_SC IP core) was found (1), not found (0)
	dev_found = module_parm.dev_found;

	// Check if the device is not the first one
	if(dev_found) return -1;	// Error. Only one SPACIROC3_SC IP core is supported

	// Set the flag: device (SPACIROC3_SC IP core) was found
	module_parm.dev_found = 1;

	// Success. First SPACIROC3_SC IP core was found
	return 0;
}

/******************************** spInitParm() ********************************
* SPACIROC3_SC initialization: init local SPACIROC3_SC parameters
* (This function must be called before all SPACIROC3_SC initializations)
* Used variable:
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spInitParm(void)
{
	// Clear all flags in the parameters structure
	spInitParmFlg();

	// Init same data parameters to load to to all SPACIROCs
	spInitParmSameData();
}

/****************************** spInitParmFlg() *******************************
* SPACIROC3_SC initialization: init local SPACIROC3_SC parameters
* Clears all flags in the parameters structure
* Used variable:
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spInitParmFlg(void)
{
	// Clear all flags in the parameters structure
	sp_parm.flcr_cmd_load_data = 0;
	sp_parm.flcr_same_misc_reg0 = 0;
	sp_parm.flcr_same_x2_tst_msk_dac = 0;
	sp_parm.flcr_same_misc_reg1 = 0;
	sp_parm.flcr_same_x4_gain = 0;
	sp_parm.flcr_same_x4_dac_7b_sub = 0;
	sp_parm.flcr_same_misc_reg2 = 0;
	sp_parm.io_base_mapped = 0;
	sp_parm.io_mem_allocated = 0;
}

/**************************** spInitParmSameData() ****************************
* SPACIROC3_SC initialization: init local SPACIROC3_SC parameters
* Inits same data parameters to load to to all SPACIROCs
* Used variable:
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spInitParmSameData(void)
{
	// Write initial values to same data parameters
	sp_parm.misc_reg0 = SAME_INI_MISC_REG0;
	sp_parm.x2_tst_msk_dac = SAME_INI_X2_TST_MSK_DAC;
	sp_parm.misc_reg1 = SAME_INI_MISC_REG1;
	sp_parm.x4_gain = SAME_INI_X4_GAIN;
	sp_parm.x4_dac_7b_sub = SAME_INI_X4_DAC_7B_SUB;
	sp_parm.misc_reg2 = SAME_INI_MISC_REG2;
}

/******************************* spRemove(pdev) *******************************
* Platform device - SPACIROC3_SC remove function.
* The function is called when compatible platform device (SPACIROC3_SC IP)
*	was removed (or the driver module was removed from kernel)
* Parameter:
*	(i)pdev - structure of the platform device to remove
* Return value:
*	0  - The device was removed successfully
*******************************************************************************/
static int spRemove(struct platform_device *pdev)
{
	printk(KERN_INFO "Poroshin: spRemove EXECUTED \n");

	// Free all resurces associated with SPACIROC3_SC
	spFreeAll();

	// The device was removed successfully
	return 0; 
}

/****************************** spFilesCreate() *******************************
* Create all needed files for user I/O in the /sys file subsystem
* Used variables:
*	(i)module_parm - module parameters
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Return value:
*	0  - All files were created successfully
*	<0 - Error. Can not create all needed files
*******************************************************************************/
static int spFilesCreate(void)
{
	int rc;

	// Create file: commands to load data
	rc = spFlCmdLoadDataCr();
	if(rc != 0) return rc;

	// Create file: same data, miscellaneous register 0
	rc = spFlSMiscReg0Cr();
	if(rc != 0) return rc;

	// Create file: same data, dac test mask 2 pixels
	rc = spFlSX2TstMskDacCr();
	if(rc != 0) return rc;

	// Create file: same data, miscellaneous register 1
	rc = spFlSMiscReg1Cr();
	if(rc != 0) return rc;

	// Create file: same data, gain, 4 pixels
	rc = spFlSX4GainCr();
	if(rc != 0) return rc;

	// Create file: same data, dac 7b_sub, 32 pixels
	rc = spFlSX4Dac7bSubCr();
	if(rc != 0) return rc;

	// Create file: same data, miscellaneous register 2
	return spFlSMiscReg2Cr();
}

/****************************** spFilesRemove() *******************************
* Remove all user I/O files in the /sys file subsystem
* Each file is removed only if it was created previously
* Used variables:
*	(i)module_parm - module parameters
*	(io)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spFilesRemove(void)
{
	// Remove file: same data, miscellaneous register 2
	spFlSMiscReg2Rm();

	// Remove file: same data, dac 7b_sub, 32 pixels
	spFlSX4Dac7bSubRm();

	// Remove file: same data, gain, 4 pixels
	spFlSX4GainRm();

	// Remove file: same data, miscellaneous register 1
	spFlSMiscReg1Rm();

	// Remove file: same data, dac test mask 2 pixels
	spFlSX2TstMskDacRm();

	// Remove file: same data, miscellaneous register 0
	spFlSMiscReg0Rm();

	// Remove file: commands to load data
	spFlCmdLoadDataRm();
}

/**************************** spFlCmdLoadDataCr() *****************************
* Create file for user I/O in the /sys file subsystem
* File: commands to load data
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_cmd_load_data - attributes of the created file
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int spFlCmdLoadDataCr(void)
{
	return spFlCr(&sp_parm.flcr_cmd_load_data, &class_attr_cmd_load_data);
}

/**************************** spFlCmdLoadDataRm() *****************************
* Remove user I/O file in the /sys file subsystem
* File: commands to load data
* The file is removed only if it was created previously.
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_cmd_load_data - attributes of the created file
*	(io)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spFlCmdLoadDataRm(void)
{
	spFlRm(&sp_parm.flcr_cmd_load_data, &class_attr_cmd_load_data);
}

/********************* spFlCmdLoadDataSh(class,attr,buf) **********************
* Show function for the I/O file in the /sys file subsystem
* File: commands to load data
* Zero value is transmitted to user
* Parameters:
*	(i)class - class of the file that was read by user
*	(i)attr - attributes of the file that was read by user
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t spFlCmdLoadDataSh(struct class *class, struct class_attribute *attr,char *buf)
{
	// Transmit message with zero value to user
	return spFlShZero(buf);
}

/****************** spFlCmdLoadDataSt(class,attr,buf,count) *******************
* Store function for the I/O file in the /sys file subsystem
* File: commands to load data
* Received user command is executed here
* Supported commands:
*	1) Load same data to all spacirocs
*	2) Load individual data to all spacirocs
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters: 
*	(i)class - class of the file that was written by user
*	(i)attr - attributes of the file that was written by user
*	(i)buf - buffer with user data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t spFlCmdLoadDataSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count)
{
	ssize_t bytes_processed;
	uint32_t cmd_code;

	// Read the command code to execute
	bytes_processed = spFlStVal(&cmd_code, buf, count);

	// Execute command according to the command code
	switch(cmd_code){
		case UCMD_LOAD_SAME_DATA:
			// Load same data to all spacirocs
			spCmdLoadSameData();
			break;

		case UCMD_LOAD_IND_DATA:
			// Load individual data to all spacirocs
			spCmdLoadIndData();
			break;
	}

	// Return the number of bytes processed
	return bytes_processed;
}

/***************************** spFlSMiscReg0Cr() ******************************
* Create file for user I/O in the /sys file subsystem
* File: same data, miscellaneous register 0
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_same_misc_reg0 - attributes of the created file
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int spFlSMiscReg0Cr(void)
{
	return spFlCr(&sp_parm.flcr_same_misc_reg0, &class_attr_same_misc_reg0);
}

/***************************** spFlSMiscReg0Rm() ******************************
* Remove user I/O file in the /sys file subsystem
* File: same data, miscellaneous register 0
* The file is removed only if it was created previously.
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_same_misc_reg0 - attributes of the created file
*	(io)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spFlSMiscReg0Rm(void)
{
	spFlRm(&sp_parm.flcr_same_misc_reg0, &class_attr_same_misc_reg0);
}

/********************** spFlSMiscReg0Sh(class,attr,buf) ***********************
* Show function for the I/O file in the /sys file subsystem
* File: same data, miscellaneous register 0
* Current value of the correspondent parameter is transmitted to user
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters:
*	(i)class - class of the file that was read by user
*	(i)attr - attributes of the file that was read by user
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t spFlSMiscReg0Sh(struct class *class, struct class_attribute *attr,char *buf)
{
	return spFlShVal(sp_parm.misc_reg0, buf);
}

/******************* spFlSMiscReg0St(class,attr,buf,count) ********************
* Store function for the I/O file in the /sys file subsystem
* File: same data, miscellaneous register 0
* Used variable:
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters: 
*	(i)class - class of the file that was written by user
*	(i)attr - attributes of the file that was written by user
*	(i)buf - buffer with user data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t spFlSMiscReg0St(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count)
{
	return spFlStVal(&sp_parm.misc_reg0, buf, count);
}

/**************************** spFlSX2TstMskDacCr() ****************************
* Create file for user I/O in the /sys file subsystem
* File: same data, dac test mask 2 pixels
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_same_x2_tst_msk_dac - attributes of the created file
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int spFlSX2TstMskDacCr(void)
{
	return spFlCr(&sp_parm.flcr_same_x2_tst_msk_dac, &class_attr_same_x2_tst_msk_dac);
}

/**************************** spFlSX2TstMskDacRm() ****************************
* Remove user I/O file in the /sys file subsystem
* File: same data, dac test mask 2 pixels
* The file is removed only if it was created previously.
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_same_x2_tst_msk_dac - attributes of the created file
*	(io)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spFlSX2TstMskDacRm(void)
{
	spFlRm(&sp_parm.flcr_same_x2_tst_msk_dac, &class_attr_same_x2_tst_msk_dac);
}

/********************* spFlSX2TstMskDacSh(class,attr,buf) *********************
* Show function for the I/O file in the /sys file subsystem
* File: same data, dac test mask 2 pixels
* Current value of the correspondent parameter is transmitted to user
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters:
*	(i)class - class of the file that was read by user
*	(i)attr - attributes of the file that was read by user
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t spFlSX2TstMskDacSh(struct class *class, struct class_attribute *attr,char *buf)
{
	return spFlShVal(sp_parm.x2_tst_msk_dac, buf);
}

/****************** spFlSX2TstMskDacSt(class,attr,buf,count) ******************
* Store function for the I/O file in the /sys file subsystem
* File: same data, dac test mask 2 pixels
* Used variable:
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters: 
*	(i)class - class of the file that was written by user
*	(i)attr - attributes of the file that was written by user
*	(i)buf - buffer with user data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t spFlSX2TstMskDacSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count)
{
	return spFlStVal(&sp_parm.x2_tst_msk_dac, buf, count);
}

/***************************** spFlSMiscReg1Cr() ******************************
* Create file for user I/O in the /sys file subsystem
* File: same data, miscellaneous register 1
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_same_misc_reg1 - attributes of the created file
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int spFlSMiscReg1Cr(void)
{
	return spFlCr(&sp_parm.flcr_same_misc_reg1, &class_attr_same_misc_reg1);
}

/***************************** spFlSMiscReg1Rm() ******************************
* Remove user I/O file in the /sys file subsystem
* File: same data, miscellaneous register 1
* The file is removed only if it was created previously.
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_same_misc_reg1 - attributes of the created file
*	(io)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spFlSMiscReg1Rm(void)
{
	spFlRm(&sp_parm.flcr_same_misc_reg1, &class_attr_same_misc_reg1);
}

/********************** spFlSMiscReg1Sh(class,attr,buf) ***********************
* Show function for the I/O file in the /sys file subsystem
* File: same data, miscellaneous register 1
* Current value of the correspondent parameter is transmitted to user
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters:
*	(i)class - class of the file that was read by user
*	(i)attr - attributes of the file that was read by user
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t spFlSMiscReg1Sh(struct class *class, struct class_attribute *attr,char *buf)
{
	return spFlShVal(sp_parm.misc_reg1, buf);
}

/******************* spFlSMiscReg1St(class,attr,buf,count) ********************
* Store function for the I/O file in the /sys file subsystem
* File: same data, miscellaneous register 1
* Used variable:
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters: 
*	(i)class - class of the file that was written by user
*	(i)attr - attributes of the file that was written by user
*	(i)buf - buffer with user data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t spFlSMiscReg1St(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count)
{
	return spFlStVal(&sp_parm.misc_reg1, buf, count);
}

/****************************** spFlSX4GainCr() *******************************
* Create file for user I/O in the /sys file subsystem
* File: same data, gain, 4 pixels
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_same_x4_gain - attributes of the created file
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int spFlSX4GainCr(void)
{
	return spFlCr(&sp_parm.flcr_same_x4_gain, &class_attr_same_x4_gain);
}

/****************************** spFlSX4GainRm() *******************************
* Remove user I/O file in the /sys file subsystem
* File: same data, gain, 4 pixels
* The file is removed only if it was created previously.
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_same_x4_gain - attributes of the created file
*	(io)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spFlSX4GainRm(void)
{
	spFlRm(&sp_parm.flcr_same_x4_gain, &class_attr_same_x4_gain);
}

/*********************** spFlSX4GainSh(class,attr,buf) ************************
* Show function for the I/O file in the /sys file subsystem
* File: same data, gain, 4 pixels
* Current value of the correspondent parameter is transmitted to user
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters:
*	(i)class - class of the file that was read by user
*	(i)attr - attributes of the file that was read by user
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t spFlSX4GainSh(struct class *class, struct class_attribute *attr,char *buf)
{
	return spFlShVal(sp_parm.x4_gain, buf);
}

/******************** spFlSX4GainSt(class,attr,buf,count) *********************
* Store function for the I/O file in the /sys file subsystem
* File: same data, gain, 4 pixels
* Used variable:
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters: 
*	(i)class - class of the file that was written by user
*	(i)attr - attributes of the file that was written by user
*	(i)buf - buffer with user data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t spFlSX4GainSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count)
{
	return spFlStVal(&sp_parm.x4_gain, buf, count);
}

/**************************** spFlSX4Dac7bSubCr() *****************************
* Create file for user I/O in the /sys file subsystem
* File: same data, dac 7b_sub, 32 pixels
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_same_x4_dac_7b_sub - attributes of the created file
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int spFlSX4Dac7bSubCr(void)
{
	return spFlCr(&sp_parm.flcr_same_x4_dac_7b_sub, &class_attr_same_x4_dac_7b_sub);
}

/**************************** spFlSX4Dac7bSubRm() *****************************
* Remove user I/O file in the /sys file subsystem
* File: same data, dac 7b_sub, 32 pixels
* The file is removed only if it was created previously.
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_same_x4_dac_7b_sub - attributes of the created file
*	(io)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spFlSX4Dac7bSubRm(void)
{
	spFlRm(&sp_parm.flcr_same_x4_dac_7b_sub, &class_attr_same_x4_dac_7b_sub);
}

/********************* spFlSX4Dac7bSubSh(class,attr,buf) **********************
* Show function for the I/O file in the /sys file subsystem
* File: same data, dac 7b_sub, 32 pixels
* Current value of the correspondent parameter is transmitted to user
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters:
*	(i)class - class of the file that was read by user
*	(i)attr - attributes of the file that was read by user
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t spFlSX4Dac7bSubSh(struct class *class, struct class_attribute *attr,char *buf)
{
	return spFlShVal(sp_parm.x4_dac_7b_sub, buf);
}

/****************** spFlSX4Dac7bSubSt(class,attr,buf,count) *******************
* Store function for the I/O file in the /sys file subsystem
* File: same data, dac 7b_sub, 32 pixels
* Used variable:
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters: 
*	(i)class - class of the file that was written by user
*	(i)attr - attributes of the file that was written by user
*	(i)buf - buffer with user data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t spFlSX4Dac7bSubSt(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count)
{
	return spFlStVal(&sp_parm.x4_dac_7b_sub, buf, count);
}

/***************************** spFlSMiscReg2Cr() ******************************
* Create file for user I/O in the /sys file subsystem
* File: same data, miscellaneous register 2
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_same_misc_reg2 - attributes of the created file
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int spFlSMiscReg2Cr(void)
{
	return spFlCr(&sp_parm.flcr_same_misc_reg2, &class_attr_same_misc_reg2);
}

/***************************** spFlSMiscReg2Rm() ******************************
* Remove user I/O file in the /sys file subsystem
* File: same data, miscellaneous register 2
* The file is removed only if it was created previously.
* Used variables:
*	(i)module_parm - module parameters
*	(i)class_attr_same_misc_reg2 - attributes of the created file
*	(io)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spFlSMiscReg2Rm(void)
{
	spFlRm(&sp_parm.flcr_same_misc_reg2, &class_attr_same_misc_reg2);
}

/********************** spFlSMiscReg2Sh(class,attr,buf) ***********************
* Show function for the I/O file in the /sys file subsystem
* File: same data, miscellaneous register 2
* Current value of the correspondent parameter is transmitted to user
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters:
*	(i)class - class of the file that was read by user
*	(i)attr - attributes of the file that was read by user
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t spFlSMiscReg2Sh(struct class *class, struct class_attribute *attr,char *buf)
{
	return spFlShVal(sp_parm.misc_reg2, buf);
}

/******************* spFlSMiscReg2St(class,attr,buf,count) ********************
* Store function for the I/O file in the /sys file subsystem
* File: same data, miscellaneous register 2
* Used variable:
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters: 
*	(i)class - class of the file that was written by user
*	(i)attr - attributes of the file that was written by user
*	(i)buf - buffer with user data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t spFlSMiscReg2St(struct class *class, struct class_attribute *attr,
					const char *buf, size_t count)
{
	return spFlStVal(&sp_parm.misc_reg2, buf, count);
}

/***************************** spFlCr(flcr,attr) ******************************
* Create file for user I/O in the /sys file subsystem
* Used variable:
*	(i)module_parm - module parameters
* Parameters:
*	(o)flcr - flag, indicates that the file was created
*				(when the file is created the flag is set to 1)
*	(i)attr - attributes of the file to create
* Return value:
*	0  - The file was created successfully
*	<0 - Error. The file was not created
*******************************************************************************/
static int spFlCr(uint8_t *flcr, const struct class_attribute *attr)
{
	int rc;
	struct class *pclass;

	// Read the pointer to the module class
	pclass = module_parm.pclass;

	// Create file
	rc = class_create_file(pclass, attr);
	if(rc != 0) return rc;					// File was not created

	// Set the flag that the file was created
	*flcr = 1;

	// The file was created successfully
	return 0;
}

/***************************** spFlRm(flcr,attr) ******************************
* Remove user I/O file in the /sys file subsystem
* The file is removed only if it was created previously.
* Used variable:
*	(i)module_parm - module parameters
* Parameters:
*	(io)flcr - flag, indicates that the file was created previously
*				(the flag is cleared here)
*	(i)attr - attributes of the file to remove
*******************************************************************************/
static void spFlRm(uint8_t *flcr, const struct class_attribute *attr)
{
	struct class *pclass;

	// The file is removed only if it was created previously
	if(*flcr){
		// Read the pointer to the module class
		pclass = module_parm.pclass;

		// Remove the file
		class_remove_file(pclass, attr);	

		// Clear the flag that indicates that the file was created
		*flcr = 0;
	}
}

/***************************** spFlShVal(val,buf) *****************************
* Show function for the I/O file in the /sys file subsystem
* Stores user value in the buffer
* The function can be called from any I/O file "show" function
* Parameters:
*	(i)val - 32-bit user value to show
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t spFlShVal(uint32_t val, char *buf)
{
	ssize_t len;

	// Create message for the user
	len = snprintf(buf, SYSFS_MSGTR_LEN_MAX,"%.8X", val);
	buf[len] = 0;			// String must end with zero

	// Return the length of the message for the user
	// +1 for the zero at the end of a string
	return (len + 1);
}

/****************************** spFlShZero(buf) *******************************
* Show function for the I/O file in the /sys file subsystem
* Stores zero value in the buffer
* The function can be called from any I/O file "show" function
* Parameter:
*	(o)buf - buffer where the data for user is stored
* Return value:
*	length of the data transmitted to user (including zero at the end of string)
*******************************************************************************/
static ssize_t spFlShZero(char *buf)
{
	return spFlShVal(0, buf);
}

/************************** spFlStVal(val,buf,count) **************************
* Store function for the I/O file in the /sys file subsystem
* Reads parameter written to the buffer, stores this parameter in user variable
* Parameters:
*	(o)val - 32-bit user variable
*	(i)buf - buffer with data
*	(i)count - number of bytes in the buffer
* Return value:
*	number of bytes processed (equals to "count" user parameter)
*******************************************************************************/
static ssize_t spFlStVal(uint32_t *val, const char *buf, size_t count)
{
	int rc;
	uint32_t received_val;

	// Read received value
	rc = sscanf(buf, "%x", &received_val);

	// Check that the value was received
	if(rc == 1)
		// Valid value was received. Store it in the user variable
		*val = received_val;

	// Return the number of bytes processed (all bytes were processed)
	return count;
}

/****************************** spPlatInit(pdev) ******************************
* Platform device - SPACIROC3_SC initialization function
* Allocates resources for the device
* Performs device reset
* Parameter:
*	(io)pdev - structure of the platform device to initialize
* Return value:
*	0  - Platform device was initialized successfully
*	<0 - Error code
*******************************************************************************/
static int spPlatInit(struct platform_device *pdev)
{
	int rc;
	struct device *dev;

	// Get device structure pointer for the platform device
	dev = &pdev->dev;

	// Init platform device io memory parameters
	rc = spPlatInitGetIOMem(pdev);
	if(rc != 0) return rc;				// Can not get device io memory resourses

	// Allocate device IO space resources
	rc = spPlatInitAllocIO(dev);
	if(rc != 0) return rc;				// Can not allocate device iospace resourses

	// Init the device - perform device reset
	spPlatInitRst();

	// Platform device was initialized successfully
	return 0;
}

/************************** spPlatInitGetIOMem(pdev) **************************
* Initialization of SPACIROC3_SC:
*	Init platform device io memory parameters
* Used variable:
*	(o)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameter:
*	(i)pdev - platform device structure
* Return value:
*	0  - Success. The parameters were initialized
*	-ENODEV - Error. Can not get device io memory resourses
*******************************************************************************/
static int spPlatInitGetIOMem(struct platform_device *pdev)
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
	sp_parm.mem_start = r_mem -> start;
	sp_parm.mem_end = r_mem -> end;

	// Parameters were initialized successfully
	return 0;
}

/*************************** spPlatInitAllocIO(dev) ***************************
* Initialization of SPACIROC3_SC:
*	Allocate device IO memory resources, set base address pointer
* Used variable:
*	(io)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameter:
*	(i)dev - pointer to the device structure of the device
* Return value:
*	0  - Success. Device IO resourses were allocated
*	-EBUSY - Error. Can not allocate memory region
*	-EIO   - Error. Can not init device base address
*******************************************************************************/
static int spPlatInitAllocIO(struct device *dev)
{
	int rc;

	// Allocate device IO memory resources
	rc = spPlatInitAllocMem(dev);
	if(rc != 0) return rc;				// Can not lock memory region

	// Set device base address pointer
	return spPlatInitAllocBase(dev);
}

/************************** spPlatInitAllocMem(dev) ***************************
* Initialization of SPACIROC3_SC:
*	Allocate device IO memory resources
* Used variable:
*	(io)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameter:
*	(i)dev - pointer to the device structure of the device
* Return value:
*	0  - Success. Device IO memory resourses were allocated
* 	-EBUSY - Error. Can not allocate memory region
*******************************************************************************/
static int spPlatInitAllocMem(struct device *dev)
{
	unsigned long mem_start;
	unsigned long mem_end;
	unsigned long device_iomem_size;

	// Read device IO memory start/end addresses
	mem_start = sp_parm.mem_start;
	mem_end = sp_parm.mem_end;

	// Calculate device IO memory size to allocate
	device_iomem_size = mem_end - mem_start + 1;

	// Allocate device IO memory resources
	if (!request_mem_region(mem_start, device_iomem_size, DRIVER_NAME)) {
		dev_err(dev, "Can not lock memory region at %p\n", (void *)mem_start);
		return -EBUSY;
	}

	// Set flag: device IO memory allocated
	sp_parm.io_mem_allocated = 1;

	// Device IO memory resourses were allocated
	return 0;
}

/************************** spPlatInitAllocBase(dev) **************************
* Initialization of SPACIROC3_SC:
*	Set device base address pointer
* Used variable:
*	(io)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameter:
*	(i)dev - pointer to the device structure of the device
* Return value:
*	0  - Success. Device base address pointer was initialized
*	-EIO - Error. Can not init device base address pointer
*******************************************************************************/
static int spPlatInitAllocBase(struct device *dev)
{
	unsigned long mem_start;
	unsigned long mem_end;
	uint32_t __iomem *base_addr;
	unsigned long device_iomem_size;

	// Read device IO memory start/end addresses
	mem_start = sp_parm.mem_start;
	mem_end = sp_parm.mem_end;
	
	// Calculate device IO memory size
	device_iomem_size = mem_end - mem_start + 1;

	// Init device IO memory pointer
	base_addr = (uint32_t __iomem *)ioremap(mem_start, device_iomem_size);
	if(! base_addr)  {
		dev_err(dev, "Can not init device base address \n");
		return -EIO;
	}

	// Store base address in the device parameters structure
	sp_parm.base_addr = base_addr;

	printk(KERN_INFO "Poroshin: spPlatInitAllocBase base_addr=%.8x \n", (uint32_t)base_addr);

	// Set flag: base address mapped to the device
	sp_parm.io_base_mapped = 1;

	// Device base address pointer was initialized successfully
	return 0;
}

/****************************** spPlatInitRst() *******************************
* Initialization of SPACIROC3_SC:
*	Init the device - perform device reset
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spPlatInitRst(void)
{
	// Setting "reset" bit in the "reset" register for a shot time initiates reset
	spPlatRegWr(BIT_MASK(REGW_RESETREG_BIT_RESET), REGW_RESETREG);
	spPlatRegWr(0, REGW_RESETREG);
}

/******************************** spPlatTran() ********************************
* Tramsmit data to spacirocs
* Initiates transmission, blocks until the transmission is finished
* The function must be called only when the trasmission is configured
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spPlatTran(void)
{
	// Initiate data transmission to spacirocs
	spPlatTranStart();

	// Give some time for data transmission to finish
	mdelay(SP_LOAD_TIME_MAX);
}

/***************************** spPlatTranStart() ******************************
* Initiate data transmission to spacirocs
* The function must be called only when the trasmission is configured
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spPlatTranStart(void)
{
	// Setting "start" bit in the "control" register for a shot time initiates transmission
	spPlatRegWr(BIT_MASK(REGW_CONTROLREG_BIT_START), REGW_CONTROLREG);
	spPlatRegWr(0, REGW_CONTROLREG);
}

/*************************** spPlatRegWr(val,regw) ****************************
* Write 32-bit register value to the SPACIROC3_SC IP core
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
* Parameters:
*	(i)val  - value to write
*	(i)regw - register number (not checked here, must be valid)
*******************************************************************************/
static void spPlatRegWr(uint32_t val, uint32_t regw)
{
	uint32_t __iomem *base_addr;

	// Read device base address
	base_addr = sp_parm.base_addr;

	// Write 32-bit register value
	iowrite32(val,&base_addr[regw]);
}

/****************************** spPlatFreeAll() *******************************
* Free all resources allocated for the SPACIROC3_SC platform device
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spPlatFreeAll(void)
{
	// Unmap device base address
	spPlatFreeBaseUnmap();

	// Release allocated device IO memory
	spPlatFreeReleaseMem();
}

/*************************** spPlatFreeBaseUnmap() ****************************
* Unmap device base address
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spPlatFreeBaseUnmap(void)
{
	uint32_t io_base_mapped;
	uint32_t __iomem *base_addr;

	// Read device base address and "base address mapped" flag
	io_base_mapped = sp_parm.io_base_mapped;
	base_addr = sp_parm.base_addr;

	// Unmap device base address if needed
	if(io_base_mapped) iounmap(base_addr);
}

/*************************** spPlatFreeReleaseMem() ***************************
* Release allocated device IO memory
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spPlatFreeReleaseMem(void)
{
	uint32_t io_mem_allocated;
	unsigned long mem_start;
	unsigned long mem_end;
	unsigned long device_iomem_size;

	// Read platform device IO memory parameters
	io_mem_allocated = sp_parm.io_mem_allocated;
	mem_start = sp_parm.mem_start;
	mem_end = sp_parm.mem_end;

	// Calculate device IO memory size to release;
	device_iomem_size = mem_end - mem_start + 1;

	// Release allocated device IO memory
	if(io_mem_allocated) release_mem_region(mem_start, device_iomem_size);	
}

/**************************** spCmdLoadSameData() *****************************
* Execute user command:
*	Load same data to all spacirocs
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spCmdLoadSameData(void)
{
	// Write "same data" parameters into the SPACIROC3_SC IP core registers
	spPlatRegWr(sp_parm.misc_reg0, REGW_GENERALREG_0);
	spPlatRegWr(sp_parm.x2_tst_msk_dac, REGW_GENERALREG_1);
	spPlatRegWr(sp_parm.misc_reg1, REGW_GENERALREG_2);
	spPlatRegWr(sp_parm.x4_gain, REGW_GENERALREG_3);
	spPlatRegWr(sp_parm.x4_dac_7b_sub, REGW_GENERALREG_4);
	spPlatRegWr(sp_parm.misc_reg2, REGW_GENERALREG_5);

	// Set transmission configuration: same data for all spacirocs
	spPlatRegWr(
		BIT_MASK(REGW_CONFIG_BIT_IS_SAME)  | \
		BIT_MASK(REGW_CONFIG_BIT_USER_LED) | \
		BIT_MASK(REGW_CONFIG_BIT_SEL_DIN), 
		REGW_CONFIG);

	// Tramsmit data to spacirocs
	spPlatTran();
}

/***************************** spCmdLoadIndData() *****************************
* Execute user command:
*	Load individual data to all spacirocs
* The data for spacirocs must be loaded to the hardware fifo by user application
* This function assumes hardware fifo is already loaded
* Used variable:
*	(i)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spCmdLoadIndData(void)
{
	// Hardware fifo contains the data to transmit
	// Set transmission configuration: individual data for all spacirocs
	spPlatRegWr(
		BIT_MASK(REGW_CONFIG_BIT_USER_LED) | \
		BIT_MASK(REGW_CONFIG_BIT_SEL_DIN), 
		REGW_CONFIG);
	
	// Tramsmit data to spacirocs
	spPlatTran();
}

/******************************** spFreeAll() *********************************
* Free all resurces associated with SPACIROC3_SC
* The function is called from SPACIROC3_SC remove function
* It is also called from SPACIROC3_SC probe function in case of errors
* Used variable:
*	(io)sp_parm - SPACIROC3_SC parameters (for SPACIROC3_SC IP core)
*******************************************************************************/
static void spFreeAll(void)
{
	// Remove all user I/O files in the /sys file subsystem
	spFilesRemove();

	// Free all resources allocated for the platform device
	spPlatFreeAll();
}

/******************************************************************************
* A module must use the "module_init" "module_exit" macros from linux/init.h,
*	which identify the initialization function at insertion time, 
*	the cleanup function for the module removal
*******************************************************************************/
module_init(moduleInit);
module_exit(moduleExit);

