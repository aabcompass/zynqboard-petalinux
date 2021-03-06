#ifndef XPARAMETERS_H   /* prevent circular inclusions */
#define XPARAMETERS_H   /* by using protection macros */

/* Definition for CPU ID */
#define XPAR_CPU_ID 0U

/* Definitions for peripheral PS7_CORTEXA9_0 */
#define XPAR_PS7_CORTEXA9_0_CPU_CLK_FREQ_HZ 800000000


/******************************************************************/

/* Canonical definitions for peripheral PS7_CORTEXA9_0 */
#define XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ 800000000


/******************************************************************/

#include "xparameters_ps.h"

#define STDIN_BASEADDRESS 0xE0000000
#define STDOUT_BASEADDRESS 0xE0000000

/******************************************************************/

/* Platform specific definitions */
#define PLATFORM_ZYNQ
 
/* Definitions for sleep timer configuration */
#define XSLEEP_TIMER_IS_DEFAULT_TIMER
 
 
/******************************************************************/
/* Definitions for driver AXIDMA */
#define XPAR_XAXIDMA_NUM_INSTANCES 2

/* Definitions for peripheral AXI_DMA_0 */
#define XPAR_AXI_DMA_0_DEVICE_ID 0
#define XPAR_AXI_DMA_0_BASEADDR 0x40400000
#define XPAR_AXI_DMA_0_HIGHADDR 0x4040FFFF
#define XPAR_AXI_DMA_0_SG_INCLUDE_STSCNTRL_STRM 0
#define XPAR_AXI_DMA_0_INCLUDE_MM2S_DRE 0
#define XPAR_AXI_DMA_0_INCLUDE_S2MM_DRE 0
#define XPAR_AXI_DMA_0_INCLUDE_MM2S 0
#define XPAR_AXI_DMA_0_INCLUDE_S2MM 1
#define XPAR_AXI_DMA_0_M_AXI_MM2S_DATA_WIDTH 32
#define XPAR_AXI_DMA_0_M_AXI_S2MM_DATA_WIDTH 512
#define XPAR_AXI_DMA_0_INCLUDE_SG 0
#define XPAR_AXI_DMA_0_ENABLE_MULTI_CHANNEL 0
#define XPAR_AXI_DMA_0_NUM_MM2S_CHANNELS 1
#define XPAR_AXI_DMA_0_NUM_S2MM_CHANNELS 1
#define XPAR_AXI_DMA_0_MM2S_BURST_SIZE 16
#define XPAR_AXI_DMA_0_S2MM_BURST_SIZE 16
#define XPAR_AXI_DMA_0_MICRO_DMA 0
#define XPAR_AXI_DMA_0_ADDR_WIDTH 32


/* Definitions for peripheral AXI_DMA_SC36 */
#define XPAR_AXI_DMA_SC36_DEVICE_ID 1
#define XPAR_AXI_DMA_SC36_BASEADDR 0x40420000
#define XPAR_AXI_DMA_SC36_HIGHADDR 0x4042FFFF
#define XPAR_AXI_DMA_SC36_SG_INCLUDE_STSCNTRL_STRM 0
#define XPAR_AXI_DMA_SC36_INCLUDE_MM2S_DRE 0
#define XPAR_AXI_DMA_SC36_INCLUDE_S2MM_DRE 0
#define XPAR_AXI_DMA_SC36_INCLUDE_MM2S 0
#define XPAR_AXI_DMA_SC36_INCLUDE_S2MM 1
#define XPAR_AXI_DMA_SC36_M_AXI_MM2S_DATA_WIDTH 32
#define XPAR_AXI_DMA_SC36_M_AXI_S2MM_DATA_WIDTH 512
#define XPAR_AXI_DMA_SC36_INCLUDE_SG 0
#define XPAR_AXI_DMA_SC36_ENABLE_MULTI_CHANNEL 0
#define XPAR_AXI_DMA_SC36_NUM_MM2S_CHANNELS 1
#define XPAR_AXI_DMA_SC36_NUM_S2MM_CHANNELS 1
#define XPAR_AXI_DMA_SC36_MM2S_BURST_SIZE 16
#define XPAR_AXI_DMA_SC36_S2MM_BURST_SIZE 16
#define XPAR_AXI_DMA_SC36_MICRO_DMA 0
#define XPAR_AXI_DMA_SC36_ADDR_WIDTH 32


/******************************************************************/

