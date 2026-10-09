#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>
#include <linux/cpu.h>
#include <linux/memblock.h>
#include <linux/byteorder/generic.h>
#include <linux/soc/qcom/smem.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/string.h>
#include "mv.h"
#include "mi_memory_sysfs.h"
#include "mem_interface.h"

#define K(x) ((x) << (PAGE_SHIFT - 10))
extern unsigned int round_kbytes_to_readable_mbytes(unsigned int k);

struct mv_ufs *mvufs = NULL;
/* Huaqin modify for HQ-157807 by luocheng at 2021/11/02 start */
char DDR_ID[10];
/* Huaqin modify for HQ-157807 by luocheng at 2021/11/02 end */

static void mv_ufs_init(void)
{
	u64 raw_device_capacity = 0;
	u16 ufs_id = 0;

	struct ufs_data *ufs_data_ptr = get_ufs_data();
	struct ufs_hba *hba = ufs_data_ptr->hba;
	mvufs = kzalloc(sizeof(struct mv_ufs), GFP_KERNEL);
	ufs_get_string_desc(hba, mvufs->product_name, (sizeof(mvufs->product_name) - 1), DEVICE_DESC_PARAM_PRDCT_NAME, SD_ASCII_STD);
	ufs_get_string_desc(hba, mvufs->product_revision, (sizeof(mvufs->product_revision) - 1), DEVICE_DESC_PARAM_PRDCT_REV, SD_ASCII_STD);

	ufs_read_desc_param(hba, QUERY_DESC_IDN_DEVICE, 0, DEVICE_DESC_PARAM_MANF_ID, &ufs_id, 2);

	mvufs->vendor_id = ufs_id;

	ufs_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0, GEOMETRY_DESC_PARAM_DEV_CAP, &raw_device_capacity, 8);
	raw_device_capacity = (raw_device_capacity * 512) / 1024 / 1024 / 1024;
	if (raw_device_capacity > 512 && raw_device_capacity <= 1024) {
		mvufs->density = 1024;
	} else if (raw_device_capacity > 256) {
		mvufs->density = 512;
	} else if (raw_device_capacity > 128) {
		mvufs->density = 256;
	} else if (raw_device_capacity > 64) {
		mvufs->density = 128;
	} else if (raw_device_capacity > 32) {
		mvufs->density = 64;
	} else if (raw_device_capacity > 16) {
		mvufs->density = 32;
	} else if (raw_device_capacity > 8) {
		mvufs->density = 8;
	} else {
		mvufs->density = 0;
		pr_info("mv unkonwn ufs size %d\n", raw_device_capacity);
	}
}

/* Huaqin modify for HQ-157807 by luocheng at 2021/11/02 start */
static int __init ddr_id_setup(char *str)
{
	sprintf(DDR_ID,"%s",str);
	pr_info("ddr_id_kernel:%s\n",str);
	return 1;
}
__setup("ddr_id=", ddr_id_setup);

static int mv_proc_show(struct seq_file *m, void *v)
{
	char ddr_size[10];
	struct sysinfo i;
	si_meminfo(&i);
	if (NULL == mvufs)
	{
	mv_ufs_init();
	}

	if (round_kbytes_to_readable_mbytes(K(i.totalram)) >= 1024) {
	sprintf(ddr_size, "%d", round_kbytes_to_readable_mbytes(K(i.totalram))/1024);
	} else{
	sprintf(ddr_size, "%dMB", round_kbytes_to_readable_mbytes(K(i.totalram)));
	}
	seq_printf(m, "D: %s  %s\n", DDR_ID, ddr_size);
	seq_printf(m, "U: 0x%04x %d %s %s\n", mvufs->vendor_id, mvufs->density,mvufs->product_name, mvufs->product_revision);

/* Huaqin modify for HQ-157807 by luocheng at 2021/11/02 end */
	return 0;
}

static int mv_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mv_proc_show, NULL);
}

static const struct file_operations mv_proc_fops = {
	.open		= mv_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int add_proc_mv_node(void)
{
	proc_create(MV_NAME, 0555, NULL, &mv_proc_fops);
	return 0;
}

int remove_proc_mv_node(void)
{
	remove_proc_entry(MV_NAME, NULL);
	return 0;
}
