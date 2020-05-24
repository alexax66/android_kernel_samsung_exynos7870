/*
 * Author: Paul Reioux aka Faux123 <reioux@gmail.com>
 *
 * Copyright 2013 Paul Reioux
 * Copyright 2012 Paul Reioux
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/writeback.h>
#include <linux/fb.h>

#ifdef CONFIG_DYNAMIC_FSYNC_BG_SYNC
#include <linux/delay.h>
#endif

#define DYN_FSYNC_VERSION_MAJOR 1
#define DYN_FSYNC_VERSION_MINOR 1

struct notifier_block dyn_fsync_fb_notif;

#ifdef CONFIG_DYNAMIC_FSYNC_BG_SYNC
#define BG_SYNC_TIMEOUT 10	// 10*10ms

static struct workqueue_struct *suspend_sync_wq;
static void work_sync_fn(struct work_struct *work);
static DECLARE_WORK(work_sync, work_sync_fn);
static int suspend_sync_done;
#endif

/*
 * fsync_mutex protects dyn_fsync_active during fb suspend / resume
 * transitions
 */
static DEFINE_MUTEX(fsync_mutex);
bool dyn_sync_scr_suspended = false;
bool dyn_fsync_active __read_mostly = true;

extern void dyn_fsync_suspend_actions(void);

#ifdef CONFIG_DYNAMIC_FSYNC_BG_SYNC
static int bg_sync(void)
{
	int timeout_in_ms = BG_SYNC_TIMEOUT;
	bool ret = false;

	if (work_busy(&work_sync)) {
		pr_info("[dynamic_fsync_bg_sync] work_sync already run\n");
		return -EBUSY;
	}

	pr_info("[dynamic_fsync_bg_sync] queue start\n");
	suspend_sync_done = 0;
	ret = queue_work(suspend_sync_wq, &work_sync);
	pr_info("[dynamic_fsync_bg_sync] queue end, ret = %s\n", ret?"true":"false");

	while (timeout_in_ms--) {
		if (suspend_sync_done)
			break;
		msleep(10);
	}

	if (suspend_sync_done) {
		pr_info("[dynamic_fsync_bg_sync] (%d * 10ms) ...\n", BG_SYNC_TIMEOUT - timeout_in_ms);
		return 0;
	}

	return -EBUSY;
}

static void work_sync_fn(struct work_struct *work)
{
	pr_info("[dynamic_fsync_bg_sync] sync start\n");
	dyn_fsync_suspend_actions();
	pr_info("[dynamic_fsync_bg_sync] sync done\n");
	suspend_sync_done = 1;
}
#endif

static ssize_t dyn_fsync_active_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (dyn_fsync_active ? 1 : 0));
}

static ssize_t dyn_fsync_active_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int data;

	if(sscanf(buf, "%u\n", &data) == 1) {
		if (data == 1) {
			pr_info("%s: dynamic fsync enabled\n", __func__);
			dyn_fsync_active = true;
		}
		else if (data == 0) {
			pr_info("%s: dyanamic fsync disabled\n", __func__);
			dyn_fsync_active = false;
		}
		else
			pr_info("%s: bad value: %u\n", __func__, data);
	} else
		pr_info("%s: unknown input!\n", __func__);

	return count;
}

static ssize_t dyn_fsync_version_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "version: %u.%u by faux123\n",
		DYN_FSYNC_VERSION_MAJOR,
		DYN_FSYNC_VERSION_MINOR);
}

static struct kobj_attribute dyn_fsync_active_attribute =
	__ATTR(Dyn_fsync_active, 0660,
		dyn_fsync_active_show,
		dyn_fsync_active_store);

static struct kobj_attribute dyn_fsync_version_attribute =
	__ATTR(Dyn_fsync_version, 0444, dyn_fsync_version_show, NULL);

static struct attribute *dyn_fsync_active_attrs[] =
	{
		&dyn_fsync_active_attribute.attr,
		&dyn_fsync_version_attribute.attr,
		NULL,
	};

static struct attribute_group dyn_fsync_active_attr_group =
	{
		.attrs = dyn_fsync_active_attrs,
	};

static struct kobject *dyn_fsync_kobj;

static void dyn_fsync_suspend(void)
{
	if (!mutex_trylock(&fsync_mutex))
		return;
	/* flush all outstanding buffers */
	if (dyn_fsync_active) {
#ifdef CONFIG_DYNAMIC_FSYNC_BG_SYNC
		if (bg_sync())
			pr_info("[dynamic_fsync_bg_sync] Syncing busy ...\n");
#else
		dyn_fsync_suspend_actions();
#endif
	}
	mutex_unlock(&fsync_mutex);

	pr_info("%s: flushing work finished.\n", __func__);
}

static int dyn_fsync_fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (!dyn_fsync_active)
		return 0;

	if (event == FB_EVENT_BLANK) {
		blank = evdata->data;

		switch (*blank) {
		case FB_BLANK_UNBLANK:
		case FB_BLANK_VSYNC_SUSPEND:
			dyn_sync_scr_suspended = false;
			break;
		default:
			dyn_sync_scr_suspended = true;
			dyn_fsync_suspend();
			break;
		}
	}

	return 0;
}

struct notifier_block dyn_fsync_fb_notif = {
	.notifier_call = dyn_fsync_fb_notifier_callback,
};

static int __init dyn_fsync_init(void)
{
	int ret;

	ret = fb_register_client(&dyn_fsync_fb_notif);
	if (ret) {
		pr_info("%s fb register failed!\n", __func__);
		return ret;
	}

#ifdef CONFIG_DYNAMIC_FSYNC_BG_SYNC
	suspend_sync_wq = create_singlethread_workqueue("suspend_sync");
	if (!suspend_sync_wq) {
		pr_info("%s suspend_sync_wq register failed!\n", __func__);
		return ret;
	}
#endif

	dyn_fsync_kobj = kobject_create_and_add("dyn_fsync", kernel_kobj);
	if (!dyn_fsync_kobj) {
		pr_err("%s dyn_fsync kobject create failed!\n", __func__);
		return -ENOMEM;
        }

	ret = sysfs_create_group(dyn_fsync_kobj,
			&dyn_fsync_active_attr_group);
	if (ret) {
		pr_info("%s dyn_fsync sysfs create failed!\n", __func__);
		kobject_put(dyn_fsync_kobj);
	}

	return ret;
}

static void __exit dyn_fsync_exit(void)
{
	if (dyn_fsync_kobj != NULL)
		kobject_put(dyn_fsync_kobj);
	fb_unregister_client(&dyn_fsync_fb_notif);
}

module_init(dyn_fsync_init);
module_exit(dyn_fsync_exit);

MODULE_AUTHOR("Paul Reioux <reioux@gmail.com>");
MODULE_DESCRIPTION("dynamic fsync - automatic fs sync optimizaition using"
		    "Power_suspend driver!");
MODULE_LICENSE("GPL v2");