/* Canonical definitions for peripheral AXI_DMA_0 */
#define XPAR_AXIDMA_0_DEVICE_ID XPAR_AXI_DMA_0_DEVICE_ID
#define XPAR_AXIDMA_0_BASEADDR 0x40400000
#define XPAR_AXIDMA_0_SG_INCLUDE_STSCNTRL_STRM 0
#define XPAR_AXIDMA_0_INCLUDE_MM2S 0
#define XPAR_AXIDMA_0_INCLUDE_MM2S_DRE 0
#define XPAR_AXIDMA_0_M_AXI_MM2S_DATA_WIDTH 32
#define XPAR_AXIDMA_0_INCLUDE_S2MM 1
#define XPAR_AXIDMA_0_INCLUDE_S2MM_DRE 0
#define XPAR_AXIDMA_0_M_AXI_S2MM_DATA_WIDTH 512
#define XPAR_AXIDMA_0_INCLUDE_SG 0
#define XPAR_AXIDMA_0_ENABLE_MULTI_CHANNEL 0
#define XPAR_AXIDMA_0_NUM_MM2S_CHANNELS 1
#define XPAR_AXIDMA_0_NUM_S2MM_CHANNELS 1
#define XPAR_AXIDMA_0_MM2S_BURST_SIZE 16
#define XPAR_AXIDMA_0_S2MM_BURST_SIZE 16
#define XPAR_AXIDMA_0_MICRO_DMA 0
#define XPAR_AXIDMA_0_c_addr_width 32

/* Canonical definitions for peripheral AXI_DMA_SC36 */
#define XPAR_AXIDMA_1_DEVICE_ID XPAR_AXI_DMA_SC36_DEVICE_ID
#define XPAR_AXIDMA_1_BASEADDR 0x40420000
#define XPAR_AXIDMA_1_SG_INCLUDE_STSCNTRL_STRM 0
#define XPAR_AXIDMA_1_INCLUDE_MM2S 0
#define XPAR_AXIDMA_1_INCLUDE_MM2S_DRE 0
#define XPAR_AXIDMA_1_M_AXI_MM2S_DATA_WIDTH 32
#define XPAR_AXIDMA_1_INCLUDE_S2MM 1
#define XPAR_AXIDMA_1_INCLUDE_S2MM_DRE 0
#define XPAR_AXIDMA_1_M_AXI_S2MM_DATA_WIDTH 512
#define XPAR_AXIDMA_1_INCLUDE_SG 0
#define XPAR_AXIDMA_1_ENABLE_MULTI_CHANNEL 0
#define XPAR_AXIDMA_1_NUM_MM2S_CHANNELS 1
#define XPAR_AXIDMA_1_NUM_S2MM_CHANNELS 1
#define XPAR_AXIDMA_1_MM2S_BURST_SIZE 16
#define XPAR_AXIDMA_1_S2MM_BURST_SIZE 16
#define XPAR_AXIDMA_1_MICRO_DMA 0
#define XPAR_AXIDMA_1_c_addr_width 32


/******************************************************************/


/* Definitions for peripheral PS7_DDR_0 */
#define XPAR_PS7_DDR_0_S_AXI_BASEADDR 0x00100000
#define XPAR_PS7_DDR_0_S_AXI_HIGHADDR 0x3FFFFFFF


/******************************************************************/

/* Definitions for driver DEVCFG */
#define XPAR_XDCFG_NUM_INSTANCES 1U

/* Definitions for peripheral PS7_DEV_CFG_0 */
#define XPAR_PS7_DEV_CFG_0_DEVICE_ID 0U
#define XPAR_PS7_DEV_CFG_0_BASEADDR 0xF8007000U
#define XPAR_PS7_DEV_CFG_0_HIGHADDR 0xF80070FFU


/******************************************************************/

/* Canonical definitions for peripheral PS7_DEV_CFG_0 */
#define XPAR_XDCFG_0_DEVICE_ID XPAR_PS7_DEV_CFG_0_DEVICE_ID
#define XPAR_XDCFG_0_BASEADDR 0xF8007000U
#define XPAR_XDCFG_0_HIGHADDR 0xF80070FFU


/******************************************************************/

/* Definitions for driver DMAPS */
#define XPAR_XDMAPS_NUM_INSTANCES 2

/* Definitions for peripheral PS7_DMA_NS */
#define XPAR_PS7_DMA_NS_DEVICE_ID 0
#define XPAR_PS7_DMA_NS_BASEADDR 0xF8004000
#define XPAR_PS7_DMA_NS_HIGHADDR 0xF8004FFF


/* Definitions for peripheral PS7_DMA_S */
#define XPAR_PS7_DMA_S_DEVICE_ID 1
#define XPAR_PS7_DMA_S_BASEADDR 0xF8003000
#define XPAR_PS7_DMA_S_HIGHADDR 0xF8003FFF


/******************************************************************/

