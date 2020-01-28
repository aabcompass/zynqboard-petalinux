/*================================ ZYNQBOARD ==================================
*	PROJECT:	ZYNQ3 v1:	 "ZynqBoard software (Xilinx Zynq-7000, Linux) "
*	FILE:		dataprov-mod.c
*	CONTENTS:	Kernel module. DATA-PROVIDER IP Core driver.
*					provides ioctl interface for the user application
*	VERSION:	01.01  11.12.2019
*	AUTHOR:		Andrey Poroshin
*	UPDATES :
*	1) 01.01   11 December 2019 - Initial version
 ============================================================================== */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#include "scurve-adder-mod-intf.h"

// Standard module information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Poroshin Andrey");
MODULE_DESCRIPTION("scurve-adder-mod - scurve adder user access module");

/******************************************************************************
*	Internal definitions
*******************************************************************************/
// This module name
#define DRIVER_NAME	"scurve-adder-mod"

// Created class name
#define CLASS_NAME	"scurve-adder-cls"

// Created character device name
#define	CDEV_NAME	"scurve-adder-dev"

/******************************************************************************
*	Internal structures
*******************************************************************************/
// Module parameters structure
typedef struct MODULE_PARM_s {
	uint8_t plat_drv_registered;	// Flag: platform driver was registered (1)
	uint8_t dev_found;				// Flag: device (DATA-PROVIDER IP core) was found (1)
	struct class *pclass;			// Pointer to the created class
} MODULE_PARM_t;

// DATA-PROVIDER parameters structure
typedef struct DP_PARM_s {
	// DATA-PROVIDER platform device support
	uint8_t io_base_mapped;			// Flag: base address mapped to the device (1)
	uint8_t io_mem_allocated;		// Flag: device IO memory allocated (1)
	unsigned long mem_start;		// IO memory start address
	unsigned long mem_end;			// IO memory end address
	uint32_t __iomem *base_addr;	// Device base address

	// Character device support
	uint8_t cdev_region_alloc;		// Flag: character device major+minor numbers allocated (1)
	uint8_t cdev_added;				// Flag: character device was added to the kernel (1)
	uint8_t cdev_created;			// Flag: character device was created (1)
	uint8_t cdev_opened;			// Flag: character device was opened (1)
	dev_t cdev_node;				// 32-bit value, contains major and minor numbers
	struct cdev cdev;				// Kernel character device structure
} DP_PARM_t;

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
static int dpProbe(struct platform_device *pdev);
static int dpProbeDevFound(void);
static void dpInitParm(void);
static int dpRemove(struct platform_device *pdev);
static int dpPlatInit(struct platform_device *pdev);
static int dpPlatInitGetIOMem(struct platform_device *pdev);
static int dpPlatInitAllocIO(struct device *dev);
static int dpPlatInitAllocMem(struct device *dev);
static int dpPlatInitAllocBase(struct device *dev);
static uint32_t dpPlatRegRd(uint32_t regw);
static void dpPlatRegWr(uint32_t val, uint32_t regw);
static void dpPlatFreeAll(void);
static void dpPlatFreeBaseUnmap(void);
static void dpPlatFreeReleaseMem(void);
static int dpCdevInit(void);
static int dpCdevInitRegion(void);
static int dpCdevInitCdev(void);
static int dpCdevInitCrDev(void);
static int dpCdevOpen(struct inode *ino, struct file *file);
static long dpCdevIoctl(struct file *file, unsigned int cmd, unsigned long arg);
static int dpCdevIoctlChk(unsigned int cmd, unsigned long arg, _DATAPROV_REG_t *reg);
static int dpCdevIoctlRd(unsigned int cmd, unsigned long arg, _DATAPROV_REG_t *reg);
static int dpCdevIoctlWr(unsigned int cmd, unsigned long arg, _DATAPROV_REG_t *reg);
static int dpCdevRelease(struct inode *ino, struct file *file);
static void dpCdevFreeAll(void);
static void dpCdevFreeDestDev(void);
static void dpCdevFreeDelDev(void);
static void dpCdevFreeUnReg(void);
static void dpFreeAll(void);

/******************************************************************************
*	Internal data
*******************************************************************************/
// Module parameters
static MODULE_PARM_t module_parm;

// DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
static DP_PARM_t dp_parm;

// List of platform driver compatible devices
static struct of_device_id plat_of_match[] = {
	{ .compatible = "xlnx,scurve-adder36-1.0", },
	{ /* end of list */ },
};
MODULE_DEVICE_TABLE(of, plat_of_match);		// Make the list global for the kernel

// DATA-PROVIDER platform driver structure
static struct platform_driver plat_drv = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= plat_of_match,
	},
	.probe  = dpProbe,
	.remove = dpRemove,
};

// Character device file operations
static struct file_operations dp_cdev_fops = {
	.owner = THIS_MODULE,
	.open = dpCdevOpen,
	.release = dpCdevRelease,
	.unlocked_ioctl = dpCdevIoctl
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

	// Register platform driver for DATA-PROVIDER IP core
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
		printk(KERN_INFO "dataprov-mod: failed to register class \n");

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
* Register platform driver for DATA-PROVIDER IP core
* Used variables:
*	(i)plat_drv - DATA-PROVIDER platform driver structure
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
	// Unregister platform driver for DATA-PROVIDER IP core
	moduleUnreg();

	// Destroy registered class
	moduleDestrCls();
}

/******************************* moduleUnreg() ********************************
* Unregister platform driver for DATA-PROVIDER IP core
* The driver is unregistered only if it was registered previously
* Used variables:
*	(i)plat_drv - DATA-PROVIDER platform driver structure
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

/******************************* dpProbe(pdev) ********************************
* DATA-PROVIDER device probe function.
* The function is called when compatible with this driver platform device
*	(DATA-PROVIDER IP) was found
* Only one DATA-PROVIDER IP core is supported
* Allocates resources for the device
* Creates character device in /dev folder for user ioctl requests
* Parameter:
*	(i)pdev - structure of the platform device to initialize
* Return value:
*	0  - Device was initialized successfully
*	<0 - Error code
*******************************************************************************/
static int dpProbe(struct platform_device *pdev)
{
	int rc;

	printk(KERN_INFO "Poroshin: dpProbe START \n");

	// Check that only one DATA-PROVIDER IP core was found
	rc = dpProbeDevFound();
	if(rc != 0) return -1;			// Error: Only one DATA-PROVIDER IP core is supported

	// Init local DATA-PROVIDER parameters
	dpInitParm();

	// Init DATA-PROVIDER platform device
	rc = dpPlatInit(pdev);
	if(rc != 0)	goto DP_PROBE_FAILED;

	// Create character device in /dev folder for user ioctl requests
	rc = dpCdevInit();
	if(rc != 0)	goto DP_PROBE_FAILED;
	
	// Device was initialized successfully
	return 0;

DP_PROBE_FAILED:
	// Free all resources associated with DATA-PROVIDER
	dpFreeAll();

	// Return error code
	return rc;
}

/***************************** dpProbeDevFound() ******************************
* DATA-PROVIDER initialization: check that only one DATA-PROVIDER IP core 
*	was found
* Used variable:
*	(io)module_parm - module parameters
* Return value:
*	0  - Success. First DATA-PROVIDER IP core was found
*	-1 - Error. Only one DATA-PROVIDER IP core is supported. 
*******************************************************************************/
static int dpProbeDevFound(void)
{
	uint32_t dev_found;

	// Read the flag: device (DATA-PROVIDER IP core) was found (1), not found (0)
	dev_found = module_parm.dev_found;

	// Check if the device is not the first one
	if(dev_found) return -1;	// Error. Only one DATA-PROVIDER IP core is supported.

	// Set the flag: device (DATA-PROVIDER IP core) was found
	module_parm.dev_found = 1;
	
	// Success. First DATA-PROVIDER IP core was found
	return 0;
}

/******************************** dpInitParm() ********************************
* DATA-PROVIDER initialization: init local DATA-PROVIDER parameters
* (This function must be called before all DATA-PROVIDER initializations)
* Used variable:
*	(o)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
*******************************************************************************/
static void dpInitParm(void)
{
	// Clear all flags in the parameters structure
	dp_parm.io_base_mapped = 0;
	dp_parm.io_mem_allocated = 0;
	dp_parm.cdev_region_alloc = 0;
	dp_parm.cdev_added = 0;
	dp_parm.cdev_created = 0;
	dp_parm.cdev_opened = 0;
}

