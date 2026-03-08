/*
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <environment.h>
#include <fdt_support.h>
#include <libfdt.h>
#include <asm/cpu_id.h>
#include <asm/arch/secure_apb.h>
#include <partition_table.h>
#ifdef CONFIG_SYS_I2C_MESON
#include <amlogic/i2c.h>
#endif
#ifdef CONFIG_PWM_MESON
#include <pwm.h>
#include <amlogic/pwm.h>
#endif
#include <dm.h>
#ifdef CONFIG_AML_VPU
#include <vpu.h>
#endif
#include <vpp.h>
#ifdef CONFIG_AML_V2_FACTORY_BURN
#include <amlogic/aml_v2_burning.h>
#endif// #ifdef CONFIG_AML_V2_FACTORY_BURN
#ifdef CONFIG_AML_HDMITX20
#include <amlogic/hdmi.h>
#endif
#ifdef CONFIG_AML_LCD
#include <amlogic/aml_lcd.h>
#endif
#include <asm/arch/eth_setup.h>
#include <phy.h>
#include <linux/mtd/partitions.h>
#include <linux/sizes.h>
#include <asm-generic/gpio.h>
#include <dm.h>
#ifdef CONFIG_AML_SPIFC
#include <amlogic/spifc.h>
#endif
#ifdef CONFIG_AML_SPICC
#include <amlogic/spicc.h>
#endif
#include <asm/arch/timer.h>

#include <asm/arch/bl31_apis.h>
#include <asm/reboot.h>
#include <amzn_multiconfigs.h>

//add amzn start
#ifdef CONFIG_IDME
#include <idme.h>
#endif
#include <mmc.h>
#include "fs.h"
#if defined(UFBL_FEATURE_ONETIME_UNLOCK)
#include <amzn_onetime_unlock.h>
#endif
//add amzn end
DECLARE_GLOBAL_DATA_PTR;
int board_id_type_check(void);
//new static eth setup
struct eth_board_socket*  eth_board_skt;

#ifdef CONFIG_PWM_LED
#define LED_STATUS_BREATH     1     /* BREATH*/
#define LED_STATUS_ON         2     /* ON, the Brightness 20% */
#define LED_STATUS_OFF        3     /* OFF*/
#endif


int serial_set_pin_port(unsigned long port_base)
{
    //UART in "Always On Module"
    //GPIOAO_0==tx,GPIOAO_1==rx
    //setbits_le32(P_AO_RTI_PIN_MUX_REG,3<<11);
    return 0;
}

int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_1_SIZE;
	return 0;
}

/* secondary_boot_func
 * this function should be write with asm, here, is is only for compiling pass
 * */
void secondary_boot_func(void)
{
}
#ifdef  ETHERNET_INTERNAL_PHY
void internalPhyConfig(struct phy_device *phydev)
{
}

static int dwmac_meson_cfg_pll(void)
{
	writel(0x39C0040A, P_ETH_PLL_CTL0);
	writel(0x927E0000, P_ETH_PLL_CTL1);
	writel(0xAC5F49E5, P_ETH_PLL_CTL2);
	writel(0x00000000, P_ETH_PLL_CTL3);
	udelay(200);
	writel(0x19C0040A, P_ETH_PLL_CTL0);
	return 0;
}

static int dwmac_meson_cfg_analog(void)
{
	/*Analog*/
	writel(0x20200000, P_ETH_PLL_CTL5);
	writel(0x0000c002, P_ETH_PLL_CTL6);
	writel(0x00000023, P_ETH_PLL_CTL7);

	return 0;
}

static int dwmac_meson_cfg_ctrl(void)
{
	/*config phyid should between  a 0~0xffffffff*/
	/*please don't use 44000181, this has been used by internal phy*/
	writel(0x33000180, P_ETH_PHY_CNTL0);

	/*use_phy_smi | use_phy_ip | co_clkin from eth_phy_top*/
	writel(0x260, P_ETH_PHY_CNTL2);

	writel(0x74043, P_ETH_PHY_CNTL1);
	writel(0x34043, P_ETH_PHY_CNTL1);
	writel(0x74043, P_ETH_PHY_CNTL1);
	return 0;
}

static void setup_net_chip(void)
{
	eth_aml_reg0_t eth_reg0;
	*P_RESET1_LEVEL |= (1<<11);
	eth_reg0.d32 = 0;
	eth_reg0.b.phy_intf_sel = 4;
	eth_reg0.b.rx_clk_rmii_invert = 0;
	eth_reg0.b.rgmii_tx_clk_src = 0;
	eth_reg0.b.rgmii_tx_clk_phase = 0;
	eth_reg0.b.rgmii_tx_clk_ratio = 4;
	eth_reg0.b.phy_ref_clk_enable = 1;
	eth_reg0.b.clk_rmii_i_invert = 1;
	eth_reg0.b.clk_en = 1;
	eth_reg0.b.adj_enable = 1;
	eth_reg0.b.adj_setup = 0;
	eth_reg0.b.adj_delay = 9;
	eth_reg0.b.adj_skew = 0;
	eth_reg0.b.cali_start = 0;
	eth_reg0.b.cali_rise = 0;
	eth_reg0.b.cali_sel = 0;
	eth_reg0.b.rgmii_rx_reuse = 0;
	eth_reg0.b.eth_urgent = 0;
	setbits_le32(P_PREG_ETH_REG0, eth_reg0.d32);// rmii mode

	dwmac_meson_cfg_pll();
	dwmac_meson_cfg_analog();
	dwmac_meson_cfg_ctrl();

	/* eth core clock */
	setbits_le32(HHI_GCLK_MPEG1, (0x1 << 3));
	/* eth phy clock */
	setbits_le32(HHI_GCLK_MPEG0, (0x1 << 4));

	/* eth phy pll, clk50m */
	setbits_le32(HHI_FIX_PLL_CNTL3, (0x1 << 5));

	/* power on memory */
	clrbits_le32(HHI_MEM_PD_REG0, (1 << 3) | (1<<2));
}
#endif

#if defined(CONFIG_IDME)
#define IDME_BLOCK_SIZE_OLD 200
#define IDME_BLOCK_SIZE_NEW 300
#define IDME_BACKUP_OFFSET  0
#define CFG_FASTBOOT_MMC_NO (1)
#define IDME_ITEM_INIT(pitem, limit, item, max_size, export, item_permission, item_value) \
		{ \
					memset(&(pitem->data[0]), 0, max_size); \
					memcpy(&(pitem->data[0]), item_value, MIN(max_size, strlen(item_value))); \
					memset(pitem->desc.name, 0, IDME_MAX_NAME_LEN); \
					memcpy(pitem->desc.name, item, MIN(IDME_MAX_NAME_LEN, strlen(item)));\
					pitem->desc.size = max_size;\
					pitem->desc.exportable = export;\
					pitem->desc.permission = item_permission;\
				}


/* Align data in memory */
#define IDME_ITEM_NEXT(curr_item) \
	curr_item = (struct item_t *)((char *)curr_item + ((sizeof(struct idme_desc) \
	+ curr_item->desc.size + IDME_ALIGN_SIZE - 1) & (~(IDME_ALIGN_SIZE - 1))));


extern const struct idme_init_values idme_default_values[];
extern int idme_platform_write(const unsigned char *pbuf);

