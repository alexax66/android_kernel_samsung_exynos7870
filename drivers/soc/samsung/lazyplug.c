/*
 * Author: Park Ju Hyung aka arter97 <qkrwngud825@gmail.com>
 * Base intelli_plug author: Paul Reioux aka Faux123 <reioux@gmail.com>
 *
 * Copyright 2012~2014 Paul Reioux
 * Copyright 2015 Park Ju Hyung
 * Copyright 2017 Stenkin Evgeniy
 * Copyright 2017-2018 Joe Maples
 * Copyright 2019-2020 Alexander Alexeev
 *
 *
 ** Introduction
 *
 * Other hotplugging methods including mpdecision and intelli_plug focuses
 * on how should we turn off CPU cores. They hotplugs the individual CPU
 * cores based on the current load divided by thread capacity.
 * Lazyplug takes a whole new approach on how we should do hotplugging
 * based on the foundation of the other side of the coin;
 * “Linux’s hotplugging is very inefficient.”
 *
 * Current hotplugging code on Linux is a total waste of CPU cycles and
 * delays, so rather than hotplugging and hurt performance & battery life,
 * just leaving the CPU cores on might be a better choice. This kind of
 * approach is spreading out more and more.
 * Samsung has been using this method for a very long time with big.LITTLE
 * devices and recent Nexus 6 firmware also does the similar thing.
 *
 * Lazyplug just leaves them on, most of the time. It also tries to solve
 * some problems with the “Always on” approach. On situations such as video
 * playback, turning on all CPU cores is not battery friendly. So Lazyplug
 * *does* actually turns off CPU cores, but only when idle state is long
 * enough(to reduce the number of CPU core switchings) and when the device
 * has its screen off (determination is done via framebuffer API).
 *
 * Basic methodology :
 * Lazyplug uses majority of the codes from intelli_plug by faux123 to
 * determine when to turn off CPU cores. If the system has been idle for
 * (DEF_SAMPLING_MS * DEF_IDLE_COUNT)ms, it turns off the CPU cores. And if
 * the next poll determines 1 core isn’t enough, it fires up all CPU cores
 * (instead of selective CPU cores; which is the traditional intelli_plug’s
 * method).
 * There’s also a “lazy mode” for *not* aggressively turning on CPU cores
 * on scenario such as video playback. For example, if you hook up
 * lazyplug_enter_lazy() to the video session open function, Lazyplug won’t
 * aggressively turn on CPU cores and tries to handle it with 1 CPU core.
 *
 ** TODO :
 ** Dual-core mode : YouTube video playback is mostly single-threaded.
 ** It usually hovers around 10% ~ 30% of total CPU usage on quad-core
 ** device. That means 1 CPU core might not be enough to handle it, but
 ** also turning on all CPU cores is unnecessarily wasting power.
 *
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

#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/fb.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>

//#define DEBUG_LAZYPLUG
#undef DEBUG_LAZYPLUG

#define LAZYPLUG_MAJOR_VERSION	2
#define LAZYPLUG_MINOR_VERSION	1

#define DEF_SAMPLING_MS		(132)
#define DEF_IDLE_COUNT		(19) /* 132 * 19 = 2508, almost equals to 2.5 seconds */

#define BUSY_PERSISTENCE	(3500 / DEF_SAMPLING_MS)

static DEFINE_MUTEX(lazyplug_mutex);
static DEFINE_MUTEX(lazymode_mutex);

static struct delayed_work lazyplug_work;
static struct delayed_work lazyplug_cac;

static struct workqueue_struct *lazyplug_wq;
static struct workqueue_struct *lazyplug_cac_wq;

static unsigned int __read_mostly lazyplug_active = 1;
module_param(lazyplug_active, uint, 0664);

static unsigned int __read_mostly nr_run_profile_sel = 0;
module_param(nr_run_profile_sel, uint, 0664);

/* default to something sane rather than zero */
static unsigned int __read_mostly sampling_time = DEF_SAMPLING_MS;

static int persist_count = 0;

static bool __read_mostly suspended = false;
static bool __read_mostly cac_bool = true;
static bool __read_mostly lazymode = false;

struct ip_cpu_info {
	unsigned int sys_max;
	unsigned int cur_max;
	unsigned long cpu_nr_running;
};

static DEFINE_PER_CPU(struct ip_cpu_info, ip_info);

#define CAPACITY_RESERVE	50
#define THREAD_CAPACITY		(520 - CAPACITY_RESERVE)
#define MULT_FACTOR		4
#define DIV_FACTOR		100000
#define NR_FSHIFT		3

static unsigned int nr_fshift = NR_FSHIFT;