/******************************* dpRemove(pdev) *******************************
* Platform device - DATA-PROVIDER remove function.
* The function is called when compatible platform device (DATA-PROVIDER IP)
*	was removed (or the driver module was removed from kernel)
* Parameter:
*	(i)pdev - structure of the platform device to remove
* Return value:
*	0  - The device was removed successfully
*******************************************************************************/
static int dpRemove(struct platform_device *pdev)
{
	printk(KERN_INFO "Poroshin: dpRemove EXECUTED \n");

	// Free all resources associated with DATA-PROVIDER
	dpFreeAll();

	// The device was removed successfully
	return 0;
}

/****************************** dpPlatInit(pdev) ******************************
* Platform device - DATA-PROVIDER initialization function
* Allocates resources for the device
* Parameter:
*	(io)pdev - structure of the platform device to initialize
* Return value:
*	0  - Platform device was initialized successfully
*	<0 - Error code
*******************************************************************************/
static int dpPlatInit(struct platform_device *pdev)
{
	int rc;
	struct device *dev;

	// Get device structure pointer for the platform device
	dev = &pdev->dev;

	// Init platform device io memory parameters
	rc = dpPlatInitGetIOMem(pdev);
	if(rc != 0) return rc;				// Can not get device io memory resourses

	// Allocate device IO space resources
	return dpPlatInitAllocIO(dev);
}

/************************** dpPlatInitGetIOMem(pdev) **************************
* Initialization of DATA-PROVIDER:
*	Init platform device io memory parameters
* Used variable:
*	(o)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
* Parameter:
*	(i)pdev - platform device structure
* Return value:
*	0  - Success. The parameters were initialized
*	-ENODEV - Error. Can not get device io memory resourses
*******************************************************************************/
static int dpPlatInitGetIOMem(struct platform_device *pdev)
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
	dp_parm.mem_start = r_mem -> start;
	dp_parm.mem_end = r_mem -> end;

	// Parameters were initialized successfully
	return 0;
}

/*************************** dpPlatInitAllocIO(dev) ***************************
* Initialization of DATA-PROVIDER:
*	Allocate device IO memory resources, set base address pointer
* Used variable:
*	(io)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
* Parameter:
*	(i)dev - pointer to the device structure of the device
* Return value:
*	0  - Success. Device IO resourses were allocated
* 	-EBUSY - Error. Can not allocate memory region
*	-EIO   - Error. Can not init device base address
*******************************************************************************/
static int dpPlatInitAllocIO(struct device *dev)
{
	int rc;

	// Allocate device IO memory resources
	rc = dpPlatInitAllocMem(dev);
	if(rc != 0) return rc;				// Can not lock memory region

	// Set device base address pointer
	return dpPlatInitAllocBase(dev);
}

/************************** dpPlatInitAllocMem(dev) ***************************
* Initialization of DATA-PROVIDER:
*	Allocate device IO memory resources
* Used variable:
*	(io)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
* Parameter:
*	(i)dev - pointer to the device structure of the device
* Return value:
*	0  - Success. Device IO memory resourses were allocated
* 	-EBUSY - Error. Can not allocate memory region
*******************************************************************************/
static int dpPlatInitAllocMem(struct device *dev)
{
	unsigned long mem_start;
	unsigned long mem_end;
	unsigned long device_iomem_size;

	// Read device IO memory start/end addresses
	mem_start = dp_parm.mem_start;
	mem_end = dp_parm.mem_end;

	// Calculate device IO memory size to allocate
	device_iomem_size = mem_end - mem_start + 1;

	// Allocate device IO memory resources
	if (!request_mem_region(mem_start, device_iomem_size, DRIVER_NAME)) {
		dev_err(dev, "Can not lock memory region at %p\n", (void *)mem_start);
		return -EBUSY;
	}

	// Set flag: device IO memory allocated
	dp_parm.io_mem_allocated = 1;

	// Device IO memory resourses were allocated
	return 0;	
}