/* Canonical definitions for peripheral PS7_DMA_NS */
#define XPAR_XDMAPS_0_DEVICE_ID XPAR_PS7_DMA_NS_DEVICE_ID
#define XPAR_XDMAPS_0_BASEADDR 0xF8004000
#define XPAR_XDMAPS_0_HIGHADDR 0xF8004FFF

/* Canonical definitions for peripheral PS7_DMA_S */
#define XPAR_XDMAPS_1_DEVICE_ID XPAR_PS7_DMA_S_DEVICE_ID
#define XPAR_XDMAPS_1_BASEADDR 0xF8003000
#define XPAR_XDMAPS_1_HIGHADDR 0xF8003FFF


/******************************************************************/

/* Definitions for driver EMACPS */
#define XPAR_XEMACPS_NUM_INSTANCES 1

/* Definitions for peripheral PS7_ETHERNET_0 */
#define XPAR_PS7_ETHERNET_0_DEVICE_ID 0
#define XPAR_PS7_ETHERNET_0_BASEADDR 0xE000B000
#define XPAR_PS7_ETHERNET_0_HIGHADDR 0xE000BFFF
#define XPAR_PS7_ETHERNET_0_ENET_CLK_FREQ_HZ 125000000
#define XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0 8
#define XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV1 1
#define XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV0 8
#define XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV1 5
#define XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV0 8
#define XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV1 50
#define XPAR_PS7_ETHERNET_0_ENET_TSU_CLK_FREQ_HZ 0


/******************************************************************/

#define XPAR_PS7_ETHERNET_0_IS_CACHE_COHERENT 0
/* Canonical definitions for peripheral PS7_ETHERNET_0 */
#define XPAR_XEMACPS_0_DEVICE_ID XPAR_PS7_ETHERNET_0_DEVICE_ID
#define XPAR_XEMACPS_0_BASEADDR 0xE000B000
#define XPAR_XEMACPS_0_HIGHADDR 0xE000BFFF
#define XPAR_XEMACPS_0_ENET_CLK_FREQ_HZ 125000000
#define XPAR_XEMACPS_0_ENET_SLCR_1000Mbps_DIV0 8
#define XPAR_XEMACPS_0_ENET_SLCR_1000Mbps_DIV1 1
#define XPAR_XEMACPS_0_ENET_SLCR_100Mbps_DIV0 8
#define XPAR_XEMACPS_0_ENET_SLCR_100Mbps_DIV1 5
#define XPAR_XEMACPS_0_ENET_SLCR_10Mbps_DIV0 8
#define XPAR_XEMACPS_0_ENET_SLCR_10Mbps_DIV1 50
#define XPAR_XEMACPS_0_ENET_TSU_CLK_FREQ_HZ 0


/******************************************************************/


/* Definitions for peripheral HV_AERA_IP_0 */
#define XPAR_HV_AERA_IP_0_BASEADDR 0x43C00000
#define XPAR_HV_AERA_IP_0_HIGHADDR 0x43C0FFFF


/* Definitions for peripheral AXI_ARTIX_CONF_V1_0_0 */
#define XPAR_AXI_ARTIX_CONF_V1_0_0_BASEADDR 0x43C40000
#define XPAR_AXI_ARTIX_CONF_V1_0_0_HIGHADDR 0x43C4FFFF


/* Definitions for peripheral AXI_DATA_PROVIDER_Z3_0 */
#define XPAR_AXI_DATA_PROVIDER_Z3_0_BASEADDR 0x43C70000
#define XPAR_AXI_DATA_PROVIDER_Z3_0_HIGHADDR 0x43C7FFFF


/* Definitions for peripheral HV_HK_V1_0_0 */
#define XPAR_HV_HK_V1_0_0_BASEADDR 0x43C20000
#define XPAR_HV_HK_V1_0_0_HIGHADDR 0x43C2FFFF


/* Definitions for peripheral PS7_AFI_0 */
#define XPAR_PS7_AFI_0_S_AXI_BASEADDR 0xF8008000
#define XPAR_PS7_AFI_0_S_AXI_HIGHADDR 0xF8008FFF


/* Definitions for peripheral PS7_AFI_1 */
#define XPAR_PS7_AFI_1_S_AXI_BASEADDR 0xF8009000
#define XPAR_PS7_AFI_1_S_AXI_HIGHADDR 0xF8009FFF


/* Definitions for peripheral PS7_AFI_2 */
#define XPAR_PS7_AFI_2_S_AXI_BASEADDR 0xF800A000
#define XPAR_PS7_AFI_2_S_AXI_HIGHADDR 0xF800AFFF


