//=============================================================
//
// Copyright (c) 2023 Simon Southwell. All rights reserved.
//
// Date: 17th September 2023
//
// This file is part of the kernel module exmaple.
//
// The code is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This code is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this code. If not, see <http://www.gnu.org/licenses/>.
//
//=============================================================

// ------------------------------------------------------------
// Headers
// ------------------------------------------------------------

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

// Task specific APIs
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>

// ------------------------------------------------------------
// Definitions
// ------------------------------------------------------------

#define CLASS_NAME  "chardrv"
#define DEVICE_NAME "wy_module"

// ------------------------------------------------------------
// Set the module configurations
// ------------------------------------------------------------

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wyvern Semiconductors");
MODULE_DESCRIPTION("A simple Linux module.");
MODULE_VERSION("0.01");

// ------------------------------------------------------------
// Device file operation function prototypes
// ------------------------------------------------------------

// Prototypes for device functions
static int  __init wy_module_init      (void);
static void __exit wy_module_exit      (void);
static int         wy_module_open      (struct inode *, struct file *);
static int         wy_module_release   (struct inode *, struct file *);
static ssize_t     wy_module_read      (struct file *,  char *, size_t, loff_t *);
static ssize_t     wy_module_write     (struct file *,  const char *, size_t, loff_t *);

// ------------------------------------------------------------
// Device file structure configuration
// ------------------------------------------------------------

// This structure points to the driver's device functions for a character device
static struct file_operations fops =
{
 .read    = wy_module_read,
 .write   = wy_module_write,
 .open    = wy_module_open,
 .release = wy_module_release
};

// Register initialisation and exit functions
module_init (wy_module_init);
module_exit (wy_module_exit);

// ------------------------------------------------------------
// Internal driver parameter structure definition
// ------------------------------------------------------------

// Define a structure for received parameters
typedef struct {
    uint32_t  cmd;
    uint32_t* vaddr;
    uint32_t  len;
} params_t;

// ------------------------------------------------------------
// Static variables
// ------------------------------------------------------------

static int            wy_module_open_count = 0;  // Count of open instances---we will only allow 1
static int            wy_module_major_num;       // Storage for major number assigned at initialisation
static params_t       params;                    // Structure containing driver parameters
static struct class*  wy_module_class;
static struct device* wy_module_device;

// ------------------------------------------------------------
// Module initialisation on loading
// ------------------------------------------------------------

static int __init wy_module_init(void)
{
    // Try to register character device. A first argument of 0 means allocate a major number for us.
    // This value is returned by the function.
    wy_module_major_num = register_chrdev(0, "wy_module", &fops);

    // If major number negative, an error occured
    if (wy_module_major_num < 0)
    {
        printk(KERN_ALERT "Could not register device: %d\n", wy_module_major_num);

        return wy_module_major_num;
    }

    // Send a message to advertise the assigned major number.
    printk(KERN_INFO "wy_module module loaded successfully. Major number = %d\n", wy_module_major_num);

    // Register the device class
    wy_module_class = class_create(THIS_MODULE, CLASS_NAME);

    // Check for error and clean up if there is
    if (IS_ERR(wy_module_class))
    {
        unregister_chrdev(wy_module_major_num, DEVICE_NAME);

        printk(KERN_ALERT "Failed to register device class\n");

        return PTR_ERR(wy_module_class); // Correct way to return an error on a pointer
    }

    printk(KERN_INFO "wy_module: device class registered correctly\n");

    // Register the device driver
    wy_module_device = device_create(wy_module_class, NULL, MKDEV(wy_module_major_num, 0), NULL, DEVICE_NAME);

    if (IS_ERR(wy_module_device))
    {
        // Clean up if there is an error
        class_destroy(wy_module_class);
        unregister_chrdev(wy_module_major_num, DEVICE_NAME);

        printk(KERN_ALERT "wy_module: Failed to create the device\n");

        return PTR_ERR(wy_module_device);
    }

    printk(KERN_INFO "wy_module: device class created correctly\n");

    return 0;
}
// ------------------------------------------------------------
// Functon called on unloading the module
// ------------------------------------------------------------

static void __exit wy_module_exit(void)
{
    // Remove the device
    device_destroy(wy_module_class, MKDEV(wy_module_major_num, 0));

    // Unregister the device class
    class_unregister(wy_module_class);

    // Remove the device class
    class_destroy(wy_module_class);

    // Unregister the character device
    unregister_chrdev(wy_module_major_num, DEVICE_NAME);

    // #########################
    // Put any tidy up code here
    // #########################

    printk(KERN_INFO "Exiting wy_module\n");
}

// ------------------------------------------------------------
// Called on opening the device file
// ------------------------------------------------------------

static int wy_module_open(struct inode *inode, struct file *file)
{
    // If device is open, return busy
    if (wy_module_open_count)
    {
        return -EBUSY;
    }

    // Increment the open count
    wy_module_open_count++;

    try_module_get(THIS_MODULE);

    return 0;
}

// ------------------------------------------------------------
// Called upon closing the device file
// ------------------------------------------------------------

static int wy_module_release(struct inode *inode, struct file *file)
{
    // Decrement the open counter
    if (wy_module_open_count)
    {
        wy_module_open_count--;
    }

    // #########################
    // Put any tidy up code here
    // #########################

    module_put(THIS_MODULE);

    return 0;
}

// ------------------------------------------------------------
// Device write operation
// ------------------------------------------------------------

static ssize_t wy_module_write(struct file *fp, const char *buffer, size_t len, loff_t *offset)
{
    int   bytes_written = 0;
    char* paramPtr      = (char*)&params;

    // Expecting exactly the right number of parameter bytes
    if (len != sizeof(params_t))
    {
        return 0;
    }

    // Get the user-land bytes and put in the parameter buffer
    while (bytes_written < len)
    {
        // Use put_user to send message to user domain.
        get_user(*paramPtr, buffer++);
        paramPtr++;
        bytes_written++;
    }

    // ######################
    // Driver write code here
    // ######################
    switch(params.cmd)
    {

        default:
        printk(KERN_INFO "wy_module write default operation\n");
    break;
    }
    // ######################

    return bytes_written;
}

// ------------------------------------------------------------
// Device read operation
// ------------------------------------------------------------

static ssize_t wy_module_read(struct file *fp, char *buffer, size_t len, loff_t *offset)
{
    int   bytes_read = 0;
    char* paramPtr    = (char*)&params;

    // Expecting exactly the right number of parameter bytes to be read
    if (len != sizeof(params_t))
    {
        return 0;
    }

    // Put the parameter bytes in the user-land buffer
    while (bytes_read < len)
    {
        // Use put_user to send message to user domain.
        put_user(*paramPtr, buffer++);
        paramPtr++;
        bytes_read++;
    }

    return bytes_read;
}
