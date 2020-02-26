/*================================ ZYNQBOARD ==================================
*	PROJECT:	ZYNQ3 v1:	 "ZynqBoard software (Xilinx Zynq-7000, Linux) "
*	FILE:		dma-mod.c
*	CONTENTS:	Kernel module. Pseudo-device driver.
*				Provides user application access interface to one or more DMA
*				channels for DMA data receive operations
*	VERSION:	01.01  10.02.2020
*	AUTHOR:		Andrey Poroshin
*	UPDATES :
*	1) 01.01   10 February 2020 - Initial version
 ============================================================================== */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>

#include "dma-mod-intf.h"

// Standard module information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Poroshin Andrey");
MODULE_DESCRIPTION("dma-mod - dma channels user access module");

/******************************************************************************
*	Internal definitions
*******************************************************************************/
// This module name
#define DRIVER_NAME	"dma-mod"

// Created class name
#define CLASS_NAME	"dma-cls"

/******************************************************************************
*	Internal structures
*******************************************************************************/
// Module parameters structure
typedef struct MODULE_PARM_s {
	uint8_t plat_drv_registered;	// Flag: platform driver was registered (1)
	uint8_t dev_found;				// Flag: DMA-PROXY device was found (1)
	struct class *pclass;			// Pointer to the created class
} MODULE_PARM_t;

// DMA-PROXY DMA channel parameters
typedef struct DM_CHAN_s {
	// Unique index of DMA channel
	uint32_t ch_idx;

	// DMA channel support
	struct dma_chan *dma_chan;		// DMA Engine channel parameters
	uint8_t *dma_buffer;			// Pointer to the allocated DMA buffer
	dma_addr_t dma_buffer_phadd;	// DMA buffer physical address
	struct dma_async_tx_descriptor *tran_desc;	// Async DMA transaction descriptor
	struct completion cmp;			// DMA transaction complete indicator
	dma_cookie_t cookie;			// The cookie to track the status of DMA transaction
	uint32_t res_code;				// DMA transaction result code (for user app)

	// Character device support
	uint8_t cdev_region_alloc;		// Flag: character device major+minor numbers allocated (1)
	uint8_t cdev_added;				// Flag: character device was added to the kernel (1)
	uint8_t cdev_created;			// Flag: character device was created (1)
	uint8_t cdev_opened;			// Flag: character device was opened (1)
	dev_t cdev_node;				// 32-bit value, contains major and minor numbers
	struct cdev cdev;				// Kernel character device structure
} DM_CHAN_t;

// DMA-PROXY parameters structure
typedef struct DM_PARM_s {
	// DMA-PROXY device structure pointer
	struct device *dev;

	// Parameters for each DMA channel
	DM_CHAN_t	ch[_DM_CH_NUM];
} DM_PARM_t;

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
static int dmProbe(struct platform_device *pdev);
static int dmProbeDevFound(void);
static int dmRemove(struct platform_device *pdev);
static void dmInitParm(struct platform_device *pdev);
static void dmInitParmCh(uint32_t ch_idx);
static int dmInitAllCh(void);
static int dmInitCh(DM_CHAN_t *pch);
static int dmInitChReq(DM_CHAN_t *pch);
static int dmInitChMem(DM_CHAN_t *pch);
static int dmInitChDev(DM_CHAN_t *pch);
static int dmInitChDevRegion(DM_CHAN_t *pch);
static int dmInitChDevCdev(DM_CHAN_t *pch);
static int dmInitChDevCrDev(DM_CHAN_t *pch);
static int dmCdevOpen(struct inode *ino, struct file *file);
static int dmCdevRelease(struct inode *ino, struct file *file);
static long dmCdevIoctl(struct file *file, unsigned int cmd, unsigned long arg);
static int dmCdevMmap(struct file *file, struct vm_area_struct *vma);
static void dmChTransfer(DM_CHAN_t *pch);
static void dmChTrCallBack(void *cmp);
static int dmChTrStart(DM_CHAN_t *pch);
static int dmChTrIniSing(DM_CHAN_t *pch);
static void dmChTrIniCallBack(DM_CHAN_t *pch);
static void dmChTrIniCmp(DM_CHAN_t *pch);
static int dmChTrIniSubmit(DM_CHAN_t *pch);
static void dmChTrIniIssuePend(DM_CHAN_t *pch);
static void dmChTrWait(DM_CHAN_t *pch);
static void dmChTrWaitCmp(DM_CHAN_t *pch);
static enum dma_status dmChTrWaitGetStat(DM_CHAN_t *pch);
static void dmChTrWaitRes(DM_CHAN_t *pch, enum dma_status status);
static void dmChTrStResCode(DM_CHAN_t *pch, uint32_t res_code);
static int dmChResToUser(DM_CHAN_t *pch, unsigned long arg);
static void dmChTerm(DM_CHAN_t *pch);
static void dmFreeAll(void);
static void dmFreeCh(DM_CHAN_t *pch);
static void dmFreeChDev(DM_CHAN_t *pch);
static void dmFreeChDevDest(DM_CHAN_t *pch);
static void dmFreeChDevCdev(DM_CHAN_t *pch);
static void dmFreeChDevRegion(DM_CHAN_t *pch);
static void dmFreeChMem(DM_CHAN_t *pch);
static void dmFreeChRelease(DM_CHAN_t *pch);

