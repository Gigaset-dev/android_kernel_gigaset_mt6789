#include "parallel_driver.h"
#include "sd77428_2.h"
#include "sd77122.h"

static int32_t g_fuse_rsoc = 0;     //increase 100 times
static int32_t g_lastfuse_rsoc = 0;     //increase 100 times

static int32_t g_mapping_soc_region = 97;
//------------------------------------------fuse function-------------------------------------------//
int32_t fg_fuse_voltage(void)
{
    int32_t m_volt, r_volt;
    //int32_t pack1_volt, pack2_volt;
    int32_t fuse_volt;
    m_volt = sd77428main_get_battery_voltage();
    r_volt = sd77428rnt_get_battery_voltage();
    fuse_volt = (m_volt * 6 + (m_volt + r_volt) * 2)/10;
    return fuse_volt;
}
EXPORT_SYMBOL(fg_fuse_voltage);

int32_t fg_fuse_current(void)
{
    int32_t m_curr, r_curr;
    int32_t fuse_curr;

    m_curr = sd77428main_get_battery_current();
    r_curr = sd77428rnt_get_battery_current();

    fuse_curr = (m_curr + r_curr);
    return fuse_curr;
}
EXPORT_SYMBOL(fg_fuse_current);

int32_t fg_fuse_thm(void)
{
    int32_t m_thm, r_thm;
    int32_t fuse_thm;

    m_thm = sd77428main_get_battery_temp();
    r_thm = sd77428rnt_get_battery_temp();

    fuse_thm = (m_thm + r_thm)/2;
    return fuse_thm;
}
EXPORT_SYMBOL(fg_fuse_thm);
#if 0
//alg v1, base on main soc, fuse rnt_soc, add filter
int32_t fg_fuse_rsoc_v1(void)
{
    int32_t m_rsoc, r_rsoc;
    int32_t delta_tmp;
    int32_t avg_soc, fuse_rsoc;
    int32_t ext_charger;
    int32_t f_use_current;

    ext_charger = sd77428main_get_ext_charger();
    f_use_current = fg_fuse_current();

    m_rsoc = sd77428main_get_soc();
    r_rsoc = sd77428rnt_get_soc();
    pr_info("fg_fuse_rsoc ext_charger is %d, last_soc:%d, c:%d, m_rsoc:%d, r_rsoc:%d \n", ext_charger, g_lastfuse_rsoc, f_use_current, m_rsoc, r_rsoc);

    // avg_soc = (m_rsoc + r_rsoc) * 100 / 2 ;
    avg_soc = (m_rsoc * 7 + r_rsoc * 3) * 10;
    //solution1
    if (g_fuse_rsoc == 0)
    {
        g_fuse_rsoc = avg_soc;
        g_lastfuse_rsoc = avg_soc/100;
    }
    else
    {
        // g_fuse_rsoc = (g_fuse_rsoc * 7 + avg_soc * 3)/10 ;
        
        // g_fuse_rsoc = g_fuse_rsoc + (avg_soc - g_fuse_rsoc) * 3 / 10 ;
        // 
        delta_tmp = (avg_soc - g_fuse_rsoc) * 2;
        if (delta_tmp > 0 && delta_tmp < 10)
        {
            delta_tmp = 1;
        }
        else if (delta_tmp < 0 && delta_tmp > -10)
        {
            delta_tmp = -1;
        }
        else
        {
            delta_tmp = delta_tmp / 10;
        }
        g_fuse_rsoc = g_fuse_rsoc + delta_tmp;
    }

    fuse_rsoc = g_fuse_rsoc/100;
    if (g_fuse_rsoc % 100 >=50)
    {
        fuse_rsoc = fuse_rsoc + 1;
    }
    // charge 插入 状态, soc不允许下降
    if (ext_charger > 0) //适配器插入, 充电中
    {
        if (g_lastfuse_rsoc > fuse_rsoc && f_use_current > -50)
        {
            fuse_rsoc = g_lastfuse_rsoc;
        }
    }else
    {
        //charge 没有插入，或者放电状态， soc不允许上升
        if (f_use_current < -10)
        {
            if (g_lastfuse_rsoc < fuse_rsoc)
            {
                fuse_rsoc = g_lastfuse_rsoc;
            }
        }
        //else
        //{
        //    if (g_lastfuse_rsoc > fuse_rsoc)
        //    {
        //        fuse_rsoc = g_lastfuse_rsoc;
        //    }
        //}
    }
    g_lastfuse_rsoc = fuse_rsoc;

    return fuse_rsoc;
}
#endif

