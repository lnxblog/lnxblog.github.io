#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include "bme280_defs.h"
#define BUS_NO 1
#define BME280_ID_ADDR 0xd0
#define BME280_ID 0x60
#define TEMP_MSB_CMD 0xFA
#define TEMP_LSB_CMD 0xFB
#define TEMP_XLSB_CMD 0xFC

MODULE_AUTHOR("lnxblog"); 
MODULE_LICENSE("Dual BSD/GPL");

int bme280_major =   0; // use dynamic major
int bme280_minor =   0;
uint16_t calib_t[3];

struct bme280_dev
{
    struct cdev cdev;     /* Char device structure */
    struct i2c_adapter *bme280_i2c_adapter;
    struct i2c_client *bme280_i2c_client;
  //  uint8_t calib_data[CALIB_DATA_PT_LEN];
}bme280_s;

static struct i2c_device_id bme280_idtable[] = {
      { "bme280", 0 },
      {}
};

MODULE_DEVICE_TABLE(i2c, bme280_idtable);

static int32_t compensate_temperature(uint32_t uncomp_temp)
{
    int32_t var1;
    int32_t var2;
    int32_t t_fine;
    int32_t temperature;
    int32_t temperature_min = -4000;
    int32_t temperature_max = 8500;

    var1 = (int32_t)((uncomp_temp / 8) - ((int32_t)calib_t[0] * 2));
    var1 = (var1 * ((int32_t)calib_t[1])) >> 11;
    var2 = (int32_t)((uncomp_temp >> 4) - ((int32_t)calib_t[0]));
    var2 = (((var2 * var2) / 4096) * ((int32_t)calib_t[2])) / 16384;
    t_fine = var1 + var2;
    printk(KERN_INFO "t_fine %d t0 %d t1 %d t2 %d",t_fine,calib_t[0],calib_t[1],calib_t[2]);
    temperature = (t_fine * 5 + 128) / 256;

    if (temperature < temperature_min)
    {
        temperature = temperature_min;
    }
    else if (temperature > temperature_max)
    {
        temperature = temperature_max;
    }

    return temperature;
}

static void parse_sensor_data(const uint8_t *reg_data, uint32_t *temperature_raw)
{
    /* Variables to store the sensor data */
    uint32_t data_xlsb;
    uint32_t data_lsb;
    uint32_t data_msb;

    /* Store the parsed register values for temperature data */
    data_msb = (uint32_t)reg_data[0] << BME280_12_BIT_SHIFT;
    data_lsb = (uint32_t)reg_data[1] << BME280_4_BIT_SHIFT;
    data_xlsb = (uint32_t)reg_data[2] >> BME280_4_BIT_SHIFT;

    *temperature_raw = data_msb | data_lsb | data_xlsb;
}

int bme280_read_value(struct i2c_client *client, u8 cmd)
{
      int ret;
      ret = i2c_smbus_read_byte_data(client, cmd);
      if(ret<0)
      {
            printk(KERN_ERR"i2c read word failed\n");
            return -1;
      }
      //printk(KERN_INFO "reg value %d\n",ret);

      return ret;
}

int bme280_write_value(struct i2c_client *client, u8 cmd, u8 val)
{
      int ret;
      ret = i2c_smbus_write_byte_data(client, cmd, val);
      if(ret<0)
      {
            printk(KERN_ERR"i2c write byte failed\n");
            return -1;
      }
      return ret;
}

