#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

char *tzargentina = "America/Buenos_Aires";
char *tzutc = "UTC";
char *tzberlin = "Europe/Berlin";

static Display *dpy;

char *smprintf(char *fmt, ...)
{
        va_list fmtargs;
        char *ret;
        int len;

        va_start(fmtargs, fmt);
        len = vsnprintf(NULL, 0, fmt, fmtargs);
        va_end(fmtargs);

        ret = malloc(++len);
        if (ret == NULL) {
                perror("Error: Memory allocation failed in smprintf");
                exit(1);
        }

        va_start(fmtargs, fmt);
        vsnprintf(ret, len, fmt, fmtargs);
        va_end(fmtargs);

        return ret;
}

void settz(char *tzname)
{
        setenv("TZ", tzname, 1);
}

char *mk_times(char *fmt, char *tzname)
{
        char buf[129];
        time_t tim;
        struct tm *timtm;

        settz(tzname);
        tim = time(NULL);
        timtm = localtime(&tim);
        if (timtm == NULL)
                return smprintf("");

        if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
                fprintf(stderr, "strftime == 0\n");
                return smprintf("");
        }

        return smprintf("%s", buf);
}

void set_status(char *str)
{
        XStoreName(dpy, DefaultRootWindow(dpy), str);
        XSync(dpy, False);
}

char *load_avg(void)
{
        double avgs[3];

        if (getloadavg(avgs, 3) < 0)
                return smprintf("");

        return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char *readfile(char *base, char *file)
{
        char *path, line[513];
        FILE *fd;

        memset(line, 0, sizeof(line));

        path = smprintf("%s/%s", base, file);
        fd = fopen(path, "r");
        free(path);
        if (fd == NULL)
        return NULL;

        if (fgets(line, sizeof(line)-1, fd) == NULL) {
                fclose(fd);
                return NULL;
        }
        fclose(fd);

        return smprintf("%s", line);
}

char *get_battery(char *base)
{
        char *co;
        char status;

        int descap;
        int remcap;

        descap = -1;
        remcap = -1;

        co = readfile(base, "present");
        if (co == NULL)
                return smprintf("");
        if (co[0] != '1') {
                free(co);
                return smprintf("not present");
        }
free(co);

        co = readfile(base, "charge_full_design");
        if (co == NULL) {
                co = readfile(base, "energy_full_design");
        if (co == NULL)
                return smprintf("");
        }
        sscanf(co, "%d", &descap);
free(co);

        co = readfile(base, "charge_now");
        if (co == NULL) {
                co = readfile(base, "energy_now");
        if (co == NULL)
                return smprintf("");
        }
        sscanf(co, "%d", &remcap);
free(co);

        co = readfile(base, "status");
        if (!strncmp(co, "Discharging", 11)) {
                status = '-';
        } else if(!strncmp(co, "Charging", 8)) {
                status = '+';
} else {
                status = '.'; //default is ? but we use . to reduce clutter 
        }
        free(co);

        if (remcap < 0 || descap < 0)
                return smprintf("invalid");

        if( (float)remcap / (float)descap * 100 >= 99) {
                return smprintf("%s", "charged", status);
        }
        return smprintf("%.0f%%%c", ((float)remcap / (float)descap) * 100, status);
}

char *get_temperature(char *base, char *sensor)
{
        char *co;

        co = readfile(base, sensor);
        if (co == NULL) {
                free(co); 
                return smprintf("");
        }
        return smprintf("%02.0f°C", atof(co) / 1000);//co freed as free(get_temperature)
}

char *exec_script(char *cmd)
{
        FILE *fp;
        char retval[1025], *rv;

        memset(retval, 0, sizeof(retval));

        fp = popen(cmd, "r");
        if (fp == NULL)
                return smprintf("");

        rv = fgets(retval, sizeof(retval), fp);
        pclose(fp);
        if (rv == NULL)
                return smprintf("");
        retval[strlen(retval)-1] = '\0';

        return smprintf("%s", retval);
}
void try_display_open()
{
        if (!(dpy = XOpenDisplay(NULL))) {
                fprintf(stderr, "dwmstatus: cannot open display.\n");
                exit(1);
        }
}
int main(void)
{
        char *status;
        char *avgs;
        char *bat;
        char *tmar;
        char *tmutc;
        char *tmbln;
        char *t0;
        char *t1;

        try_display_open();
        for(;;sleep(1)) {
                avgs  = load_avg();
                bat   = get_battery("/sys/class/power_supply/BAT0");
                tmar  = mk_times("%H:%M", tzargentina);
                tmutc = mk_times("%H:%M", tzutc);
                tmbln = mk_times("KW %W %a %d %b %H:%M %Z %Y", tzberlin);
                t0    = get_temperature("/sys/devices/virtual/thermal/thermal_zone3", "temp"); //Might want to change thermal_zone
                t1    = get_temperature("/sys/devices/virtual/thermal/thermal_zone4", "temp"); //on different motherboard configs
                /*format ("someString %typedef %orJustTypeDef", args) use %s if unsure
                                *number of %typedef must equal to amount of args
                                */
                status = smprintf("T:%s|%s L:%s %s A:%s U:%s %s",
                                  t0, t1, avgs, bat, tmar, tmutc,
                tmbln);
                set_status(status);










                //free stuff
                free(t0);
                free(t1);
                free(avgs);
                free(bat);
                free(tmar);
                free(tmutc);
                free(tmbln);
                free(status);
                sleep(30);
        }

        XCloseDisplay(dpy);

        return 0;
}