/************************** dpPlatInitAllocBase(dev) **************************
* Initialization of DATA-PROVIDER:
*	Set device base address pointer
* Used variable:
*	(io)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
* Parameter:
*	(i)dev - pointer to the device structure of the device
* Return value:
*	0  - Success. Device base address pointer was initialized
*	-EIO - Error. Can not init device base address pointer
*******************************************************************************/
static int dpPlatInitAllocBase(struct device *dev)
{
	unsigned long mem_start;
	unsigned long mem_end;
	uint32_t __iomem *base_addr;
	unsigned long device_iomem_size;

	// Read device IO memory start/end addresses
	mem_start = dp_parm.mem_start;
	mem_end = dp_parm.mem_end;

	// Calculate device IO memory size
	device_iomem_size = mem_end - mem_start + 1;

	// Init device IO memory pointer
	base_addr = (uint32_t __iomem *)ioremap(mem_start, device_iomem_size);
	if(! base_addr)  {
		dev_err(dev, "Can not init device base address \n");
		return -EIO;
	}

	// Store base address in the device parameters structure
	dp_parm.base_addr = base_addr;
	
	printk(KERN_INFO "Poroshin: dpPlatInitAllocBase base_addr=%.8x \n", (uint32_t)base_addr);

	// Set flag: base address mapped to the device
	dp_parm.io_base_mapped = 1;

	// Device base address pointer was initialized successfully
	return 0;	
}

/***************************** dpPlatRegRd(regw) ******************************
* Read 32-bit register value from the DATA-PROVIDER IP core.
* Used variable:
*	(i)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
* Parameter:
*	(i)regw - register number (not checked here, must be valid)
* Return value:
*	32-bit register value
*******************************************************************************/
static uint32_t dpPlatRegRd(uint32_t regw)
{
	uint32_t __iomem *base_addr;

	// Read device base address
	base_addr = dp_parm.base_addr;

	// Return 32-bit register value
	return ioread32(&base_addr[regw]);	
}

/*************************** dpPlatRegWr(val,regw) ****************************
* Write 32-bit register value to the DATA-PROVIDER IP core.
* Used variable:
*	(i)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
* Parameters:
*	(i)val  - value to write
*	(i)regw - register number (not checked here, must be valid)
*******************************************************************************/
static void dpPlatRegWr(uint32_t val, uint32_t regw)
{
	uint32_t __iomem *base_addr;

	// Read device base address
	base_addr = dp_parm.base_addr;

	// Write 32-bit register value
	iowrite32(val,&base_addr[regw]);
}

/****************************** dpPlatFreeAll() *******************************
* Free all resources allocated for the DATA-PROVIDER platform device
* Used variable:
*	(i)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
*******************************************************************************/
static void dpPlatFreeAll(void)
{
	// Unmap device base address
	dpPlatFreeBaseUnmap();

	// Release allocated device IO memory
	dpPlatFreeReleaseMem();
}

/*************************** dpPlatFreeBaseUnmap() ****************************
* Unmap device base address
* Used variable:
*	(i)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
*******************************************************************************/
static void dpPlatFreeBaseUnmap(void)
{
	uint32_t io_base_mapped;
	uint32_t __iomem *base_addr;

	// Read device base address and "base address mapped" flag
	io_base_mapped = dp_parm.io_base_mapped;
	base_addr = dp_parm.base_addr;

	// Unmap device base address if needed
	if(io_base_mapped) iounmap(base_addr);
}

/*************************** dpPlatFreeReleaseMem() ***************************
* Release allocated device IO memory
* Used variable:
*	(i)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
*******************************************************************************/
static void dpPlatFreeReleaseMem(void)
{
	uint32_t io_mem_allocated;
	unsigned long mem_start;
	unsigned long mem_end;
	unsigned long device_iomem_size;

	// Read platform device IO memory parameters
	io_mem_allocated = dp_parm.io_mem_allocated;
	mem_start = dp_parm.mem_start;
	mem_end = dp_parm.mem_end;

	// Calculate device IO memory size to release;
	device_iomem_size = mem_end - mem_start + 1;

	// Release allocated device IO memory
	if(io_mem_allocated) release_mem_region(mem_start, device_iomem_size);
}