/******************************************************************************
*	Internal data
*******************************************************************************/
// Module parameters
static MODULE_PARM_t module_parm;

// DMA-PROXY device parameters
static DM_PARM_t dm_parm;

// List of platform driver compatible devices
static struct of_device_id plat_of_match[] = {
	{ .compatible = "por,dma-proxy-pseudo-dev", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, plat_of_match);		// Make the list global for the kernel

// DMA-PROXY pseudo-device platform driver structure
static struct platform_driver plat_drv = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= plat_of_match,
	},
	.probe  = dmProbe,
	.remove = dmRemove,
};

// DMA channel names
static const char	*dm_ch_name[_DM_CH_NUM] = {
	_DM_CHN_AXI_DMA_0,			// Index - _DM_CH_AXI_DMA_0
	_DM_CHN_AXI_DMA_SC			// Index - _DM_CH_AXI_DMA_SC
};

// DMA channel transaction sizes
static const uint32_t dm_ch_trsz[_DM_CH_NUM] = {
	_DM_AXI_DMA_0_TRSZ,			// Index - _DM_CH_AXI_DMA_0
	_DM_AXI_DMA_SC_TRSZ			// Index - _DM_CH_AXI_DMA_SC
};

// Character device file operations
static struct file_operations dm_cdev_fops = {
	.owner = THIS_MODULE,
	.open = dmCdevOpen,
	.release = dmCdevRelease,
	.unlocked_ioctl = dmCdevIoctl,
	.mmap = dmCdevMmap
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

	// Register platform driver for DMA-PROXY pseudo-device
	rc = moduleReg();
	if(rc != 0)
		// Free all resources associated with this module
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
		printk(KERN_INFO "dma-mod: failed to register class \n");

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
* Register platform driver for DMA-PROXY pseudo-device
* Used variables:
*	(i)plat_drv - DMA-PROXY pseudo-device platform driver structure
*	(o)module_parm - module parameters
* Return value:
*	0  Platform driver was registered successfully
*	<0 Error code
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
	// Free all resources associated with this module
	moduleFreeAll();

	printk(KERN_INFO "Poroshin: exit %s \n", DRIVER_NAME);
}

/****************************** moduleFreeAll() *******************************
* Free all resources associated with this module
* The function is called from module exit function
* It is also called from module initialization function in case of errors
* Used variable:
*	(i)module_parm - module parameters
*******************************************************************************/
static void moduleFreeAll(void)
{
	// Unregister platform driver for DMA-PROXY pseudo-device
	moduleUnreg();

	// Destroy registered class
	moduleDestrCls();
}

/******************************* moduleUnreg() ********************************
* Unregister platform driver for DMA-PROXY pseudo-device
* The driver is unregistered only if it was registered previously
* Used variables:
*	(i)plat_drv - DMA-PROXY pseudo-device platform driver structure
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
* Used variable:
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

/******************************* dmProbe(pdev) ********************************
* DMA-PROXY pseudo-device probe function.
* The function is called when compatible with this driver platform device
*	(DMA-PROXY) was found
* Only one DMA-PROXY device is supported
* Inits dma channels
* Creates character devices in /dev folder for each dma channel
*	(for user ioctl requests)
* Allocates kernel buffers for for each dma channel
* Parameter:
*	(i)pdev - structure of the detected platform device 
* Return value:
*	0  - Success. Dma channels, character devices were initialized.
*	<0 - Error code
*******************************************************************************/
static int dmProbe(struct platform_device *pdev)
{
	int rc;

	printk(KERN_INFO "Poroshin: dmProbe START \n");

	// Check that only one DMA-PROXY pseudo-device was found
	rc = dmProbeDevFound();
	if(rc != 0) return -1;			// Error: Only one DMA-PROXY device is supported.

	// Init DMA-PROXY device parameters
	dmInitParm(pdev);

	// Init all DMA channels
	rc = dmInitAllCh();
	if(rc != 0) 
		// Free all resources associated with DMA-PROXY
		dmFreeAll();

	// Return success/error code
	return rc;
}

/***************************** dmProbeDevFound() ******************************
* DMA-PROXY initialization: check that only one DMA-PROXY device was found
* Used variable:
*	(io)module_parm - module parameters
* Return value:
*	0  Success. First DMA-PROXY device was found
*	-1 Error. Only one DMA-PROXY device is supported. 
*******************************************************************************/
static int dmProbeDevFound(void)
{
	uint32_t dev_found;

	// Read the flag: DMA-PROXY device was found (1), not found (0)
	dev_found = module_parm.dev_found;

	// Check if the device is not the first one
	if(dev_found) return -1;	// Error. Only one DMA-PROXY device is supported. 

	// Set the flag: DMA-PROXY device was found
	module_parm.dev_found = 1;

	// Success. First DMA-PROXY device was found
	return 0;
}

/******************************* dmRemove(pdev) *******************************
* DMA-PROXY pseudo-device remove function
* The function is called when compatible platform device (DMA-PROXY)
*	was removed (or the driver module was removed from kernel)
* Parameter:
*	(i)pdev - structure of the platform device to remove
* Return value:
*	0  - The device was removed successfully
*******************************************************************************/
static int dmRemove(struct platform_device *pdev)
{
	printk(KERN_INFO "Poroshin: dmRemove EXECUTED \n");

	// Free all resources associated with DMA-PROXY
	dmFreeAll();

	// The device was removed successfully
	return 0;
}

