#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <mpd/client.h>

#define isspace(c) ((((c) == ' ') || (((unsigned int)((c) - 9)) <= (13 - 9))))
#define SCAN(match, name) \
    p = grab_number(linebuf, match, sizeof(match)-1); \
    if (p) { name = p; continue; }

#define LONG_BUFF_SIZE  32
#define MID_BUFF_SIZE  24
#define SHORT_BUFF_SIZE 8

#define MPD_IP "127.0.0.1"
#define MPD_PORT 6600
struct cpu_info
{
    char name;
    unsigned int user;
    unsigned int nice;
    unsigned int system;
    unsigned int idle;
};

int g_cpu_used;
int cpu_num;
char mpc_buff[LONG_BUFF_SIZE] = "";
char ldavg_buff[LONG_BUFF_SIZE] = "";
char temp_buff[SHORT_BUFF_SIZE] = "";
char net_buff[LONG_BUFF_SIZE] = "";
char clk_buff[LONG_BUFF_SIZE] = "";
char bat_buff[SHORT_BUFF_SIZE] = "";

void mpc(void)
{
    FILE *fp;
    int i;

    fp = popen("mpc current" ,"r");
    fgets(mpc_buff , sizeof(mpc_buff) , fp);
    for (i = 0; i < LONG_BUFF_SIZE; i++) {
        if(mpc_buff[i] == '\n') {
            mpc_buff[i] = '\0';
            break;
        }
    }

    pclose(fp);
}

void cal_cpu_info(struct cpu_info *o, struct cpu_info *n)
{
    unsigned int od, nd;
    unsigned int id, sd;

    od = o->user + o->nice + o->system +o->idle;
    nd = n->user + n->nice + n->system +n->idle;
    id = n->user - o->user;
    sd = n->system - o->system;
    g_cpu_used = (((sd + id) * 100.0) / (nd - od));
}


void get_cpu_info(struct cpu_info *o)
{
    FILE *fd;
    int n;
    char buff[LONG_BUFF_SIZE * 4];

    fd = fopen("/proc/stat", "r");
    fgets (buff, sizeof(buff), fd);
    for(n=0;n<cpu_num;n++)
    {
        fgets (buff, sizeof(buff),fd);
        sscanf (buff, "%s %u %u %u %u", &o[n].name, &o[n].user, &o[n].nice,&o[n].system, &o[n].idle);
    }
    fclose(fd);
}

char *xstrdup(const char *s)
{
    char *t;

    if (s == NULL)
        return NULL;

    t = strdup(s);

    if (t == NULL)
        return NULL;

    return t;
}


char *skip_whitespace(const char *s)
{
    while (isspace(*s)) ++s;

    return (char *) s;
}


char *skip_non_whitespace(const char *s)
{
    while (*s && !isspace(*s)) ++s;

    return (char *) s;
}


static char *grab_number(char *str, const char *match, unsigned sz)
{
    if (strncmp(str, match, sz) == 0) {
        str = skip_whitespace(str + sz);
        (skip_non_whitespace(str))[1] = '\0';
        return xstrdup(str);
    }
    return NULL;
}

int cpu(void)
{
    struct cpu_info ocpu[2];
    struct cpu_info ncpu[2];
    int cpu_sum;
    int i;

    cpu_num = sysconf(_SC_NPROCESSORS_ONLN);
    get_cpu_info(ocpu);
    sleep(1);
    get_cpu_info(ncpu);

    cpu_sum = 0;
    for (i=0; i<cpu_num; i++)
    {
        cal_cpu_info(&ocpu[i], &ncpu[i]);
        cpu_sum += g_cpu_used;
    }
    cpu_sum /= cpu_num;

    return cpu_sum;
}

