#include <linux/fs.h>
#include <linux/device-mapper.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/bio.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/spinlock.h>
#include <linux/blkdev.h>

#define DM_MSG_PREFIX "dmp"

static atomic64_t read_reqs = ATOMIC64_INIT(0);
static atomic64_t write_reqs = ATOMIC64_INIT(0);
static atomic64_t total_reqs = ATOMIC64_INIT(0);
static atomic64_t read_sectors = ATOMIC64_INIT(0);
static atomic64_t write_sectors = ATOMIC64_INIT(0);
static atomic64_t total_sectors = ATOMIC64_INIT(0);

static struct kobject *dmp_kobj;

static ssize_t read_reqs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sysfs_emit(buf, "%llu\n", atomic64_read(&read_reqs));
}

static ssize_t write_reqs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sysfs_emit(buf, "%llu\n", atomic64_read(&write_reqs));
}

static ssize_t total_reqs_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sysfs_emit(buf, "%llu\n", atomic64_read(&total_reqs));
}

static ssize_t avg_read_size_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    u64 reads = atomic64_read(&read_reqs);
    u64 sectors = atomic64_read(&read_sectors);
    return sysfs_emit(buf, "%llu\n", reads ? sectors / reads : 0);
}

static ssize_t avg_write_size_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    u64 writes = atomic64_read(&write_reqs);
    u64 sectors = atomic64_read(&write_sectors);
    return sysfs_emit(buf, "%llu\n", writes ? sectors / writes : 0);
}

static ssize_t avg_size_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    u64 total = atomic64_read(&total_reqs);
    u64 sectors = atomic64_read(&total_sectors);
    return sysfs_emit(buf, "%llu\n", total ? sectors / total : 0);
}

static struct kobj_attribute read_reqs_attr = __ATTR_RO(read_reqs);
static struct kobj_attribute write_reqs_attr = __ATTR_RO(write_reqs);
static struct kobj_attribute total_reqs_attr = __ATTR_RO(total_reqs);
static struct kobj_attribute avg_read_size_attr = __ATTR_RO(avg_read_size);
static struct kobj_attribute avg_write_size_attr = __ATTR_RO(avg_write_size);
static struct kobj_attribute avg_size_attr = __ATTR_RO(avg_size);

static struct attribute *attrs[] = {
    &read_reqs_attr.attr,
    &write_reqs_attr.attr,
    &total_reqs_attr.attr,
    &avg_read_size_attr.attr,
    &avg_write_size_attr.attr,
    &avg_size_attr.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};


static int dmp_ctr(struct dm_target *ti, unsigned int argc, char **argv) {
    struct dm_dev *dev;
    struct block_device *bdev;

    if (argc != 1) {
        ti->error = "Expected one argument: underlying device path";
        return -EINVAL;
    }

    if (dm_get_device(ti, argv[0], FMODE_READ | FMODE_WRITE, &dev)) {
        ti->error = "Failed to open underlying device via dm_get_device";
        return -EINVAL;
    }

    bdev = dev->bdev;

    if (get_capacity(bdev->bd_disk) < ti->len) {
        dm_put_device(ti, dev);
        ti->error = "Underlying device size smaller than target length";
        return -EINVAL;
    }

    ti->private = dev;

    ti->num_discard_bios = 1;
    ti->discards_supported = true;

    return 0;
}

static void dmp_dtr(struct dm_target *ti) {
    struct dm_dev *dev = ti->private;
    if (dev)
        dm_put_device(ti, dev);
    ti->private = NULL;
}

static int dmp_map(struct dm_target *ti, struct bio *bio) {
    struct dm_dev *dev = ti->private;
    struct block_device *bdev = dev->bdev;
    unsigned int sectors = bio_sectors(bio);

    atomic64_add(sectors, &total_sectors);
    atomic64_inc(&total_reqs);

	switch (bio_op(bio)) {
        case REQ_OP_READ:
            atomic64_inc(&read_reqs);
            atomic64_add(sectors, &read_sectors);
            break;
        case REQ_OP_WRITE:
            atomic64_inc(&write_reqs);
            atomic64_add(sectors, &write_sectors);
            break;
        case REQ_OP_DISCARD:
            break;
        default:
            return DM_MAPIO_KILL;
    }

    bio_set_dev(bio, bdev);
    submit_bio(bio);

    return DM_MAPIO_SUBMITTED;
}

static void dmp_io_hints(struct dm_target *ti, struct queue_limits *limits) {
    limits->max_hw_discard_sectors = UINT_MAX;
    limits->discard_granularity = 512;
}

static struct target_type dmp_target = {
    .name = "dmp",
    .version = {1, 0, 0},
    .features = DM_TARGET_NOWAIT,
    .module = THIS_MODULE,
    .ctr = dmp_ctr,
    .dtr = dmp_dtr,
    .map = dmp_map,
    .io_hints = dmp_io_hints,
};

static int __init dmp_init(void) {
    int ret;

    ret = dm_register_target(&dmp_target);
    if (ret)
        return ret;

    dmp_kobj = kobject_create_and_add("dmp", kernel_kobj);
    if (!dmp_kobj)
        return -ENOMEM;

    ret = sysfs_create_group(dmp_kobj, &attr_group);
    if (ret)
        kobject_put(dmp_kobj);

    return ret;
}

static void __exit dmp_exit(void) {
    sysfs_remove_group(dmp_kobj, &attr_group);
    kobject_put(dmp_kobj);
    dm_unregister_target(&dmp_target);
}

module_init(dmp_init);
module_exit(dmp_exit);

MODULE_LICENSE("GPL");