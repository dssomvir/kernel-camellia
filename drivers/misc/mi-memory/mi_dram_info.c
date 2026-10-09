#include <linux/string.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include "mi_memory_sysfs.h"
#if MTK_PLATFORM
#include <memory/mediatek/dramc.h>
#else
#include <linux/soc/qcom/smem.h>
#endif
#include "mem_interface.h"

#if MEMORYTPYE_DIS
#if !MTK_PLATFORM
#define SMEM_ID_VENDOR1 	135
#endif
#endif

extern char DDR_ID[10];

u8 get_ddr_size(void)
{
	u8 ddr_size_in_GB = 0;

	ddr_size_in_GB = memblock_mem_size_in_gb();
	pr_info("memblock_mem_size %lx\n", ddr_size_in_GB);

	if (ddr_size_in_GB > 16 && ddr_size_in_GB <= 18) {
		ddr_size_in_GB = 18;
	} else if (ddr_size_in_GB > 12) {
		ddr_size_in_GB = 16;
	} else if (ddr_size_in_GB > 10) {
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
	} else if (ddr_size_in_GB > 1) {
		ddr_size_in_GB = 2;
	}else {
		ddr_size_in_GB = 0;
	}

	return ddr_size_in_GB;
}

static void get_ddr_id(char* MI_DDR_ID)
{
	memcpy(MI_DDR_ID, DDR_ID, sizeof(DDR_ID));	
}

static void get_ddr_vendor(char* vendor)
{
	char MI_DDR_ID[10] = {0};
	long mi_ddr_id;
	int ret;
       
	get_ddr_id((char *)&MI_DDR_ID);
	ret = kstrtol(MI_DDR_ID, 16, &mi_ddr_id);

	switch (mi_ddr_id) {
	case HWINFO_DDRID_SAMSUNG:
		memcpy(vendor, "SAMSUNG", sizeof("SAMSUNG"));
		break;
	case HWINFO_DDRID_HYNIX:
		memcpy(vendor, "HYNIX", sizeof("HYNIX"));
		break;
	case HWINFO_DDRID_ELPIDA:
		memcpy(vendor, "ELPIDA", sizeof("ELPIDA"));
		break;
	case HWINFO_DDRID_MICRON:
		memcpy(vendor, "MICRON", sizeof("MICRON"));
		break;
	case HWINFO_DDRID_NANYA:
		memcpy(vendor, "NANYA", sizeof("NANYA"));
		break;
	case HWINFO_DDRID_INTEL:
		memcpy(vendor, "INTEL", sizeof("INTEL"));
		break;
	default:
		memcpy(vendor, "UNKNOWN", sizeof("UNKNOWN"));
		break;
	}

}

static ssize_t ddr_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", get_ddr_size());
}

static DEVICE_ATTR_RO(ddr_size);

static ssize_t ddr_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char MI_DDR_ID[10] = {0};

	get_ddr_id((char *)&MI_DDR_ID);

	return snprintf(buf, PAGE_SIZE, "%s\n",MI_DDR_ID);
}

static DEVICE_ATTR_RO(ddr_id);

static ssize_t ddr_vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char vendor[8] = {0};

	get_ddr_vendor((char *)&vendor);

	return snprintf(buf, PAGE_SIZE, "%s\n", vendor);
}

static DEVICE_ATTR_RO(ddr_vendor);

static struct attribute *dram_sysfs[] = {
	&dev_attr_ddr_size.attr,
	&dev_attr_ddr_id.attr,
	&dev_attr_ddr_vendor.attr,
	NULL,
};

const struct attribute_group dram_sysfs_group = {
	.name = "ddr",
	.attrs = dram_sysfs,
};
