/*
 * nuvoton_procfs.c
 *
 * (c) ? Gustav Gans
 * (c) 2015 Audioniek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *
 * procfs for Fortis front panel driver.
 *
 *
 ****************************************************************************
 *
 * Changes
 *
 * Date     By              Description
 * --------------------------------------------------------------------------
 * 20160523 Audioniek       Initial version based on tffpprocfs.c.
 * 
 ****************************************************************************/

#include <linux/proc_fs.h>      /* proc fs */
#include <asm/uaccess.h>        /* copy_from_user */
#include <linux/time.h>
#include <linux/kernel.h>
#include "nuvoton.h"

/*
 *  /proc/stb/fp
 *             |
 *             +--- version (r)             SW version of front processor (hundreds = major, ten/units = minor)
 *             +--- rtc (rw)                RTC time (UTC, seconds since Unix epoch))
 *             +--- rtc_offset (rw)         RTC offset in seconds from UTC
 *             +--- wakeup_time (rw)        Next wakeup time (absolute, local, seconds since Unix epoch)
 *             +--- was_timer_wakeup (r)    Wakeup reason (1 = timer, 0 = other)
 *             +--- led0_pattern (rw)       Blink pattern for LED 1 (currently limited to on (0xffffffff) or off (0))
 *             +--- led1_pattern (rw)       Blink pattern for LED 2 (currently limited to on (0xffffffff) or off (0))
 *             +--- led_patternspeed (rw)   Blink speed for pattern (not implemented)
 *             +--- oled_brightness (w)     Direct control of display brightness (VFD models only)
 *             +--- text (w)                Direct writing of display text
 */

/* from e2procfs */
extern int install_e2_procs(char *name, read_proc_t *read_proc, write_proc_t *write_proc, void *data);
extern int remove_e2_procs(char *name, read_proc_t *read_proc, write_proc_t *write_proc);

/* from other nuvoton modules */
extern int nuvotonSetIcon(int which, int on);
extern void VFD_set_all_icons(int onoff);
extern int nuvotonWriteString(char *buf, size_t len);
#if !defined(HS7110)
extern int nuvotonSetBrightness(int level);
#endif
extern void clear_display(void);
extern int nuvotonGetTime(char *time);
extern int nuvotonGetWakeUpMode(int *wakeup_mode);
extern int nuvotonGetVersion(int *version);
extern int nuvotonSetTime(char *time);
extern int nuvotonSetLED(int which, int level);
extern int nuvotonGetWakeUpTime(char *time);
extern int nuvotonSetWakeUpTime(char *time);

/* Globals */
static int rtc_offset = 3600;
//static int progress = 0;
//static int progress_done = 0;
static u32 led0_pattern = 0;
static u32 led1_pattern = 0;
static int led_pattern_speed = 20;

static int text_write(struct file *file, const char __user *buf, unsigned long count, void *data)
{
	char *page;
	int ret = -ENOMEM;

	page = (char *)__get_free_page(GFP_KERNEL);

	if (page)
	{
		ret = -EFAULT;
		if (copy_from_user(page, buf, count) == 0)
		{
			ret = nuvotonWriteString(page, count);
			if (ret >= 0)
			{
				ret = count;
			}
		}
		free_page((unsigned long)page);
	}
	return ret;
}

/* Time subroutines */
/*
struct tm {
int	tm_sec      //seconds after the minute 0-61*
int	tm_min      //minutes after the hour   0-59
int	tm_hour     //hours since midnight     0-23
int	tm_mday     //day of the month         1-31
int	tm_mon      //months since January     0-11
int	tm_year     //years since 1900
int	tm_wday     //days since Sunday        0-6
int	tm_yday   	//days since January 1     0-365
}

time_t long seconds //UTC since epoch  
*/

time_t calcGetNuvotonTime(char *time)
{ //mjd hh:mm:ss -> seconds since epoch
	unsigned int    mjd     = ((time[0] & 0xff) * 256) + (time[1] & 0xff);
	unsigned long   epoch   = ((mjd - 40587) * 86400);
	unsigned int    hour    = time[2] & 0xff;
	unsigned int    min     = time[3] & 0xff;
	unsigned int    sec     = time[4] & 0xff;

	epoch += (hour * 3600 + min * 60 + sec);
	return epoch;
}

