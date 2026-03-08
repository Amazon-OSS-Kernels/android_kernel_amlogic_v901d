/* Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved. */
#include "amzn_multiconfigs.h"
#include <common.h>
#include "ini/ini_proxy.h"
#include "ini/ini_platform.h"
#include <asm/arch/io.h>
#include <vsprintf.h>
#include "aes.h"
#include <config.h>
#include <malloc.h>
#include <part.h>
#include <aboot.h>
#include <sparse_format.h>
#include <mmc.h>
#include <amlogic/aml_mmc.h>
#include <idme.h>
#include <fb_mmc.h>

#define VERSION_INI_PATH	"/tvconfig/ready.ini"
#define OEM_INI_PATH		"/oemconfig/multi_oem.ini"
#define FILE_READ_ADDR		0x20000000
#define FILE_UNZIP_ADDR		0x30000000
#define FILE_DECRYPT_ADDR	0x10000000 //0x4000000
#define TVCONFIG_PART		"tvconfig"
#define HWID_TAG		"MODEL_HWID_"
#define HWID_MAX		40
#define TVCONFIG_NUM		40
#define PACKAGE_FILE_PATH	"/oemconfig/final.img"
#define GUNZIP_BUF_SIZE		0x10000000 //256M

#define DEBUG			1

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s():", __func__); printf(fmt, ##args);} while (0)
#else
#define debugf(fmt,args...)
#endif

#define IS_GZIP_FORMAT(data)     ((data & (0x0000FFFF)) == (0x00008B1F))
#define strtoul simple_strtoul
//#define DIV_ROUND_UP(n,d)  (((n) + (d) -1)/(d))
struct oem_tv_config
{
	unsigned int oem_id;
	unsigned int img_offset;
	unsigned int img_size;
	unsigned int reserve;
};
struct oem_tv_config oem_configs[TVCONFIG_NUM];

struct module_oemid
{
	char		name[128];
	char		hwid[10];
	unsigned int	oem_id;
	unsigned char   config[16];
	unsigned char	key[16];
};

struct module_oemid module_list[] =
{
	//	{"Bride"	,	"1110"	,	1,	"agcei516",		{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}},
	{"ABC"	,	"1100"	,	1,	"_AMAZON",		{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}},
	{"ABC"     ,       "1100"  ,       1,      "_2RG",              	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}},
	{"ABC"     ,       "1100"  ,       1,      "_1RG",              	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}},
	{"ABC"     ,       "1100"  ,       2,      "_APB6C13",		{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}},
	{"ABC"     ,       "1100"  ,       3,      "_AKAI7G26",            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}},
	{"NULL"		,	"NULL"	,	0,	"null",			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}
};


