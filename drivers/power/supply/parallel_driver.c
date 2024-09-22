#include "parallel_driver.h"
#include "sd77428.h"
#include "sd77122.h"

//-------------------------------------mainpack driver----------------------------------------------//
static int32_t fg_mainpack_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
    int32_t ret = 0;
    struct fg_bms_chip *chip = NULL;
    pr_info("Start, fg_mainpakc i2c addr 0x%02X\n",client->addr);

    chip = devm_kzalloc(&client->dev, sizeof(struct fg_bms_chip),GFP_KERNEL);
    if (!chip)
    {
        dev_err(&client->dev, "Can't alloc fg_mainpakc struct\n");
        return -ENOMEM;
    } 
    //init mutex ASAP,before iic operate
    mutex_init(&chip->i2c_rw_lock);
    mutex_init(&chip->irq_lock);

    chip->client = client;
    chip->dev = &client->dev;
    i2c_set_clientdata(client, chip);

    //sd77122 init
    ret = sd77122_ic_init(chip);
    pr_info("fg_mainpack sd77122_ic_init done.\n");

    //sd77428main init
    ret = sd77428main_ic_init(chip);
    pr_info("fg_mainpack sd77428_ic_init done.\n");

    //sd77122 init process task
    ret = sd77122_parse_dt(chip, &client->dev);
    if (ret < 0)
        pr_err("sd77122_parse_dt failed ret %d\n",ret);
    ret = sd77122_register_irq(chip);
    if (ret < 0) 
        pr_err("register irq fail(%d)\n", ret);
    // //process irq once when probe
    // ret = sd77122_process_irq(chip);
    // if (ret < 0) 
    //     pr_err("process irq fail(%d)\n", ret);
    device_init_wakeup(chip->dev, true);

    pr_info("fg_mainpack_probe end.\n");

    return 0;   
}

static int fg_mainpack_resume(struct device *dev_chip)
{
    sd77428main_resume(dev_chip);
    sd77122_resume(dev_chip);
    return 0;
}
static int fg_mainpack_suspend(struct device *dev_chip)
{
    sd77428main_suspend(dev_chip);
    sd77122_suspend(dev_chip);
    return 0;
}
static int32_t fg_mainpack_remove(struct i2c_client *client)
{
    sd77428main_remove(client);
    sd77122_remove(client);
    return 0;
}
static void fg_mainpack_shutdown(struct i2c_client *client)
{
    sd77428main_shutdown(client);
    sd77122_shutdown(client);
}

static const struct dev_pm_ops fg_mainpack_pm_ops = {
    .resume         = fg_mainpack_resume,
    .suspend        = fg_mainpack_suspend,
};

static const struct of_device_id fg_mainpack_of_match[] = {
    {.compatible = "bmt,sd77122_sd77428"},
    {},
};

static const struct i2c_device_id fg_mainpack_i2c_id[] = { 
    {"sd77122_sd77428",   0}, 
    { },

};

static struct i2c_driver fg_mainpack_driver = {
    .driver = {
        .name            = "sd77122_sd77428",
        .owner           = THIS_MODULE,
        .pm              = &fg_mainpack_pm_ops,
        .of_match_table = fg_mainpack_of_match,
    },
    .id_table   = fg_mainpack_i2c_id,
    .probe      = fg_mainpack_probe,
    .remove     = fg_mainpack_remove,
    .shutdown   = fg_mainpack_shutdown,
};

//-------------------------------------rntpack driver----------------------------------------------//
static int32_t fg_rntpack_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
    int32_t ret = 0;
    struct fg_bms_chip *chip = NULL;

    pr_info("Start,fg_rntpack i2c addr 0x%02X\n",client->addr);
    //内核内存分配函数 devm_kzalloc()是跟设备(device)有关的，当设备(device)被detached或者驱动(driver)卸载(unloaded)时，内存会被自动释放
    chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
    if (!chip)
    {
        dev_err(&client->dev, "Can't alloc BMT_data struct\n");
        return -ENOMEM;
    } 
    //init mutex ASAP,before iic operate
    mutex_init(&chip->i2c_rw_lock);

    chip->client = client; 
    chip->dev    = &client->dev;
    i2c_set_clientdata(client, chip);

    //sd77428rnt init
    ret = sd77428rnt_ic_init(chip);

    pr_info("fg_rntpack_probe end.\n");
    return 0;
}

static int32_t fg_rntpack_suspend(struct device *dev)
{
    return sd77428rnt_suspend(dev);
}

static int32_t fg_rntpack_resume(struct device *dev)
{
    return sd77428rnt_resume(dev);
}

static int32_t fg_rntpack_remove(struct i2c_client *client)
{
    return sd77428rnt_remove(client);
}

static void fg_rntpack_shutdown(struct i2c_client *client)
{
    sd77428rnt_shutdown(client);
}
static const struct dev_pm_ops pm_ops = 
{
    .suspend    = fg_rntpack_suspend,
    .resume     = fg_rntpack_resume,
};

static const struct of_device_id fg_rntpack_of_match[] = 
{
    {.compatible = "bmt,sd77428"},
    {},
};

static const struct i2c_device_id fg_rntpack_id[] = 
{
    {"sd77428", 0 },
    { }
};

static struct i2c_driver fg_rntpack_driver = 
{ 
    .driver = {
        .name   = "sd77428",
        .owner  = THIS_MODULE,
        .pm   = &pm_ops,
        .of_match_table = fg_rntpack_of_match,
    },
    .id_table   = fg_rntpack_id, 
    .probe      = fg_rntpack_probe,
    .remove     = fg_rntpack_remove,
    .shutdown   = fg_rntpack_shutdown,
};


//-------------------------------------end---------------------------------------------------------//
static int32_t __init parallel_fg_init(void)
{
    int32_t ret;
    ret = i2c_add_driver(&fg_mainpack_driver);
    if (ret) 
        pr_info("Failed to register sd77122_sd77428 i2c driver.\n");
    else 
        pr_info("Success to register sd77122_sd77428 i2c driver.\n");
    
    ret = i2c_add_driver(&fg_rntpack_driver);
    if (ret) 
        pr_info("Failed to register fg_rnt sd77428 i2c driver.\n");
    else 
        pr_info("Success to register fg_rnt sd77428 i2c driver.\n");

    return 0;
}

static void __exit parallel_fg_exit(void)
{
    i2c_del_driver(&fg_mainpack_driver);

    i2c_del_driver(&fg_rntpack_driver);
}

module_init(parallel_fg_init);
module_exit(parallel_fg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Parallel_fuel_gauge Driver");
MODULE_AUTHOR("cheng.huang <cheng.huang@bigmtech.com>");