/****************************** dmInitParm(pdev) ******************************
* DMA-PROXY initialization: init DMA-PROXY device parameters
* (This function must be called before all DMA-PROXY initializations)
* Used variable:
*	(o)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pdev - structure of the platform device - DMA-PROXY
*******************************************************************************/
static void dmInitParm(struct platform_device *pdev)
{
	struct device *dev;
	uint32_t ch_idx;

	// Read the pointer to the device structure of DMA-PROXY
	dev = &pdev->dev;

	// Set device structure pointer in DMA-PROXY parameters
	dm_parm.dev = dev;

	// Init DMA channel parameters cycle
	for(ch_idx = 0; ch_idx < _DM_CH_NUM; ch_idx++)
		// Init current channel
		dmInitParmCh(ch_idx);
}

/**************************** dmInitParmCh(ch_idx) ****************************
* DMA-PROXY initialization: init DMA-PROXY channel parameters
* Used variable:
*	(o)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)ch_idx - channel index
*******************************************************************************/
static void dmInitParmCh(uint32_t ch_idx)
{
	DM_CHAN_t *pch;

	// Set the pointer to the DMA-PROXY DMA channel parameters
	pch = &dm_parm.ch[ch_idx];

	// Set channel index
	pch -> ch_idx = ch_idx;

	// Clear pointers to the allocated resources
	pch -> dma_chan = NULL;
	pch -> dma_buffer = NULL;

	// Clear character device support flags
	pch -> cdev_region_alloc = 0;
	pch -> cdev_added = 0;
	pch -> cdev_created = 0;
	pch -> cdev_opened = 0;
}

/******************************* dmInitAllCh() ********************************
* DMA-PROXY initialization: init all DMA channels
* Used variable:
*	(o)dm_parm - DMA-PROXY parameters
* Return value:
*	0  Success. All DMA channels were initialized
*	-1 Error. Can not init one or more DMA channels
*******************************************************************************/
static int dmInitAllCh(void)
{	
	uint32_t ch_idx;
	DM_CHAN_t *pch;
	int rc;

	// Init channels cycle
	for(ch_idx = 0; ch_idx < _DM_CH_NUM; ch_idx++){
		// Set the pointer to the DMA-PROXY channel parameters
		pch = &dm_parm.ch[ch_idx];
	
		// Init the channel
		rc = dmInitCh(pch);
		if(rc < 0) return -1;			// Can not init the channel
	}

	// All DMA channels were initialized successfully
	return 0;
}

/******************************* dmInitCh(pch) ********************************
* DMA-PROXY initialization:
*	init DMA channel and character device for the user access to the channel
* Requests DMA channel from DMA Engine
* Allocates memory for DMA operations in the kernel space
* Creates character device in the /dev folder
* Used variable:
*	(io)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
* Return value:
*	0  Success. DMA channel was initialized
*	-1 Error. Can not init DMA channel
*******************************************************************************/
static int dmInitCh(DM_CHAN_t *pch)
{
	int rc;

	// Request DMA channel from the DMA engine
	rc = dmInitChReq(pch);
	if(rc < 0) return -1;				// Can not allocate DMA channel

	// Allocate memory for DMA operations in the kernel space
	rc = dmInitChMem(pch);
	if(rc < 0) return -1;				// Can not allocate memory

	// Create character device
	return dmInitChDev(pch);
}

/****************************** dmInitChReq(pch) ******************************
* DMA-PROXY channel initialization:
*	Request DMA channel from DMA Engine
* Used variable:
*	(io)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
* Return value:
*	0  Success. DMA channel was allocated
*	-1 Error. Can not allocate DMA channel
*******************************************************************************/
static int dmInitChReq(DM_CHAN_t *pch)
{
	struct device *dev;
	uint32_t ch_idx;
	const char	*chan_name;
	struct dma_chan *dma_chan;

	// Set the pointer to the device structure of DMA-PROXY
	dev = dm_parm.dev;

	// Get DMA-PROXY channel index
	ch_idx = pch -> ch_idx;

	// Set the pointer to the channel name
	chan_name = dm_ch_name[ch_idx];

	// Request DMA channel from DMA Engine
	dma_chan = dma_request_slave_channel(dev,chan_name);
	if(dma_chan == NULL) {
		dev_err(dev, "DMA channel request error\n");

		// Can not allocate DMA channel
		return -1;
	}
	
	// Set the pointer to the allocated channel in DMA-PROXY channel parameters
	pch -> dma_chan = dma_chan;

	// DMA channel was allocated successfully
	return 0;
}

