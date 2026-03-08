/*
 * uboot log driver.
 *
 * This module use to export uboot log to user space
 *
 * Copyright (C) 2019-2020 amazon
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/memblock.h>
#include <linux/device.h>
#include <linux/io.h>
 #include <linux/errno.h>

#include <linux/of.h>
#include <linux/of_reserved_mem.h>

#define ULOG_COOKIE    0x474f4c55 /* "ULOG" in ASCII */
struct uboot_log {
	struct uboot_log_header {
		unsigned int cookie;
		unsigned int max_size;
		unsigned int size_written;
		unsigned int idx;
	} header;
	char data[0];
};

static phys_addr_t log_paddr;
static unsigned long log_size;
static void *log_vaddr;

static int mmap_uboot_log(void)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int npages;
	pgprot_t prot;
	unsigned int i;
	phys_addr_t start = log_paddr;
	unsigned long size = log_size;

	printk("uboot_log log_paddr   %p\n", (void *)log_paddr);
	/* mmap bl_log header */
	page_start = start - offset_in_page(start);
	npages = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_noncached(PAGE_KERNEL);
	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return -ENOMEM;

	for (i = 0; i < npages; i++) {
		phys_addr_t addr;

		addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	log_vaddr = vmap(pages, npages, VM_MAP, prot);
	printk("uboot_log log_vaddr   %p\n", log_vaddr);
	vfree(pages);
	if (!log_vaddr) {
		pr_err("%s: Failed to map %u pages\n", __func__, npages);
		return -ENOMEM;
	}

	return 0;
}

static int uboot_log_show(struct seq_file *m, void *v)
{
	unsigned int log_len;
	struct uboot_log *uboot_log_hr;
	int ret = 0;

	if (!log_vaddr)
		ret = mmap_uboot_log();
	if (ret)
		return 0;

	uboot_log_hr = (struct uboot_log *)((char *)log_vaddr + offset_in_page(log_paddr));

	if (ULOG_COOKIE == uboot_log_hr->header.cookie) {
		log_len = min(uboot_log_hr->header.size_written, uboot_log_hr->header.max_size);
		seq_write(m, uboot_log_hr->data, log_len);

	} else{
		printk("ulog magic number 0x%x\n", uboot_log_hr->header.cookie);
		seq_printf(m, "error !");

	}

	return 0;
}

static int uboot_log_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, uboot_log_show, inode->i_private);
}

static const struct file_operations uboot_log_file_ops = {
	.owner = THIS_MODULE,
	.open = uboot_log_file_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init uboot_log_init(void)
{
	struct proc_dir_entry *entry;

	if (log_paddr && log_size) {
		entry = proc_create("bl_log", 0444, NULL, &uboot_log_file_ops);
		if (!entry)
			pr_err("uboot_log_init: failed to create proc entry\n");
	}
	printk("uboot_log_init\n");
	return 0;
}

static void __exit uboot_log_exit(void)
{
	remove_proc_entry("bl_log", NULL);
	if (log_vaddr)
		vunmap(log_vaddr);
}

module_init(uboot_log_init);
module_exit(uboot_log_exit);
/*
static int __init uboot_log_setup(char *str)
{
	char *tmp;
	int ret;

	tmp = strchr(str, ',');
	*tmp = 0;
	tmp++;

	ret = kstrtoul(str, 16, (unsigned long *)&log_paddr);
	if (ret)
		return 1;
	ret = kstrtoul(tmp, 10, &log_size);
	if (ret)
		return 1;

	memblock_reserve(log_paddr, log_size);
	return 1;
}
__setup("uboot_log=", uboot_log_setup);

*/
static int uboot_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	printk("uboot memory init!\n");
	return 0;
}

static const struct reserved_mem_ops rmem_uboot_ops = {
	.device_init =  uboot_mem_device_init,
};

static int __init uboot_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_uboot_ops;
	log_paddr = rmem->base;
	log_size  = rmem->size;
	printk("uboot mem setup, base=%p , size=0x%lx\n", (void *)log_paddr, log_size);
	return 0;
}

RESERVEDMEM_OF_DECLARE(uboot_log, "bl_log", uboot_mem_setup);

MODULE_DESCRIPTION("export uboot log driver");