static unsigned int __read_mostly nr_run_thresholds_balance[] = {
	(THREAD_CAPACITY * 625 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 875 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 1125 * MULT_FACTOR) / DIV_FACTOR,
	UINT_MAX
};

static unsigned int __read_mostly nr_run_thresholds_performance[] = {
	(THREAD_CAPACITY * 380 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 625 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 875 * MULT_FACTOR) / DIV_FACTOR,
	UINT_MAX
};

static unsigned int __read_mostly nr_run_thresholds_conservative[] = {
	(THREAD_CAPACITY * 875 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 1625 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 2125 * MULT_FACTOR) / DIV_FACTOR,
	UINT_MAX
};

static unsigned int __read_mostly nr_run_thresholds_eco[] = {
        (THREAD_CAPACITY * 380 * MULT_FACTOR) / DIV_FACTOR,
	UINT_MAX
};

static unsigned int __read_mostly nr_run_thresholds_eco_extreme[] = {
        (THREAD_CAPACITY * 750 * MULT_FACTOR) / DIV_FACTOR,
	UINT_MAX
};

static unsigned int __read_mostly nr_run_thresholds_disable[] = {
	0,  0,  0,  UINT_MAX
};

static unsigned int __read_mostly nr_run_thresholds_lazy[] = {
	(THREAD_CAPACITY * 995 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 1875 * MULT_FACTOR) / DIV_FACTOR,
	(THREAD_CAPACITY * 2350 * MULT_FACTOR) / DIV_FACTOR,
	UINT_MAX
};

static unsigned int __read_mostly *nr_run_profiles[] = {
	nr_run_thresholds_balance,
	nr_run_thresholds_performance,
	nr_run_thresholds_conservative,
	nr_run_thresholds_eco,
	nr_run_thresholds_eco_extreme,
	nr_run_thresholds_disable,
	nr_run_thresholds_lazy,
};

#define NR_RUN_ECO_MODE_PROFILE	3
#define NR_RUN_HYSTERESIS_OCTA	16
#define NR_RUN_HYSTERESIS_HEXA	12
#define NR_RUN_HYSTERESIS_QUAD	8
#define NR_RUN_HYSTERESIS_DUAL	4

#define CPU_NR_THRESHOLD	((THREAD_CAPACITY << 1) + (THREAD_CAPACITY / 2))

static unsigned int __read_mostly nr_possible_cores = NR_CPUS;
module_param(nr_possible_cores, uint, 0660);

static unsigned int __read_mostly cpu_nr_run_threshold = CPU_NR_THRESHOLD;
module_param(cpu_nr_run_threshold, uint, 0664);

static unsigned int __read_mostly nr_run_hysteresis = NR_RUN_HYSTERESIS_OCTA;
module_param(nr_run_hysteresis, uint, 0664);

#ifdef DEBUG_LAZYPLUG
/* those 2 counters will malfunction if uptime exceeds 36.4 years */
static unsigned int offline_state_count = 0;	/* Total time in all cores(except for CPU0) off, divided by DEF_SAMPLING_MS */
module_param(offline_state_count, uint, 0444);
static unsigned int online_state_count = 0;	/* Total time in all cores on, divided by DEF_SAMPLING_MS */
module_param(online_state_count, uint, 0444);
static unsigned int switch_count = 0;		/* Counts of switches between those two states, less is better */
module_param(switch_count, uint, 0444);
static bool previous_online_status = true;	/* Internal boolean to determine previous status */
#endif

static unsigned int nr_run_last;

static unsigned int idle_count = 0;

extern unsigned long Lavg_nr_running(void);
extern unsigned long Lavg_cpu_nr_running(unsigned int cpu);

static void __ref cpu_all_ctrl(bool online) {
	unsigned int cpu;

	if (online) {
		/* start from the smaller ones */
		if (lazymode) {
			/* Mess around with the first cluster only */
			for(cpu = 1; cpu <= 3; cpu++) {
				cpu_up(cpu);
			}
		} else {
			for(cpu = 1; cpu <= nr_cpu_ids - 1; cpu++) {
				cpu_up(cpu);
			}
		}
	} else {
		/* kill from the bigger ones */
		for(cpu = nr_cpu_ids - 1; cpu >= 2; cpu--) {
			cpu_down(cpu);
		}
	}
}