static void tvconfig_write_sparse_image(block_dev_desc_t *dev_desc,
		disk_partition_t *info, const char *part_name,
		void *data, unsigned sz)
{
	lbaint_t blk;
	lbaint_t blkcnt;
	lbaint_t blks;
	uint64_t bytes_written = 0;
	unsigned int chunk;
	uint64_t chunk_data_sz;
	uint32_t *fill_buf = NULL;
	uint32_t fill_val;
	sparse_header_t *sparse_header;
	chunk_header_t *chunk_header;
	uint32_t total_blocks = 0;
	int i;

	/* Read and skip over sparse image header */
	sparse_header = (sparse_header_t *) data;

	data += sparse_header->file_hdr_sz;
	if (sparse_header->file_hdr_sz > sizeof(sparse_header_t))
	{
		/*
		 * Skip the remaining bytes in a header that is longer than
		 * we expected.
		 */
		data += (sparse_header->file_hdr_sz - sizeof(sparse_header_t));
	}

	debug("=== Sparse Image Header ===\n");
	debug("magic: 0x%x\n", sparse_header->magic);
	debug("major_version: 0x%x\n", sparse_header->major_version);
	debug("minor_version: 0x%x\n", sparse_header->minor_version);
	debug("file_hdr_sz: %d\n", sparse_header->file_hdr_sz);
	debug("chunk_hdr_sz: %d\n", sparse_header->chunk_hdr_sz);
	debug("blk_sz: %d\n", sparse_header->blk_sz);
	debug("total_blks: %d\n", sparse_header->total_blks);
	debug("total_chunks: %d\n", sparse_header->total_chunks);

	/* verify sparse_header->blk_sz is an exact multiple of info->blksz */
	if (sparse_header->blk_sz !=
	    (sparse_header->blk_sz & ~(info->blksz - 1))) {
		printf("%s: Sparse image block size issue [%u]\n",
		       __func__, sparse_header->blk_sz);
		return;
	}

	puts("Flashing Sparse Image\n");

	/* Start processing chunks */
	blk = info->start;
	for (chunk=0; chunk<sparse_header->total_chunks; chunk++)
	{
		/* Read and skip over chunk header */
		chunk_header = (chunk_header_t *) data;
		data += sizeof(chunk_header_t);

		if (chunk_header->chunk_type != CHUNK_TYPE_RAW) {
			debug("=== Chunk Header ===\n");
			debug("chunk_type: 0x%x\n", chunk_header->chunk_type);
			debug("chunk_data_sz: 0x%x\n", chunk_header->chunk_sz);
			debug("total_size: 0x%x\n", chunk_header->total_sz);
		}

		if (sparse_header->chunk_hdr_sz > sizeof(chunk_header_t))
		{
			/*
			 * Skip the remaining bytes in a header that is longer
			 * than we expected.
			 */
			data += (sparse_header->chunk_hdr_sz -
				 sizeof(chunk_header_t));
		}

		chunk_data_sz = (uint64_t)sparse_header->blk_sz * (uint64_t)chunk_header->chunk_sz;
		blkcnt = chunk_data_sz / info->blksz;
		switch (chunk_header->chunk_type)
		{
			case CHUNK_TYPE_RAW:
			if (chunk_header->total_sz !=
			    (sparse_header->chunk_hdr_sz + chunk_data_sz))
			{
				printf(
					"Bogus chunk size for chunk type Raw");
				return;
			}

			if (blk + blkcnt > info->start + info->size) {
				printf(
				    "%s: Request would exceed partition size!\n",
				    __func__);
				return;
			}

			blks = dev_desc->block_write(dev_desc->dev, blk, blkcnt,
						     data);
			if (blks != blkcnt) {
				printf("%s: Write failed " LBAFU "\n",
				       __func__, blks);
				return;
			}
			blk += blkcnt;
			bytes_written += blkcnt * info->blksz;
			total_blocks += chunk_header->chunk_sz;
			data += chunk_data_sz;
			break;

			case CHUNK_TYPE_FILL:
			if (chunk_header->total_sz !=
			    (sparse_header->chunk_hdr_sz + sizeof(uint32_t)))
			{
				printf(
					"Bogus chunk size for chunk type FILL\n");
				return;
			}

			fill_buf = (uint32_t *)
				   memalign(ARCH_DMA_MINALIGN,
					    ROUNDUP(info->blksz,
						    ARCH_DMA_MINALIGN));
			if (!fill_buf)
			{
				printf(
					"Malloc failed for: CHUNK_TYPE_FILL\n");
				return;
			}

			fill_val = *(uint32_t *)data;
			data = (char *) data + sizeof(uint32_t);

			for (i = 0; i < (info->blksz / sizeof(fill_val)); i++)
				fill_buf[i] = fill_val;

			if (blk + blkcnt > info->start + info->size) {
				printf(
				    "%s: Request would exceed partition size!\n",
				    __func__);
				return;
			}

			for (i = 0; i < blkcnt; i++) {
				blks = dev_desc->block_write(dev_desc->dev,
							     blk, 1, fill_buf);
				if (blks != 1) {
					printf(
					    "%s: Write failed, block # " LBAFU "\n",
					    __func__, blkcnt);
					free(fill_buf);
					return;
				}
				blk++;
			}
			bytes_written += blkcnt * info->blksz;
			total_blocks += chunk_data_sz / sparse_header->blk_sz;

			free(fill_buf);
			break;

			case CHUNK_TYPE_DONT_CARE:
			blk += blkcnt;
			total_blocks += chunk_header->chunk_sz;
			break;

			case CHUNK_TYPE_CRC32:
			if (chunk_header->total_sz !=
			    sparse_header->chunk_hdr_sz)
			{
				printf(
					"Bogus chunk size for chunk type Dont Care");
				return;
			}
			total_blocks += chunk_header->chunk_sz;
			data += chunk_data_sz;
			break;

			default:
			printf("%s: Unknown chunk type: %x\n", __func__,
			       chunk_header->chunk_type);
			return;
		}
	}

	debug("Wrote %d blocks, expected to write %d blocks\n",
	      total_blocks, sparse_header->total_blks);
	printf("........ wrote %u bytes to '%s'\n", (int)bytes_written, part_name);

	if (total_blocks != sparse_header->total_blks)
		printf("sparse image write failure");

	return;
}