/* Definitions for peripheral PS7_AFI_3 */
#define XPAR_PS7_AFI_3_S_AXI_BASEADDR 0xF800B000
#define XPAR_PS7_AFI_3_S_AXI_HIGHADDR 0xF800BFFF


/* Definitions for peripheral PS7_DDRC_0 */
#define XPAR_PS7_DDRC_0_S_AXI_BASEADDR 0xF8006000
#define XPAR_PS7_DDRC_0_S_AXI_HIGHADDR 0xF8006FFF


/* Definitions for peripheral PS7_GLOBALTIMER_0 */
#define XPAR_PS7_GLOBALTIMER_0_S_AXI_BASEADDR 0xF8F00200
#define XPAR_PS7_GLOBALTIMER_0_S_AXI_HIGHADDR 0xF8F002FF


/* Definitions for peripheral PS7_GPV_0 */
#define XPAR_PS7_GPV_0_S_AXI_BASEADDR 0xF8900000
#define XPAR_PS7_GPV_0_S_AXI_HIGHADDR 0xF89FFFFF


/* Definitions for peripheral PS7_INTC_DIST_0 */
#define XPAR_PS7_INTC_DIST_0_S_AXI_BASEADDR 0xF8F01000
#define XPAR_PS7_INTC_DIST_0_S_AXI_HIGHADDR 0xF8F01FFF


/* Definitions for peripheral PS7_IOP_BUS_CONFIG_0 */
#define XPAR_PS7_IOP_BUS_CONFIG_0_S_AXI_BASEADDR 0xE0200000
#define XPAR_PS7_IOP_BUS_CONFIG_0_S_AXI_HIGHADDR 0xE0200FFF


/* Definitions for peripheral PS7_L2CACHEC_0 */
#define XPAR_PS7_L2CACHEC_0_S_AXI_BASEADDR 0xF8F02000
#define XPAR_PS7_L2CACHEC_0_S_AXI_HIGHADDR 0xF8F02FFF


/* Definitions for peripheral PS7_OCMC_0 */
#define XPAR_PS7_OCMC_0_S_AXI_BASEADDR 0xF800C000
#define XPAR_PS7_OCMC_0_S_AXI_HIGHADDR 0xF800CFFF


/* Definitions for peripheral PS7_PL310_0 */
#define XPAR_PS7_PL310_0_S_AXI_BASEADDR 0xF8F02000
#define XPAR_PS7_PL310_0_S_AXI_HIGHADDR 0xF8F02FFF


/* Definitions for peripheral PS7_PMU_0 */
#define XPAR_PS7_PMU_0_S_AXI_BASEADDR 0xF8891000
#define XPAR_PS7_PMU_0_S_AXI_HIGHADDR 0xF8891FFF
#define XPAR_PS7_PMU_0_PMU1_S_AXI_BASEADDR 0xF8893000
#define XPAR_PS7_PMU_0_PMU1_S_AXI_HIGHADDR 0xF8893FFF


/* Definitions for peripheral PS7_RAM_0 */
#define XPAR_PS7_RAM_0_S_AXI_BASEADDR 0x00000000
#define XPAR_PS7_RAM_0_S_AXI_HIGHADDR 0x0003FFFF


/* Definitions for peripheral PS7_RAM_1 */
#define XPAR_PS7_RAM_1_S_AXI_BASEADDR 0xFFFC0000
#define XPAR_PS7_RAM_1_S_AXI_HIGHADDR 0xFFFFFFFF


/* Definitions for peripheral PS7_SCUC_0 */
#define XPAR_PS7_SCUC_0_S_AXI_BASEADDR 0xF8F00000
#define XPAR_PS7_SCUC_0_S_AXI_HIGHADDR 0xF8F000FC


/* Definitions for peripheral PS7_SLCR_0 */
#define XPAR_PS7_SLCR_0_S_AXI_BASEADDR 0xF8000000
#define XPAR_PS7_SLCR_0_S_AXI_HIGHADDR 0xF8000FFF


/* Definitions for peripheral SPACIROC3_SC_0 */
#define XPAR_SPACIROC3_SC_0_BASEADDR 0x43C30000
#define XPAR_SPACIROC3_SC_0_HIGHADDR 0x43C3FFFF


/******************************************************************/

/* Definitions for driver GPIO */
#define XPAR_XGPIO_NUM_INSTANCES 2

/* Definitions for peripheral AXI_GPIO_0 */
#define XPAR_AXI_GPIO_0_BASEADDR 0x41200000
#define XPAR_AXI_GPIO_0_HIGHADDR 0x4120FFFF
#define XPAR_AXI_GPIO_0_DEVICE_ID 0
#define XPAR_AXI_GPIO_0_INTERRUPT_PRESENT 0
#define XPAR_AXI_GPIO_0_IS_DUAL 0