//alg v2, base on main soc, fuse rnt_soc, add filter
int32_t fg_fuse_rsoc(void)
{
    int32_t m_rsoc, r_rsoc;
    // int32_t delta_tmp;
    // int32_t avg_soc;
    int32_t fuse_rsoc;
    int32_t ext_charger;
    int32_t f_use_current;
    //int32_t f_use_rsoc_resth = 50;           //rsoc 小数部分四舍五入

    int32_t f_disp_rsoc;           
    int32_t remainder;
    
    ext_charger = sd77428main_get_ext_charger();
    f_use_current = fg_fuse_current();

    m_rsoc = sd77428main_get_soc();
    r_rsoc = sd77428rnt_get_soc();

    pr_info("fg_fuse_rsoc ext_charger is %d, last_soc:%d, c:%d, m_rsoc:%d, r_rsoc:%d \n", ext_charger, g_lastfuse_rsoc, f_use_current, m_rsoc, r_rsoc);


    g_fuse_rsoc = (m_rsoc * 2900 + r_rsoc * 1250);
    g_fuse_rsoc = (g_fuse_rsoc * 100)/(2900+1250);  //扩大100倍, 0.01%

#if 1       //add display soc mapping
    f_disp_rsoc = g_fuse_rsoc / g_mapping_soc_region;  //1%的精度范围, 1%
    remainder = g_fuse_rsoc % g_mapping_soc_region;
    if(remainder * 2 >= g_mapping_soc_region)
        f_disp_rsoc += 1;
    if(f_disp_rsoc >= 100)
        f_disp_rsoc = 100;
    fuse_rsoc = f_disp_rsoc;
#else
    fuse_rsoc = g_fuse_rsoc/100;
    if (g_fuse_rsoc % 100 >= f_use_rsoc_resth)
    {
        fuse_rsoc = fuse_rsoc + 1;
    }

#endif


    if (g_lastfuse_rsoc == 0)
    {
        g_lastfuse_rsoc = fuse_rsoc;
    }
    
    // charge 插入 状态, soc不允许下降
    if (ext_charger > 0) //适配器插入, 充电中
    {
        if (g_lastfuse_rsoc > fuse_rsoc && f_use_current > -20)
        {
            fuse_rsoc = g_lastfuse_rsoc;
        }
    }else
    {
        //charge 没有插入，或者放电状态， soc不允许上升
        if (f_use_current < 0)
        {
            if (g_lastfuse_rsoc < fuse_rsoc)
            {
                fuse_rsoc = g_lastfuse_rsoc;
            }
        }
        //else
        //{
        //    if (g_lastfuse_rsoc > fuse_rsoc)
        //    {
        //        fuse_rsoc = g_lastfuse_rsoc;
        //    }
        //}
    }
    g_lastfuse_rsoc = fuse_rsoc;

    return fuse_rsoc;
}
EXPORT_SYMBOL(fg_fuse_rsoc);
static void fg_use_work_func(struct work_struct *work)
{
    int32_t fuse_volt, fuse_curr, fuse_thm, fuse_rsoc;
    struct fg_bms_chip *chip = container_of(work, struct fg_bms_chip, chip_fuse_work.work);
    if(false == sd77428main_chip_ok() ||
        false == sd77428rnt_chip_ok())
    {
        pr_info("Sd77428 is not ready %d, %d \r\n", sd77428main_chip_ok(), sd77428main_chip_ok());
        goto out;
    }

    fuse_volt = fg_fuse_voltage();
    fuse_curr = fg_fuse_current();
    fuse_thm = fg_fuse_thm();
    fuse_rsoc = fg_fuse_rsoc();

    chip->pack_fuse_info.pack_fuse_voltage = fuse_volt;
    chip->pack_fuse_info.pack_fuse_current = fuse_curr;
    chip->pack_fuse_info.pack_fuse_temp = fuse_thm;
    chip->pack_fuse_info.pack_fuse_rsoc = fuse_rsoc;
    pr_info("fuse_volt:%d, fuse_curr:%d, fuse_thm:%d, fuse_rsoc:%d.\n",fuse_volt, fuse_curr, fuse_thm, fuse_rsoc);
	power_supply_changed(chip->bat);
out:
    schedule_delayed_work(&chip->chip_fuse_work, msecs_to_jiffies(1000));
}