/****************************** dmInitChMem(pch) ******************************
* DMA-PROXY channel initialization:
*	Allocate memory for DMA operations in the kernel space
* Used variable:
*	(io)dm_parm - DMA-PROXY parameters
*	(i)dm_ch_trsz - DMA channel transaction sizes array
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
* Return value:
*	0  Success. The memory was allocated
*	-1 Error. Can not allocate memory
*******************************************************************************/
static int dmInitChMem(DM_CHAN_t *pch)
{
	struct device *dev;
	uint32_t ch_idx;
	uint32_t trsz;
	dma_addr_t *dma_handle;
	uint8_t *dma_buffer;

	// Set the pointer to the DMA-PROXY device structure
	dev = dm_parm.dev;

	// Get DMA-PROXY channel index
	ch_idx = pch -> ch_idx;

	// Get the size of the memory to allocate (b)
	trsz = dm_ch_trsz[ch_idx];

	// Set the pointer to the DMA buffer physical address (aka DMA handle)
	dma_handle = &(pch -> dma_buffer_phadd);

	// Allocate coherent memory for DMA-PROXY channel in kernel space
	dma_buffer = (uint8_t *)
		dmam_alloc_coherent(dev, trsz, dma_handle, GFP_KERNEL);
	if(dma_buffer == NULL) return -1;				// Can not allocate memory

	// Set the pointer to the allocated memory in "DMA channel parameters" structure
	pch -> dma_buffer = dma_buffer;

	// The memory was allocated successfully
	return 0;
}

/****************************** dmInitChDev(pch) ******************************
* DMA-PROXY channel initialization:
*	Create character device in /dev folder for user ioctl requests
* Allocates character device major and minor numbers
* Inits character device data structure
* Creates character device
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
* Return value:
*	-1 Error. Character device was not created
*	0  Success. Character device was created
*******************************************************************************/
static int dmInitChDev(DM_CHAN_t *pch)
{
	int rc;

	// Allocate character device major and minor numbers
	rc = dmInitChDevRegion(pch);
	if(rc < 0) return rc;				// Can not allocate major+minor numbers

	// Init character device data structure, add character device to the kernel
	rc = dmInitChDevCdev(pch);
	if(rc < 0) return rc;				// Can not add character device to the kernel

	// Create character device
	return dmInitChDevCrDev(pch);
}

/*************************** dmInitChDevRegion(pch) ***************************
* DMA-PROXY channel character device initialization:
*	Allocate character device major and minor numbers
* Used variable:
*	(o)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
* Return value:
*	-1 Error. Can not allocate major+minor numbers
*	0  Success. Character device major and minor numbers were allocated
*******************************************************************************/
static int dmInitChDevRegion(DM_CHAN_t *pch)
{
	dev_t *pnode;
	int rc;

	// Create the pointer to the 32-bit major+minor number (for the channel)
	pnode = &(pch -> cdev_node);

	// Allocate major number and one minor number for the device
	rc = alloc_chrdev_region(pnode, 0, 1, DRIVER_NAME);
	if(rc != 0) return -1;			// Can not allocate major+minor numbers

	// Major+minor number were allocated. Set correspondent flag
	pch -> cdev_region_alloc = 1;

	// Character device major and minor numbers were allocated successfully
	return 0;
}

/**************************** dmInitChDevCdev(pch) ****************************
* DMA-PROXY channel character device initialization:
*	Init character device data structure, add character device to the kernel
* Used variables:
*	(i)dm_cdev_fops - character device file operations structure
*	(io)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
* Return value:
*	-1 Error. Can not add character device to the kernel
*	0  Success. Character device data structure was initialized and
*					added to the kernel
*******************************************************************************/
static int dmInitChDevCdev(DM_CHAN_t *pch)
{
	struct cdev *pcdev;
	dev_t node;
	int rc;

	// Set the pointer to the character device structure (for kernel)
	pcdev = &(pch -> cdev);

	// Get character device 32-bit major+minor number
	node = pch -> cdev_node;

	// Initialize the device data structure
	cdev_init(pcdev, &dm_cdev_fops);

	// Set the owner of the character device
	pcdev -> owner = THIS_MODULE;

	// Add character device to the kernel (one device)
	rc = cdev_add(pcdev, node, 1);
	if(rc != 0) return -1;				// Can not add character device to the kernel

	// Character device was added to the kernel. Set correspondent flag
	pch -> cdev_added = 1;

	// Character device was successfully added to the kernel
	return 0;
}

/*************************** dmInitChDevCrDev(pch) ****************************
* DMA-PROXY channel character device initialization:
*	Create character device
* Used variables:
*	(i)module_parm - module parameters
*	(io)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
* Return value:
*	-1 Error. Failed to create character device
*	0  Success. Character device was created
*******************************************************************************/
static int dmInitChDevCrDev(DM_CHAN_t *pch)
{
	struct class *pclass;
	struct device *char_dev;
	dev_t node;
	uint32_t ch_idx;
	const char	*chan_name;

	// Set the pointer to the module class
	pclass = module_parm.pclass;

	// Get character device 32-bit major+minor number
	node = pch -> cdev_node;

	// Get DMA-PROXY channel index
	ch_idx = pch -> ch_idx;

	// Set the pointer to the channel name
	chan_name = dm_ch_name[ch_idx];

	// Create character device
	char_dev = device_create(pclass, NULL, node, NULL, chan_name);
	if(IS_ERR(char_dev)) return -1;			// Failed to create character device

	// Character device was created. Set correspondent flag
	pch -> cdev_created = 1;

	// Character device was created successfully
	return 0;
}