/* Definitions for peripheral GPIO_CTRL */
#define XPAR_GPIO_CTRL_BASEADDR 0x41210000
#define XPAR_GPIO_CTRL_HIGHADDR 0x4121FFFF
#define XPAR_GPIO_CTRL_DEVICE_ID 1
#define XPAR_GPIO_CTRL_INTERRUPT_PRESENT 0
#define XPAR_GPIO_CTRL_IS_DUAL 0


/******************************************************************/

/* Canonical definitions for peripheral AXI_GPIO_0 */
#define XPAR_GPIO_0_BASEADDR 0x41200000
#define XPAR_GPIO_0_HIGHADDR 0x4120FFFF
#define XPAR_GPIO_0_DEVICE_ID XPAR_AXI_GPIO_0_DEVICE_ID
#define XPAR_GPIO_0_INTERRUPT_PRESENT 0
#define XPAR_GPIO_0_IS_DUAL 0

/* Canonical definitions for peripheral GPIO_CTRL */
#define XPAR_GPIO_1_BASEADDR 0x41210000
#define XPAR_GPIO_1_HIGHADDR 0x4121FFFF
#define XPAR_GPIO_1_DEVICE_ID XPAR_GPIO_CTRL_DEVICE_ID
#define XPAR_GPIO_1_INTERRUPT_PRESENT 0
#define XPAR_GPIO_1_IS_DUAL 0


/******************************************************************/

/* Definitions for driver LLFIFO */
#define XPAR_XLLFIFO_NUM_INSTANCES 2U

/* Definitions for peripheral AXI_FIFO_MM_S_0 */
#define XPAR_AXI_FIFO_MM_S_0_DEVICE_ID 0U
#define XPAR_AXI_FIFO_MM_S_0_BASEADDR 0x43C10000U
#define XPAR_AXI_FIFO_MM_S_0_HIGHADDR 0x43C1FFFFU
#define XPAR_AXI_FIFO_MM_S_0_AXI4_BASEADDR 0U
#define XPAR_AXI_FIFO_MM_S_0_AXI4_HIGHADDR 0U
#define XPAR_AXI_FIFO_MM_S_0_DATA_INTERFACE_TYPE 0U

/* Canonical definitions for peripheral AXI_FIFO_MM_S_0 */
#define XPAR_AXI_FIFO_0_DEVICE_ID 0U
#define XPAR_AXI_FIFO_0_BASEADDR 0x43C10000U
#define XPAR_AXI_FIFO_0_HIGHADDR 0x43C1FFFFU
#define XPAR_AXI_FIFO_0_AXI4_BASEADDR 0U
#define XPAR_AXI_FIFO_0_AXI4_HIGHADDR 0U
#define XPAR_AXI_FIFO_0_DATA_INTERFACE_TYPE 0U



/* Definitions for peripheral AXI_FIFO_MM_S_TESTING */
#define XPAR_AXI_FIFO_MM_S_TESTING_DEVICE_ID 1U
#define XPAR_AXI_FIFO_MM_S_TESTING_BASEADDR 0x43C60000U
#define XPAR_AXI_FIFO_MM_S_TESTING_HIGHADDR 0x43C6FFFFU
#define XPAR_AXI_FIFO_MM_S_TESTING_AXI4_BASEADDR 0U
#define XPAR_AXI_FIFO_MM_S_TESTING_AXI4_HIGHADDR 0U
#define XPAR_AXI_FIFO_MM_S_TESTING_DATA_INTERFACE_TYPE 0U

/* Canonical definitions for peripheral AXI_FIFO_MM_S_TESTING */
#define XPAR_AXI_FIFO_1_DEVICE_ID 1U
#define XPAR_AXI_FIFO_1_BASEADDR 0x43C60000U
#define XPAR_AXI_FIFO_1_HIGHADDR 0x43C6FFFFU
#define XPAR_AXI_FIFO_1_AXI4_BASEADDR 0U
#define XPAR_AXI_FIFO_1_AXI4_HIGHADDR 0U
#define XPAR_AXI_FIFO_1_DATA_INTERFACE_TYPE 0U



/******************************************************************/

/* Definitions for Fabric interrupts connected to ps7_scugic_0 */
#define XPAR_FABRIC_AXI_DMA_SC36_S2MM_INTROUT_INTR 61U
#define XPAR_FABRIC_HV_HK_V1_0_0_INTR_OUT_INTR 62U
#define XPAR_FABRIC_AXI_QUAD_SPI_0_IP2INTC_IRPT_INTR 63U
#define XPAR_FABRIC_AXI_FIFO_MM_S_0_INTERRUPT_INTR 64U
#define XPAR_FABRIC_AXI_FIFO_MM_S_TESTING_INTERRUPT_INTR 65U
#define XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR 66U

