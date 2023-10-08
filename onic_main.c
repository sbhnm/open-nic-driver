/*
 * Copyright (c) 2020 Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/moduleparam.h>

#include "onic.h"
#include "onic_hardware.h"
#include "onic_lib.h"
#include "onic_common.h"
#include "onic_netdev.h"
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#ifndef ONIC_VF
#define DRV_STR "OpenNIC Linux Kernel Driver"
char onic_drv_name[] = "onic";
#else
#define DRV_STR "OpenNIC Linux Kernel Driver (VF)"
char onic_drv_name[] = "open-nic-vf";
#endif
#define DEVICE_NAME "spmv_dev"
#define DRV_VER "0.21"
#define BUFFER_SIZE 4096
const char onic_drv_str[] = DRV_STR;
const char onic_drv_ver[] = DRV_VER;

MODULE_AUTHOR("Xilinx Research Labs");
MODULE_DESCRIPTION(DRV_STR);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VER);

static int RS_FEC_ENABLED=1;
module_param(RS_FEC_ENABLED, int, 0644);
struct class *class; 


static const struct pci_device_id onic_pci_tbl[] = {
	/* PCIe lane width x16 */
	{ PCI_DEVICE(0x10ee, 0x903f), },	/* PF 0 */


	{0,}
};

MODULE_DEVICE_TABLE(pci, onic_pci_tbl);

/**
 * Default MAC address 00:0A:35:00:00:00
 * First three octets indicate OUI (00:0A:35 for Xilinx)
 * Note that LSB of the first octet must be 0 (unicast)
 **/
static const unsigned char onic_default_dev_addr[] = {
	0x00, 0x0A, 0x35, 0x00, 0x00, 0x00
};

static int spmv_chardev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "spmvdev: Device opened\n");
    return 0;
}

static int spmv_chardev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "spmvdev: Device closed\n");
    return 0;
}
static ssize_t spmv_chardev_read(struct file *file, char __user *user_buffer, size_t count, loff_t *offset)
{
    // ssize_t bytes_read = 0;
    // if (*offset < BUFFER_SIZE) {
    //     bytes_read = min(count, (size_t)(BUFFER_SIZE - *offset));
    //     if (copy_to_user(user_buffer, &device_buffer[*offset], bytes_read)) {
    //         return -EFAULT;
    //     }
    //     *offset += bytes_read;
    // }
    // return bytes_read;
	return 0;
}
void test_end(void)
{
	return;
}
static ssize_t spmv_chardev_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *offset)
{
	struct onic_private * priv = dev_get_drvdata(file->f_inode->i_cdev->dev);
	void * spmv_dma_data;
	ssize_t bytes_written = 0;
	// enable_dma_send
	if (*offset < BUFFER_SIZE) 
	{
		bytes_written = min(count, (size_t)(BUFFER_SIZE - *offset));
		spmv_dma_data = kmalloc(bytes_written,GFP_KERNEL);
		if (copy_from_user(spmv_dma_data, user_buffer, bytes_written)) 
		{
            return -EFAULT;
        }
	}
	
	onic_xmit_frame(,priv);
	
	test_end();
	// test send end

	// free
	kfree(spmv_dma_data);
    // ssize_t bytes_written = 0;
    // if (*offset < BUFFER_SIZE) {
    //     bytes_written = min(count, (size_t)(BUFFER_SIZE - *offset));
    //     if (copy_from_user(&device_buffer[*offset], user_buffer, bytes_written)) {
    //         return -EFAULT;
    //     }
    //     *offset += bytes_written;
    // }
    // return bytes_written;
	return 0;
}
static long spmv_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static struct file_operations spmv_fops = {
    .owner = THIS_MODULE,
    .open = spmv_chardev_open,
    .release = spmv_chardev_release,
    .read = spmv_chardev_read,
    .write = spmv_chardev_write,
	.unlocked_ioctl = spmv_unlocked_ioctl,
};

extern void onic_set_ethtool_ops(struct net_device *netdev);

/**
 * onic_probe - Probe and initialize PCI device
 * @pdev: pointer to PCI device
 * @ent: pointer to PCI device ID entries
 *
 * Return 0 on success, negative on failure
 **/