static void tvconfig_write_raw_image(block_dev_desc_t *dev_desc, disk_partition_t *info,
                const char *part_name, void *buffer,
                unsigned int download_bytes)
{
	lbaint_t blkcnt;
	lbaint_t blks;

	/* determine number of blocks to write */
	blkcnt = ((download_bytes + (info->blksz - 1)) & ~(info->blksz - 1));
	blkcnt = blkcnt / info->blksz;

	if (blkcnt > info->size) {
		error("too large for partition: '%s'\n", part_name);
		return;
	}

	puts("Flashing Raw Image\n");

        blks = dev_desc->block_write(dev_desc->dev, info->start, blkcnt,
	                             buffer);
	if (blks != blkcnt) {
		error("failed writing to device %d\n", dev_desc->dev);
		return;
	}

	printf("........ wrote " LBAFU " bytes to '%s'\n", blkcnt * info->blksz,
               part_name);
}


void tvconfig_mmc_flash_write(const char *cmd, void *download_buffer,
                        unsigned int download_bytes)
{
	block_dev_desc_t *dev_desc;
	disk_partition_t info;
	int ret = 0;

	dev_desc = get_dev("mmc", CONFIG_FASTBOOT_FLASH_MMC_DEV);
	if (!dev_desc || dev_desc->type == DEV_TYPE_UNKNOWN) {
		error("invalid mmc device\n");
		return;
	}
#ifdef CONFIG_AML_PARTITION
	if (get_partition_info_aml_by_name(dev_desc, cmd, &info)) {
		error("cannot find partition: '%s'\n", cmd);
		return;
	}
#endif
	if (is_sparse_image(download_buffer))
		tvconfig_write_sparse_image(dev_desc, &info, cmd, download_buffer,
					download_bytes);
	else
		tvconfig_write_raw_image(dev_desc, &info, cmd, download_buffer,
					download_bytes);
}

static int get_oem_index(const char *hwid)
{
	int i = 0;
	unsigned char buf[256] = "";
	idme_get_var_external("model_name", buf, sizeof(buf));
        debugf("model_name = %s , hwid = %s\n", buf, hwid);
	while(strcmp(module_list[i].hwid, "NULL")!=0)
	{
		debugf("multiconfig = %s, hwid = %s \n", module_list[i].config, module_list[i].hwid);
		if((strcmp(module_list[i].hwid, hwid)==0)&&(strstr(buf,module_list[i].config)!=NULL))
		return i;
		i++;
	}
	return -1;
}
static int parse_current_ver(const char *file_name, const char *brand_name, unsigned char version[])
{
	const char *ini_value = NULL;
	IniParserInit();

	if (IniParseFile(file_name) < 0) {
		debugf("%s, ready ini load file error!\n", __func__);
		IniParserUninit();
		return -1;
	}
	ini_value = IniGetString(brand_name, "VERSION", "null");
	if (strcmp(ini_value, "null") != 0)
	{
		//*version = strtoul(ini_value, NULL, 10);
		strcpy(version, ini_value);
		debugf("current version =%s, cpy version=%s\n", ini_value, version);
		IniParserUninit();
		return 0;
	}

	IniParserUninit();
	return -1;
}