/******************************************************************/

/* Canonical definitions for Fabric interrupts connected to ps7_scugic_0 */
#define XPAR_FABRIC_AXIDMA_1_VEC_ID XPAR_FABRIC_AXI_DMA_SC36_S2MM_INTROUT_INTR
#define XPAR_FABRIC_SPI_0_VEC_ID XPAR_FABRIC_AXI_QUAD_SPI_0_IP2INTC_IRPT_INTR
#define XPAR_FABRIC_LLFIFO_0_VEC_ID XPAR_FABRIC_AXI_FIFO_MM_S_0_INTERRUPT_INTR
#define XPAR_FABRIC_LLFIFO_1_VEC_ID XPAR_FABRIC_AXI_FIFO_MM_S_TESTING_INTERRUPT_INTR
#define XPAR_FABRIC_AXIDMA_0_VEC_ID XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR

/******************************************************************/

/* Definitions for driver SCUGIC */
#define XPAR_XSCUGIC_NUM_INSTANCES 1U

/* Definitions for peripheral PS7_SCUGIC_0 */
#define XPAR_PS7_SCUGIC_0_DEVICE_ID 0U
#define XPAR_PS7_SCUGIC_0_BASEADDR 0xF8F00100U
#define XPAR_PS7_SCUGIC_0_HIGHADDR 0xF8F001FFU
#define XPAR_PS7_SCUGIC_0_DIST_BASEADDR 0xF8F01000U


/******************************************************************/

/* Canonical definitions for peripheral PS7_SCUGIC_0 */
#define XPAR_SCUGIC_0_DEVICE_ID 0U
#define XPAR_SCUGIC_0_CPU_BASEADDR 0xF8F00100U
#define XPAR_SCUGIC_0_CPU_HIGHADDR 0xF8F001FFU
#define XPAR_SCUGIC_0_DIST_BASEADDR 0xF8F01000U


/******************************************************************/

/* Definitions for driver SCURVE_ADDER36 */
#define XPAR_XSCURVE_ADDER36_NUM_INSTANCES 1

/* Definitions for peripheral SCURVE_ADDER36_0 */
#define XPAR_SCURVE_ADDER36_0_DEVICE_ID 0
#define XPAR_SCURVE_ADDER36_0_S_AXI_CTRL_BUS_BASEADDR 0x43C80000
#define XPAR_SCURVE_ADDER36_0_S_AXI_CTRL_BUS_HIGHADDR 0x43C8FFFF


/******************************************************************/

/* Canonical definitions for peripheral SCURVE_ADDER36_0 */
#define XPAR_XSCURVE_ADDER36_0_DEVICE_ID XPAR_SCURVE_ADDER36_0_DEVICE_ID
#define XPAR_XSCURVE_ADDER36_0_S_AXI_CTRL_BUS_BASEADDR 0x43C80000
#define XPAR_XSCURVE_ADDER36_0_S_AXI_CTRL_BUS_HIGHADDR 0x43C8FFFF


/******************************************************************/

/* Definitions for driver SCUTIMER */
#define XPAR_XSCUTIMER_NUM_INSTANCES 1

/* Definitions for peripheral PS7_SCUTIMER_0 */
#define XPAR_PS7_SCUTIMER_0_DEVICE_ID 0
#define XPAR_PS7_SCUTIMER_0_BASEADDR 0xF8F00600
#define XPAR_PS7_SCUTIMER_0_HIGHADDR 0xF8F0061F


/******************************************************************/

/* Canonical definitions for peripheral PS7_SCUTIMER_0 */
#define XPAR_XSCUTIMER_0_DEVICE_ID XPAR_PS7_SCUTIMER_0_DEVICE_ID
#define XPAR_XSCUTIMER_0_BASEADDR 0xF8F00600
#define XPAR_XSCUTIMER_0_HIGHADDR 0xF8F0061F


/******************************************************************/

/* Definitions for driver SCUWDT */
#define XPAR_XSCUWDT_NUM_INSTANCES 1

/* Definitions for peripheral PS7_SCUWDT_0 */
#define XPAR_PS7_SCUWDT_0_DEVICE_ID 0
#define XPAR_PS7_SCUWDT_0_BASEADDR 0xF8F00620
#define XPAR_PS7_SCUWDT_0_HIGHADDR 0xF8F006FF


/******************************************************************/