static int onic_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct device *spmv_device;
	struct onic_private *priv;
	int major_num,minor_num;

	
	// struct sockaddr saddr;
	// char dev_name[IFNAMSIZ];
	int rv;
	/* int pci_using_dac; */


	dev_info(&pdev->dev,"Setup Start!");
	rv = pci_enable_device_mem(pdev);
	if (rv < 0) {
		dev_err(&pdev->dev, "pci_enable_device_mem, err = %d", rv);
		return rv;
	}

	// /* QDMA only supports 32-bit consistent DMA for descriptor ring */
	rv = dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
	if (rv < 0) {
		dev_err(&pdev->dev, "Failed to set DMA masks");
		goto disable_device;
	} else {
		dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
	}

	rv = pci_request_mem_regions(pdev, onic_drv_name);
	if (rv < 0) {
		dev_err(&pdev->dev, "pci_request_mem_regions, err = %d", rv);
		goto disable_device;
	}

	// /* enable relaxed ordering */
	pcie_capability_set_word(pdev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_RELAX_EN);
	// /* enable extended tag */
	pcie_capability_set_word(pdev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_EXT_TAG);
	pci_set_master(pdev);
	pci_save_state(pdev);
	pcie_set_readrq(pdev, 512);





	// priv = netdev_priv(netdev);
	priv = kmalloc(sizeof(struct onic_private),GFP_KERNEL);

	memset(priv, 0, sizeof(struct onic_private));
	priv->RS_FEC = RS_FEC_ENABLED;

	if (PCI_FUNC(pdev->devfn) == 0) {
		dev_info(&pdev->dev, "device is a master PF");
		set_bit(ONIC_FLAG_MASTER_PF, priv->flags);
	}
	priv->pdev = pdev;
	// priv->netdev = netdev;
	spin_lock_init(&priv->tx_lock);
	spin_lock_init(&priv->rx_lock);

	rv = onic_init_capacity(priv);
	if (rv < 0) {
		dev_err(&pdev->dev, "onic_init_capacity, err = %d", rv);
		goto clear_capacity;
	}

	rv = onic_init_hardware(priv);
	if (rv < 0) {
		dev_err(&pdev->dev, "onic_init_hardware, err = %d", rv);
		goto clear_capacity;
	}

	rv = onic_init_interrupt(priv);
	if (rv < 0) {
		dev_err(&pdev->dev, "onic_init_interrupt, err = %d", rv);
		goto clear_hardware;
	}


	// rv = register_netdev(netdev);
	// if (rv < 0) {
	// 	dev_err(&pdev->dev, "register_netdev, err = %d", rv);
	// 	goto clear_interrupt;
	// }

	pci_set_drvdata(pdev, priv);

	if (alloc_chrdev_region(&priv->spmv_dev, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_ERR "Failed to allocate character device region\n");
        return -1;
    }

	cdev_init(&priv->spmv_cdev, &spmv_fops);
	
	if (cdev_add(&priv->spmv_cdev, priv->spmv_dev, 1) < 0) {
        printk(KERN_ERR "Failed to add character device\n");
        unregister_chrdev_region(priv->spmv_dev, 1);
        return -1;
    }
    major_num = MAJOR(priv->spmv_dev);
    minor_num = MINOR(priv->spmv_dev);
    printk(KERN_INFO"spmvdev: device  major_num = %d\n",major_num);
    printk(KERN_INFO"spmvdev: device minor_num = %d\n",minor_num);


    spmv_device = device_create(class,NULL,priv->spmv_dev,NULL,DEVICE_NAME);
	dev_set_drvdata(spmv_device, priv);
	dev_info(&pdev->dev,"Setup OK!");
	return 0;

// clear_interrupt:
	// onic_clear_interrupt(priv);
clear_hardware:
	onic_clear_interrupt(priv);
	onic_clear_hardware(priv);
clear_capacity:
	onic_clear_capacity(priv);
// release_pci_mem:
// 	pci_release_mem_regions(pdev);
disable_device:
	pci_disable_device(pdev);

	return rv;
}

/**
 * onic_remove - remove PCI device
 * @pdev: pointer to PCI device
 **/
static void onic_remove(struct pci_dev *pdev)
{
	struct onic_private *priv = pci_get_drvdata(pdev);


	onic_clear_interrupt(priv);
	onic_clear_hardware(priv);
	onic_clear_capacity(priv);


	pci_set_drvdata(pdev, NULL);
	pci_release_mem_regions(pdev);
    
	dev_info(&pdev->dev,"Rm OK!");

	pci_disable_device(pdev);
	kfree(priv);

	
	device_destroy(class,priv->spmv_dev);  //注销设备
	
	cdev_del(&priv->spmv_cdev);
    unregister_chrdev_region(priv->spmv_dev, 1);

}

/* static const struct pci_error_handlers qdma_err_handler = { */
/*     .error_detected		    = qdma_error_detected, */
/*     .slot_reset		    = qdma_slot_reset, */
/*     .resume			    = qdma_error_resume, */
/* #if KERNEL_VERSION(4, 13, 0) <= LINUX_VERSION_CODE */
/*     .reset_prepare		    = qdma_reset_prepare, */
/*     .reset_done		    = qdma_reset_done, */
/* #elif KERNEL_VERSION(3, 16, 0) <= LINUX_VERSION_CODE */
/*     .reset_notify		    = qdma_reset_notify, */
/* #endif */
/* }; */

static struct pci_driver pci_driver = {
	.name = onic_drv_name,
	.id_table = onic_pci_tbl,
	.probe = onic_probe,
	.remove = onic_remove,
};

static int __init onic_init_module(void)
{
    
	pr_info("%s %s", onic_drv_str, onic_drv_ver);

    class = class_create(THIS_MODULE,DEVICE_NAME); //注册类
	// 


    printk(KERN_INFO "spmvdev: Character device registered\n");
  
	// return 0;
	return pci_register_driver(&pci_driver);
}

static void __exit onic_exit_module(void)
{
	
	pci_unregister_driver(&pci_driver);

	class_destroy(class); //注销类
    printk(KERN_INFO "spmvdev: Character device unregistered\n");
}

module_init(onic_init_module);
module_exit(onic_exit_module);