/******************************** dpCdevInit() ********************************
* Create character device in /dev folder for user ioctl requests
* Allocates character device major and minor numbers
* Inits character device data structure
* Creates character device
* Return value:
*	-1 Error. Character device was not created
*	0  Success. Character device was created
*******************************************************************************/
static int dpCdevInit(void)
{
	int rc;

	// Allocate character device major and minor numbers
	rc = dpCdevInitRegion();
	if(rc < 0) return rc;				// Can not allocate major+minor numbers

	// Init character device data structure, add character device to the kernel
	rc = dpCdevInitCdev();
	if(rc < 0) return rc;				// Can not add character device to the kernel

	// Create character device
	return dpCdevInitCrDev();
}

/***************************** dpCdevInitRegion() *****************************
* Initialization of character device:
* Allocate character device major and minor numbers
* Used variable:
*	(o)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
* Return value:
*	-1 Error. Can not allocate major+minor numbers
*	0  Success. Character device major and minor numbers were allocated
*******************************************************************************/
static int dpCdevInitRegion(void)
{
	dev_t *pnode;
	int rc;

	// Create the pointer to the 32-bit major+minor number
	pnode = &dp_parm.cdev_node;

	// Allocate major number and one minor number for the device
	rc = alloc_chrdev_region(pnode, 0, 1, DRIVER_NAME);
	if(rc != 0) return -1;			// Can not allocate major+minor numbers

	// Major+minor number was allocated. Set correspondent flag
	dp_parm.cdev_region_alloc = 1;

	// Character device major and minor numbers were allocated successfully
	return 0;
}

/****************************** dpCdevInitCdev() ******************************
* Initialization of character device:
* Init character device data structure, add character device to the kernel
* Used variables:
*	(i)dp_cdev_fops - character device file operations structure
*	(io)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
* Return value:
*	-1 Error. Can not add character device to the kernel
*	0  Success. Character device data structure was initialized and
*					added to the kernel
*******************************************************************************/
static int dpCdevInitCdev(void)
{
	struct cdev *pcdev;
	dev_t node;
	int rc;

	// Set the pointer to the character device structure (for kernel)
	pcdev = &dp_parm.cdev;

	// Get character device 32-bit major+minor number
	node = dp_parm.cdev_node;

	// Initialize the device data structure
	cdev_init(pcdev, &dp_cdev_fops);

	// Set the owner of the character device
	pcdev -> owner = THIS_MODULE;

	// Add character device to the kernel (one device)
	rc = cdev_add(pcdev, node, 1);
	if(rc != 0) return -1;				// Can not add character device to the kernel

	// Character device was added to the kernel. Set correspondent flag
	dp_parm.cdev_added = 1;

	// Character device was successfully added to the kernel
	return 0;
}

/***************************** dpCdevInitCrDev() ******************************
* Initialization of character device:
* Create character device
* Used variables:
*	(i)module_parm - module parameters
*	(io)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
* Return value:
*	-1 Error. Failed to create character device
*	0  Success. Character device was created
*******************************************************************************/
static int dpCdevInitCrDev(void)
{
	struct class *pclass;
	struct device *char_dev;
	dev_t node;

	// Set the pointer to the module class
	pclass = module_parm.pclass;

	// Get character device 32-bit major+minor number
	node = dp_parm.cdev_node;

	// Create character device
	char_dev = device_create(pclass, NULL, node, NULL, CDEV_NAME);
	if(IS_ERR(char_dev)) return -1;			// Failed to create character device

	// Character device was created. Set correspondent flag
	dp_parm.cdev_created = 1;

	// Character device was created successfully
	return 0;
}

/**************************** dpCdevOpen(ino,file) ****************************
* Open function for the character device
* Only one user can have access to the device. This is checked here.
* Used variable:
*	(io)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
* Parameters:
*	(i)ino   - opened file parameters structure (not used)
*	(i)file - opened file state structure (not used)
* Return value:
*	0 		Success. The file was opened
*	-EBUSY  Error. The file is busy. It was already opened.
*******************************************************************************/
static int dpCdevOpen(struct inode *ino, struct file *file)
{
	uint32_t cdev_opened;

	// Read the flag: character device was opened / not opened
	cdev_opened = dp_parm.cdev_opened;

	// If device was already opened - the file is busy
	if(cdev_opened) return -EBUSY;

	// Open the device
	dp_parm.cdev_opened = 1;

	// The file was opened successfully
	return 0;
}