int32_t fg_use_suspend(struct device *dev)
{
    struct fg_bms_chip *chip  = i2c_get_clientdata(to_i2c_client(dev));
    cancel_delayed_work_sync(&chip->chip_fuse_work);
    pr_info("fg_use is suspend\n");
    return 0;
}

int32_t fg_use_resume(struct device *dev)
{
    struct fg_bms_chip *chip = i2c_get_clientdata(to_i2c_client(dev));
    schedule_delayed_work(&chip->chip_fuse_work,msecs_to_jiffies(50));
    pr_info("fg_use is resume\n");
    return 0;
}

int32_t fg_use_remove(struct i2c_client *client)
{
    struct fg_bms_chip *chip = i2c_get_clientdata(client);
    // sysfs_remove_group(&(chip->client->dev.kobj), &sd77428_attribute_group);
    // power_supply_unregister(chip->bat);
    cancel_delayed_work_sync(&chip->chip_fuse_work);
    pr_info("fg_use is remove\n");
    return 0;
}

void fg_use_shutdown(struct i2c_client *client)
{
    struct fg_bms_chip *chip = i2c_get_clientdata(client);
    // power_supply_unregister(chip->bat);
    cancel_delayed_work_sync(&chip->chip_fuse_work);
    pr_info("fg_use is shutdown\n");
}

//-----------------------------------------------------power_supply-----------------------------------------------------//
static enum power_supply_property sd77428_fuse_battery_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_TEMP,
    POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
    POWER_SUPPLY_PROP_CHARGE_NOW,
    POWER_SUPPLY_PROP_HEALTH,
};
#if 0
static char *sd77428_fuse_supplied_from[] = {
    "usb",
    "charger",
    "ac",
};
#endif
static int32_t sd77428_fuse_battery_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
    struct fg_bms_chip *chip = (struct fg_bms_chip *)power_supply_get_drvdata(psy);
	//dev_info(chip->dev,"%s %d\n",__func__,psp);
    switch (psp) {

    case POWER_SUPPLY_PROP_STATUS:
        break;

    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = 1;
        break;

    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        // val->intval = chip->batt_info.batt_voltage * 1000;
        val->intval = chip->pack_fuse_info.pack_fuse_voltage * 1000;
        break;

    case POWER_SUPPLY_PROP_CURRENT_NOW:
        // val->intval = chip->batt_info.batt_current * 1000;
        val->intval = chip->pack_fuse_info.pack_fuse_current * 1000;
        break;

    case POWER_SUPPLY_PROP_CAPACITY:
        // val->intval = chip->batt_info.batt_rsoc;
        val->intval = chip->pack_fuse_info.pack_fuse_rsoc;
        break;

    case POWER_SUPPLY_PROP_TEMP:
        // val->intval = chip->batt_info.batt_temp;
        val->intval = chip->pack_fuse_info.pack_fuse_temp;
        break;

    case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
        val->intval = chip->batt_info.batt_fcc;
        break;

    case POWER_SUPPLY_PROP_CHARGE_NOW:
        val->intval = chip->batt_info.batt_capacity;
        break;
    case POWER_SUPPLY_PROP_HEALTH:
        val->intval = chip->batt_info.batt_soh;;
        break;  
    default:
        return -EINVAL;
    }
    dev_info(chip->dev,"%s %d, value:%d.\n",__func__,psp, val->intval);
    return 0;
}
#if 0
static void sd77428_fuse_external_power_changed(struct power_supply *psy)
{
    struct sd77428_data *chip = (struct sd77428_data *)power_supply_get_drvdata(psy);
    dev_info(chip->dev,"enter\n");
    if(true == sd77428_is_ok)
    {
        cancel_delayed_work_sync(&chip->sd77428_work);
        schedule_delayed_work(&chip->sd77428_work,0);
        power_supply_changed(chip->bat);
    }
}
#endif
static int32_t sd77428_fuse_power_supply_init(struct fg_bms_chip *chip)
{
    chip->bat_cfg.drv_data          = chip;
    chip->bat_cfg.of_node           = chip->client->dev.of_node;

    chip->bat_desc.name             = "cw-bat";//cw-bat   sd77428
    chip->bat_desc.type             = POWER_SUPPLY_TYPE_BATTERY;
    chip->bat_desc.properties       = sd77428_fuse_battery_props;
    chip->bat_desc.num_properties = ARRAY_SIZE(sd77428_fuse_battery_props);
    chip->bat_desc.get_property     = sd77428_fuse_battery_get_property;
    chip->bat_desc.no_thermal       = 1;
    //chip->bat_desc.external_power_changed = sd77428_external_power_changed;

    chip->bat = devm_power_supply_register(chip->dev,&chip->bat_desc,&chip->bat_cfg);

    if (IS_ERR(chip->bat)) 
    {
        pr_err("Couldn't register power supply\n");
        return PTR_ERR(chip->bat);
    }
    else
    {
        //chip->bat->supplied_from = sd77428_fuse_supplied_from;
        //chip->bat->num_supplies  = ARRAY_SIZE(sd77428_fuse_supplied_from);
    }

    return 0;
}