/**************************** dmCdevOpen(ino,file) ****************************
* Character device file operations:
* 	Open function for the character device
* Only one user can have access to the device. This is checked here.
* Sets up the data pointer to the DMA-PROXY channel parameters
*	(such that the ioctl function can access the parameters structure later)
* Parameters:
*	(i)ino   - opened file parameters structure
*	(o)file - opened file state structure
* Return value:
*	0 		Success. The file was opened
*	-EBUSY  Error. The file is busy. It was already opened.
*******************************************************************************/
static int dmCdevOpen(struct inode *ino, struct file *file)
{
	struct cdev	*pcdev;
	DM_CHAN_t *pch;
	uint32_t cdev_opened;
	
	// Set the pointer to the kernel character device structure (for the opened device)
	pcdev = ino -> i_cdev;

	// Set the pointer to the DMA-PROXY channel parameters (for the opened device)
	pch = container_of(pcdev, DM_CHAN_t, cdev);

	// Read the flag: character device was opened / not opened
	cdev_opened = pch -> cdev_opened;

	// If device was already opened - the file is busy
	if(cdev_opened) return -EBUSY;

	// Open the device
	pch -> cdev_opened = 1;

	// Set up the data pointer to the DMA-PROXY channel parameters structure 
	// in the opened file state structure
	file -> private_data = pch;

	// The file was opened successfully
	return 0;
}

/************************** dmCdevRelease(ino,file) ***************************
* Character device file operations:
*	Release function for the character device
* The function is called when character device is closed
* Parameters:
*	(i)ino  - opened file parameters structure
*	(o)file - opened file state structure
* Return value:
*	0 Character device file was successfully closed
*******************************************************************************/
static int dmCdevRelease(struct inode *ino, struct file *file)
{
	struct cdev	*pcdev;
	DM_CHAN_t *pch;
	uint32_t cdev_opened;

	// Set the pointer to the kernel character device structure
	pcdev = ino -> i_cdev;

	// Set the pointer to the DMA-PROXY channel parameters
	pch = container_of(pcdev, DM_CHAN_t, cdev);

	// Read the flag: character device was opened / not opened
	cdev_opened = pch -> cdev_opened;

	// If the device was opened - abort transfers on DMA channel
	if(cdev_opened) dmChTerm(pch);

	// Clear "device opened" flag	
	pch -> cdev_opened = 0;

	// Clear closed file private data pointer
	file -> private_data = NULL;
	
	// Character device file was successfully closed
	return 0;	
}

/************************* dmCdevIoctl(file,cmd,arg) **************************
* Character device file operations:
*	Ioctl call processing for the character device.
* Executes DMA channel data receive operation.
* This function can block.
* Parameters:
*	(i)file - opened file state structure
*	(i)cmd  - ioctl request code
*	(o)arg - pointer to the user space transaction result buffer
* Return value:
*	0 Success. DMA data receive transaction was executed
*	-ENOTTY Error. Bad ioctl call (incorrect request)
*	-EPERM  Error. Character device file was not opened by user
*	-EFAULT Error. Can not copy transaction result code to user
*******************************************************************************/
static long dmCdevIoctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	DM_CHAN_t *pch;

	// Check ioctl command code
	if(cmd != _DM_IOCTL_TRAN_RC)
		return -ENOTTY;					// Incorrect request command code

	// Set the pointer to the DMA-PROXY channel parameters
	pch = file -> private_data;

	// Check that the file was opened
	if(pch == NULL) return -EPERM;		// The file was not opened

	// Perform transfer on DMA channel (data receive)
	dmChTransfer(pch);

	// Copy DMA transaction result code to the user space app
	return dmChResToUser(pch,arg);
}

/**************************** dmCdevMmap(file,vma) ****************************
* Character device file operations:
* 	Map the memory for DMA operations to into user space
* Used variable:
*	(i)dm_parm - DMA-PROXY parameters
* Parameters:
*	(i)file - opened file state structure
*	(i)vma - user space virtual memory area parameters structure
*				(the area to map data to)
* Return value:
*	0  Success. The memory was mapped
*	<0 Error. Can not map the memory
*******************************************************************************/
static int dmCdevMmap(struct file *file, struct vm_area_struct *vma)
{
	DM_CHAN_t *pch;
	struct device *dev;
	uint8_t *dma_buffer;
	dma_addr_t dma_handle;
	unsigned long vm_start;
	unsigned long vm_end;
	uint32_t trsz;

	// Set the pointer to the DMA-PROXY channel parameters
	pch = file -> private_data;

	// Set the pointer to the DMA-PROXY device structure
	dev = dm_parm.dev;

	// Check that the file was opened
	if(pch == NULL) return -EPERM;		// The file was not opened

	// Set the pointer to the DMA channel allocated buffer (in the kernel space)
	dma_buffer = pch -> dma_buffer;

	// Get DMA buffer physical address (aka DMA handle)
	dma_handle = pch -> dma_buffer_phadd;

	// Read start and end addresses in the user space virtual memory
	vm_start = vma -> vm_start;
	vm_end = vma -> vm_end;

	// Calculate the size of the memory to map (b)
	trsz = vm_end - vm_start;

	// Map coherent memory used by DMA-PROXY channel from kernel to user space
	return dma_mmap_coherent(dev,vma,dma_buffer,dma_handle,trsz);
}