static int idme_platform_read_from_offset(unsigned char *pbuf, long idme_offset, int idme_block)
{
	struct mmc* mmc;
	int nread = 0;
	u64 block_offset =0;
	if (!pbuf) {
		printf("Null pbuf used in %s\n", __FUNCTION__);
		return -1;
	}

	mmc = find_mmc_device(CFG_FASTBOOT_MMC_NO);
	if (!mmc) {
		printf( "no mmc devices available\n");
		return -1;
	}
	/* switch to boot partition */
	if (mmc_switch_part(CFG_FASTBOOT_MMC_NO, CONFIG_IDME_PARTITION_NUM) != 0) {
		printf("ERROR: couldn't switch to boot partition\n");
		return -1;
	}
	/* Always use the ending part of boot partition since uboot is backed up from beginning
	 * 	capacity is usually 4M */
//	block_offset = mmc->capacity - IDME_NUM_OF_EMMC_BLOCKS * CONFIG_MMC_BLOCK_SIZE;
	if (idme_offset == -1){
	    block_offset = mmc->capacity - idme_block * CONFIG_MMC_BLOCK_SIZE;
	}else{
		block_offset = idme_offset;
	}
	printf( "%s block_offset=%lx, capacity=%lx\n",  __FUNCTION__, (long)block_offset, (long)mmc->capacity);
	nread = mmc->block_dev.block_read(CFG_FASTBOOT_MMC_NO,
	block_offset/CONFIG_MMC_BLOCK_SIZE,idme_block, pbuf);

	if (mmc_switch_part(CFG_FASTBOOT_MMC_NO, 0) != 0) {
		printf( "ERROR: couldn't switch to user partition\n");
		return -1;
	}
	if (nread < 0) {
		printf( "ERROR: idme read failure, nread %d\n", nread);
		return -1;
	}
	return 0;
}

static int idme_platform_write_offset(const unsigned char *pbuf, long idme_offset, int block_num)
{
	struct mmc* mmc;
	int nwrite = 0;
	u64 block_offset = 0;

	if (!pbuf) {
		printf( "Null pbuf used in %s\n", __FUNCTION__);
		return -1;
	}

	mmc = find_mmc_device(CFG_FASTBOOT_MMC_NO);
	if (!mmc) {
		printf( "no mmc devices available\n");
		return -1;
	}

	/* switch to boot partition */
	if (mmc_switch_part(CFG_FASTBOOT_MMC_NO, CONFIG_IDME_PARTITION_NUM) != 0) {
		printf( "ERROR: couldn't switch to boot partition\n");
		return -1;
	}
	/*
	* 	 * Always use the ending part of boot partition since uboot is backed up from beginning
	* 	 * capacity is usually 4M
	*
	 */
	if (idme_offset == -1){
		block_offset = mmc->capacity - block_num * CONFIG_MMC_BLOCK_SIZE;
	}else{
		block_offset = idme_offset;
	}

	printf( "%s block_offset=%lx, capacity=%lx\n",  __FUNCTION__, (long)block_offset, (long)mmc->capacity);
	nwrite = mmc->block_dev.block_write(CFG_FASTBOOT_MMC_NO,
						block_offset/CONFIG_MMC_BLOCK_SIZE,
						block_num, pbuf);
	if (mmc_switch_part(CFG_FASTBOOT_MMC_NO, 0) != 0) {
		printf( "ERROR: couldn't switch to user partition\n");
		return -1;
	}
	if (nwrite < 0) {
		printf( "ERROR: idme write failure, nwrite %d\n", nwrite);
		return -1;
	}
	return 0;
}

int idme_clearup()
{
	unsigned char idme_buff[CONFIG_MMC_BLOCK_SIZE * IDME_BLOCK_SIZE_OLD] __attribute__((aligned(64)));
	memset(idme_buff,0,CONFIG_MMC_BLOCK_SIZE * IDME_BLOCK_SIZE_OLD);
	if(idme_platform_write_offset(idme_buff,-1,IDME_BLOCK_SIZE_OLD) != 0){
		printf("can not clear up old idme !");
		return -1;
	}
	return 0;
}

int idme_backup(char * buff, long idme_offset_backup,int block_num)
{
	unsigned char bak[IDME_BLOCK_SIZE_OLD*CONFIG_MMC_BLOCK_SIZE] __attribute__((aligned(64)));
	struct idme_t *pidme_data = NULL;
	int iTmp;

	memset(bak, 0x00, IDME_BLOCK_SIZE_OLD*CONFIG_MMC_BLOCK_SIZE);

	if (idme_platform_read_from_offset(bak, idme_offset_backup, IDME_BLOCK_SIZE_OLD) != 0) {
		printf( "Error, failed to read idme from boot area.\n");
		return -1;
	}

	pidme_data = (struct idme_t*)&bak[0];
	iTmp = idme_check_magic_number(pidme_data);
	if (0 == iTmp) {
		printf( "Detected previous backup, leave it without touch\n");
		return 0;
	}

	if(idme_platform_write_offset(buff, idme_offset_backup, block_num) != 0){
		printf("idme backup failed\n");
		return -1;
	}
	return 0;
}

int idme_reset_items(void * new_addr, void * old_addr)
{
	struct idme_t *pidme_old = (struct idme_t *)old_addr;
	struct idme_t *pidme_new = (struct idme_t *)new_addr;
	char *idme_limit = (char *)pidme_new + CONFIG_IDME_SIZE;
	struct item_t *pitem_new = (struct item_t *)(&(pidme_new->item_data[0]));
	struct item_t *pitem_old = (struct item_t *)(&(pidme_old->item_data[0]));
	unsigned int items_num = 0;

	memset(new_addr, 0, CONFIG_IDME_SIZE);
	memcpy(pidme_new->magic, pidme_old->magic, strlen(IDME_MAGIC_NUMBER));
	memcpy(pidme_new->version, pidme_old->version, strlen(IDME_VERSION_2P1));


	/* use default values to initialize idme data */
	const struct idme_init_values *ptr_default = &idme_default_values[0];
	//const struct idme_init_values *ptr = pidme_old->item_data;
	//
	while (strlen(pitem_old->desc.name)) {

		IDME_ITEM_INIT(pitem_new, idme_limit,
				pitem_old->desc.name,
				ptr_default->desc.size,
				pitem_old->desc.exportable,
				pitem_old->desc.permission,
				pitem_old->data);
		items_num++;
	//	ptr++;
		ptr_default++;
		if (strlen(pitem_old->desc.name)){
				IDME_ITEM_NEXT(pitem_new);
				IDME_ITEM_NEXT(pitem_old);
		}
	}
	pidme_new->items_num = items_num;
	if(items_num != pidme_old->items_num){
		return -1;
	}
	return 0;
}
int idme_migrate(void)
{
	unsigned char idme_old_buff[IDME_BLOCK_SIZE_OLD*CONFIG_MMC_BLOCK_SIZE] __attribute__((aligned(64)));
	unsigned char idme_new_buff[IDME_BLOCK_SIZE_NEW*CONFIG_MMC_BLOCK_SIZE] __attribute__((aligned(64)));
	struct idme_t *pidme_data = NULL;
	int iTmp;
	u64 idme_backup_offset = IDME_BACKUP_OFFSET;
	memset(idme_old_buff, 0x00, IDME_BLOCK_SIZE_OLD*CONFIG_MMC_BLOCK_SIZE);
	memset(idme_new_buff, 0x00, IDME_BLOCK_SIZE_NEW*CONFIG_MMC_BLOCK_SIZE);

	if (idme_platform_read_from_offset(idme_old_buff, -1, IDME_BLOCK_SIZE_OLD) != 0) {
		printf( "Error, failed to read idme from boot area.\n");
 		return -1;
	}

	pidme_data = (struct idme_t*)&idme_old_buff[0];
	iTmp = idme_check_magic_number(pidme_data);
	if (-2 == iTmp) {
		printf("WARNING: Failed to find old IDME in boot area. Generating default idme ...\n");
	}else{
		printf("migrate idme from old to new !\n");
		if(idme_backup(idme_old_buff, idme_backup_offset,IDME_BLOCK_SIZE_OLD)){
			return -1;
		}
		if(idme_clearup()){
			printf("can not clear up old idme !\n");
			return -1;
		}

		if(idme_reset_items(idme_new_buff,idme_old_buff)){
			printf("error , when migrate data\n");
			return -1;
		}

		if (idme_platform_write(((const unsigned char *)idme_new_buff))){
			printf( "Error IDME: failure in idme write\n");
			return -1;
		}
	}
	return 0;
}
int board_id_type_check(void)
{
	char buf[24] = {0};
	int rtn = HVT_BOARD_ID_TYPE;

	if (!idme_get_var_external("board_id", buf, sizeof(buf))) {
		printf("board_id = %s\n", buf);
		if ((0 == strncmp(buf, HVT_BOARD_ID,strlen(HVT_BOARD_ID)))){
			rtn = HVT_BOARD_ID_TYPE;
			setenv("hw_version", "HVT");
		}else if ((0 == strncmp(buf, EVT_BOARD_ID,strlen(EVT_BOARD_ID)))){
			rtn = EVT_BOARD_ID_TYPE;
			setenv("hw_version", "EVT");
		}else if ((0 == strncmp(buf, DVT_BOARD_ID,strlen(DVT_BOARD_ID)))){
			rtn = DVT_BOARD_ID_TYPE;
			setenv("hw_version", "DVT");
		}else if ((0 == strncmp(buf, PVT_BOARD_ID,strlen(PVT_BOARD_ID)))){
			rtn = PVT_BOARD_ID_TYPE;
			setenv("hw_version", "PVT");
		}else if ((0 == strncmp(buf, REF_BOARD_ID,strlen(REF_BOARD_ID)))){
			rtn = REF_BOARD_ID_TYPE;
			setenv("hw_version", "REF");
		}
	}
	return rtn;
}

