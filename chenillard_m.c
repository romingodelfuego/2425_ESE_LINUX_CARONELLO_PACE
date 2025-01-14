#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define PROC_DIR "ensea"
#define PROC_FILE "chenille"

static int speed = 0; // Variable globale pour stocker la vitesse

ssize_t proc_read(struct file *file, char __user *buffer, size_t len, loff_t *offset) {
    char temp_buf[16];
    int ret;

    if (*offset > 0) return 0; // EOF

    ret = snprintf(temp_buf, sizeof(temp_buf), "%d\n", speed);
    if (copy_to_user(buffer, temp_buf, ret)) return -EFAULT;

    *offset = ret;
    return ret;
}

ssize_t proc_write(struct file *file, const char __user *buffer, size_t len, loff_t *offset) {
    char input_buf[16];

    if (len >= sizeof(input_buf)) return -EFAULT;
    if (copy_from_user(input_buf, buffer, len)) return -EFAULT;

    input_buf[len] = '\0';
    if (kstrtoint(input_buf, 10, &speed)) return -EINVAL;

    // Log dans le dmesg lorsque le paramètre speed est modifié
    printk(KERN_INFO "Speed updated to %d\n", speed);

    return len;
}

// Utilisation de la structure appropriée en fonction de la version du noyau
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};
#else
static const struct file_operations proc_fops = {
    .read = proc_read,
    .write = proc_write,
};
#endif

static int __init proc_speed_init(void) {
    // Créer le répertoire /proc/ensea si nécessaire
    struct proc_dir_entry *proc_dir = proc_mkdir(PROC_DIR, NULL);
    if (!proc_dir) {
        printk(KERN_ALERT "Failed to create /proc/%s\n", PROC_DIR);
        return -ENOMEM;
    }

    // Créer le fichier /proc/ensea/chenille
    if (!proc_create(PROC_FILE, 0666, proc_dir, &proc_fops)) {
        remove_proc_entry(PROC_DIR, NULL);
        printk(KERN_ALERT "Failed to create /proc/%s/%s\n", PROC_DIR, PROC_FILE);
        return -ENOMEM;
    }

    printk(KERN_INFO "Module loaded: /proc/%s/%s created\n", PROC_DIR, PROC_FILE);
    return 0;
}

static void __exit proc_speed_exit(void) {
    // Supprimer le fichier et le répertoire
    remove_proc_entry(PROC_FILE, NULL);
    remove_proc_entry(PROC_DIR, NULL);

    printk(KERN_INFO "Module unloaded: /proc/%s/%s removed\n", PROC_DIR, PROC_FILE);
}

module_init(proc_speed_init);
module_exit(proc_speed_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Minimal Example");
MODULE_DESCRIPTION("Procfs Speed Example with /proc/ensea/chenille");
