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
const char onic_drv_str[] = DRV_STR;
const char onic_drv_ver[] = DRV_VER;

MODULE_AUTHOR("Xilinx Research Labs");
MODULE_DESCRIPTION(DRV_STR);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VER);

static int RS_FEC_ENABLED=1;
module_param(RS_FEC_ENABLED, int, 0644);
struct class *class; 
struct device *device; 
static dev_t spmv_dev;
static struct cdev spmv_cdev;

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

// static const struct net_device_ops onic_netdev_ops = {
// 	.ndo_open = onic_open_netdev,
// 	.ndo_stop = onic_stop_netdev,
// 	.ndo_start_xmit = onic_xmit_frame,
// 	.ndo_set_mac_address = onic_set_mac_address,
// 	.ndo_do_ioctl = onic_do_ioctl,
// 	.ndo_change_mtu = onic_change_mtu,
// 	.ndo_get_stats64 = onic_get_stats64,
// };

static int mychardev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "spmvdev: Device opened\n");
    return 0;
}

static int mychardev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "spmvdev: Device closed\n");
    return 0;
}

static struct file_operations spmv_fops = {
    .owner = THIS_MODULE,
    .open = mychardev_open,
    .release = mychardev_release,
    // .read = mychardev_read,
    // .write = onic_xmit_frame,
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
	// struct net_device *netdev;
	struct onic_private *priv;
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


    device = device_create(class,NULL,spmv_dev,NULL,DEVICE_NAME);

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
	device_destroy(class,spmv_dev);  //注销设备
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
    int major_num,minor_num;
	pr_info("%s %s", onic_drv_str, onic_drv_ver);
	
	if (alloc_chrdev_region(&spmv_dev, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_ERR "Failed to allocate character device region\n");
        return -1;
    }

	cdev_init(&spmv_cdev, &spmv_fops);
	
	if (cdev_add(&spmv_cdev, spmv_dev, 1) < 0) {
        printk(KERN_ERR "Failed to add character device\n");
        unregister_chrdev_region(spmv_dev, 1);
        return -1;
    }
    major_num = MAJOR(spmv_dev);
    minor_num = MINOR(spmv_dev);
    printk(KERN_INFO"spmvdev: device  major_num = %d\n",major_num);
    printk(KERN_INFO"spmvdev: device minor_num = %d\n",minor_num);


    class = class_create(THIS_MODULE,DEVICE_NAME); //注册类
	// 


    printk(KERN_INFO "spmvdev: Character device registered\n");
  
	// return 0;
	return pci_register_driver(&pci_driver);
}

static void __exit onic_exit_module(void)
{
	
	pci_unregister_driver(&pci_driver);

	cdev_del(&spmv_cdev);
    unregister_chrdev_region(spmv_dev, 1);
	class_destroy(class); //注销类
    printk(KERN_INFO "spmvdev: Character device unregistered\n");
}

module_init(onic_init_module);
module_exit(onic_exit_module);