static int parse_package_ver(const char *file_name, const char *brand_name, const char *hwid, unsigned char version[])
{
	const char *ini_value = NULL;
	unsigned int i;
	char  hwid_buf[64];

	IniParserInit();
	memset(hwid_buf, 0 , sizeof(hwid_buf));
	if (IniParseFile(file_name) < 0) {
		debugf("%s, multi_oem ini load file error!\n", __func__);
		IniParserUninit();
		return -1;
	}
	ini_value = IniGetString(brand_name, "VERSION", "null");
	if (strcmp(ini_value, "null") != 0)
	{
		//*version = strtoul(ini_value, NULL, 10);
               	strcpy(version, ini_value);
		debugf("package  version =%s, cpy version=%s\n", ini_value, version);
		IniParserUninit();
		return 0;
		/*
		//check hwid
		for(i=1; i<HWID_MAX; i++)
		{
			sprintf(hwid_buf, "MODEL_HWID_%d", i);
			ini_value = IniGetString(brand_name, hwid_buf, "null");
			debugf("hwID ini_value = %s\n" , ini_value);
			if (strcmp(ini_value, "null") != 0)
			{
				//hwid supported
				if(strcmp(ini_value,hwid)==0)
				return 0;
			}
			else
			{
				//hit bottom
				debugf("hwid is not supported in package \n");
				IniParserUninit();
                		return -1;
			}
		}
		*/
	}
	debugf("model is not supported in package \n");
	IniParserUninit();
	return -1;
}