#define LEAPYEAR(year) (!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define YEARSIZE(year) (LEAPYEAR(year) ? 366 : 365)
static const int _ytab[2][12] =
{
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static struct tm * gmtime(register const time_t time)
{ //seconds since epoch -> struct tm
	static struct tm fptime;
	register unsigned long dayclock, dayno;
	int year = 70;

	dprintk(5, "%s < Time to convert: %d seconds\n", __func__, (int)time);
	dayclock = (unsigned long)time % 86400;
	dayno = (unsigned long)time / 86400;

	fptime.tm_sec = dayclock % 60;
	fptime.tm_min = (dayclock % 3600) / 60;
	fptime.tm_hour = dayclock / 3600;
	fptime.tm_wday = (dayno + 4) % 7;       /* day 0 was a thursday */
	while (dayno >= YEARSIZE(year))
	{
		dayno -= YEARSIZE(year);
		year++;
	}
	fptime.tm_year = year;
	fptime.tm_yday = dayno;
	fptime.tm_mon = 0;
	while (dayno >= _ytab[LEAPYEAR(year)][fptime.tm_mon])
	{
		dayno -= _ytab[LEAPYEAR(year)][fptime.tm_mon];
		fptime.tm_mon++;
	}
	fptime.tm_mday = dayno;
//	fptime.tm_isdst = -1;

	return &fptime;
}

int getMJD(struct tm *theTime)
{ //struct tm (date) -> mjd
	int mjd = 0;
	int year;

	year  = theTime->tm_year - 1; // do not count current year
	while (year >= 70)
	{
		mjd += 365;
		if (LEAPYEAR(year))
		{
			mjd++;
		}
		year--;
	}
	mjd += theTime->tm_yday;
	mjd += 40587;  // mjd starts on midnight 17-11-1858 which is 40587 days before unix epoch

	return mjd;
}

void calcSetNuvotonTime(time_t theTime, char *destString)
{ //seconds since epoch -> mjd h:m:s
	struct tm *now_tm;
	int mjd;

	now_tm = gmtime(theTime);
	mjd = getMJD(now_tm);
	destString[0] = (mjd >> 8);
	destString[1] = (mjd & 0xff);
	destString[2] = now_tm->tm_hour;
	destString[3] = now_tm->tm_min;
	destString[4] = now_tm->tm_sec;
}

static int read_rtc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	int res;
	long rtc_time;
	char fptime[5];

	res = nuvotonGetTime(fptime);
	rtc_time = calcGetNuvotonTime(fptime);

	if (NULL != page)
	{
//		len = sprintf(page,"%u\n", (int)(rtc_time - rtc_offset));
		len = sprintf(page,"%u\n", (int)(rtc_time)); //Fortis FP is in UTC
	}
	return len;
}

static int write_rtc(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *page = NULL;
	ssize_t ret = -ENOMEM;
	time_t argument = 0;
	int test = -1;
	char *myString = kmalloc(count + 1, GFP_KERNEL);
	char time[5];

	page = (char *)__get_free_page(GFP_KERNEL);

	if (page)
	{
		ret = -EFAULT;

		if (copy_from_user(page, buffer, count))
		{
			goto out;
		}
		strncpy(myString, page, count);
		myString[count] = '\0';

		test = sscanf (myString, "%u", (unsigned int *)&argument);

		if (test > 0)
		{
//			calcSetNuvotonTime((argument + rtc_offset), time); //set time as u32
			calcSetNuvotonTime((argument), time); //set time as u32 (Fortis FP is in UTC)
			ret = nuvotonSetTime(time);
		}
		/* always return count to avoid endless loop */
		ret = count;
	}
out:
	free_page((unsigned long)page);
	kfree(myString);
	dprintk(10, "%s <\n", __func__);
	return ret;
}

static int read_rtc_offset(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;

	if (NULL != page)
	{
		len = sprintf(page, "%d\n", rtc_offset);
	}
	return len;
}

