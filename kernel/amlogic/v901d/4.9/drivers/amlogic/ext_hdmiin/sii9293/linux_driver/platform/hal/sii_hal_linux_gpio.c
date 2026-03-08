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

#define SII_HAL_LINUX_GPIO_C

/***** #include statements ***************************************************/
#include "sii_hal.h"
#include "sii_hal_priv.h"
#include "mhl_linuxdrv.h"
#include <linux/ioport.h>
#include <linux/io.h>



enum {
	DIRECTION_IN = 0,
	DIRECTION_OUT,
};

struct GPIOInfo_t {
	uint32_t index;
	int gpio_number;
	char gpio_descripion[40];
	uint32_t gpio_direction;
	int init_value;
};

#define GPIO_ITEM(a, b, c, d)                                                  \
	{                                                                      \
		.index = (a), .gpio_number = (b), .gpio_descripion = (#a),     \
		.gpio_direction = (c), .init_value = (d),                      \
	}

static struct GPIOInfo_t GPIO_List[] = {
    //    GPIO_ITEM(GPIO_136,             136,DIRECTION_OUT,0),   //configure
    //    GPIO 135 134 as input at trainner board
    //    GPIO_ITEM(GPIO_140,             140,DIRECTION_OUT,1),   //configure
    //    GPIO 138 139 as output at trainner board
    //    GPIO_ITEM(GPIO_INT,             94,DIRECTION_IN,0),
    //    GPIO_ITEM(GPIO_RST,             93,DIRECTION_OUT,1),//init high
};

static struct GPIOInfo_t *GetGPIOInfo(int gpio)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(GPIO_List); i++) {
		if (gpio == GPIO_List[i].index)
			return &GPIO_List[i];
	}
	return NULL;
}

/*
 * IEN  - Input Enable
 * IDIS - Input Disable
 * PTD  - Pull type Down
 * PTU  - Pull type Up
 * DIS  - Pull type selection is inactive
 * EN   - Pull type selection is active
 * M0   - Mode 0
 */

#define IEN (1 << 8)

#define IDIS (0 << 8)
#define PTU (1 << 4)
#define PTD (0 << 4)
#define EN (1 << 3)
#define DIS (0 << 3)

#define M0 0
#define M1 1
#define M2 2
#define M3 3
#define M4 4
#define M5 5
#define M6 6
#define M7 7

#define IO_PHY_ADDRESS 0x48000000
#define PAD_CONF_OFFSET 0x2030

// get gpio configuration from device tree.
static void aml_get_gpio(void)
{
	// for gpio reset
	GPIO_List[0].gpio_number = devinfo->config.gpio_reset;
}

/*****************************************************************************/
/*
 * @brief Configure platform GPIOs needed by the MHL device.
 *
 */
uint32_t HalGpioInit(void)
{
	int status;
	int i, j;

	aml_get_gpio();

	for (i = 0; i < ARRAY_SIZE(GPIO_List); i++) {
		/* Request  GPIO . */
		status = gpio_request(GPIO_List[i].gpio_number,
				      GPIO_List[i].gpio_descripion);
		if (status < 0 && status != -EBUSY) {
			SII_DEBUG_PRINT(MSG_ERR,
			"HalInit gpio_request for GPIO %d (H/W Reset) failed, status: %d\n",
			GPIO_List[i].gpio_number, status);
			for (j = 0; j < i; j++)
				gpio_free(GPIO_List[j].gpio_number);
			return HAL_RET_FAILURE;
		}

		if (GPIO_List[i].gpio_direction == DIRECTION_OUT)
			status = gpio_direction_output(GPIO_List[i].gpio_number,
						       GPIO_List[i].init_value);
		else
			status = gpio_direction_input(GPIO_List[i].gpio_number);

		if (status < 0) {
			SII_DEBUG_PRINT(
			MSG_ERR,
			"HalInit gpio_direction_output for GPIO %d (H/W Reset) failed, status: %d\n",
			GPIO_List[i].gpio_number, status);
			for (j = 0; j <= i; j++)
				gpio_free(GPIO_List[j].gpio_number);

			return HAL_RET_FAILURE;
		}
		//        SII_DEBUG_PRINT(SII_OSAL_DEBUG_TRACE,"initialize %s
		//        successfully\n",GPIO_List[i].gpio_descripion);
	}

	return HAL_RET_SUCCESS;
}

/*****************************************************************************/
/**
 * @brief Release GPIO pins needed by the MHL device.
 *
 *****************************************************************************/
uint32_t HalGpioTerm(void)
{
	uint32_t halRet;
	int index;

	halRet = HalInitCheck();
	if (halRet != HAL_RET_SUCCESS)
		return halRet;

	for (index = 0; index < ARRAY_SIZE(GPIO_List); index++)
		gpio_free(GPIO_List[index].gpio_number);

	return HAL_RET_SUCCESS;
}

/*****************************************************************************/
/*
 * @brief Platform specific function to set the output pin to control the MHL
 * transmitter device.
 *
 */
uint32_t HalGpioSetPin(uint32_t gpio, int value)
{
	uint32_t halRet;
	struct GPIOInfo_t *pGpioInfo;

	halRet = HalInitCheck();
	if (halRet != HAL_RET_SUCCESS)
		return halRet;

	pGpioInfo = GetGPIOInfo(gpio);
	if (!pGpioInfo) {
		SII_DEBUG_PRINT(MSG_ERR, "%d is NOT right gpio_index!\n",
				(int)gpio);
		return HAL_RET_FAILURE;
	}
	if (pGpioInfo->gpio_direction != DIRECTION_OUT) {
		SII_DEBUG_PRINT(MSG_ERR, "gpio(%d) is NOT output gpio!\n",
				pGpioInfo->gpio_number);
		return HAL_RET_FAILURE;
	}

	gpio_set_value(pGpioInfo->gpio_number, value ? 1 : 0);

	if (value) {
		SII_DEBUG_PRINT(MSG_STAT, ">> %s to HIGH <<\n",
				pGpioInfo->gpio_descripion);
	} else {
		SII_DEBUG_PRINT(MSG_STAT, ">> %s to LOW <<\n",
				pGpioInfo->gpio_descripion);
	}

	return HAL_RET_SUCCESS;
}

/*****************************************************************************/
/**
 * @brief Platform specific function to get the input pin value of the MHL
 *			transmitter device.
 *
 *****************************************************************************/
uint32_t HalGpioGetPin(uint32_t gpio, int *value)
{
	uint32_t halRet;
	struct GPIOInfo_t *pGpioInfo;

	halRet = HalInitCheck();
	if (halRet != HAL_RET_SUCCESS)
		return halRet;

	pGpioInfo = GetGPIOInfo(gpio);
	if (!pGpioInfo) {
		SII_DEBUG_PRINT(MSG_ERR, "%d is NOT right gpio_index!\n",
				(int)gpio);
		return HAL_RET_FAILURE;
	}
	if (pGpioInfo->gpio_direction != DIRECTION_IN) {
		SII_DEBUG_PRINT(MSG_ERR, "gpio(%d) is NOT input gpio!\n",
				pGpioInfo->gpio_number);
		return HAL_RET_FAILURE;
	}

	*value = gpio_get_value(pGpioInfo->gpio_number);
	return HAL_RET_SUCCESS;
}