void update_tvconfig(const char *hwid)
{
	unsigned int dt_magic = 0, gzip_format = 0, read_size = 0, file_size = 0;
	int current_ret = -1, package_ret = -1, i = 0, block_nums, oem_index;
	unsigned char current_version[128] = "", package_version[128] = "";
	unsigned char key_exp[AES_EXPAND_KEY_LENGTH] = "";
	unsigned long img_size = 0, img_offset = 0, unzip_size = GUNZIP_BUF_SIZE;
        char* oem_name;
#ifdef CONFIG_IDME
	int is_fos_bootmode = 0,is_transition_done = 0,bootmode=1;
	char transition_str[8] = {0};
	bootmode = idme_boot_mode();
	is_fos_bootmode = (bootmode == IDME_BOOTMODE_NORMAL);
	if (idme_get_var_external("transition_done", transition_str, sizeof(transition_str)) == 0)
		is_transition_done = !strncmp(transition_str, "1", 1);
#endif
	if((oem_index=get_oem_index(hwid))<0)
	{
		printf("no supported oem id found \n");
		return;
	}
	oem_name = simple_itoa(module_list[oem_index].oem_id);
        debugf("oem_index = %d, oem_name = %s\n", oem_index, oem_name);
	//check current version
	current_ret = parse_current_ver(VERSION_INI_PATH, "Current", current_version);
	package_ret = parse_package_ver(OEM_INI_PATH, oem_name, hwid, package_version);
	debugf("current version =%s, package version =%s \n", current_version, package_version);
	//compare with package version
	if(package_ret!= 0)
	{
		printf("no update, no support in package\n");
		return;
	}
	if((current_ret == 0)&&(strcmp(current_version,package_version)==0))
	{
		printf("no update, already latest ver \n");
#ifdef CONFIG_IDME
		if(is_fos_bootmode&&is_transition_done)
			current_ret = run_command("store erase partition oemconfig", 0);
#endif
		return;
	}
	//update needed
	if(!iniIsFileExist(PACKAGE_FILE_PATH))
	{
		printf("no update, package file is not exist \n");
		return;
	}
	file_size = iniGetFileSize(PACKAGE_FILE_PATH);
	read_size = iniReadFileToBuffer(PACKAGE_FILE_PATH,0,file_size, (void *)FILE_READ_ADDR);
	memcpy((void *)oem_configs, (void *)FILE_READ_ADDR, sizeof(oem_configs));
	printf("read final.img size = %d \n", read_size);
	flush_cache(FILE_READ_ADDR,read_size);
	/*
	for(i = 0; i < 3; i++)
	{
		debugf("id = %d , offset = 0x%x, size = 0x%x, reserved = %d \n", oem_configs[i].oem_id, oem_configs[i].img_offset,oem_configs[i].img_size,oem_configs[i].reserve);
	}
	*/
	i = 0;
	//search oem_id
	while((i < TVCONFIG_NUM)&&(oem_configs[i].oem_id!=0))
	{
		if(module_list[oem_index].oem_id==oem_configs[i].oem_id)
		{
			img_size = oem_configs[i].img_size;
			img_offset =  oem_configs[i].img_offset;
			break;
		}
			i++;
	}
	if(img_size==0)
	{
		printf("no img found \n");
		return;
	}
	debugf("start = 0x%x, size =0x%x , oemid = %d\n", img_offset, img_size, module_list[oem_index].oem_id);
	debugf("decrypting....\n");
	block_nums = DIV_ROUND_UP(img_size,16);
	debugf("round size = 0x%x\n", block_nums*16);
	aes_expand_key(module_list[oem_index].key,key_exp);
	aes_cbc_decrypt_blocks(key_exp, (void *)FILE_READ_ADDR+img_offset, (void *)FILE_DECRYPT_ADDR, block_nums);
	flush_cache(FILE_DECRYPT_ADDR, block_nums*16);
	//flush_dcache_all();
	dt_magic = readl(FILE_DECRYPT_ADDR);
	gzip_format = IS_GZIP_FORMAT(dt_magic);
	if(gzip_format)
	{
		debugf(" GZIP format decompress... \n");
		if(gunzip((void *)FILE_UNZIP_ADDR,GUNZIP_BUF_SIZE,(void *)(FILE_DECRYPT_ADDR),&unzip_size)!=0)
		{
			printf("unzip failed \n");
			return;
		}
		debugf("unzip_size = 0x%x \n", unzip_size);
		if(unzip_size > GUNZIP_BUF_SIZE)
		{
			debugf(" Wanring! GUNZIP overflow... \n");
		}
		//erase logo part
		debugf("erasing tvconfig...\n");
		current_ret = run_command("store erase partition tvconfig", 0);
		if(current_ret)
		{
			printf("erase tvconfig partition failed\n");
			return;
		}
		mdelay(10);
		flush_cache(FILE_UNZIP_ADDR,unzip_size);
		//write back to logo
		debugf("writing back to tvconfig...\n");
		tvconfig_mmc_flash_write("tvconfig", (void *)FILE_UNZIP_ADDR, unzip_size);
		//fb_mmc_flash_write("tvconfig", (void *)FILE_UNZIP_ADDR, unzip_size);
		/*
		memset(cmd, 0 , sizeof(cmd));
		sprintf(cmd, "store write tvconfig 0x%x 0 0x%x", FILE_UNZIP_ADDR, unzip_size);
		debugf("store cmd = %s \n", cmd);
		current_ret = run_command(cmd,0);
		if(current_ret)
		{
			printf("store tvconfig partition failed\n");
			return -1;
		}
		*/
		debugf("update success!\n");
#ifdef CONFIG_IDME
		if(is_fos_bootmode&&is_transition_done)
			current_ret = run_command("store erase partition oemconfig", 0);
#endif
		return;
	}
	printf("no valid gzip package\n");
	return;
}