/* Canonical definitions for peripheral PS7_SCUWDT_0 */
#define XPAR_SCUWDT_0_DEVICE_ID XPAR_PS7_SCUWDT_0_DEVICE_ID
#define XPAR_SCUWDT_0_BASEADDR 0xF8F00620
#define XPAR_SCUWDT_0_HIGHADDR 0xF8F006FF


/******************************************************************/

/* Definitions for driver SDPS */
#define XPAR_XSDPS_NUM_INSTANCES 2

/* Definitions for peripheral PS7_SD_0 */
#define XPAR_PS7_SD_0_DEVICE_ID 0
#define XPAR_PS7_SD_0_BASEADDR 0xE0100000
#define XPAR_PS7_SD_0_HIGHADDR 0xE0100FFF
#define XPAR_PS7_SD_0_SDIO_CLK_FREQ_HZ 100000000
#define XPAR_PS7_SD_0_HAS_CD 0
#define XPAR_PS7_SD_0_HAS_WP 0
#define XPAR_PS7_SD_0_BUS_WIDTH 0
#define XPAR_PS7_SD_0_MIO_BANK 0
#define XPAR_PS7_SD_0_HAS_EMIO 0


/* Definitions for peripheral PS7_SD_1 */
#define XPAR_PS7_SD_1_DEVICE_ID 1
#define XPAR_PS7_SD_1_BASEADDR 0xE0101000
#define XPAR_PS7_SD_1_HIGHADDR 0xE0101FFF
#define XPAR_PS7_SD_1_SDIO_CLK_FREQ_HZ 100000000
#define XPAR_PS7_SD_1_HAS_CD 0
#define XPAR_PS7_SD_1_HAS_WP 0
#define XPAR_PS7_SD_1_BUS_WIDTH 0
#define XPAR_PS7_SD_1_MIO_BANK 0
#define XPAR_PS7_SD_1_HAS_EMIO 0


/******************************************************************/

#define XPAR_PS7_SD_0_IS_CACHE_COHERENT 0
#define XPAR_PS7_SD_1_IS_CACHE_COHERENT 0
/* Canonical definitions for peripheral PS7_SD_0 */
#define XPAR_XSDPS_0_DEVICE_ID XPAR_PS7_SD_0_DEVICE_ID
#define XPAR_XSDPS_0_BASEADDR 0xE0100000
#define XPAR_XSDPS_0_HIGHADDR 0xE0100FFF
#define XPAR_XSDPS_0_SDIO_CLK_FREQ_HZ 100000000
#define XPAR_XSDPS_0_HAS_CD 0
#define XPAR_XSDPS_0_HAS_WP 0
#define XPAR_XSDPS_0_BUS_WIDTH 0
#define XPAR_XSDPS_0_MIO_BANK 0
#define XPAR_XSDPS_0_HAS_EMIO 0

/* Canonical definitions for peripheral PS7_SD_1 */
#define XPAR_XSDPS_1_DEVICE_ID XPAR_PS7_SD_1_DEVICE_ID
#define XPAR_XSDPS_1_BASEADDR 0xE0101000
#define XPAR_XSDPS_1_HIGHADDR 0xE0101FFF
#define XPAR_XSDPS_1_SDIO_CLK_FREQ_HZ 100000000
#define XPAR_XSDPS_1_HAS_CD 0
#define XPAR_XSDPS_1_HAS_WP 0
#define XPAR_XSDPS_1_BUS_WIDTH 0
#define XPAR_XSDPS_1_MIO_BANK 0
#define XPAR_XSDPS_1_HAS_EMIO 0


/******************************************************************/

/* Definitions for driver SPI */
#define XPAR_XSPI_NUM_INSTANCES 1U

/* Definitions for peripheral AXI_QUAD_SPI_0 */
#define XPAR_AXI_QUAD_SPI_0_DEVICE_ID 0U
#define XPAR_AXI_QUAD_SPI_0_BASEADDR 0x41E00000U
#define XPAR_AXI_QUAD_SPI_0_HIGHADDR 0x41E0FFFFU
#define XPAR_AXI_QUAD_SPI_0_FIFO_DEPTH 256U
#define XPAR_AXI_QUAD_SPI_0_FIFO_EXIST 1U
#define XPAR_AXI_QUAD_SPI_0_SPI_SLAVE_ONLY 0U
#define XPAR_AXI_QUAD_SPI_0_NUM_SS_BITS 1U
#define XPAR_AXI_QUAD_SPI_0_NUM_TRANSFER_BITS 8U
#define XPAR_AXI_QUAD_SPI_0_SPI_MODE 0U
#define XPAR_AXI_QUAD_SPI_0_TYPE_OF_AXI4_INTERFACE 0U
#define XPAR_AXI_QUAD_SPI_0_AXI4_BASEADDR 0U
#define XPAR_AXI_QUAD_SPI_0_AXI4_HIGHADDR 0U
#define XPAR_AXI_QUAD_SPI_0_XIP_MODE 0U