static unsigned int calculate_thread_stats(void)
{
	unsigned int avg_nr_run = Lavg_nr_running();
	unsigned int nr_run;
	unsigned int threshold_size;
	unsigned int *current_profile;

	current_profile = nr_run_profiles[nr_run_profile_sel];

	if (nr_run_profile_sel >= NR_RUN_ECO_MODE_PROFILE)
		threshold_size =
			ARRAY_SIZE(nr_run_thresholds_eco);
	else
		threshold_size =
			ARRAY_SIZE(nr_run_thresholds_balance);

	if (nr_run_profile_sel >= NR_RUN_ECO_MODE_PROFILE)
		nr_fshift = 1;
	else
		nr_fshift = 3;

	for (nr_run = 1; nr_run < threshold_size; nr_run++) {
		unsigned int nr_threshold;
		nr_threshold = current_profile[nr_run - 1];

		if (nr_run_last <= nr_run)
			nr_threshold += nr_run_hysteresis;
		if (avg_nr_run <= (nr_threshold << (FSHIFT - nr_fshift)))
			break;
	}
	nr_run_last = nr_run;

	return nr_run;
}

static void lazyplug_cac_fn(struct work_struct *work)
{
	cpu_all_ctrl(cac_bool);
}

static void update_per_cpu_stat(void)
{
	unsigned int cpu;
	struct ip_cpu_info *l_ip_info;

	for_each_online_cpu(cpu) {
		l_ip_info = &per_cpu(ip_info, cpu);
		l_ip_info->cpu_nr_running = Lavg_cpu_nr_running(cpu);
#ifdef DEBUG_LAZYPLUG
		pr_info("cpu %u nr_running => %lu\n", cpu,
			l_ip_info->cpu_nr_running);
#endif
	}
}

static void unplug_cpu(int min_active_cpu)
{
	unsigned int cpu;
	struct ip_cpu_info *l_ip_info;
	int l_nr_threshold;

	for(cpu = nr_cpu_ids - 1; cpu >= 1; cpu--) {
		if (!cpu_online(cpu))
			continue;

		l_nr_threshold =
			cpu_nr_run_threshold << 1 / (num_online_cpus());

		l_ip_info = &per_cpu(ip_info, cpu);
		if (cpu > min_active_cpu)
			if (l_ip_info->cpu_nr_running < l_nr_threshold)
				cpu_down(cpu);
	}
}

static void set_cpus(void)
{
	unsigned int cpu;

	for(cpu = nr_cpu_ids-1; cpu >= 1; cpu--) {
		if (!cpu_online(cpu))
			continue;

		if (cpu >= nr_possible_cores)
			cpu_down(cpu);
	}
}

static void lazyplug_work_fn(struct work_struct *work)
{
	unsigned int nr_run_stat;
	unsigned int cpu_count = 0;
	unsigned int nr_cpus = 0;
	unsigned int old_nr = NR_CPUS;

	if (lazyplug_active) {
		nr_run_stat = calculate_thread_stats();
		update_per_cpu_stat();

#ifdef DEBUG_LAZYPLUG
		pr_info("nr_run_stat: %u\n", nr_run_stat);
#endif
		cpu_count = nr_run_stat;
		nr_cpus = num_online_cpus();
		if (old_nr > nr_possible_cores) {
			set_cpus();
		}

		if (!suspended) {
			if (persist_count > 0)
				persist_count--;

			if (cpu_count == 1) {
				/* start counting idle states */
				if (idle_count < DEF_IDLE_COUNT)
					idle_count++;

				if (idle_count == DEF_IDLE_COUNT && persist_count == 0) {
					/* take down all cpu except CPU0 and CPU1 */
					cpu_down(7);
					cpu_down(6);
					cpu_down(5);
					cpu_down(4);
					cpu_down(3);
					cpu_down(2);
#ifdef DEBUG_LAZYPLUG
					offline_state_count++;
					if (previous_online_status == true) {
						previous_online_status = false;
						switch_count++;
					}
				} else {
					online_state_count++;
					if (previous_online_status == false) {
						previous_online_status = true;
						switch_count++;
					}
#endif
				}
			} else {
				idle_count = 0;
				cpu_all_ctrl(true);
#ifdef DEBUG_LAZYPLUG
				online_state_count++;
				if (previous_online_status == false) {
					previous_online_status = true;
					switch_count++;
				}
#endif
			}
		}
#ifdef DEBUG_LAZYPLUG
		else
			pr_info("lazyplug is suspended!\n");
#endif
	}
	old_nr = nr_possible_cores;
	queue_delayed_work(lazyplug_wq, &lazyplug_work,
		msecs_to_jiffies(sampling_time));
}

static void cpu_all_up(struct work_struct *work);
static DECLARE_WORK(cpu_all_up_work, cpu_all_up);

static void cpu_all_up(struct work_struct *work)
{
	cpu_all_ctrl(true);
}