static int32_t sd77428_fuse__init_batt_info(struct fg_bms_chip *chip)
{
    //初始化融合电量计信息
    chip->pack_fuse_info.pack_fuse_voltage = 3800;
    chip->pack_fuse_info.pack_fuse_current = 0;
    chip->pack_fuse_info.pack_fuse_temp = 250;
    chip->pack_fuse_info.pack_fuse_rsoc = 1;

    // chip->batt_info.batt_soh = 100;
    // chip->batt_info.batt_fcc = 2800; 
    // chip->batt_info.batt_capacity = 2800;
    return 0;
}
//---------------------------------------------------------------------------------------------------------------------//

//-------------------------------------mainpack driver----------------------------------------------//
static int32_t fg_mainpack_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
    int32_t ret = 0;
    struct fg_bms_chip *chip = NULL;
    pr_info("Start, fg_mainpack i2c addr 0x%02X\n",client->addr);

    chip = devm_kzalloc(&client->dev, sizeof(struct fg_bms_chip),GFP_KERNEL);
    if (!chip)
    {
        dev_err(&client->dev, "Can't alloc fg_mainpakc struct\n");
        return -ENOMEM;
    } 
    sd77428_fuse__init_batt_info(chip);
    
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
    sd77428_fuse_power_supply_init(chip);
    //----------------------------------------//
    INIT_DELAYED_WORK(&chip->chip_fuse_work, fg_use_work_func);
    schedule_delayed_work(&chip->chip_fuse_work, msecs_to_jiffies(1000));

    pr_info("fg_mainpack_probe end.\n");

    return 0;   
}

static int fg_mainpack_resume(struct device *dev_chip)
{
    sd77428main_resume(dev_chip);
    sd77122_resume(dev_chip);
    fg_use_resume(dev_chip);
    return 0;
}
static int fg_mainpack_suspend(struct device *dev_chip)
{
    sd77428main_suspend(dev_chip);
    sd77122_suspend(dev_chip);
    fg_use_suspend(dev_chip);
    return 0;
}
static int32_t fg_mainpack_remove(struct i2c_client *client)
{
    sd77428main_remove(client);
    sd77122_remove(client);
    fg_use_remove(client);
    return 0;
}
static void fg_mainpack_shutdown(struct i2c_client *client)
{
    sd77428main_shutdown(client);
    sd77122_shutdown(client);
    fg_use_shutdown(client);
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