unsigned long amz_dev_flags_check(void)
{
	char buf[24] = "";
	unsigned long rtn = 0;
	char tmp_buf[8] = "";

	if (!idme_get_var_external("dev_flags", buf, sizeof(buf))){
		printf("dev_flags = %s\n", buf);
		rtn = simple_strtoul (buf, NULL, 16);
	}
// disable STR for lagunaf
	if (1/*rtn & DEV_FLAGS_BYPASS_SECONDARY_BOOT*/) {
		sprintf(tmp_buf, "%d", 1);
		setenv("bypass_standby", tmp_buf);
	}

	return rtn;
}

#define BUILD_TAG_LEN 128
#define BUILD_INFO_LEN 128
static void get_build_tag(){
	char buf[BUILD_TAG_LEN] = "unknown";
	char  bl2_version[8]={0};
	memcpy((void *)bl2_version, (const void *)0x00e00000, 7);
#if defined BUILD_TAG
	char * info_start = strstr(BUILD_TAG,"build");
	if(info_start && ((strlen(CONFIG_DEVICE_PRODUCT) + strlen(info_start))<(BUILD_TAG_LEN-18))){
		memset(buf,0,BUILD_TAG_LEN);
		if (strlen(BUILD_TAG) < BUILD_INFO_LEN){
			sprintf(buf, "%s_Uboot_AMZN_%s", CONFIG_DEVICE_PRODUCT,info_start);
		}
	}
#else
	if(strlen(CONFIG_DEVICE_PRODUCT)<(BUILD_TAG_LEN-24)){
		memset(buf,0,BUILD_TAG_LEN);
		sprintf(buf, "%s_Uboot_%s", CONFIG_DEVICE_PRODUCT,"localbuild");
	}
#endif

#if defined UBOOT_BUILD_TAG_SUFFIX_DIRTY
	strcat(buf,"_DIRTY");
#endif
	printf("UbootBuildTag---------------%s\n",buf);
	setenv("UbootBuildTag",buf);
	setenv("bl2_version",bl2_version);
}

static bool store_demo_mode(void)
{
	char usr_flags_buf[8] = {0};
	unsigned usr_flags = 0;
	bool store_demo_mode = false;

	/* treat usr_flags as an unsigned integer in hex */
	/* fix to store mode */
	if (!idme_get_var_external("usr_flags", usr_flags_buf, sizeof(usr_flags_buf) - 1)) {
		usr_flags = simple_strtoul(usr_flags_buf, NULL, 16);
		if (1/*usr_flags & USR_FLAGS_STOREDEMO_MODE*/) {
			store_demo_mode = true;
#ifdef CONFIG_PWM_LED
			send_to_led_pattern(LED_STATUS_BREATH);/* send date to bl30 */
#endif
			printf("cold boot directly,breathing\n");
		}
	}

	printf("store demo mode: %d\n", store_demo_mode);
	return store_demo_mode;
}
#endif

#if defined(UBOOT_TARGET_PRODUCT_NAME_ABC)
#define USB_STR_FILE_NAME "fac_boot_aging_exit.cvt"
static void check_usb_str(void)
{
#ifdef CONFIG_IDME
	if(idme_boot_mode() == IDME_BOOTMODE_DIAG)
	{
		mdelay(1000);
		run_command("usb start", 0);
		printf(" Check USB when bootup in Diag mode \n");
		if(file_exists("usb", "0", USB_STR_FILE_NAME, FS_TYPE_FAT)==1) {
			run_command("systemoff", 0);
			printf(" Diag directly call systemoff to enter standby!\n");
			/* setenv("bypass_standby", "0"); */
		}
	}
#endif
}
#endif

#ifdef ETHERNET_EXTERNAL_PHY

static int dwmac_meson_cfg_drive_strength(void)
{
	writel(0xaaaaaaa5, P_PAD_DS_REG4A);
	return 0;
}

static void setup_net_chip_ext(void)
{
	eth_aml_reg0_t eth_reg0;
	writel(0x11111111, P_PERIPHS_PIN_MUX_6);
	writel(0x111111, P_PERIPHS_PIN_MUX_7);

	eth_reg0.d32 = 0;
	eth_reg0.b.phy_intf_sel = 1;
	eth_reg0.b.rx_clk_rmii_invert = 0;
	eth_reg0.b.rgmii_tx_clk_src = 0;
	eth_reg0.b.rgmii_tx_clk_phase = 1;
	eth_reg0.b.rgmii_tx_clk_ratio = 4;
	eth_reg0.b.phy_ref_clk_enable = 1;
	eth_reg0.b.clk_rmii_i_invert = 0;
	eth_reg0.b.clk_en = 1;
	eth_reg0.b.adj_enable = 0;
	eth_reg0.b.adj_setup = 0;
	eth_reg0.b.adj_delay = 0;
	eth_reg0.b.adj_skew = 0;
	eth_reg0.b.cali_start = 0;
	eth_reg0.b.cali_rise = 0;
	eth_reg0.b.cali_sel = 0;
	eth_reg0.b.rgmii_rx_reuse = 0;
	eth_reg0.b.eth_urgent = 0;
	setbits_le32(P_PREG_ETH_REG0, eth_reg0.d32);// rmii mode

	setbits_le32(HHI_GCLK_MPEG1, 0x1 << 3);
	/* power on memory */
	clrbits_le32(HHI_MEM_PD_REG0, (1 << 3) | (1<<2));
}
#endif
extern struct eth_board_socket* eth_board_setup(char *name);
extern int designware_initialize(ulong base_addr, u32 interface);

int board_eth_init(bd_t *bis)
{
#ifdef CONFIG_ETHERNET_NONE
	return 0;
#endif

#ifdef ETHERNET_EXTERNAL_PHY
	dwmac_meson_cfg_drive_strength();
	setup_net_chip_ext();
#endif
#ifdef ETHERNET_INTERNAL_PHY
	setup_net_chip();
#endif
	udelay(1000);
	designware_initialize(ETH_BASE, PHY_INTERFACE_MODE_RMII);
	return 0;
}

#if CONFIG_AML_SD_EMMC
#include <mmc.h>
#include <asm/arch/sd_emmc.h>
static int  sd_emmc_init(unsigned port)
{
    switch (port)
	{
		case SDIO_PORT_A:
			break;
		case SDIO_PORT_B:
			//todo add card detect
			/* check card detect */
			clrbits_le32(P_PERIPHS_PIN_MUX_9, 0xF << 24);
			setbits_le32(P_PREG_PAD_GPIO1_EN_N, 1 << 6);
			setbits_le32(P_PAD_PULL_UP_EN_REG1, 1 << 6);
			setbits_le32(P_PAD_PULL_UP_REG1, 1 << 6);
			break;
		case SDIO_PORT_C:
			//enable pull up
			//clrbits_le32(P_PAD_PULL_UP_REG3, 0xff<<0);
			break;
		default:
			break;
	}

	return cpu_sd_emmc_init(port);
}