static void lazyplug_suspend(void)
{
	if (lazyplug_active) {
#ifdef DEBUG_LAZYPLUG
		pr_info("lazyplug: screen-off, turn off cores\n");
#endif
		flush_workqueue(lazyplug_wq);

		mutex_lock(&lazyplug_mutex);
		suspended = true;
		mutex_unlock(&lazyplug_mutex);

		/* put rest of the cores to sleep unconditionally! */
		cac_bool = false;
		queue_delayed_work(lazyplug_wq, &lazyplug_cac,
			msecs_to_jiffies(0));
	}
}

static void lazyplug_resume(void)
{
	if (lazyplug_active) {
#ifdef DEBUG_LAZYPLUG
		pr_info("lazyplug: screen-on, turn on cores\n");
#endif
		mutex_lock(&lazyplug_mutex);
		/* keep cores awake long enough for faster wake up */
		persist_count = BUSY_PERSISTENCE;
		suspended = false;
		mutex_unlock(&lazyplug_mutex);

		schedule_work(&cpu_all_up_work);
		cac_bool = true;
		queue_delayed_work(lazyplug_wq, &lazyplug_cac,
			msecs_to_jiffies(10));
	}
	queue_delayed_work(lazyplug_wq, &lazyplug_work,
		msecs_to_jiffies(0));
}

static unsigned int Lnr_run_profile_sel = 0;
void lazyplug_enter_lazy(bool enter)
{
	mutex_lock(&lazymode_mutex);
	if (enter && !lazymode) {
#ifdef DEBUG_LAZYPLUG
		pr_info("lazyplug: entering lazy mode\n");
#endif
		Lnr_run_profile_sel = nr_run_profile_sel;
		nr_run_profile_sel = 6; /* lazy profile */
		lazymode = true;

		/* take down all cpu except for CPU0 and CPU1 */
		cpu_down_nocheck(7);
		cpu_down_nocheck(6);
		cpu_down_nocheck(5);
		cpu_down_nocheck(4);
		cpu_down_nocheck(3);
		cpu_down_nocheck(2);
	} else if (!enter && lazymode) {
#ifdef DEBUG_LAZYPLUG
		pr_info("lazyplug: exiting lazy mode\n");
#endif
		nr_run_profile_sel = Lnr_run_profile_sel;
		lazymode = false;

		cac_bool = true;
		queue_delayed_work(lazyplug_wq, &lazyplug_cac,
			msecs_to_jiffies(10));
	}
	mutex_unlock(&lazymode_mutex);
}

static int fb_state_change(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank = evdata->data;

	if (event == FB_EVENT_BLANK) {
		switch (*blank) {
			case FB_BLANK_POWERDOWN:
				lazyplug_suspend();
				break;
			case FB_BLANK_UNBLANK:
				idle_count = 0;
				lazyplug_resume();
				break;
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block fb_block = {
		.notifier_call = fb_state_change,
};

int __init lazyplug_init(void)
{
	int ret;

	pr_info("lazyplug: version %d.%d by arter97\n"
		"          based on intelli_plug by faux123\n",
		 LAZYPLUG_MAJOR_VERSION,
		 LAZYPLUG_MINOR_VERSION);

	if (nr_possible_cores > 6) {
		nr_run_hysteresis = NR_RUN_HYSTERESIS_OCTA;
		nr_run_profile_sel = 0;
	} else if (nr_possible_cores > 4) {
		nr_run_hysteresis = NR_RUN_HYSTERESIS_HEXA;
		nr_run_profile_sel = 0;
	} else if (nr_possible_cores > 2) {
		nr_run_hysteresis = NR_RUN_HYSTERESIS_QUAD;
		nr_run_profile_sel = 0;
	} else {
		nr_run_hysteresis = NR_RUN_HYSTERESIS_DUAL;
		nr_run_profile_sel = NR_RUN_ECO_MODE_PROFILE;
	}

	ret = fb_register_client(&fb_block);
	if (ret) {
		pr_err("Failed to register fb notifier\n");
	}

	lazyplug_wq = alloc_workqueue("lazyplug", WQ_FREEZABLE, 1);
	lazyplug_cac_wq = alloc_workqueue("lplug_cac", WQ_FREEZABLE, 1);
	INIT_DELAYED_WORK(&lazyplug_work, lazyplug_work_fn);
	INIT_DELAYED_WORK(&lazyplug_cac, lazyplug_cac_fn);

	queue_delayed_work(lazyplug_wq, &lazyplug_work,
		msecs_to_jiffies(10));

	return 0;
}

late_initcall(lazyplug_init);

MODULE_AUTHOR("Park Ju Hyung <qkrwngud825@gmail.com>");
MODULE_DESCRIPTION("The conservative hotplugging, lazyplug by arter97 based on intelli_plug.");
MODULE_LICENSE("GPL v2");