static int write_rtc_offset(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *page = NULL;
	ssize_t ret = -ENOMEM;
	int test = -1;
	char *myString = kmalloc(count + 1, GFP_KERNEL);

	page = (char *)__get_free_page(GFP_KERNEL);

	if (page)
	{
		ret = -EFAULT;
		if (copy_from_user(page, buffer, count))
		{
			goto out;
		}
		strncpy(myString, page, count);
		myString[count] = '\0';

		test = sscanf (myString, "%d", &rtc_offset);

		/* always return count to avoid endless loop */
		ret = count;
	}
out:
	free_page((unsigned long)page);
	kfree(myString);
	dprintk(10, "%s <\n", __func__);
	return ret;
}

static int wakeup_time_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char *page = NULL;
	ssize_t ret = -ENOMEM;
	time_t wakeup_time = 0;
	int test = -1;
	char *myString = kmalloc(count + 1, GFP_KERNEL);
	char wtime[5];

	page = (char *)__get_free_page(GFP_KERNEL);

	if (page)
	{
		ret = -EFAULT;

		if (copy_from_user(page, buffer, count))
		{
			goto out;
		}
		strncpy(myString, page, count);
		myString[count] = '\0';
		dprintk(5, "%s > %s\n", __func__, myString);

		test = sscanf(myString, "%u", (unsigned int *)&wakeup_time);

		if (0 < test)
		{
//			calcSetNuvotonTime((wakeup_time + rtc_offset), wtime); //set time as u32
			calcSetNuvotonTime((wakeup_time), wtime); //set time as u32 (Fortis FP is in UTC)
			ret = nuvotonSetWakeUpTime(wtime);
		}
		/* always return count to avoid endless loop */
		ret = count;
	}
out:
	free_page((unsigned long)page);
	kfree(myString);
	dprintk(10, "%s <\n", __func__);
	return ret;
}

static int wakeup_time_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	char wtime[5];
	int	w_time;

	if (NULL != page)
	{
		
		nuvotonGetWakeUpTime(wtime);
		w_time = calcGetNuvotonTime(wtime);

//		len = sprintf(page, "%u\n", w_time - rtc_offset);
		len = sprintf(page, "%u\n", w_time); //Fortis FP uses UTC
	}
	return len;
}

static int was_timer_wakeup_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	int res = 0;
	int wakeup_mode = -1;

	if (NULL != page)
	{
		res = nuvotonGetWakeUpMode(&wakeup_mode);

		if (res == 0)
		{
			dprintk(5, "%s > wakeup_mode= %d\n", __func__, wakeup_mode);
			if (wakeup_mode == 3) // if timer wakeup
			{
				wakeup_mode = 1;
			}
			else
			{
				wakeup_mode = 0;
			}
		}
		else
		{
				wakeup_mode = -1;
		}
		len = sprintf(page,"%d\n", wakeup_mode);
	}
	return len;
}

static int fp_version_read(char *page, char **start, off_t off, int count, int *eof, void *data_unused)
{
	int len = 0;
	int version;

	version = -1;
	if (nuvotonGetVersion(&version) == 0)
	{
		len = sprintf(page, "%d\n", version);
	}
	else
	{
		return -EFAULT;
	}
	return len;
}

static int led_pattern_write(struct file *file, const char __user *buf, unsigned long count, void *data, int which)
{
	char *page;
	long pattern;
	int ret = -ENOMEM;

	page = (char *)__get_free_page(GFP_KERNEL);

	if (page)
	{
		ret = -EFAULT;

		if (copy_from_user(page, buf, count) == 0)
		{
			page[count - 1] = '\0';
			pattern = simple_strtol(page, NULL, 16);

			if (which == 1)
			{
				led1_pattern = pattern;
			}
			else
			{
				led0_pattern = pattern;
			}

//TODO: Not implemented completely; only the cases 0 and 0xffffffff (ever off/on) are handled
//Other patterns are blink patterned in time, so handling those should be done in a timer

			if (pattern == 0)
			{
				nuvotonSetLED(which + 1, 0);
			}
			else if (pattern == 0xffffffff)
			{
#if defined(FORTIS_HDBOX) || defined(ATEVIO7500)
				nuvotonSetLED(which + 1, 31);
#else
				nuvotonSetLED(which+ 1 , 7);
#endif
			}
			ret = count;
		}
		free_page((unsigned long)page);
	}
	return ret;
}

