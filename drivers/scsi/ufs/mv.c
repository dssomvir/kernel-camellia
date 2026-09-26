#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>
#include <linux/cpu.h>
#include <linux/memblock.h>
#include <linux/byteorder/generic.h>

#define SMEM_ID_VENDOR2                 136

/* Raw data of DDR manufacturer id(MR5) */
#define HWINFO_DDRID_SAMSUNG	0x01
#define HWINFO_DDRID_HYNIX	0x06
#define HWINFO_DDRID_ELPIDA	0x03
#define HWINFO_DDRID_MICRON	0xFF
#define HWINFO_DDRID_NANYA	0x05
#define HWINFO_DDRID_INTEL	0x0E

static unsigned char ddr_info;

#include <linux/mm.h>
#include <linux/swap.h>

struct {
	u16 ufsinfo_vid;
	u32 ufsinfo_blk_cnt;
	u8  ufsinfo_pid[30];
	u8	ufsinfo_product_revision_level[30];
	u8	ufsinfo_serial_hex[30];
	char *vendor_name;
} mv_ufs;


static int __init ufsinfo_vid(char *str)
{
	mv_ufs.ufsinfo_vid = simple_strtol(str, NULL, 0);
	return 1;
}
__setup("ufsinfo_vid=", ufsinfo_vid);


static int __init ufsinfo_pid(char *str)
{
	strcpy(mv_ufs.ufsinfo_pid, str);
	return 1;
}

__setup("ufsinfo_pid=", ufsinfo_pid);


static int __init ufsinfo_product_revision_level(char *str)
{
	strcpy(mv_ufs.ufsinfo_product_revision_level, str);
	return 1;
}
__setup("ufsinfo_product_revision_level=", ufsinfo_product_revision_level);


static int __init ufsinfo_serial_hex(char *str)
{
	strcpy(mv_ufs.ufsinfo_serial_hex, str);
	return 1;
}
__setup("ufsinfo_serial_hex=", ufsinfo_serial_hex);


static int __init ufsinfo_blk_cnt(char *str)
{
	mv_ufs.ufsinfo_blk_cnt = simple_strtol(str, NULL, 0);
	return 1;
}
__setup("ufsinfo_blk_cnt=", ufsinfo_blk_cnt);


static int mv_proc_show(struct seq_file *m, void *v)
{
	u8 ddr_size_in_GB = 0;
	u16 ufs_size_in_GB = 0;
	u8 inquiry_tmp[37] = {};
	pr_info("memblock_phys_mem_size %lx\n", (long unsigned int)memblock_phys_mem_size());
	pr_info("memblock_reserved_size %lx\n", (long unsigned int)memblock_reserved_size());
	pr_info("memblock_start_of_DRAM %lx\n", (long unsigned int)memblock_start_of_DRAM());
	pr_info("memblock_end_of_DRAM %lx\n", (long unsigned int)memblock_end_of_DRAM());
	pr_info("geometry  qTotalRawDeviceCapacity %lx\n", mv_ufs.ufsinfo_blk_cnt);

	ddr_size_in_GB = (memblock_phys_mem_size() + memblock_reserved_size()) / 1024 / 1024 / 1024;

	if (ddr_size_in_GB > 10 && ddr_size_in_GB <= 12) {
		ddr_size_in_GB = 12;
	} else if (ddr_size_in_GB > 8) {
		ddr_size_in_GB = 10;
	} else if (ddr_size_in_GB > 6) {
		ddr_size_in_GB = 8;
	} else if (ddr_size_in_GB > 4) {
		ddr_size_in_GB = 6;
	} else if (ddr_size_in_GB > 3) {
		ddr_size_in_GB = 4;
	} else if (ddr_size_in_GB > 2) {
		ddr_size_in_GB = 3;
	} else{
		pr_info("mv unkonwn ddr size %d\n", ddr_size_in_GB);
	}

	ufs_size_in_GB = mv_ufs.ufsinfo_blk_cnt * 4 / 1024 / 1024;
	printk("mv---------- %d----------%d", mv_ufs.ufsinfo_blk_cnt, ufs_size_in_GB);

	if (ufs_size_in_GB > 512 && ufs_size_in_GB <= 1024) {
		ufs_size_in_GB = 1024;
	} else if (ufs_size_in_GB > 256) {
		ufs_size_in_GB = 512;
	} else if (ufs_size_in_GB > 128) {
		ufs_size_in_GB = 256;
	} else if (ufs_size_in_GB > 64) {
		ufs_size_in_GB = 128;
	} else if (ufs_size_in_GB > 32) {
		ufs_size_in_GB = 64;
	} else if (ufs_size_in_GB > 16) {
		ufs_size_in_GB = 32;
	} else{
		pr_info("mv unkonwn ufs size %d\n", ufs_size_in_GB);
	}

	switch (mv_ufs.ufsinfo_vid) {
	case 0x01ce:
		ddr_info = 0x01;
		mv_ufs.vendor_name = "Samsung";
		break;
	case 0x01ad:
		ddr_info = 0x06;
		mv_ufs.vendor_name = "Hynix";
		break;
	case 0x012c:
		ddr_info = 0xff;
		mv_ufs.vendor_name = "Micron";
		break;
	default:
		seq_printf(m, "DDR:Unknown, UFS:%x\n", mv_ufs.ufsinfo_vid);
		break;
	}
	seq_printf(m, "D: 0x%02x %d\n", (u32)ddr_info, ddr_size_in_GB); /* 0000 0001B */
	seq_printf(m, "U: %s %d %s %s\n", mv_ufs.vendor_name, ufs_size_in_GB, mv_ufs.ufsinfo_pid, mv_ufs.ufsinfo_product_revision_level); /* 0000 0001B */
	seq_printf(m, "serial:%s\n", mv_ufs.ufsinfo_serial_hex); /* 0000 0001B */
	return 0;
}

static int memory_type_proc_show(struct seq_file *mem, void *vd)
{
	seq_printf(mem, "UFS\n");
	return 0;
}


static int mv_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mv_proc_show, NULL);
}

static int memory_type_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, memory_type_proc_show, NULL);
}

static const struct file_operations mv_proc_fops = {
	.open		= mv_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations memory_type_proc_fops = {
	.open		= memory_type_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_mv_init(void)
{
	proc_create("memory_type", 0555, NULL, &memory_type_proc_fops);
	proc_create("mv", 0555, NULL, &mv_proc_fops);
	return 0;
}
late_initcall(proc_mv_init);