int men(void)
{
    FILE *fp;
    int i, used_men;
    int arr[4];
    char linebuf[LONG_BUFF_SIZE * 4];
    union {
        struct {
            char *total;
            char *mfree;
            char *buf;
            char *cache;
        } u;
        char *str[4];
    } z;

    fp = fopen("/proc/meminfo", "r");
    while (fgets(linebuf, sizeof(linebuf), fp))
    {
        char *p;

        SCAN("MemTotal:", z.u.total);
        SCAN("MemFree:", z.u.mfree);
        SCAN("Buffers:", z.u.buf);
        SCAN("Cached:", z.u.cache);
    }
    fclose(fp);

    for (i = 0; i < 4; i++)
    {
        arr[i] = atoi(z.str[i]);
    }

    used_men = (arr[0] >> 10) - (arr[1] >> 10) - (arr[2] >> 10) - (arr[3] >> 10);

    for (i = 0; i < 4; i++)
        free(z.str[i]);

    return used_men;
}

void loadavg(void)
{
    FILE *fp;
    int i, n;

    n = 0;
    fp = fopen("/proc/loadavg", "r");
    fgets(ldavg_buff, sizeof(ldavg_buff), fp);
    for(i = 0; i < 64; i++) {
        if(isspace(ldavg_buff[i]))
            n++;
        if(n == 3) {
            ldavg_buff[i] = '\0';
            break;
        }
        if(ldavg_buff[i] == '\n')
            ldavg_buff[i] = '\0';
    }
    fclose(fp);
}

void temp(void)
{
    FILE *fp;
    int i;

    fp = popen("acpi -t|cut -d\" \" -f4" ,"r");
    fgets(temp_buff , sizeof(temp_buff) , fp);

    for(i = 0; i < SHORT_BUFF_SIZE; i++) {
        if(temp_buff[i] == '.') {
            temp_buff[i] = '\0';
            break;
        }
    }

    pclose(fp);
}

void net(void)
{
    FILE *fp;
    int i, j, k;
    char *p;
    char buf[64];

    fp = popen("ifstat -i eth0,wlan0 -q 1 1" ,"r");
    fgets(buf , sizeof(buf) , fp);
    fgets(buf , sizeof(buf) , fp);
    fgets(buf , sizeof(buf) , fp);

    j = k = 0;
    p = buf;

    for(i = 0; i < LONG_BUFF_SIZE; i++) {
        if(!(*p)) {
            break;
        }

        if((*p <= '9') && (*p >= '0')) {
            net_buff[k] = *p;
            k++;
        } else if(*p == '.') {
            p += 2;
            switch(j) {
                case 0:
                case 2:
                    net_buff[k] = '/';
                    k++;
                    break;
                case 1:
                    net_buff[k] = ' ';
                    k++;
                    break;
                case 3:
                    net_buff[k] = '\0';
                    k++;
                    goto exitnet;
            }
            j++;
        }
        p++;
    }

exitnet:
    pclose(fp);
}

void clk(void)
{
    time_t curtime;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    strftime(clk_buff, LONG_BUFF_SIZE, "%a %Y-%m-%d %I:%M %P", localtime(&curtime));
}

int batt(void)
{
    FILE *fp;
    int j;
    char tmp_buff[128];
    char *p;

    fp = popen("acpi -b" ,"r");
    fgets(tmp_buff , sizeof(tmp_buff) , fp);

    p = tmp_buff;
    j = 0;

    while ((*p != '\0')) {
        if((*p == ',') && (*(p + 1) == ' ')) {
            p += 2;
            while(*p != '%') {
                bat_buff[j] = *p;
                j++;
                p++;
            }
            break;
        }
        p++;
    }
    bat_buff[j] = '\0';

    pclose(fp);

    if(strcmp(bat_buff, "100") == 0) {
        return 1;
    } else {
        return 0;
    }
}

int main(void)
{
    mpc();
    loadavg();
    temp();
    net();
    clk();
    int bat = batt();

    if (bat) {
        printf("音乐.%s | 处理器.%d%%  %s| 内存.%dM | 温度.%s°C | 网络.%s | %s\n", mpc_buff, cpu(), ldavg_buff, men(), temp_buff, net_buff, clk_buff);
    } else {
        printf("音乐.%s | 处理器.%d%%  %s| 内存.%dM | 温度.%s°C | 电池.%s%% | 网络.%s | %s\n", mpc_buff, cpu(), ldavg_buff, men(), temp_buff, bat_buff, net_buff, clk_buff);
    }

    return 0;
}