static int led0_pattern_read(char *page, char **start, off_t off, int count, int *eof, void *data_unused)
{
	int len = 0;

	if (NULL != page)
	{
		len = sprintf(page, "%08x\n", led0_pattern);
	}
	return len;
}

static int led0_pattern_write(struct file *file, const char __user *buf, unsigned long count, void *data)
{
	return led_pattern_write(file, buf, count, data, 0);
}

static int led1_pattern_read(char *page, char **start, off_t off, int count, int *eof, void *data_unused)
{
	int len = 0;

	if (NULL != page)
	{
		len = sprintf(page, "%08x\n", led1_pattern);
	}
	return len;
}
static int led1_pattern_write(struct file *file, const char __user *buf, unsigned long count, void *data)
{
	return led_pattern_write(file, buf, count, data, 1);
}

static int led_pattern_speed_write(struct file *file, const char __user *buf, unsigned long count, void *data)
{
	char *page;
	int ret = -ENOMEM;

	page = (char *)__get_free_page(GFP_KERNEL);

	if (page)
	{
		ret = -EFAULT;

		if (copy_from_user(page, buf, count) == 0)
		{
			page[count - 1] = '\0';
			led_pattern_speed = (int)simple_strtol(page, NULL, 10);

			ret = count;
		}
		free_page((unsigned long)page);
	}
	return ret;
}

static int led_pattern_speed_read(char *page, char **start, off_t off, int count, int *eof, void *data_unused)
{
	int len = 0;

	if (NULL != page)
	{
		len = sprintf(page, "%d\n", led_pattern_speed);
	}
	return len;
}

#if !defined(HS7110)
static int oled_brightness_write(struct file *file, const char __user *buf, unsigned long count, void *data)
{
	char *page;
	long level;
	int ret = -ENOMEM;

	page = (char *)__get_free_page(GFP_KERNEL);

	if (page)
	{
		ret = -EFAULT;

		if (copy_from_user(page, buf, count) == 0)
		{
			page[count - 1] = '\0';

			level = simple_strtol(page, NULL, 10);
			nuvotonSetBrightness((int)level);
			ret = count;
		}
		free_page((unsigned long)page);
	}
	return ret;
}
#endif

/*
static int null_write(struct file *file, const char __user *buf, unsigned long count, void *data)
{
	return count;
}
*/

struct fp_procs
{
	char *name;
	read_proc_t *read_proc;
	write_proc_t *write_proc;
} fp_procs[] =
{
	{ "stb/fp/rtc", read_rtc, write_rtc },
	{ "stb/fp/rtc_offset", read_rtc_offset, write_rtc_offset },
	{ "stb/fp/led0_pattern", led0_pattern_read, led0_pattern_write },
	{ "stb/fp/led1_pattern", led1_pattern_read, led1_pattern_write },
	{ "stb/fp/led_pattern_speed", led_pattern_speed_read, led_pattern_speed_write },
#if !defined(HS7110)
	{ "stb/fp/oled_brightness", NULL, oled_brightness_write },
#else
	{ "stb/fp/oled_brightness", NULL, NULL },
#endif
	{ "stb/fp/text", NULL, text_write },
	{ "stb/fp/wakeup_time", wakeup_time_read, wakeup_time_write },
	{ "stb/fp/was_timer_wakeup", was_timer_wakeup_read, NULL },
	{ "stb/fp/version", fp_version_read, NULL },
};

void create_proc_fp(void)
{
	int i;

	for (i = 0; i < sizeof(fp_procs)/sizeof(fp_procs[0]); i++)
	{
		install_e2_procs(fp_procs[i].name, fp_procs[i].read_proc, fp_procs[i].write_proc, NULL);
	}
}

void remove_proc_fp(void)
{
	int i;

	for (i = sizeof(fp_procs)/sizeof(fp_procs[0]) - 1; i >= 0; i--)
	{
		remove_e2_procs(fp_procs[i].name, fp_procs[i].read_proc, fp_procs[i].write_proc);
	}
}
// vim:ts=4