/************************* dpCdevIoctl(file,cmd,arg) **************************
* Ioctl call processing for the character device.
* Provides DATA-PROVIDER register read/write interface for the user application
* Used variable:
*	(i)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
* Parameters:
*	(i)file - opened file state structure (not used)
*	(i)cmd  - ioctl request code
*	(io)arg - pointer to the user space buffer for data read/write
* Return value:
*	0 Success. Read/write operation was executed
*	-ENOTTY Error. Bad ioctl call (incorrect request)
*	-ENXIO Error. Register address is out of range
*	-EFAULT Error. Can not copy the data to/from user
*******************************************************************************/
static long dpCdevIoctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc;
	_DATAPROV_REG_t reg;

	// Check ioctl call type
	if(_IOC_TYPE(cmd) != _DATAPROV_IOC_MAGIC) 
		return -ENOTTY;					// Incorrect request code

	// Read and check ioctl request data
	rc = dpCdevIoctlChk(cmd,arg,&reg);
	if(rc != 0) return rc;				// Bad request or can not copy data

	// Execute the command according to the request code
	switch(cmd) {
		case _DATAPROV_IOCTL_REG_RD:
			// Execute "read register" user application request
			return dpCdevIoctlRd(cmd,arg,&reg);

		case _DATAPROV_IOCTL_REG_WR:
			// Execute "write register" user application request
			return dpCdevIoctlWr(cmd,arg,&reg);
	}

	// Incorrect request code
	return -ENOTTY;
}

/************************ dpCdevIoctlChk(cmd,arg,reg) *************************
* Read and check ioctl request data
* Parameters:
*	(i)cmd - ioctl request code
*	(i)arg - pointer to the user space buffer
*	(o)reg - the structure with DATA-PROVIDER register value
* Return value:
*	0 Success. Request data was read
*	-EFAULT Error. Can not copy the data from user space
*	-ENXIO Error. Register address is out of range
*******************************************************************************/
static int dpCdevIoctlChk(unsigned int cmd, unsigned long arg, _DATAPROV_REG_t *reg)
{
	int rc;
	uint32_t regw;

	// Copy the data from user space
	rc = copy_from_user(reg,(void*)arg,_IOC_SIZE(cmd));
	if(rc != 0) return -EFAULT;			// Can not copy the data from user space

	// Read register number
	regw = reg -> regw;

	// Check register number
	if(regw >= _DATAPROV_REGS_NUM)
		return -ENXIO;					// Register address is out of range
	
	// Request data was read successfully
	return 0;
}

/************************* dpCdevIoctlRd(cmd,arg,reg) *************************
* Execute "read register" user application request
* Parameters:
*	(i)cmd - ioctl request code
*	(i)arg - pointer to the user space buffer
*	(io)reg - the structure with DATA-PROVIDER register value
* Return value:
*	0 Success. Register value was transmitted to user app
*	-EFAULT Error. Can not copy the data to user space
*******************************************************************************/
static int dpCdevIoctlRd(unsigned int cmd, unsigned long arg, _DATAPROV_REG_t *reg)
{
	int rc;
	uint32_t regw;

	// Read register number
	regw = reg -> regw;

	// Read 32-bit register value
	reg -> val = dpPlatRegRd(regw);

	// Copy the data to user space
	rc = copy_to_user((void*)arg,reg,_IOC_SIZE(cmd));
	if(rc != 0) return -EFAULT;			// Can not copy the data to user space

	// Register value was transmitted to user app successfully
	return 0;
}

/************************* dpCdevIoctlWr(cmd,arg,reg) *************************
* Execute "write register" user application request
* Parameters:
*	(i)cmd - ioctl request code (not used)
*	(i)arg - pointer to the user space buffer (not used)
*	(i)reg - the structure with DATA-PROVIDER register value
* Return value:
*	0 Success. The register was written
*******************************************************************************/
static int dpCdevIoctlWr(unsigned int cmd, unsigned long arg, _DATAPROV_REG_t *reg)
{
	uint32_t regw;
	uint32_t val;

	// Read register number and register value
	regw = reg -> regw;
	val = reg -> val;

	// Write 32-bit register value
	dpPlatRegWr(val,regw);

	// The register was written successfully
	return 0;
}