/***************************** dmChTransfer(pch) ******************************
* Perform transfer on DMA channel
* Starts DMA transfer
* Waits until DMA transfer is finished
* The status result of DMA transfer is stored in channel parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmChTransfer(DM_CHAN_t *pch)
{
	int rc;

	// Start transfer on DMA channel
	rc = dmChTrStart(pch);

	// Check that the transfer was started
	if(rc == 0)
		// The transfer was started
		// Wait until the transfer is finished 
		// (function stores DMA transaction result code in channel parameters)
		dmChTrWait(pch);
	else
		// Can not start DMA transfer
		// Store DMA transaction result code (for user) - error
		dmChTrStResCode(pch, _DM_TRAN_RES_ERROR);
}

/**************************** dmChTrCallBack(cmp) *****************************
* Callback function for "transfer finished" event
* Indicates that the DMA transfer is complete to another thread of control
* Parameter:
*	(o)cmp - DMA transaction complete indicator
*******************************************************************************/
static void dmChTrCallBack(void *cmp)
{
	complete(cmp);
}

/****************************** dmChTrStart(pch) ******************************
* Start transfer on DMA channel
* Inits DMA transaction
* Inits callback function for "transfer finished" event
* Submits DMA transaction to the DMA engine
* Initiates DMA transfer
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
* Return value:
*	0  Success. The transfer was started
*	-1 Error. Can not start DMA transfer
*******************************************************************************/
static int dmChTrStart(DM_CHAN_t *pch)
{
	int rc;

	// Init DMA single entry transaction
	rc = dmChTrIniSing(pch);
	if(rc < 0) return rc;				// Can not init single entry transaction

	// Init callback function
	dmChTrIniCallBack(pch);

	// Init DMA transaction complete indicator before using it
	dmChTrIniCmp(pch);

	// Submit DMA transaction to the DMA engine
	rc = dmChTrIniSubmit(pch);
	if(rc < 0) return rc;				// Can not submit DMA transaction

	// Start the DMA transaction which was submitted to the DMA engine
	dmChTrIniIssuePend(pch);

	// The transfer was started successfully
	return 0;
}

