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
#include <linux/of_platform.h>
#include <linux/iio/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/printk.h>
#include <linux/err.h>
#include "hqsys_pcba.h"

PCBA_CONFIG huaqin_pcba_config = PCBA_UNKNOW;

static int light_pcba_config;
static int light_pcba_stage;
static int light_pcba_count;

static int __init get_light_pcba_config(char *p)
{
	char pcba[10];

	strlcpy(pcba, p, sizeof(pcba));

	if (kstrtoint(pcba, 10, &light_pcba_config))
		return -1;

	pr_err("[%s]: pcba config = %d\n", __func__, light_pcba_config);

	return 0;
}
early_param("pcba_config", get_light_pcba_config);

static int __init get_light_pcba_count(char *p)
{
	char count[10];

	strlcpy(count, p, sizeof(count));

	if (kstrtoint(count, 10, &light_pcba_count))
		return -1;

	printk("[%s]: pcba count = %d\n", __func__, light_pcba_count);

	return 0;
}
early_param("pcba_count", get_light_pcba_count);
/* Huaqin modify for L19-69 by wangzhaoguo at 2021/12/24 start */
/* Huaqin modify for HQ-158772 by wangzhaoguo at 2021/10/21 start */
struct project_stage {
	int voltage_min;
	int voltage_max;
	PROJECT_STAGE stage;
} stage_map[] = {
	{ 130,  225,   P0_1, },
	{ 226,  315,   P1, },
	{ 316,  405,   P1_1, },
	{ 406,  495,   P2, },
	{ 496,  585,   MP, },
};
/* Huaqin modify for HQ-158772 by wangzhaoguo at 2021/10/21 end */
/* Huaqin modify for L19-69 by wangzhaoguo at 2021/12/24 end */
static bool read_pcba_config_light(void)
{
	if (light_pcba_config == PCBA_UNKNOW) {
		huaqin_pcba_config = PCBA_UNKNOW;
		return false;
	}
	/* Huaqin modify for HQ-158772 by wangzhaoguo at 2021/10/21 start */
	int ret = 0, auxadc_voltage = 0, board_vol = 0;
	struct iio_channel *channel;
	struct device_node *board_id_node;
	struct platform_device *board_id_dev;
	PROJECT_STAGE light_pcba_stage = UNKNOW;
	board_id_node = of_find_node_by_name(NULL, "board_id");
	if (board_id_node == NULL) {
		pr_err("[%s] find board_id node fail \n", __func__);
		return false;
	} else {
		pr_err("[%s] find board_id node success %s \n", __func__, board_id_node->name);
	}
	board_id_dev = of_find_device_by_node(board_id_node);
	if (board_id_dev == NULL) {
		pr_err("[%s] find board_id dev fail \n", __func__);
		return false;
	} else {
		pr_err("[%s] find board_id dev success %s \n", __func__, board_id_dev->name);
	}
	channel = iio_channel_get(&(board_id_dev->dev), "board_id-channel");
	if (IS_ERR(channel)) {
		ret = PTR_ERR(channel);
		pr_err("[%s] iio channel not found %d\n", __func__, ret);
		return false;
	} else {
		pr_err("[%s] get channel success\n", __func__);
	}
	if (channel != NULL) {
		ret = iio_read_channel_processed(channel, &auxadc_voltage);
	} else {
		pr_err("[%s] no channel to processed \n", __func__);
		return false;
	}
	if (ret < 0) {
		pr_err("[%s] IIO channel read failed %d \n", __func__, ret);
		return false;
	} else {
		pr_err("[%s] auxadc_voltage is %d\n", __func__, auxadc_voltage);
		board_vol = auxadc_voltage ;
		pr_err("[%s] board_id_voltage is %d\n", __func__, board_vol);
	}
	pr_err("[%s] read_pcba_config board_vol: %d\n", __func__, board_vol);
	
	for (int i = 0; i < sizeof(stage_map)/sizeof(struct project_stage); i++) {
        	if (stage_map[i].voltage_min <= board_vol && board_vol <= stage_map[i].voltage_max) {
			light_pcba_stage = stage_map[i].stage;
			break;
		}
	}
	if (i >= sizeof(stage_map)/sizeof(struct project_stage))
		huaqin_pcba_config = PCBA_UNKNOW;
	/* Huaqin modify for HQ-158772 by wangzhaoguo at 2021/10/21 end */
	huaqin_pcba_config = (light_pcba_stage - 1) * light_pcba_count + light_pcba_config;

	printk("[%s]: huaqin_pcba_config = %d\n", __func__, huaqin_pcba_config);

	return true;
}

static int board_id_probe(struct platform_device *pdev)
{
	int ret;

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret) {
		pr_err("[%s] Failed %d!!!\n", __func__, ret);
		return ret;
	}

	read_pcba_config_light();
	return 0;
}

static int board_id_remove(struct platform_device *pdev)
{
	pr_err("enter [%s] \n", __func__);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id boardId_of_match[] = {
	{.compatible = "mediatek,board_id",},
	{},
};
#endif

static struct platform_driver boardId_driver = {
	.probe = board_id_probe,
	.remove = board_id_remove,
	.driver = {
		.name = "board_id",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = boardId_of_match,
#endif
	},
};

PCBA_CONFIG get_huaqin_pcba_config(void)
{
	return huaqin_pcba_config;
}
EXPORT_SYMBOL_GPL(get_huaqin_pcba_config);


static int __init huaqin_pcba_early_init(void)
{
	int ret;
	pr_err("[%s]start to register boardId driver\n", __func__);

	ret = platform_driver_register(&boardId_driver);
	if (ret) {
		pr_err("[%s]Failed to register boardId driver\n", __func__);
		return ret;
	}
	return 0;
}

static void __exit huaqin_pcba_exit(void)
{
	platform_driver_unregister(&boardId_driver);
}

module_init(huaqin_pcba_early_init);//before device_initcall
module_exit(huaqin_pcba_exit);

MODULE_DESCRIPTION("huaqin sys pcba");
MODULE_LICENSE("GPL");