extern unsigned sd_debug_board_1bit_flag;


static void sd_emmc_pwr_prepare(unsigned port)
{
	cpu_sd_emmc_pwr_prepare(port);
}

static void sd_emmc_pwr_on(unsigned port)
{
    switch (port)
	{
		case SDIO_PORT_A:
			break;
		case SDIO_PORT_B:
//            clrbits_le32(P_PREG_PAD_GPIO5_O,(1<<31)); //CARD_8
//            clrbits_le32(P_PREG_PAD_GPIO5_EN_N,(1<<31));
			/// @todo NOT FINISH
			break;
		case SDIO_PORT_C:
			break;
		default:
			break;
	}
	return;
}
static void sd_emmc_pwr_off(unsigned port)
{
	/// @todo NOT FINISH
    switch (port)
	{
		case SDIO_PORT_A:
			break;
		case SDIO_PORT_B:
//            setbits_le32(P_PREG_PAD_GPIO5_O,(1<<31)); //CARD_8
//            clrbits_le32(P_PREG_PAD_GPIO5_EN_N,(1<<31));
			break;
		case SDIO_PORT_C:
			break;
				default:
			break;
	}
	return;
}

// #define CONFIG_TSD      1
static void board_mmc_register(unsigned port)
{
	struct aml_card_sd_info *aml_priv=cpu_sd_emmc_get(port);
    if (aml_priv == NULL)
		return;

	aml_priv->sd_emmc_init=sd_emmc_init;
	aml_priv->sd_emmc_detect=sd_emmc_detect;
	aml_priv->sd_emmc_pwr_off=sd_emmc_pwr_off;
	aml_priv->sd_emmc_pwr_on=sd_emmc_pwr_on;
	aml_priv->sd_emmc_pwr_prepare=sd_emmc_pwr_prepare;
	aml_priv->desc_buf = malloc(NEWSD_MAX_DESC_MUN*(sizeof(struct sd_emmc_desc_info)));

	if (NULL == aml_priv->desc_buf)
		printf(" desc_buf Dma alloc Fail!\n");
	else
		printf("aml_priv->desc_buf = 0x%p\n",aml_priv->desc_buf);

	sd_emmc_register(aml_priv);
}
int board_mmc_init(bd_t	*bis)
{
#ifdef CONFIG_VLSI_EMULATOR
	//board_mmc_register(SDIO_PORT_A);
#else
	//board_mmc_register(SDIO_PORT_B);
#endif
	board_mmc_register(SDIO_PORT_B);
	board_mmc_register(SDIO_PORT_C);
//	board_mmc_register(SDIO_PORT_B1);
	return 0;
}
#endif

#if defined(CONFIG_BOARD_EARLY_INIT_F)
int board_early_init_f(void){
	/*add board early init function here*/
	return 0;
}
#endif

#ifdef CONFIG_USB_XHCI_AMLOGIC_V2
#include <asm/arch/usb-v2.h>
#include <asm/arch/gpio.h>
#define CONFIG_GXL_USB_U2_PORT_NUM	CONFIG_USB_U2_PORT_NUM

#ifdef CONFIG_USB_XHCI_AMLOGIC_USB3_V2
#define CONFIG_GXL_USB_U3_PORT_NUM	1
#else
#define CONFIG_GXL_USB_U3_PORT_NUM	0
#endif
#if 1
static void gpio_set_vbus_power(char is_power_on)
{
	int ret;

	ret = gpio_request(CONFIG_USB_GPIO_PWR,
		CONFIG_USB_GPIO_PWR_NAME);
	if (ret && ret != -EBUSY) {
		printf("gpio: requesting pin %u failed\n",
			CONFIG_USB_GPIO_PWR);
		return;
	}

	if (is_power_on) {
		gpio_direction_output(CONFIG_USB_GPIO_PWR, 1);
	} else {
		gpio_direction_output(CONFIG_USB_GPIO_PWR, 0);
	}
}
#endif
struct amlogic_usb_config g_usb_config_GXL_skt={
	CONFIG_GXL_XHCI_BASE,
	USB_ID_MODE_HARDWARE,
	gpio_set_vbus_power,//gpio_set_vbus_power, //set_vbus_power
	CONFIG_GXL_USB_PHY2_BASE,
	CONFIG_GXL_USB_PHY3_BASE,
	CONFIG_GXL_USB_U2_PORT_NUM,
	CONFIG_GXL_USB_U3_PORT_NUM,
	.usb_phy2_pll_base_addr = {
		CONFIG_USB_PHY_20,
		CONFIG_USB_PHY_21,
		CONFIG_USB_PHY_22,
	}
};

#endif /*CONFIG_USB_XHCI_AMLOGIC*/

#ifndef CONFIG_PXP_EMULATOR
#ifdef CONFIG_AML_HDMITX20
static void hdmi_tx_set_hdmi_5v(void)
{
}
#endif
#endif

/*
 * mtd nand partition table, only care the size!
 * offset will be calculated by nand driver.
 */
#ifdef CONFIG_AML_MTD
static struct mtd_partition normal_partition_info[] = {
#ifdef CONFIG_DISCRETE_BOOTLOADER
    /* MUST NOT CHANGE this part unless u know what you are doing!
     * inherent parition for descrete bootloader to store fip
     * size is determind by TPL_SIZE_PER_COPY*TPL_COPY_NUM
     * name must be same with TPL_PART_NAME
     */
    {
        .name = "tpl",
        .offset = 0,
        .size = 0,
    },
#endif
    {
        .name = "logo",
        .offset = 0,
        .size = 2*SZ_1M,
    },
    {
        .name = "recovery",
        .offset = 0,
        .size = 16*SZ_1M,
    },
    {
        .name = "boot",
        .offset = 0,
        .size = 16*SZ_1M,
    },
    {
        .name = "system",
        .offset = 0,
        .size = 64*SZ_1M,
    },
	/* last partition get the rest capacity */
    {
        .name = "data",
        .offset = MTDPART_OFS_APPEND,
        .size = MTDPART_SIZ_FULL,
    },
};
struct mtd_partition *get_aml_mtd_partition(void)
{
	return normal_partition_info;
}
int get_aml_partition_count(void)
{
	return ARRAY_SIZE(normal_partition_info);
}
#endif /* CONFIG_AML_MTD */

#ifdef CONFIG_AML_SPIFC
/*
 * BOOT_3: NOR_HOLDn:reg0[15:12]=3
 * BOOT_4: NOR_D:reg0[19:16]=3
 * BOOT_5: NOR_Q:reg0[23:20]=3
 * BOOT_6: NOR_C:reg0[27:24]=3
 * BOOT_7: NOR_WPn:reg0[31:28]=3
 * BOOT_13: NOR_CS:reg1[23:20]=3
 */
#define SPIFC_NUM_CS 1
static int spifc_cs_gpios[SPIFC_NUM_CS] = {GPIOEE(BOOT_13)};

static int spifc_pinctrl_enable(void *pinctrl, bool enable)
{
	unsigned int val;

	val = readl(P_PERIPHS_PIN_MUX_0);
	val &= ~(0xfffff << 12);
	if (enable)
		val |= 0x33333 << 12;
	writel(val, P_PERIPHS_PIN_MUX_0);

	val = readl(P_PERIPHS_PIN_MUX_1);
	val &= ~(0xf << 20);
	writel(val, P_PERIPHS_PIN_MUX_1);
	return 0;
}

static const struct spifc_platdata spifc_platdata = {
	.reg = 0xffd14000,
	.mem_map = 0xf6000000,
	.pinctrl_enable = spifc_pinctrl_enable,
	.num_chipselect = SPIFC_NUM_CS,
	.cs_gpios = spifc_cs_gpios,
};

