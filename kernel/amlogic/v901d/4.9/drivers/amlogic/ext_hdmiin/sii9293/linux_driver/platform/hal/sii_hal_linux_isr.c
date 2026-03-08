/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#define SII_HAL_LINUX_ISR_C

/***** #include statements ***************************************************/
#include "sii_hal.h"
#include "sii_hal_priv.h"
//#include "si_drvisrconfig.h"
#include "mhl_linuxdrv.h"

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
//#include <mach/irqs.h>

/***** local macro definitions ***********************************************/

/***** local type definitions ************************************************/

/***** local variable declarations *******************************************/

/***** local function prototypes *********************************************/

/***** global variable declarations *******************************************/

/***** local functions *******************************************************/

/*****************************************************************************/
/*
 *  @brief Interrupt handler for MHL transmitter interrupts.
 *
 *  @param[in]		irq		The number of the asserted IRQ line that
 *caused this handler to be called.
 *  @param[in]		data	Data pointer passed when the interrupt was
 *enabled, which in this case is a pointer to the MhlDeviceContext of the I2c
 *device.
 *
 *  @return     Always returns IRQ_HANDLED.
 *
 *****************************************************************************/
static irqreturn_t HalThreadedIrqHandler(int irq, void *data)
{
	struct mhlDeviceContext_t *pMhlDevContext =
	 (struct mhlDeviceContext_t *)data;

	//	SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,"HalThreadedIrqHandler
	//called\n");
	if (HalAcquireIsrLock() == HAL_RET_SUCCESS) {
		if (pMhlDevContext->irqHandler)
			(pMhlDevContext->irqHandler)();
		HalReleaseIsrLock();
	} else {
		SII_DEBUG_PRINT(MSG_ERR,
		"------------- irq missing! -------------\n");
	}

	return IRQ_HANDLED;
}

/***** public functions ******************************************************/
static void aml_config_gpio_irq(void)
{
	gMhlDevice.pI2cClient->irq = gpio_to_irq(GPIO_INT_PIN);
	gMhlDevice.gpio_intr_irq_flags = GPIO_INT_IRQ_FLAG;

	SII_DEBUG_PRINT(
		MSG_ALWAYS,
		"gpio_to_irq, irq_gpio:%d irq_num:%d irq_flags:%x\n",
		GPIO_INT_PIN,
		gMhlDevice.pI2cClient->irq,
		gMhlDevice.gpio_intr_irq_flags);
}

/*****************************************************************************/
/**
 * @brief Install IRQ handler.
 *
 *****************************************************************************/
uint32_t HalInstallIrqHandler(fwIrqHandler_t irqHandler)
{
	int retStatus;
	uint32_t halRet;

	if (irqHandler == NULL) {
		SII_DEBUG_PRINT(
		    MSG_ERR,
		    "HalInstallIrqHandler: irqHandler cannot be NULL!\n");
		return HAL_RET_PARAMETER_ERROR;
	}

	halRet = I2cAccessCheck();
	if (halRet != HAL_RET_SUCCESS)
		return halRet;

	if (gMhlDevice.pI2cClient->irq == 0) {
		SII_DEBUG_PRINT(
		    MSG_ERR,
		    "HalInstallIrqHandler: No IRQ assigned to I2C device!\n");
		return HAL_RET_FAILURE;
	}

	gMhlDevice.irqHandler = irqHandler;

	aml_config_gpio_irq();

	retStatus = request_threaded_irq(
	    gMhlDevice.pI2cClient->irq, NULL, HalThreadedIrqHandler,
	    (unsigned long)gMhlDevice.gpio_intr_irq_flags,
	    gMhlI2cIdTable[0].name, &gMhlDevice);

	if (retStatus != 0) {
		SII_DEBUG_PRINT(MSG_ERR,
		"HalInstallIrqHandler: request_threaded_irq failed, status: %d\n",
		retStatus);
		gMhlDevice.irqHandler = NULL;
		return HAL_RET_FAILURE;
	}
	SII_DEBUG_PRINT(MSG_ALWAYS, "request_threaded_irq success\n");

	return HAL_RET_SUCCESS;
}

/*****************************************************************************/
/**
 * @brief Remove IRQ handler.
 *
 *****************************************************************************/
uint32_t HalRemoveIrqHandler(void)
{
	uint32_t halRet;

	halRet = I2cAccessCheck();
	if (halRet != HAL_RET_SUCCESS)
		return halRet;

	if (gMhlDevice.irqHandler == NULL) {
		SII_DEBUG_PRINT(
		    MSG_ERR, "HalRemoveIrqHandler: no irqHandler installed!\n");
		return HAL_RET_FAILURE;
	}

	free_irq(gMhlDevice.pI2cClient->irq, &gMhlDevice);

	gMhlDevice.irqHandler = NULL;

	return HAL_RET_SUCCESS;
}

/*****************************************************************************/
/**
 * @brief ON/OFF TX chip IRQ , just be used for debugging interface;
 *
 *****************************************************************************/
void HalEnableIrq(uint8_t bEnable)
{
	if (bEnable)
		enable_irq(gMhlDevice.pI2cClient->irq);
	else
		disable_irq(gMhlDevice.pI2cClient->irq);

}

bool is_interrupt_asserted(void)
{
	// return (amlogic_get_value(devinfo->config.gpio_intr,
	// gMhlI2cIdTable[0].name) == INT_IS_ASSERTED );
	return (gpio_get_value(GPIO_INT_PIN) == INT_IS_ASSERTED);
}

/*****************************************************************************/
/**
 * @brief check device before IRQ handling, it fixed the issue that in some case
 *when chip error, the program can NOT exit since in IRQ dead loop;
 *
 *****************************************************************************/
uint32_t HalInstallCheckDeviceCB(fnCheckDevice fn)
{
	gMhlDevice.CheckDevice = fn;
	return HAL_RET_SUCCESS;
}