/* Canonical definitions for peripheral AXI_QUAD_SPI_0 */
#define XPAR_SPI_0_DEVICE_ID 0U
#define XPAR_SPI_0_BASEADDR 0x41E00000U
#define XPAR_SPI_0_HIGHADDR 0x41E0FFFFU
#define XPAR_SPI_0_FIFO_DEPTH 256U
#define XPAR_SPI_0_FIFO_EXIST 1U
#define XPAR_SPI_0_SPI_SLAVE_ONLY 0U
#define XPAR_SPI_0_NUM_SS_BITS 1U
#define XPAR_SPI_0_NUM_TRANSFER_BITS 8U
#define XPAR_SPI_0_SPI_MODE 0U
#define XPAR_SPI_0_TYPE_OF_AXI4_INTERFACE 0U
#define XPAR_SPI_0_AXI4_BASEADDR 0U
#define XPAR_SPI_0_AXI4_HIGHADDR 0U
#define XPAR_SPI_0_XIP_MODE 0U
#define XPAR_SPI_0_USE_STARTUP 0U



/******************************************************************/

/* Definitions for driver UARTPS */
#define XPAR_XUARTPS_NUM_INSTANCES 2

/* Definitions for peripheral PS7_UART_0 */
#define XPAR_PS7_UART_0_DEVICE_ID 0
#define XPAR_PS7_UART_0_BASEADDR 0xE0000000
#define XPAR_PS7_UART_0_HIGHADDR 0xE0000FFF
#define XPAR_PS7_UART_0_UART_CLK_FREQ_HZ 100000000
#define XPAR_PS7_UART_0_HAS_MODEM 0


/* Definitions for peripheral PS7_UART_1 */
#define XPAR_PS7_UART_1_DEVICE_ID 1
#define XPAR_PS7_UART_1_BASEADDR 0xE0001000
#define XPAR_PS7_UART_1_HIGHADDR 0xE0001FFF
#define XPAR_PS7_UART_1_UART_CLK_FREQ_HZ 100000000
#define XPAR_PS7_UART_1_HAS_MODEM 0


/******************************************************************/

/* Canonical definitions for peripheral PS7_UART_0 */
#define XPAR_XUARTPS_0_DEVICE_ID XPAR_PS7_UART_0_DEVICE_ID
#define XPAR_XUARTPS_0_BASEADDR 0xE0000000
#define XPAR_XUARTPS_0_HIGHADDR 0xE0000FFF
#define XPAR_XUARTPS_0_UART_CLK_FREQ_HZ 100000000
#define XPAR_XUARTPS_0_HAS_MODEM 0

/* Canonical definitions for peripheral PS7_UART_1 */
#define XPAR_XUARTPS_1_DEVICE_ID XPAR_PS7_UART_1_DEVICE_ID
#define XPAR_XUARTPS_1_BASEADDR 0xE0001000
#define XPAR_XUARTPS_1_HIGHADDR 0xE0001FFF
#define XPAR_XUARTPS_1_UART_CLK_FREQ_HZ 100000000
#define XPAR_XUARTPS_1_HAS_MODEM 0


/******************************************************************/

/* Definitions for driver XADCPS */
#define XPAR_XADCPS_NUM_INSTANCES 1

/* Definitions for peripheral PS7_XADC_0 */
#define XPAR_PS7_XADC_0_DEVICE_ID 0
#define XPAR_PS7_XADC_0_BASEADDR 0xF8007100
#define XPAR_PS7_XADC_0_HIGHADDR 0xF8007120


/******************************************************************/

/* Canonical definitions for peripheral PS7_XADC_0 */
#define XPAR_XADCPS_0_DEVICE_ID XPAR_PS7_XADC_0_DEVICE_ID
#define XPAR_XADCPS_0_BASEADDR 0xF8007100
#define XPAR_XADCPS_0_HIGHADDR 0xF8007120


/******************************************************************/

/* Xilinx FAT File System Library (XilFFs) User Settings */
#define FILE_SYSTEM_INTERFACE_SD
#define FILE_SYSTEM_INTERFACE_SD
#define FILE_SYSTEM_USE_MKFS
#define FILE_SYSTEM_NUM_LOGIC_VOL 2
#define FILE_SYSTEM_USE_STRFUNC 0
#define FILE_SYSTEM_SET_FS_RPATH 0
#define FILE_SYSTEM_WORD_ACCESS
#endif  /* end of protection macro */
