#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>

// Prototypes
static int leds_probe(struct platform_device *pdev);
static int leds_remove(struct platform_device *pdev);
static ssize_t leds_read(struct file *file, char *buffer, size_t len, loff_t *offset);
static ssize_t leds_write(struct file *file, const char *buffer, size_t len, loff_t *offset);
static ssize_t speed_read(struct file *file, char *buffer, size_t len, loff_t *offset);
static ssize_t speed_write(struct file *file, const char *buffer, size_t len, loff_t *offset);
static ssize_t dir_read(struct file *file, char *buffer, size_t len, loff_t *offset);
static ssize_t dir_write(struct file *file, const char *buffer, size_t len, loff_t *offset);
static void chenillard_timer_func(struct timer_list *t);
static void set_leds_pattern(u8 pattern);

// An instance of this structure will be created for every ensea_led IP in the system
struct ensea_leds_dev {
    struct miscdevice miscdev;
    void __iomem *regs;
    u8 leds_value;
    struct timer_list chenillard_timer;
    int speed;  // Vitesse du chenillard
    int direction;  // Direction du chenillard
};

// Specify which device tree devices this driver supports
static struct of_device_id ensea_leds_dt_ids[] = {
    {
        .compatible = "dev,ensea"
    },
    { /* end of table */ }
};

// Inform the kernel about the devices this driver supports
MODULE_DEVICE_TABLE(of, ensea_leds_dt_ids);

// Data structure that links the probe and remove functions with our driver
static struct platform_driver leds_platform = {
    .probe = leds_probe,
    .remove = leds_remove,
    .driver = {
        .name = "Ensea LEDs Driver",
        .owner = THIS_MODULE,
        .of_match_table = ensea_leds_dt_ids
    }
};

// The file operations that can be performed on the ensea_leds character file
static const struct file_operations ensea_leds_fops = {
    .owner = THIS_MODULE,
    .read = leds_read,
    .write = leds_write
};

// Called when the driver is installed
static int leds_init(void)
{
    int ret_val = 0;
    pr_info("Initializing the Ensea LEDs module\n");

    // Register our driver with the "Platform Driver" bus
    ret_val = platform_driver_register(&leds_platform);
    if(ret_val != 0) {
        pr_err("platform_driver_register returned %d\n", ret_val);
        return ret_val;
    }

    pr_info("Ensea LEDs module successfully initialized!\n");

    return 0;
}

// Called whenever the kernel finds a new device that our driver can handle
// (In our case, this should only get called for the one instantiation of the Ensea LEDs module)
static int leds_probe(struct platform_device *pdev)
{
    int ret_val = -EBUSY;
    struct ensea_leds_dev *dev;
    struct resource *r = NULL;

    pr_info("leds_probe enter\n");

    // Get the memory resources for this LED device
    r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if(r == NULL) {
        pr_err("IORESOURCE_MEM (register space) does not exist\n");
        goto bad_exit_return;
    }

    // Create structure to hold device-specific information (like the registers)
    dev = devm_kzalloc(&pdev->dev, sizeof(struct ensea_leds_dev), GFP_KERNEL);

    // Both request and ioremap a memory region
    dev->regs = devm_ioremap_resource(&pdev->dev, r);
    if(IS_ERR(dev->regs))
        goto bad_ioremap;

    // Initialize LEDs
    dev->leds_value = 0xFF;  // All LEDs on initially
    iowrite32(dev->leds_value, dev->regs);

    // Initialize speed and direction
    dev->speed = 1;
    dev->direction = 1;

    // Create misc device to handle user-space interactions
    dev->miscdev.minor = MISC_DYNAMIC_MINOR;
    dev->miscdev.name = "ensea_leds";
    dev->miscdev.fops = &ensea_leds_fops;

    ret_val = misc_register(&dev->miscdev);
    if(ret_val != 0) {
        pr_info("Couldn't register misc device :(\n");
        goto bad_exit_return;
    }

    // Setup the timer for the chenillard effect
    timer_setup(&dev->chenillard_timer, chenillard_timer_func, 0);
    mod_timer(&dev->chenillard_timer, jiffies + msecs_to_jiffies(1000 / dev->speed));

    // Save the device data in the platform device
    platform_set_drvdata(pdev, (void*)dev);

    pr_info("leds_probe exit\n");

    return 0;

bad_ioremap:
    ret_val = PTR_ERR(dev->regs);
bad_exit_return:
    pr_info("leds_probe bad exit :(\n");
    return ret_val;
}