/************************** dpCdevRelease(ino,file) ***************************
* Release function for the character device
* The function is called when character device is closed
* Used variable:
*	(o)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
* Parameters:
*	(i)ino   - opened file parameters structure (not used)
*	(i)file - opened file state structure (not used)
* Return value:
*	0 Character device file was successfully closed
*******************************************************************************/
static int dpCdevRelease(struct inode *ino, struct file *file)
{
	// Clear "device opened" flag
	dp_parm.cdev_opened = 0;

	// Character device file was successfully closed
	return 0;
}

/****************************** dpCdevFreeAll() *******************************
* Free all resources associated with character device
* Used variables:
*	(i)module_parm - module parameters
*	(io)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
*******************************************************************************/
static void dpCdevFreeAll(void)
{
	// Destroy character device
	dpCdevFreeDestDev();

	// Remove character device from kernel
	dpCdevFreeDelDev();

	// Unregister character device region
	dpCdevFreeUnReg();
}

/**************************** dpCdevFreeDestDev() *****************************
* Destroy character device
* Used variables:
*	(i)module_parm - module parameters
*	(io)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
*******************************************************************************/
static void dpCdevFreeDestDev(void)
{
	struct class *pclass;
	dev_t node;
	uint32_t cdev_created;

	// Set the pointer to the module class
	pclass = module_parm.pclass;

	// Get character device 32-bit major+minor number
	node = dp_parm.cdev_node;
	
	// Read the flag: character device was created / not created
	cdev_created = dp_parm.cdev_created;

	// Destroy created character device
	if(cdev_created) device_destroy(pclass, node);

	// Character device was destroyed. Clear correspondent flag
	dp_parm.cdev_created = 0;
}

/***************************** dpCdevFreeDelDev() *****************************
* Remove character device from kernel
* Used variable:
*	(io)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
*******************************************************************************/
static void dpCdevFreeDelDev(void)
{
	uint32_t cdev_added;
	struct cdev *pcdev;

	// Read the flag: character device was added / not added to the kernel
	cdev_added = dp_parm.cdev_added;

	// Set the pointer to the kernel character device structure
	pcdev = &dp_parm.cdev;

	// Remove character device from kernel
	if(cdev_added) cdev_del(pcdev);

	// Character device was removed. Clear correspondent flag
	dp_parm.cdev_added = 0;
}

/***************************** dpCdevFreeUnReg() ******************************
* Unregister character device region
* (Free allocated character device major and minor numbers)
* Used variable:
*	(io)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
*******************************************************************************/
static void dpCdevFreeUnReg(void)
{
	uint32_t cdev_region_alloc;
	dev_t node;

	// Read the flag: character device major+minor numbers allocated / not allocated
	cdev_region_alloc = dp_parm.cdev_region_alloc;

	// Get character device 32-bit major+minor number
	node = dp_parm.cdev_node;

	// Unregister character device region for one character device
	if(cdev_region_alloc) unregister_chrdev_region(node, 1);

	// Char device region was unregistered. Clear correspondent flag
	dp_parm.cdev_region_alloc = 0;
}

/******************************** dpFreeAll() *********************************
* Free all resources associated with DATA-PROVIDER
* The function is called from DATA-PROVIDER remove function
* It is also called from DATA-PROVIDER probe function in case of errors
* Used variable:
*	(io)dp_parm - DATA-PROVIDER parameters (for DATA-PROVIDER IP core)
*******************************************************************************/
static void dpFreeAll(void)
{
	// Free all resources associated with character device
	dpCdevFreeAll();

	// Free all resources allocated for the platform device
	dpPlatFreeAll();
}

/******************************************************************************
* A module must use the "module_init" "module_exit" macros from linux/init.h,
*	which identify the initialization function at insertion time, 
*	the cleanup function for the module removal
*******************************************************************************/
module_init(moduleInit);
module_exit(moduleExit);