int bme280_probe(struct i2c_client *client)
{
      int ret=0;
      int bme_id;
      uint8_t reg[2];
      printk(KERN_INFO "BME280 I2C driver probed");
      bme_id = i2c_smbus_read_byte_data(bme280_s.bme280_i2c_client,BME280_ID_ADDR);

      if(bme_id!=BME280_ID)
      {
            ret=1;
            printk(KERN_ERR "BME280 ID did not match expected\n");
            goto exit;
      }

      ret = i2c_smbus_write_byte_data(bme280_s.bme280_i2c_client,
                                    BME280_REG_RESET,
                                    BME280_SOFT_RESET_COMMAND);
      if(ret!=0)
      {
            printk(KERN_ERR "Soft reset failed\n");
            goto exit;
      }

      msleep(2);

      reg[0]=bme280_read_value(bme280_s.bme280_i2c_client,BME280_REG_TEMP_PRESS_CALIB_DATA);
      reg[1]=bme280_read_value(bme280_s.bme280_i2c_client,BME280_REG_TEMP_PRESS_CALIB_DATA+1);
      calib_t[0] = BME280_CONCAT_BYTES(reg[1],reg[0]);

      reg[0]=bme280_read_value(bme280_s.bme280_i2c_client,BME280_REG_TEMP_PRESS_CALIB_DATA+2);
      reg[1]=bme280_read_value(bme280_s.bme280_i2c_client,BME280_REG_TEMP_PRESS_CALIB_DATA+3);
      calib_t[1] = BME280_CONCAT_BYTES(reg[1],reg[0]);

      reg[0]=bme280_read_value(bme280_s.bme280_i2c_client,BME280_REG_TEMP_PRESS_CALIB_DATA+4);
      reg[1]=bme280_read_value(bme280_s.bme280_i2c_client,BME280_REG_TEMP_PRESS_CALIB_DATA+5);
      calib_t[2] = BME280_CONCAT_BYTES(reg[1],reg[0]);

      bme280_write_value(bme280_s.bme280_i2c_client,BME280_REG_CTRL_MEAS,0x6);
      bme280_write_value(bme280_s.bme280_i2c_client,BME280_REG_CONFIG,0x0);
      exit:
      return ret;
}
static struct i2c_driver bme280_driver = {
        .driver = {
            .name   = "bme280",
            .owner  = THIS_MODULE,
        },
        .probe_new  = bme280_probe,
        .id_table   = bme280_idtable,
};



static ssize_t mymodule_show(struct kobject *kobj, 
                               struct kobj_attribute *attr, char *buf) 
{ 
      uint8_t reg[3];
      int32_t act_temp;
      uint32_t temp_raw;

      bme280_write_value(bme280_s.bme280_i2c_client,BME280_REG_CTRL_MEAS,0x21);

      do
      {
            reg[0]=bme280_read_value(bme280_s.bme280_i2c_client,BME280_REG_STATUS);
      }while(reg[0]&0x9);

      reg[0]=bme280_read_value(bme280_s.bme280_i2c_client,TEMP_MSB_CMD);
      reg[1]=bme280_read_value(bme280_s.bme280_i2c_client,TEMP_LSB_CMD);
      reg[2]=bme280_read_value(bme280_s.bme280_i2c_client,TEMP_XLSB_CMD);
      printk(KERN_INFO "msb %d lsb %d xlsb %d\n",reg[0],reg[1],reg[2]);

      parse_sensor_data(reg,&temp_raw);
      printk(KERN_INFO "temp raw %x",temp_raw);
      reg[0]=bme280_read_value(bme280_s.bme280_i2c_client,BME280_REG_CTRL_MEAS);
      printk(KERN_INFO "CTRL MEAS %x",reg[0]);
      act_temp=compensate_temperature(temp_raw);

      return sprintf(buf, "%d\n", act_temp); 
} 


static struct i2c_board_info bme280_info = {
      I2C_BOARD_INFO("bme280", 0x77),
};

static struct kobject *mymodule; 
static struct kobj_attribute temp_attribute = __ATTR(temperature, 0444, mymodule_show, NULL);

static int __init bme280_init(void)
{
      int ret=0;
	struct i2c_adapter *tmp_adapter;
	struct i2c_client *tmp_client;
	tmp_adapter = i2c_get_adapter(BUS_NO);
	if(tmp_adapter==NULL)
	{
		printk(KERN_ERR "i2c_get_adapter failed\n");
            ret=1;
		goto exit;
	}
      tmp_client = i2c_new_client_device(tmp_adapter, &bme280_info);
      if(tmp_client==NULL)
	{
		printk(KERN_ERR "i2c_new_client_device failed\n");
            ret=1;
		goto exit;
	}

      bme280_s.bme280_i2c_adapter = tmp_adapter;
      bme280_s.bme280_i2c_client = tmp_client;

      ret = i2c_add_driver(&bme280_driver);
      if(ret)
            printk(KERN_ERR "i2c_add_driver failed\n");
      
      i2c_put_adapter(tmp_adapter);

      mymodule = kobject_create_and_add("bme280_demo", kernel_kobj); 
      if (!mymodule) 
            return -ENOMEM; 

      ret = sysfs_create_file(mymodule, &temp_attribute.attr); 
      if (ret) { 
            printk(KERN_ERR"failed to create the temperature file " 
                  "in /sys/kernel/bme280_demo\n"); 
      } 
 
      exit:
      return ret;

}
module_init(bme280_init);

static void __exit bme280_cleanup(void)
{
      i2c_unregister_device(bme280_s.bme280_i2c_client);
      i2c_del_driver(&bme280_driver);
      sysfs_remove_file(mymodule, &temp_attribute.attr);
      kobject_del(mymodule); 
      
      printk(KERN_INFO "i2c driver removed\n");
}
module_exit(bme280_cleanup);