U_BOOT_DEVICE(spifc) = {
	.name = "spifc",
	.platdata = &spifc_platdata,
};
#endif /* CONFIG_AML_SPIFC */

#ifdef CONFIG_AML_SPICC
/* generic config in arch gpio/clock.c */
extern int spicc0_clk_set_rate(int rate);
extern int spicc0_clk_enable(bool enable);
extern int spicc0_pinctrl_enable(bool enable);

static const struct spicc_platdata spicc0_platdata = {
	.compatible = "amlogic,meson-g12a-spicc",
	.reg = (void __iomem *)0xffd13000,
	.clk_rate = 666666666,
	.clk_set_rate = spicc0_clk_set_rate,
	.clk_enable = spicc0_clk_enable,
	.pinctrl_enable = spicc0_pinctrl_enable,
	/* case one slave without cs: {"no_cs", 0} */
	.cs_gpio_names = {"GPIOH_20", 0},
};

U_BOOT_DEVICE(spicc0) = {
	.name = "spicc",
	.platdata = &spicc0_platdata,
};
#endif /* CONFIG_AML_SPICC */

extern void aml_pwm_cal_init(int mode);

#ifdef CONFIG_SYS_I2C_MESON
static const struct meson_i2c_platdata i2c_data[] = {
	{ 0, 0xffd1f000, 166666666, 3, 15, 100000 },
	{ 1, 0xffd1e000, 166666666, 3, 15, 100000 },
	{ 2, 0xffd1d000, 166666666, 3, 15, 100000 },
	{ 3, 0xffd1c000, 166666666, 3, 15, 100000 },
	{ 4, 0xff805000, 166666666, 3, 15, 100000 },
};

U_BOOT_DEVICES(meson_i2cs) = {
	{ "i2c_meson", &i2c_data[0] },
	{ "i2c_meson", &i2c_data[1] },
	{ "i2c_meson", &i2c_data[2] },
	{ "i2c_meson", &i2c_data[3] },
	{ "i2c_meson", &i2c_data[4] },
};

/*
 *GPIOH_21 I2C_SDA_M1
 *GPIOH_22 I2C_SCK_M1
 *pinmux configuration seperated with i2c controller configuration
 * config it when you use
 */
void set_i2c_EE_M1_pinmux(void)
{
	/*ds =3 */
	clrbits_le32(PAD_DS_REG2B, 0xf << 10);
	setbits_le32(PAD_DS_REG2B, 0x3 << 10 | 0x3 << 12);
	/*pull up en*/
	clrbits_le32(PAD_PULL_UP_EN_REG2, 0x3 << 21);
	setbits_le32(PAD_PULL_UP_EN_REG2, 0x3 << 21 );
	/*pull up*/
	clrbits_le32(PAD_PULL_UP_REG2, 0x3 << 21);
	setbits_le32(PAD_PULL_UP_REG2, 0x3 << 21 );
	/*pin mux to i2cm1*/
	clrbits_le32(PERIPHS_PIN_MUX_9, 0xff << 20);
	setbits_le32(PERIPHS_PIN_MUX_9, 0x1 << 20 | 0x1 << 24);

}
#endif /*end CONFIG_SYS_I2C_MESON*/

#ifdef CONFIG_PWM_MESON
static const struct meson_pwm_platdata pwm_data[] = {
	{ PWM_AB, 0xffd1b000, IS_DOUBLE_CHANNEL, IS_BLINK },
	{ PWM_CD, 0xffd1a000, IS_DOUBLE_CHANNEL, IS_BLINK },
	{ PWM_EF, 0xffd19000, IS_DOUBLE_CHANNEL, IS_BLINK },
	{ PWMAO_AB, 0xff807000, IS_DOUBLE_CHANNEL, IS_BLINK },
	{ PWMAO_CD, 0xff802000, IS_DOUBLE_CHANNEL, IS_BLINK },
};

U_BOOT_DEVICES(meson_pwm) = {
	{ "amlogic,general-pwm", &pwm_data[0] },
	{ "amlogic,general-pwm", &pwm_data[1] },
	{ "amlogic,general-pwm", &pwm_data[2] },
	{ "amlogic,general-pwm", &pwm_data[3] },
	{ "amlogic,general-pwm", &pwm_data[4] },
};
#endif /*end CONFIG_PWM_MESON*/

int board_init(void)
{
	int val;

	/* For WOL_power enable */
	/* For usb reboot sequence,set GPIOAO_3 low in bl2_stage_init, set high here */
	// jetopto GPIOAO_3 change to GPIOAO_10
	val = readl(AO_GPIO_O);
	val |= 1 << 10;
	writel(val, AO_GPIO_O);

	/* For usb reboot sequence,set GPIOAO_8 low in bl2_stage_init, set high here */
	gpio_set_vbus_power(1);
#ifdef CONFIG_PXP_EMULATOR
	printf("\naml log : 20200211 bring up for TM2 revB@board_late_init\n");
#else
    //Please keep CONFIG_AML_V2_FACTORY_BURN at first place of board_init
    //As NOT NEED other board init If USB BOOT MODE
#ifdef CONFIG_AML_V2_FACTORY_BURN
	if ((0x1b8ec003 != readl(P_PREG_STICKY_REG2)) && (0x1b8ec004 != readl(P_PREG_STICKY_REG2))) {
				aml_try_factory_usb_burning(0, gd->bd);
	}
#endif// #ifdef CONFIG_AML_V2_FACTORY_BURN
#ifdef CONFIG_USB_XHCI_AMLOGIC_V2
	board_usb_pll_disable(&g_usb_config_GXL_skt);
	board_usb_init(&g_usb_config_GXL_skt,BOARD_USB_MODE_HOST);
#endif /*CONFIG_USB_XHCI_AMLOGIC*/

#if 0
	aml_pwm_cal_init(0);
#endif//
#ifdef CONFIG_AML_NAND
	extern int amlnf_init(unsigned char flag);
	amlnf_init(0);
#endif
#ifdef CONFIG_SYS_I2C_MESON
	set_i2c_EE_M1_pinmux();
#endif
#endif //#ifdef CONFIG_PXP_EMULATOR
#ifdef CONFIG_DM_I2C
	clrbits_le32(P_PERIPHS_PIN_MUX_9, 0xFF << 20);
	setbits_le32(P_PERIPHS_PIN_MUX_9, 0x11 << 20);
#endif
	return 0;
}


int ft_board_setup(void *blob, bd_t *bd)
{
	struct fdt_header *fdt_ptr = (struct fdt_header *)blob;
	unsigned int newsize = fdt_totalsize(fdt_ptr) + CONFIG_IDME_SIZE;

	fdt_open_into(fdt_ptr, fdt_ptr, newsize);
	idme_device_tree_initialize(fdt_ptr);
	printf("IDME inserted into FDT\n");
	return 0;
}
#ifdef CONFIG_HARDWARE_ID
char hwid[10]={0};
char bidaz[10]={0};
void pri_hardware_id()
{
	unsigned int hwid_tmp = 0;
	unsigned int bid_tmp = 0;
	int i =0;
	char buf[10];
	char bufaz[10];
	//GPIOH_8 ~ GPIOH_12
	clrbits_le32(P_PERIPHS_PIN_MUX_8,0xFFFFF);//set pinmux --gpio mode
	setbits_le32(P_PREG_PAD_GPIO2_EN_N,(0x1F << 8));//set pinmux --gpio input
	hwid_tmp = (readl(P_PREG_PAD_GPIO2_I) >> 8) & 0x1F;//get value
	//GPIOH_16 ~ GPIOH_20
        setbits_le32(P_PREG_PAD_GPIO2_EN_N,(0x1F << 16));//set pinmux --gpio input
        bid_tmp = (readl(P_PREG_PAD_GPIO2_I) >> 16) & 0x1F;//get value


	for(i = 4; i > 0; --i){
		sprintf(buf, "%d", ((hwid_tmp >> i) & 0x1));
		strcat(hwid, buf);
                sprintf(bufaz, "%d", ((bid_tmp >> i) & 0x1));
                strcat(bidaz, bufaz);
	}
	printf("SSW ID is %d\n",hwid_tmp & 0x1);
	printf("HW ID is %s\n",hwid);
	printf("bid is %d BID is %s\n", bid_tmp, bidaz);
	setenv("hwid",hwid);
}

