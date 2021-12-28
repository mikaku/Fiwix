/*
 * fiwix/kernel/timer.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/segments.h>
#include <fiwix/cmos.h>
#include <fiwix/pit.h>
#include <fiwix/timer.h>
#include <fiwix/time.h>
#include <fiwix/irq.h>
#include <fiwix/sched.h>
#include <fiwix/pic.h>
#include <fiwix/cmos.h>
#include <fiwix/signal.h>
#include <fiwix/process.h>
#include <fiwix/sleep.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

/*
 * timer.c implements a callout table using a singly linked list.
 *
 *  head
 * +---------+  ----------+  ...  ----------+
 * |data|next|  |data|next|  ...  |data|next|
 * |    |  -->  |    |  -->  ...  |    |  / |
 * +---------+  ----------+  ...  ----------+
 *  (callout)    (callout)         (callout)
 */

struct callout callout_pool[NR_CALLOUTS];
struct callout *callout_pool_head;
struct callout *callout_head;

static char month[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
unsigned int avenrun[3] = { 0, 0, 0 };

static struct bh timer_bh = { 0, &irq_timer_bh, NULL };
static struct bh callouts_bh = { 0, &do_callouts_bh, NULL };
static struct interrupt irq_config_timer = { 0, "timer", &irq_timer, NULL };

static unsigned int count_active_procs(void)
{
	int counter;
	struct proc *p;

	counter = 0;
	FOR_EACH_PROCESS_RUNNING(p) {
		counter += FIXED_1;
		p = p->next_run;
	}
	return counter;
}

static void calc_load(void)
{
	unsigned int active_procs;
	static int count = LOAD_FREQ;

	if(count-- > 0) {
		return;
	}

	count = LOAD_FREQ;
	active_procs = count_active_procs();
	CALC_LOAD(avenrun[0], EXP_1, active_procs);
	CALC_LOAD(avenrun[1], EXP_5, active_procs);
	CALC_LOAD(avenrun[2], EXP_15, active_procs);
}

static struct callout *get_free_callout(void)
{
	struct callout *new;

	new = NULL;
	if(callout_pool_head) {
		new = callout_pool_head;
		callout_pool_head = callout_pool_head->next;
		new->next = NULL;
	}
	return new;
}

static void put_free_callout(struct callout *old)
{
	old->next = callout_pool_head;
	callout_pool_head = old;
}

static void do_del_callout(struct callout *c)
{
	struct callout **tmp;

	if(callout_head) {
		tmp = &callout_head;
		while(*tmp) {
			if((*tmp) == c) {
				if((*tmp)->next != NULL) {
					*tmp = (*tmp)->next;
					(*tmp)->expires += c->expires;
				} else {
					*tmp = NULL;
				}
				put_free_callout(c);
				break;
			}
			tmp = &(*tmp)->next;
		}
	}
}

void add_callout(struct callout_req *creq, unsigned int ticks)
{
	unsigned long int flags;
	struct callout *c, **tmp;

	del_callout(creq);
	SAVE_FLAGS(flags); CLI();

	if(!(c = get_free_callout())) {
		printk("WARNING: %s(): no more callout slots!\n", __FUNCTION__);
		RESTORE_FLAGS(flags);
		return;
	}

	/* setup the new callout */
	memset_b(c, NULL, sizeof(struct callout));
	c->expires = ticks;
	c->fn = creq->fn;
	c->arg = creq->arg;

	if(!callout_head) {
		callout_head = c;
	} else {
		tmp = &callout_head;
		while(*tmp) {
			if((*tmp)->expires > c->expires) {
				(*tmp)->expires -= c->expires;
				c->next = *tmp;
				break;
			}
			c->expires -= (*tmp)->expires;
			tmp = &(*tmp)->next;
		}
		*tmp = c;
	}
	RESTORE_FLAGS(flags);
}

void del_callout(struct callout_req *creq)
{
	unsigned long int flags;
	struct callout *c;

	SAVE_FLAGS(flags); CLI();
	c = callout_head;
	while(c) {
		if(c->fn == creq->fn && c->arg == creq->arg) {
			do_del_callout(c);
			break;
		}
		c = c->next;
	}
	RESTORE_FLAGS(flags);
}

void irq_timer(int num, struct sigcontext *sc)
{
	if((++kstat.ticks % HZ) == 0) {
		CURRENT_TIME++;
		kstat.uptime++;
	}

	timer_bh.flags |= BH_ACTIVE;

	if(sc->cs == KERNEL_CS) {
		current->usage.ru_stime.tv_usec += TICK;
		if(current->usage.ru_stime.tv_usec >= 1000000) {
			current->usage.ru_stime.tv_sec++;
			current->usage.ru_stime.tv_usec -= 1000000;
		}
		if(current->pid != IDLE) {
			kstat.cpu_system++;
		}
	} else {
		current->usage.ru_utime.tv_usec += TICK;
		if(current->usage.ru_utime.tv_usec >= 1000000) {
			current->usage.ru_utime.tv_sec++;
			current->usage.ru_utime.tv_usec -= 1000000;
		}
		if(current->pid != IDLE) {
			kstat.cpu_user++;
		}
		if(current->it_virt_value > 0) {
			current->it_virt_value--;
			if(!current->it_virt_value) {
				current->it_virt_value = current->it_virt_interval;
				send_sig(current, SIGVTALRM);
			}
		}
	}
}

unsigned long int tv2ticks(const struct timeval *tv)
{
	return((tv->tv_sec * HZ) + tv->tv_usec * HZ / 1000000);
}

void ticks2tv(long int ticks, struct timeval *tv)
{
	tv->tv_sec = ticks / HZ;
	tv->tv_usec = (ticks % HZ) * 1000000 / HZ;
}

int setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value)
{
	switch(which) {
		case ITIMER_REAL:
			if((unsigned int)old_value) {
				ticks2tv(current->it_real_interval, &old_value->it_interval);
				ticks2tv(current->it_real_value, &old_value->it_value);
			}
			current->it_real_interval = tv2ticks(&new_value->it_interval);
			current->it_real_value = tv2ticks(&new_value->it_value);
			break;
		case ITIMER_VIRTUAL:
			if((unsigned int)old_value) {
				ticks2tv(current->it_virt_interval, &old_value->it_interval);
				ticks2tv(current->it_virt_value, &old_value->it_value);
			}
			current->it_virt_interval = tv2ticks(&new_value->it_interval);
			current->it_virt_value = tv2ticks(&new_value->it_value);
			break;
		case ITIMER_PROF:
			if((unsigned int)old_value) {
				ticks2tv(current->it_prof_interval, &old_value->it_interval);
				ticks2tv(current->it_prof_value, &old_value->it_value);
			}
			current->it_prof_interval = tv2ticks(&new_value->it_interval);
			current->it_prof_value = tv2ticks(&new_value->it_value);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

unsigned long int mktime(struct mt *mt)
{
	int n, total_days;
	unsigned long int seconds;

	total_days = 0;

	for(n = UNIX_EPOCH; n < mt->mt_year; n++) {
		total_days += DAYS_PER_YEAR(n);
	}
	for(n = 0; n < (mt->mt_month - 1); n++) {
		total_days += month[n];
		if(n == 1) {
			total_days += LEAP_YEAR(mt->mt_year) ? 1 : 0;
		}
	}

	total_days += (mt->mt_day - 1);
	seconds = total_days * SECS_PER_DAY;
	seconds += mt->mt_hour * SECS_PER_HOUR;
	seconds += mt->mt_min * SECS_PER_MIN;
	seconds += mt->mt_sec;
	return seconds;
}

void irq_timer_bh(void)
{
	struct proc *p;

	if(current->usage.ru_utime.tv_sec + current->usage.ru_stime.tv_sec > current->rlim[RLIMIT_CPU].rlim_cur) {
		send_sig(current, SIGXCPU);
	}

	if(current->it_prof_value > 0) {
		current->it_prof_value--;
		if(!current->it_prof_value) {
			current->it_prof_value = current->it_prof_interval;
			send_sig(current, SIGPROF);
		}
	}

	calc_load();
	FOR_EACH_PROCESS(p) {
		if(p->timeout > 0 && p->timeout < INFINITE_WAIT) {
			p->timeout--;
			if(!p->timeout) {
				wakeup_proc(p);
			}
		}
		if(p->it_real_value > 0) {
			p->it_real_value--;
			if(!p->it_real_value) {
				p->it_real_value = p->it_real_interval;
				send_sig(p, SIGALRM);
			}
		}
		p = p->next;
	}

	/* callouts */
	if(callout_head) {
		if(callout_head->expires > 0) {
			callout_head->expires--;
			if(!callout_head->expires) {
				callouts_bh.flags |= BH_ACTIVE;
			}
		} else {
			printk("%s(): callout losing ticks.\n", __FUNCTION__);
			callouts_bh.flags |= BH_ACTIVE;
		}
	}

	if(current->pid > IDLE && --current->cpu_count <= 0) {
		current->cpu_count = 0;
		need_resched = 1;
	}
}

void do_callouts_bh(void)
{
	struct callout *c;
	void (*fn)(unsigned int);
	unsigned int arg;

	while(callout_head) {
		if(callout_head->expires) {
			break;
		}
		if(lock_area(AREA_CALLOUT)) {
			continue;
		}
		fn = callout_head->fn;
		arg = callout_head->arg;
		c = callout_head;
		callout_head = callout_head->next;
		put_free_callout(c);
		unlock_area(AREA_CALLOUT);
		fn(arg);
	}
}

void get_system_time(void)
{
	short int cmos_century;
	struct mt mt;
		  
	/* read date and time from CMOS */
	mt.mt_sec = cmos_read_date(CMOS_SEC);
	mt.mt_min = cmos_read_date(CMOS_MIN);
	mt.mt_hour = cmos_read_date(CMOS_HOUR);
	mt.mt_day = cmos_read_date(CMOS_DAY);
	mt.mt_month = cmos_read_date(CMOS_MONTH);
	mt.mt_year = cmos_read_date(CMOS_YEAR);
	cmos_century = cmos_read_date(CMOS_CENTURY);
	mt.mt_year += cmos_century * 100;

	kstat.boot_time = CURRENT_TIME = mktime(&mt);
}

void set_system_time(__time_t t)
{
	int sec, spm, min, hour, d, m, y;

	sec = t;
	y = 1970;
	while(sec >= (DAYS_PER_YEAR(y) * SECS_PER_DAY)) {
		sec -= (DAYS_PER_YEAR(y) * SECS_PER_DAY);
		y++;
	}

	m = 0;
	while(sec > month[m] * SECS_PER_DAY) {
		spm = month[m] * SECS_PER_DAY;
		if(m == 1) {
			spm = LEAP_YEAR(y) ? spm + SECS_PER_DAY : spm;
		}
		sec -= spm;
		m++;
	}
	m++;

	d = 1;
	while(sec >= SECS_PER_DAY) {
		sec -= SECS_PER_DAY;
		d++;
	}

	hour = 0;
	while(sec >= SECS_PER_HOUR) {
		sec -= SECS_PER_HOUR;
		hour++;
	}

	min = 0;
	while(sec >= SECS_PER_MIN) {
		sec -= SECS_PER_MIN;
		min++;
	}

	/* write date and time to CMOS */
	cmos_write_date(CMOS_SEC, sec);
	cmos_write_date(CMOS_MIN, min);
	cmos_write_date(CMOS_HOUR, hour);
	cmos_write_date(CMOS_DAY, d);
	cmos_write_date(CMOS_MONTH, m);
	cmos_write_date(CMOS_YEAR, y % 100);
	cmos_write_date(CMOS_CENTURY, (y - (y % 100)) / 100);

	CURRENT_TIME = t;
}

void timer_init(void)
{
	int n;
	struct callout *c;

	add_bh(&timer_bh);
	add_bh(&callouts_bh);

	pit_init(HZ);

	memset_b(callout_pool, NULL, sizeof(callout_pool));

	/* callout free list initialization */
	callout_pool_head = NULL;
	n = NR_CALLOUTS;
	while(n--) {
		c = &callout_pool[n];
		put_free_callout(c);
	}
	callout_head = NULL;

	printk("clock     -                %d    type=PIT Hz=%d\n", TIMER_IRQ, HZ);
	if(!register_irq(TIMER_IRQ, &irq_config_timer)) {
		enable_irq(TIMER_IRQ);
	}
}