// Function to update the LEDs pattern
static void set_leds_pattern(u8 pattern)
{
    struct ensea_leds_dev *dev = container_of(&pattern, struct ensea_leds_dev, leds_value);
    // Write the new pattern to the device
    iowrite32(pattern, dev->regs);
}

// Timer function to implement chenillard effect
static void chenillard_timer_func(struct timer_list *t)
{
    struct ensea_leds_dev *dev = from_timer(dev, t, ensea_leds_dev.chenillard_timer);

    // Update LED pattern based on direction
    if (dev->direction == 1) {
        dev->leds_value <<= 1;  // Move left to right
        if (dev->leds_value == 0) {
            dev->leds_value = 0x01;  // Reset to the first LED
        }
    } else {
        dev->leds_value >>= 1;  // Move right to left
        if (dev->leds_value == 0) {
            dev->leds_value = 0x80;  // Reset to the last LED
        }
    }

    set_leds_pattern(dev->leds_value);

    // Reset the timer to create the chenillard effect
    mod_timer(&dev->chenillard_timer, jiffies + msecs_to_jiffies(1000 / dev->speed));
}

// Read speed from /proc/ensea/speed
static ssize_t speed_read(struct file *file, char *buffer, size_t len, loff_t *offset)
{
    struct ensea_leds_dev *dev = container_of(file->private_data, struct ensea_leds_dev, miscdev);
    return simple_read_from_buffer(buffer, len, offset, &dev->speed, sizeof(dev->speed));
}

// Write speed to /proc/ensea/speed
static ssize_t speed_write(struct file *file, const char *buffer, size_t len, loff_t *offset)
{
    struct ensea_leds_dev *dev = container_of(file->private_data, struct ensea_leds_dev, miscdev);
    int new_speed;

    if (kstrtoint_from_user(buffer, len, 10, &new_speed)) {
        pr_err("Invalid speed value\n");
        return -EINVAL;
    }

    dev->speed = new_speed;
    return len;
}

// Read direction from /proc/ensea/dir
static ssize_t dir_read(struct file *file, char *buffer, size_t len, loff_t *offset)
{
    struct ensea_leds_dev *dev = container_of(file->private_data, struct ensea_leds_dev, miscdev);
    return simple_read_from_buffer(buffer, len, offset, &dev->direction, sizeof(dev->direction));
}

// Write direction to /proc/ensea/dir
static ssize_t dir_write(struct file *file, const char *buffer, size_t len, loff_t *offset)
{
    struct ensea_leds_dev *dev = container_of(file->private_data, struct ensea_leds_dev, miscdev);
    int new_direction;

    if (kstrtoint_from_user(buffer, len, 10, &new_direction)) {
        pr_err("Invalid direction value\n");
        return -EINVAL;
    }

    dev->direction = new_direction;
    return len;
}

// Remove the device (cleanup)
static int leds_remove(struct platform_device *pdev)
{
    struct ensea_leds_dev *dev = platform_get_drvdata(pdev);

    pr_info("leds_remove enter\n");

    // Turn off LEDs
    iowrite32(0x00, dev->regs);

    // Deregister the misc device
    misc_deregister(&dev->miscdev);

    pr_info("leds_remove exit\n");

    return 0;
}

// Exit function for the driver
static void leds_exit(void)
{
    pr_info("Ensea LEDs module exit\n");
    platform_driver_unregister(&leds_platform);
    pr_info("Ensea LEDs module successfully unregistered\n");
}

// Module initialization and exit
module_init(leds_init);
module_exit(leds_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Devon Andrade <devon.andrade@oit.edu>");
MODULE_DESCRIPTION("Exposes a character device to user space that lets users turn LEDs on and off");
MODULE_VERSION("1.0");