void config_dev_board(){
	if ((0 == strcmp(hwid, "0000")) ){
		printf("dev board , set lvds vpp\n ");
		run_command("gpio set GPIOAO_3",0);
	}
}

bool check_bid(){
        if ((0 == strcmp(bidaz, "0000")) ){
                printf("evolve board\n ");
                return true;
        } else
		return false;
}
#endif
#ifdef CONFIG_BOARD_LATE_INIT
/* Reset BT-module */
void reset_mt7668(void)
{
#if 0
	/* Reset BT-module by reset-pin -- GPIOAO_5 */
	clrbits_le32(P_AO_GPIO_O_EN_N, 1 << 5);
	clrbits_le32(P_AO_GPIO_O_EN_N, 1 << 21);
	mdelay(200);
	setbits_le32(P_AO_GPIO_O_EN_N, 1 << 21);
	mdelay(100);
#else
	/* Reset BT-module by reset-pin -- GPIOC_11 */
	run_command("gpio clear GPIOC_11",0);
	mdelay(200);
	run_command("gpio set GPIOC_11",0);
	mdelay(100);
#endif
}

/* the function will add string in bootargs. the name and value can't be set to NULL*/
int append_bootargs(char * name, char * value)
{
	char *oldbootargs = NULL;
	char *add_str     = NULL;
	char *newbootargs = NULL;
	char *find_str    = NULL;
	int ret = -1;

	oldbootargs = getenv("bootargs");
	if (oldbootargs == NULL)
	{
		printf("Can't get bootargs\n");
		return ret;
	}

	//  the +2 inlcude "\0 and "="
	add_str = malloc(strlen(name) + strlen(value) + 2);
	if (add_str == NULL) {
		printf("aml log : internal sys error!\n");
		return ret;
	}
	sprintf(add_str, "%s=%s", name, value);

	//  the +2 inlcude "\0 and "="
	find_str = malloc(strlen(name) + 2);
	if (find_str == NULL) {
		printf("aml log : internal sys error!\n");
		goto free_mem;
	}
	sprintf(find_str, "%s=", name);

	// the +2 inlcude "\0 and " "
	newbootargs = malloc(strlen(oldbootargs) + strlen(add_str) + 2);
	if (newbootargs == NULL)
	{
		printf("aml log : internal sys error!\n");
		goto free_mem;
	}

	memset(newbootargs, 0, strlen(oldbootargs) + strlen(add_str) + 2);
	char *pFind = strstr(oldbootargs, find_str);
	if (pFind == NULL)
		// Add new name=value
		sprintf(newbootargs,"%s %s",oldbootargs, add_str);
	else {
		// Find if there is already name str in the old string
		// Copy the string before pFind
		memcpy(newbootargs, oldbootargs, pFind - oldbootargs);

		// Add new str
		memcpy(newbootargs + strlen(newbootargs), add_str, strlen(add_str));

		// Add string after pfind
		pFind = strstr(pFind, " ");
		if (pFind != NULL) {
			memcpy(newbootargs + strlen(newbootargs), pFind, strlen(pFind));
		}
	}

	setenv("bootargs",newbootargs);
	ret = 0;

free_mem:
	if (newbootargs != NULL) {
		free(newbootargs);
		newbootargs = NULL;
	}

	if (find_str != NULL) {
		free(find_str);
		find_str = NULL;
	}

	if (add_str != NULL) {
		free(add_str);
		add_str = NULL;
	}
	return ret;
}



/*
 *The function will return lcd backlight is on or off. 0->off, 1->on
 *GPIOAO_11 is backlight control pin, so we get the pin's status high or low
 **********ONLY BE USED TO ABC**********
 */
int get_lcd_bl_status(void) {
	uint32_t val;

	// read GPIO_AO status
	val = readl(AO_GPIO_O);
	// GPIOAO_11 level state on bit 11
	val = (val & (1 << 11))>>11;
	printf("lcd:bl status:%s\n", val ? "on":"off");
	return val;
}

#ifdef CONFIG_PWM_LED

/*
 *The function will set LED status
 */
static void update_led(void) {
	// set the default 20%
	int led_val = LED_STATUS_ON;

	/*Set LED status*/
	run_command("get_rebootmode", 0);

	char *rebootmode = NULL;
	rebootmode = getenv("reboot_mode");
	if (rebootmode == NULL) {
		printf("get rebootmode error! can't set LED\n");

		return ;
	}

	if(!strcmp(rebootmode,"factory_reset")){
		led_val = LED_STATUS_OFF;
		send_to_led_pattern(led_val);/* send date to bl30 */
		printf("reboot_mode = %s,set LED off,led_val=%d\n",rebootmode,led_val);
	}
	if(!strcmp(rebootmode,"update")){
		led_val = LED_STATUS_ON;
		send_to_led_pattern(led_val);/* send date to bl30 */
		printf("reboot_mode = %s,set 20 percent brightness,led_val=%d\n",rebootmode,led_val);
	}
	if(!strcmp(rebootmode,"normal")){
		led_val = LED_STATUS_BREATH;
		send_to_led_pattern(led_val);/* send date to bl30 */
		printf("reboot_mode = %s,breathing.led_val=%d\n",rebootmode,led_val);
	}
	return ;
}
#endif

void set_dolby_status(void)
{
	if(run_command("query Dolby", 0) == 1){
		setenv("Dolby_enabled","yes");
	} else {
		setenv("Dolby_enabled","no");
	}
}

void set_dts_status(void)
{
	if(run_command("query DTS", 0) == 1){
		setenv("DTS_enabled","yes");
	} else {
		setenv("DTS_enabled","no");
	}
}

bool is_ru = 0;
bool is_headless = 0;

