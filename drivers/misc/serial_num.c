#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <mt-plat/mtk_devinfo.h>
#include <linux/mmc/mmc.h>
#include <linux/seq_file.h>
/* Huaqin modify for HQ-158805 by wangzhaoguo at 2021/10/21 start */
#define HRID0 12
#define HRID1 13
#define HRID2 14
#define HRID3 15

#define PROC_SERIAL_NUM_FILE "serial_num"
#define PROC_CHIPID_FILE "chip_id"
/* Huaqin modify for HQ-158805 by wangzhaoguo at 2021/10/21 end */
static struct proc_dir_entry *entry;
/* Huaqin modify for HQ-158805 by wangzhaoguo at 2021/10/21 start */
static char dev_id[40];
/* Huaqin modify for HQ-158805 by wangzhaoguo at 2021/10/21 end */

extern char  *saved_command_line;

static int serial_num_proc_show(struct seq_file *file, void *data)
{
	char *tempbuf = NULL;
	char *tempbuf2 = NULL;
	char temp[60] = {0};
	pr_err("[%s]: serial_num_proc_show failed\n", __func__);
	tempbuf = strstr(saved_command_line, "androidboot.cpuid=0x");
	if (tempbuf != 0) {
		tempbuf2 = strstr(tempbuf, " ");
		if (tempbuf2 != 0) {
			strncpy(temp, tempbuf + 20, tempbuf2 - tempbuf - 20);
			temp[tempbuf2 - tempbuf - 20] = '\0';
			seq_printf(file, "0x%s\n", temp);
		}
	} else {
		seq_printf(file, "%s\n", "123456ABCDEF");
	}

	return 0;
}

static int serial_num_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, serial_num_proc_show, inode->i_private);
}

static const struct file_operations serial_num_proc_fops = {
	.open = serial_num_proc_open,
	.read = seq_read,
};

/* Huaqin modify for HQ-158805 by wangzhaoguo at 2021/10/21 start */
static int chip_id_proc_show(struct seq_file *file, void *data)
{
	seq_printf(file, "0x%s", dev_id);
	return 0;
}
static int chip_id_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, chip_id_proc_show, inode->i_private);
}
static const struct file_operations chip_id_proc_fops = {
	.open = chip_id_proc_open,
	.read = seq_read,
};
/* Huaqin modify for HQ-158805 by wangzhaoguo at 2021/10/21 end */

static int __init sn_fuse_init(void)
{	/* Huaqin modify for HQ-158805 by wangzhaoguo at 2021/10/21 start */
	unsigned int temp0, temp1, temp2, temp3;
	temp0 = get_devinfo_with_index(HRID0);
	temp1 = get_devinfo_with_index(HRID1);
	temp2 = get_devinfo_with_index(HRID2);
	temp3 = get_devinfo_with_index(HRID3);
	sprintf(dev_id, "%08x%08x%08x%08x", temp0, temp1, temp2, temp3);
	printk("dev_id is %s\n", dev_id);
	/* Huaqin modify for HQ-158805 by wangzhaoguo at 2021/10/21 end */
	entry = proc_create(PROC_SERIAL_NUM_FILE, 0644, NULL, &serial_num_proc_fops);
	if (entry == NULL) {
		pr_err("[%s]: create_proc_entry entry failed\n", __func__);
	}
	/* Huaqin modify for HQ-158805 by wangzhaoguo at 2021/10/21 start */
	entry = proc_create(PROC_CHIPID_FILE, 0644, NULL, &chip_id_proc_fops);
	if (entry == NULL)	{
	pr_err("[%s]: create_proc_entry entry failed\n", __func__);
	}
	/* Huaqin modify for HQ-158805 by wangzhaoguo at 2021/10/21 end */
	return 0;
}
late_initcall(sn_fuse_init);

static void __exit sn_fuse_exit(void)
{
	printk("sn_fuse_exit\n");
}
module_exit(sn_fuse_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("JTag Fuse driver");