/***************************** dmChTrIniSing(pch) *****************************
* DMA transfer initialization:
*	Init DMA single entry transaction
* Used variable:
*	(i)dm_ch_trsz - DMA channel transaction sizes array
* Parameter:
*	(io)pch - pointer to the DMA-PROXY channel parameters structure
* Return value:
*	0  Success. DMA transaction was initialized
*	-1 Error. Can not init single entry transaction
*******************************************************************************/
static int dmChTrIniSing(DM_CHAN_t *pch)
{
	struct dma_chan *dma_chan;
	dma_addr_t dma_handle;
	uint32_t ch_idx;
	uint32_t trsz;
	struct dma_async_tx_descriptor *tran_desc;

	// Get the pointer to the allocated DMA channel
	dma_chan = pch -> dma_chan;

	// Get DMA buffer physical address (aka DMA handle)
	dma_handle = pch -> dma_buffer_phadd;

	// Get DMA-PROXY channel index
	ch_idx = pch -> ch_idx;

	// Get the size of the transactin (b)
	trsz = dm_ch_trsz[ch_idx];

	// Init DMA single entry transaction
	tran_desc = dmaengine_prep_slave_single(
		dma_chan,dma_handle,trsz,DMA_DEV_TO_MEM,
		DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
	if(tran_desc == NULL) return -1;		// Can not init single entry transaction

	// Set the pointer to the async DMA transaction descriptor
	pch -> tran_desc = tran_desc;

	// DMA transaction was initialized successfully
	return 0;
}

/*************************** dmChTrIniCallBack(pch) ***************************
* DMA transfer initialization:
*	Init callback function for "transfer finished" event
* Parameter:
*	(io)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmChTrIniCallBack(DM_CHAN_t *pch)
{
	struct dma_async_tx_descriptor *tran_desc;
	struct completion *pcmp;

	// Get the pointer to the async DMA transaction descriptor
	tran_desc = pch -> tran_desc;

	// Get the pointer to DMA transaction complete indicator
	pcmp = &(pch -> cmp);

	// Set the routine to call after the operation is complete
	tran_desc -> callback = dmChTrCallBack;

	// Callback function parameter points to DMA transaction complete indicator
	tran_desc -> callback_param = pcmp;
}

/***************************** dmChTrIniCmp(pch) ******************************
* DMA transfer initialization:
*	Init DMA transaction complete indicator
* Parameter:
*	(io)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmChTrIniCmp(DM_CHAN_t *pch)
{
	struct completion *pcmp;

	// Set the pointer to DMA transaction complete indicator
	pcmp = &(pch -> cmp);

	// Initialize the completion structure
	init_completion(pcmp);
}

/**************************** dmChTrIniSubmit(pch) ****************************
* DMA transfer initialization:
*	Submit DMA transaction to the DMA engine
* Parameter:
*	(io)pch - pointer to the DMA-PROXY channel parameters structure
* Return value:
*	0  Success. DMA transaction was submitted to the DMA engine
*	-1 Error. Can not submit DMA transaction to the DMA engine
*******************************************************************************/
static int dmChTrIniSubmit(DM_CHAN_t *pch)
{
	struct dma_async_tx_descriptor *tran_desc;
	dma_cookie_t cookie;

	// Get the pointer to the async DMA transaction descriptor
	tran_desc = pch -> tran_desc;

	// Submit the transaction to the DMA engine
	cookie = dmaengine_submit(tran_desc);
	if(dma_submit_error(cookie))
		return -1;					// Can not submit DMA transaction to the DMA engine

	// Store the cookie to track the status of this transaction
	pch -> cookie = cookie;

	// DMA transaction was successfuly submitted to the DMA engine
	return 0;
}

/************************** dmChTrIniIssuePend(pch) ***************************
* DMA transfer initialization:
*	Start the DMA transaction which was submitted to the DMA engine
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmChTrIniIssuePend(DM_CHAN_t *pch)
{
	struct dma_chan *dma_chan;
	
	// Set the pointer to the DMA Engine channel parameters
	dma_chan = pch -> dma_chan;

	// Start the DMA transaction
	dma_async_issue_pending(dma_chan);
}

/****************************** dmChTrWait(pch) *******************************
* Wait until DMA transfer is finished
* The status result of DMA transfer is stored in channel parameters
* This function can block
* Parameter:
*	(io)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmChTrWait(DM_CHAN_t *pch)
{
	enum dma_status status;

	// Wait for the DMA transaction to complete, or error
	dmChTrWaitCmp(pch);

	// Get the status of the DMA transaction
	status = dmChTrWaitGetStat(pch);

	// Set DMA transaction result code (for user)
	dmChTrWaitRes(pch, status);
}

/***************************** dmChTrWaitCmp(pch) *****************************
* Wait for the DMA transaction to complete or error
* This function can block
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmChTrWaitCmp(DM_CHAN_t *pch)
{
	struct completion *pcmp;

	// Set the pointer to DMA transaction complete indicator
	pcmp = &(pch -> cmp);

	// Wait for the transaction to complete or error
	wait_for_completion(pcmp);
}

/*************************** dmChTrWaitGetStat(pch) ***************************
* Get the status of the DMA transaction
* The function requests status from DMA Engine
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
* Return value:
*	DMA transaction status
*******************************************************************************/
static enum dma_status dmChTrWaitGetStat(DM_CHAN_t *pch)
{
	struct dma_chan *dma_chan;
	dma_cookie_t cookie;

	// Set the pointer to the DMA Engine channel parameters
	dma_chan = pch -> dma_chan;

	// Get the cookie to track the status of DMA transaction
	cookie = pch -> cookie;

	// Get DMA transaction status
	return dma_async_is_tx_complete(dma_chan, cookie, NULL, NULL);
}

/************************* dmChTrWaitRes(pch,status) **************************
* Set DMA transaction result code - for user
* The function is called when DMA transaction was completed
* Parameters:
*	(o)pch - pointer to the DMA-PROXY channel parameters structure
*	(i)status - DMA transaction status (from DMA engine)
*******************************************************************************/
static void dmChTrWaitRes(DM_CHAN_t *pch, enum dma_status status)
{
	uint32_t res_code;

	// Make DMA transaction result code 
	if(status != DMA_COMPLETE)
		res_code = _DM_TRAN_RES_ERROR;		// DMA transaction error
	else
		res_code = _DM_TRAN_RES_SUCCESS;	// Transaction was executed successfully

	// Store DMA transaction result code
	dmChTrStResCode(pch,res_code);
}

/*********************** dmChTrStResCode(pch,res_code) ************************
* Store DMA transaction result code in channel parameters structure
* Parameters:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*	(i)res_code - result code to store
*******************************************************************************/
static void dmChTrStResCode(DM_CHAN_t *pch, uint32_t res_code)
{
	pch -> res_code = res_code;
}

/*************************** dmChResToUser(pch,arg) ***************************
* Copy DMA transaction result code to the user space app
* Parameters:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*	(o)arg - pointer to the user space transaction result buffer
* Return value:
*	0 Success. DMA transaction result code was copied to user space
*	-EFAULT Error. Can not copy transaction result code to user
*******************************************************************************/
static int dmChResToUser(DM_CHAN_t *pch, unsigned long arg)
{
	uint32_t res_code;
	unsigned long error_count;

	// Get DMA transaction result code
	res_code = pch -> res_code;

	// Copy result code to user
	error_count = copy_to_user((void *)arg, &res_code, sizeof(uint32_t));
	if(error_count != 0)
		return -EFAULT;	// Failed to copy data to user

	// DMA transaction result code was successfully copied to user space
	return 0;
}

/******************************* dmChTerm(pch) ********************************
* Abort current transfers on DMA channel
* If the channel was not allocated, no activity is performed
* Used variable:
*	(i)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmChTerm(DM_CHAN_t *pch)
{
	struct dma_chan *dma_chan;

	// Set the pointer to the DMA Engine channel parameters
	dma_chan = pch -> dma_chan;

	// Abort current transfers on the channel
	if(dma_chan != NULL)
		dma_chan -> device -> device_terminate_all(dma_chan);
}

/******************************** dmFreeAll() *********************************
* Free all resources associated with DMA-PROXY
* The function is called from DMA-PROXY remove function
* It is also called from DMA-PROXY probe function in case of errors
* Used variable:
*	(io)dm_parm - DMA-PROXY parameters
*******************************************************************************/
static void dmFreeAll(void)
{
	uint32_t ch_idx;
	DM_CHAN_t *pch;

	// Free channel resources cycle
	for(ch_idx = 0; ch_idx < _DM_CH_NUM; ch_idx++) {
		// Set the pointer to the DMA-PROXY channel parameters
		pch = &dm_parm.ch[ch_idx];

		// Free resources of the current channel
		dmFreeCh(pch);
	}
}

/******************************* dmFreeCh(pch) ********************************
* Free all resources associated with one DMA channel
* Used variable:
*	(io)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmFreeCh(DM_CHAN_t *pch)
{
	// Abort current transfers on DMA channel
	dmChTerm(pch);

	// Free all resources associated with character device
	dmFreeChDev(pch);

	// Free the memory allocated for DMA operations
	dmFreeChMem(pch);

	// Release allocated DMA channel
	dmFreeChRelease(pch);
}

/****************************** dmFreeChDev(pch) ******************************
* Free all resources associated with character device
* Used variables:
*	(i)module_parm - module parameters
*	(io)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmFreeChDev(DM_CHAN_t *pch)
{
	// Destroy character device
	dmFreeChDevDest(pch);

	// Remove character device from kernel
	dmFreeChDevCdev(pch);

	// Unregister character device region
	dmFreeChDevRegion(pch);
}

/**************************** dmFreeChDevDest(pch) ****************************
* Destroy character device
* Used variables:
*	(i)module_parm - module parameters
*	(io)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmFreeChDevDest(DM_CHAN_t *pch)
{
	struct class *pclass;
	dev_t node;
	uint32_t cdev_created;

	// Set the pointer to the module class
	pclass = module_parm.pclass;

	// Get character device 32-bit major+minor number
	node = pch -> cdev_node;

	// Read the flag: character device was created / not created
	cdev_created = pch -> cdev_created;

	// Destroy created character device
	if(cdev_created) device_destroy(pclass, node);

	// Character device was destroyed. Clear correspondent flag
	pch -> cdev_created = 0;
}

/**************************** dmFreeChDevCdev(pch) ****************************
* Remove character device from kernel
* Used variable:
*	(io)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmFreeChDevCdev(DM_CHAN_t *pch)
{
	uint32_t cdev_added;
	struct cdev *pcdev;
	
	// Read the flag: character device was added / not added to the kernel
	cdev_added = pch -> cdev_added;

	// Set the pointer to the kernel character device structure
	pcdev = &(pch -> cdev);

	// Remove character device from kernel
	if(cdev_added) cdev_del(pcdev);

	// Character device was removed. Clear correspondent flag
	pch -> cdev_added = 0;
}

/*************************** dmFreeChDevRegion(pch) ***************************
* Unregister character device region
* (Free allocated character device major and minor numbers)
* Used variable:
*	(io)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmFreeChDevRegion(DM_CHAN_t *pch)
{
	uint32_t cdev_region_alloc;
	dev_t node;

	// Read the flag: character device major+minor numbers allocated / not allocated
	cdev_region_alloc = pch -> cdev_region_alloc;

	// Get character device 32-bit major+minor number
	node = pch -> cdev_node;

	// Unregister character device region for one character device
	if(cdev_region_alloc) unregister_chrdev_region(node, 1);

	// Char device region was unregistered. Clear correspondent flag
	pch -> cdev_region_alloc = 0;
}

/****************************** dmFreeChMem(pch) ******************************
* Free the memory allocated for DMA operations
* Used variable:
*	(io)dm_parm - DMA-PROXY parameters
*	(i)dm_ch_trsz - DMA channel transaction sizes array
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmFreeChMem(DM_CHAN_t *pch)
{
	struct device *dev;
	uint32_t ch_idx;
	uint32_t trsz;
	dma_addr_t dma_handle;
	uint8_t *dma_buffer;

	// Set the pointer to the DMA-PROXY device structure
	dev = dm_parm.dev;

	// Get DMA-PROXY channel index
	ch_idx = pch -> ch_idx;

	// Get the size of the allocated memory (b)
	trsz = dm_ch_trsz[ch_idx];

	// Get DMA buffer physical address (aka DMA handle)
	dma_handle = pch -> dma_buffer_phadd;

	// Set the pointer to the allocated buffer
	dma_buffer = pch -> dma_buffer;

	// Free allocated coherent memory 
	if(dma_buffer != NULL)
		dmam_free_coherent(dev,trsz,dma_buffer, dma_handle);

	// Clear DMA buffer memory pointer
	pch -> dma_buffer = NULL;
}

/**************************** dmFreeChRelease(pch) ****************************
* Release allocated DMA channel
* Used variable:
*	(io)dm_parm - DMA-PROXY parameters
* Parameter:
*	(i)pch - pointer to the DMA-PROXY channel parameters structure
*******************************************************************************/
static void dmFreeChRelease(DM_CHAN_t *pch)
{
	struct dma_chan *dma_chan;

	// Set the pointer to the allocated DMA channel
	dma_chan = pch -> dma_chan;

	// Release the channel
	if(dma_chan != NULL)
		dma_release_channel(dma_chan);

	// Clear DMA Engine channel parameters pointer
	pch -> dma_chan = NULL;
}

/******************************************************************************
* A module must use the "module_init" "module_exit" macros from linux/init.h,
*	which identify the initialization function at insertion time, 
*	the cleanup function for the module removal
*******************************************************************************/
module_init(moduleInit);
module_exit(moduleExit);