int board_late_init(void)
{
#if defined(CONFIG_IDME)
	char buf[256] = "";
#endif
	run_command("watchdog 15", 0);
	printf("watchdog enabled ,15s\n");
#ifdef CONFIG_HARDWARE_ID
	pri_hardware_id();
	config_dev_board();
#endif
#ifdef CONFIG_CMD_WOL_POWER
	/* HERE should read idme wol power config
	DEFAULT is disable
	*/
	// SET WOL POWER
	run_command("wol_power disable", 0);
#endif

	/*read reboot mode*/
	uint32_t reboot_mode_val = ((readl(AO_SEC_SD_CFG15) >> 12) & 0xf);
	printf("******reboot_mode_val: %d\n", reboot_mode_val);


	uint32_t val = 0;

	// save GPIOAO_11 and GPIOAO_4 status
	val = readl(AO_GPIO_CNTL_SEL);
	val |= ((1<<10) | (1<<4));
	writel(val, AO_GPIO_CNTL_SEL);


	int bl_status = get_lcd_bl_status();

#ifdef CONFIG_PXP_EMULATOR
	printf("\naml log : 20200211 bring up for TM2 revB@board_late_init\n");
#else
	TE(__func__);
	char outputModePre[30] = {0};
	char outputModeCur[30] = {0};
	outputModePre[29] = '\0';
	outputModeCur[29] = '\0';
	strncpy(outputModePre,getenv("outputmode"),sizeof(outputModePre)-1);

#ifdef  CONFIG_LOGOPARAM_ENABLE
    //update env before anyone using it
    run_command("get_rebootmode; echo reboot_mode=${reboot_mode}; "\
                        "if test ${reboot_mode} = factory_reset; then "\
                        "defenv_reserv;setlogo logoparam.var.upgrade_step 2;save; fi;", 0);

    run_command("if itest ${upgrade_step} == 1; then "\
                        "defenv_reserv;setlogo logoparam.var.upgrade_step 2;saveenv; fi;", 0);
#else
	//update env before anyone using it
	run_command("get_rebootmode; echo reboot_mode=${reboot_mode}; "\
						"if test ${reboot_mode} = factory_reset; then "\
						"defenv_reserv;setenv upgrade_step 2;save; fi;", 0);
	run_command("if itest ${upgrade_step} == 1; then "\
						"defenv_reserv; setenv upgrade_step 2; saveenv; fi;", 0);
#endif

#ifdef CONFIG_PWM_LED
dd
	update_led();
#endif

	/*add board late init function here*/
	//run_command("env default -a;saveenv;", 0);

#ifdef CONFIG_LOGOPARAM_ENABLE
    // set outputmode to default
    run_command("setlogo logoparam.var.outputmode ${outputmode}", 0);
#endif

#ifndef DTB_BIND_KERNEL
	int ret;
	printf(" store dtb read\n");
	ret = run_command("store dtb read $dtb_mem_addr", 1);
	if (ret) {
		printf("%s(): [store dtb read $dtb_mem_addr] fail\n", __func__);
#ifdef CONFIG_DTB_MEM_ADDR
		char cmd[64];
		printf("load dtb to %x\n", CONFIG_DTB_MEM_ADDR);
		sprintf(cmd, "store dtb read %x", CONFIG_DTB_MEM_ADDR);
		ret = run_command(cmd, 1);
		if (ret) {
			printf("%s(): %s fail\n", __func__, cmd);
		}
#endif
	}
#elif defined(CONFIG_DTB_MEM_ADDR)
	{
		char cmd[128];
		int ret;

		// Set active_slot if available
		run_command("get_valid_slot", 0);

		if (!getenv("dtb_mem_addr")) {
			sprintf(cmd, "setenv dtb_mem_addr 0x%x", CONFIG_DTB_MEM_ADDR);
			run_command(cmd, 0);
		}
		if (active_slot == 'a') {
			printf(" imgread dtb boot_a\n");
			sprintf(cmd, "imgread dtb boot_a ${dtb_mem_addr}");
		} else if (active_slot == 'b') {
			printf(" imgread dtb boot_b\n");
			sprintf(cmd, "imgread dtb boot_b ${dtb_mem_addr}");
		} else {
			printf(" imgread dtb boot\n");
			sprintf(cmd, "imgread dtb boot ${dtb_mem_addr}");
		}
		ret = run_command(cmd, 0);
		if (ret) {
			printf("%s(): cmd[%s] fail, ret=%d\n", __func__, cmd, ret);
		}
	}
#endif// #ifndef DTB_BIND_KERNEL
#if defined(CONFIG_IDME)
	if (store_demo_mode()){
		setenv("bypass_standby", "1");
	}

	if (!idme_get_var_external("model_name", buf, sizeof(buf))) {
		printf("get idme model_name: %s\n", buf);
		if(strcmp(buf, "APTIV")==0)
			is_ru = true;
		if(strcmp(buf, "NOPANEL")==0)
			is_headless = true;
	}
	printf("amz_dev_flags_check: 0x%lu\n", amz_dev_flags_check());
#endif

#if defined(UBOOT_TARGET_PRODUCT_NAME_ABC)
	check_usb_str();
#endif

	/* load unifykey */
	run_command("keyunify init 0x1234", 0);
	get_build_tag();
	set_dolby_status();
	set_dts_status();

#ifdef CONFIG_AML_VPU
	vpu_probe();
#endif
	vpp_init();
	update_tvconfig(hwid);
	run_command("ini_model", 0);
#ifdef CONFIG_AML_HDMITX20
	hdmi_tx_set_hdmi_5v();
	hdmi_tx_init();
#endif
#ifdef CONFIG_AML_CVBS
	run_command("cvbs init", 0);
#endif
#ifdef CONFIG_AML_LCD
	lcd_probe();
#endif

	//set backlight status, this is status before reboot
	setenv("bl_status", bl_status ? "1":"0");

#ifdef CONFIG_AML_V2_FACTORY_BURN
	if (0x1b8ec003 == readl(P_PREG_STICKY_REG2))
		aml_try_factory_usb_burning(1, gd->bd);
		aml_try_factory_sdcard_burning(0, gd->bd);
        if (0x1b8ec004 == readl(P_PREG_STICKY_REG2)) {
                writel(0x0, P_PREG_STICKY_REG2);
                run_command("run storeargs;fastboot", 0);
                printf("run storeargs fastboot\n");
        }
#endif// #ifdef CONFIG_AML_V2_FACTORY_BURN

	TE(__func__);
#endif
	strncpy(outputModeCur,getenv("outputmode"),sizeof(outputModeCur)-1);

	if (strcmp(outputModeCur,outputModePre)) {
		printf("uboot outputMode change saveenv old:%s - new:%s\n",outputModePre,outputModeCur);
		run_command("saveenv", 0);
	}
	return 0;
}
#endif

#ifdef CONFIG_AML_TINY_USBTOOL
int usb_get_update_result(void)
{
#ifdef CONFIG_LOGOPARAM_ENABLE
	unsigned long upgrade_step;

	upgrade_step = get_logoparam_value("logoparam.var.upgrade_step");
	if (upgrade_step != NULL) {
		printf("upgrade_step: %s\n", upgrade_step);

		upgrade_step = simple_strtoul (upgrade_step, NULL, 16);
		printf("upgrade_step = %d\n", (int)upgrade_step);
		if (upgrade_step == 1)
		{
			run_command("defenv", 1);
			run_command("setenv upgrade_step 2", 1);
			run_command("setlog logoparam.var.upgrade_step", "2", 1);
			run_command("saveenv", 1);
			return 0;
		}
	}
	return -1;
#else
	unsigned long upgrade_step;
	upgrade_step = simple_strtoul (getenv ("upgrade_step"), NULL, 16);
	printf("upgrade_step = %d\n", (int)upgrade_step);
	if (upgrade_step == 1)
	{
		run_command("defenv", 1);
		run_command("setenv upgrade_step 2", 1);
		run_command("saveenv", 1);
		return 0;
	}
	else
	{
		return -1;
	}
#endif
}
#endif

phys_size_t get_effective_memsize(void)
{
	// >>16 -> MB, <<20 -> real size, so >>16<<20 = <<4
#if defined(CONFIG_SYS_MEM_TOP_HIDE)
	return (((readl(AO_SEC_GP_CFG0)) & 0xFFFF0000) << 4) - CONFIG_SYS_MEM_TOP_HIDE;
#else
	return (((readl(AO_SEC_GP_CFG0)) & 0xFFFF0000) << 4);
#endif
}

#ifdef CONFIG_MULTI_DTB
#if defined(CONFIG_IDME)
bool isevolve = 0;
bool isevolve_10 = 0;
bool isevolve_10_v2 = 0;
bool isevolve_10_war = 0;

void evolvecheck(void)
{
        char buf[256] = "";

        if (!idme_get_var_external("product_model", buf, sizeof(buf))) {
                printf("get idme model_name: %s\n", buf);

                if(strcmp(buf, "10_v2")==0)
                        isevolve_10_v2 = true;
        }

        if (!idme_get_var_external("model_name", buf, sizeof(buf))) {
                printf("get idme model_name: %s\n", buf);

                if(strcmp(buf, "HD")==0 || strcmp(buf, "1080P")==0 || strcmp(buf, "1080P_WAR")==0)
                        isevolve = true;

		if(strcmp(buf, "1080P")==0)
			isevolve_10 = true;

		if(strcmp(buf, "1080P_WAR") == 0)
			isevolve_10_war = true;

                if((board_id_type_check() == HVT_BOARD_ID_TYPE) || (strstr(buf, "_AMAZON_") != NULL)){
                        run_command("setenv logo_name amazonboot", 1);
                }else{
                        printf("set default logo ! \n");
                        run_command("setenv logo_name amazonboot", 1);
                }
        }else{
                printf("can not get model_name ! \n");
        }
}
#endif

int checkhw(char * name)
{
	char loc_name[64] = {0};
	/* add your logic code here */
	cpu_id_t cpu_id = get_cpu_id();

	if (MESON_CPU_MAJOR_ID_TM2 == cpu_id.family_id) {
		switch (cpu_id.chip_rev) {
			case 0xA:
				strcpy(loc_name, "tm2_t962x3_ab301\0");
			break;
			case 0xB:
				strcpy(loc_name, "tm2-revb_t962x3_ab301\0");
			break;
			default:
				strcpy(loc_name, "tm2_t962e2_unsupport");
			break;
		}
	}
#if defined(CONFIG_IDME)
	printf("chip version = %d\n", cpu_id.chip_rev);
	if (MESON_CPU_MAJOR_ID_TM2 == cpu_id.family_id) {
		switch (cpu_id.chip_rev) {
			case 0xA:
				strcpy(loc_name, "tm2reva_lagunaf_32b\0");
				setenv("cpu_version", "rev_a");
			break;
			case 0xB:
				strcpy(loc_name, "tm2revb_lagunaf_32b\0");
				setenv("cpu_version", "rev_b");
			break;
			default:
				strcpy(loc_name, "tm2reva_lagunaf_32b");
				setenv("cpu_version", "rev_a");
			break;
		}
	}

	if((board_id_type_check() == REF_BOARD_ID_TYPE)){
		memset(loc_name,0,64);
		strcpy(loc_name, "tm2reva_lagunaf_32b\0");
	}
	if(is_ru){
                memset(loc_name,0,64);
                strcpy(loc_name, "tm2revb_lagunaf_ru_32b\0");
	}

	if(is_headless){
                memset(loc_name,0,64);
                strcpy(loc_name, "tm2revb_crane_32b\0");
	}

	evolvecheck();

	if(isevolve) {
		memset(loc_name,0,64);
		if(board_id_type_check() >= DVT_BOARD_ID_TYPE) {
			if(isevolve_10)
				strcpy(loc_name, "tm2revb_evolve10_dv\0");
			else if(isevolve_10_war)
				strcpy(loc_name, "tm2revb_evolve10_war\0");
			else
				strcpy(loc_name, "tm2revb_evolve_dv\0");
		} else {
                        if(isevolve_10_v2)
				strcpy(loc_name, "tm2revb_evolve10v2_32b\0");
                        else
				strcpy(loc_name, "tm2revb_evolve_32b\0");
		}
	}

#endif
	/* set aml_dt */
	strcpy(name, loc_name);
	setenv("aml_dt", loc_name);
	return 0;
}
#endif

extern int do_setMtkBT( cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
U_BOOT_CMD(
	setMtkBT, CONFIG_SYS_MAXARGS, 1, do_setMtkBT,
	"load MTK BT driver, and set woble\n",
	NULL
);

const char * const _env_args_reserve_[] =
{
		"aml_dt",
		"firstboot",
		"lock",
		"upgrade_step",
		"model_name",

		NULL//Keep NULL be last to tell END
};



static int get_logo_filepath(char *logo_path, int size)
{
	int ret = -1;
	char model_name[256] = "";

#if defined(CONFIG_IDME)
	if (idme_get_var_external("model_name", model_name, sizeof(model_name)) != 0) {
		printf("can not get model_name ! \n");
		return ret;
	}
	printf("get idme model_name: %s\n", model_name);
#else
	#error "Must defined CONFIG_IDME"
#endif

	const char *ini_value = NULL;
	IniParserInit();

	if (IniParseFile(get_model_sum_path()) < 0) {
		printf("%s, model ini load file error!\n", __func__);
		goto exit;
	}

	ini_value = IniGetString(model_name, "BOOTUP_LOGO_FILE_PATH", "null");
	if (strcmp(ini_value, "null") == 0) {
		printf("%s, get \"BOOTUP_LOGO_FILE_PATH\" item failed!\n", __func__);
		goto exit;
	}

	memset(logo_path, 0 , size);
	strncpy(logo_path, ini_value, size - 1);

	ret = 0;
exit:
	IniParserUninit();
	return ret;
}


static int do_logo_display(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]) {
    char cmd_buf[512]     = { 0 };
    char logo_path[256]   = { 0 };
    int  file_size        = 0;
    unsigned int mem_addr = 0;

    if (argv[1] == NULL) {
        return 1;
    }

    if (get_logo_filepath(logo_path, sizeof(logo_path)) != 0 ) {
        printf("Can't get logo path\n");
        goto read_logo;
    }

    file_size = iniGetFileSize(logo_path);
    if (file_size <= 0) {
        printf("Can't get logo file size\n");
        goto read_logo;
    }
    char * loadaddr = getenv("loadaddr");
    if (loadaddr == NULL) {
        printf("Can't get logo memory addr\n");
        goto read_logo;
    }

    mem_addr = simple_strtoul(loadaddr, NULL, 16);
    if (iniReadFileToBuffer(logo_path, 0, file_size, mem_addr) <=0 ) {
        printf("Read logo file error\n");
        goto read_logo;
    }

    printf("Show logo from tvconfig\n");
    sprintf(cmd_buf, "bmp display $loadaddr");
    run_command(cmd_buf, 0);
    return 0;

read_logo:
    printf("Show logo from logo partition\n");
    sprintf(cmd_buf, "imgread pic logo %s $loadaddr", argv[1]);
    run_command(cmd_buf, 0);

    sprintf(cmd_buf, "bmp display $%s_offset", argv[1]);
    run_command(cmd_buf, 0);
    return 0;
}

U_BOOT_CMD(
    logo_display, 3, 0, do_logo_display,
    "logo_display",
    "logo_display\n"
);

#ifdef UFBL_FEATURE_ONETIME_UNLOCK
int do_onetimeunlock(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    int ret = -1;
    unsigned char *b64 , new_p;
    unsigned int out_len;

    if (argc < 2) {
		ret = -1;
		goto done;
    }

    if (!strcmp(argv[1], "getcode")) {
        unsigned char one_tu_code[ONETIME_UNLOCK_CODE_LEN + 1] = {0};
        unsigned int unlock_code_len = sizeof(one_tu_code);

        if (amzn_get_one_tu_code(one_tu_code, &unlock_code_len)) {
	    printf("cannot get onetime unlock code\n");
	    ret = -2;
            goto done;
         } else {
	    printf("%s\n", one_tu_code);
         }
    } else if (!strcmp(argv[1], "setcode")) {

        if(argc < 3) {
	    ret = -3;
            goto done;
        }
        b64 = (unsigned char *)argv[2];
	out_len = strlen(b64);

        if (amzn_set_onetime_unlock_code(b64, out_len)) {
            printf("set onetime unlock code error\n");
	    ret = -4;
            goto done;
        } else {
            printf("set onetime unlock code OKAY\n");
        }
    } else if (!strcmp(argv[1], "setcert")) {

        if(argc < 3) {
	    ret = -5;
            goto done;
        }
        b64 = (unsigned char *)argv[2];
	out_len = strlen(b64);
	// do not need to free in uboot
	new_p = malloc(out_len+1);
	memset(new_p,0,out_len+1);
	memcpy(new_p,b64,out_len);
        if (amzn_set_onetime_unlock_cert(new_p, out_len)) {
            printf("set onetime unlock cert error\n");
	    ret = -6;
            goto done;
        } else {
            printf("set onetime unlock cert OKAY\n");
        }
    } else {
	ret = -10;
	goto done;
    }
	ret = 0;
done:
    if (ret)
	printf("do_onetimeunlock fail: %d\n", ret);
    else
	printf("do_onetimeunlock pass\n");
    return 0;
}

U_BOOT_CMD(
     onetimeunlock ,    CONFIG_SYS_MAXARGS,    1,     do_onetimeunlock,
     "onetimeunlock   - one time unlock\n",
     "[onetimeunlock getcode]\n"
     "[onetimeunlock setcode signed_code]\n"
     "[onetimeunlock setcert signed_cert]\n"
);

#endif
