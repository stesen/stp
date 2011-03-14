#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define isspace(c) ((((c) == ' ') || (((unsigned int)((c) - 9)) <= (13 - 9))))
#define SCAN(match, name) \
    p = grab_number(linebuf, match, sizeof(match)-1); \
    if (p) { name = p; continue; }

#define LONG_BUFF_SIZE  32
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
char mpd_buff[LONG_BUFF_SIZE] = "";
char ldavg_buff[LONG_BUFF_SIZE] = "";
char net_buff[LONG_BUFF_SIZE] = "";
char clk_buff[LONG_BUFF_SIZE] = "";

void mpd(void)
{
    int sock_fd;
    char sock_buff[256];
    char stat_buf[8];
    struct sockaddr_in sin;
    char *p, *ptr;
    int i;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(sock_fd < 0) {
        printf("socket err\n");
        exit(1);
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons(MPD_PORT);
    sin.sin_addr.s_addr = inet_addr(MPD_IP);

    if(connect(sock_fd, (const struct sockaddr *)&sin, sizeof(sin)) == -1) {
        strcpy(mpd_buff, "connect err\n");
        return;
    }
    recv(sock_fd, sock_buff, sizeof(sock_buff), 0);

    send(sock_fd, "status\n", 7, 0);
    recv(sock_fd, sock_buff, sizeof(sock_buff), 0);

    p = strstr(sock_buff, "state");
    if(p == NULL) {
        strcpy(mpd_buff, "read status err\n");
        return;
    }
    p += sizeof("state: ") - 1;
    i = 0;
    while(*p != '\n') {
        stat_buf[i] = *p;
        p++;
        i++;
    }
    stat_buf[i] = '\0';
    if(strcmp(stat_buf, "play") == 0) {
        send(sock_fd, "currentsong\n", 12, 0);
        recv(sock_fd, sock_buff, sizeof(sock_buff), 0);
        p = strstr(sock_buff, "file");
        ptr = strrchr(p, '/');
        if(ptr == NULL) {
            strcpy(mpd_buff, "read file err\n");
            return;
        }

        ptr++;
        i = 0;
        while(*ptr != '\n') {
            mpd_buff[i] = *ptr;
            ptr++;
            i++;
        }
        mpd_buff[i] = '\0';
    } else if ((strcmp(stat_buf, "pause") == 0) || (strcmp(stat_buf, "stop") == 0)) {
        strcpy(mpd_buff, stat_buf);
    }
}

void cal_cpu_info(struct cpu_info *o, struct cpu_info *n)
{
    unsigned int od, nd;
    unsigned int id, sd;

    od = o->user + o->nice + o->system +o->idle;
    nd = n->user + n->nice + n->system +n->idle;
    id = n->user - o->user;
    sd = n->system - o->system;
    g_cpu_used = (((sd + id) * 99.0) / (nd - od));
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

    bzero(&z, sizeof(z));

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

int temp(void)
{
    FILE *fp;
    int n;
    char tmp_buff[SHORT_BUFF_SIZE];

    fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    fgets(tmp_buff, sizeof(tmp_buff), fp);
    n = atoi(tmp_buff);

    fclose(fp);
    return (n / 1000);
}

void net(void)
{
    FILE *fp;
    int i;
    char *p;
    char buf[256];
    unsigned int recved1[2], sent1[2];
    unsigned int recved2[2], sent2[2];

    fp = fopen("/proc/net/dev" ,"r");
    fgets(buf, sizeof(buf), fp);
    fgets(buf, sizeof(buf), fp);

    while(fgets(buf , sizeof(buf) , fp)) {
        p = buf;
        p = skip_whitespace(p);
        if((strncmp(p, "eth", 3)) == 0) {
            p = skip_non_whitespace(p);
            p = skip_whitespace(p);

            sscanf(p, "%u", &recved1[0]);

            for(i = 0; i < 8; i++) {
                p = skip_whitespace(p);
                p = skip_non_whitespace(p);
            }
            p = skip_whitespace(p);

            sscanf(p, "%u", &sent1[0]);
        } else if ((strncmp(p, "wlan", 4)) == 0) {
            p = skip_non_whitespace(p);
            p = skip_whitespace(p);

            sscanf(p, "%u", &recved1[1]);

            for(i = 0; i < 8; i++) {
                p = skip_whitespace(p);
                p = skip_non_whitespace(p);
            }
            p = skip_whitespace(p);

            sscanf(p, "%u", &sent1[1]);
        }
    }
    sleep(1);
    fseek(fp, 0, SEEK_SET);
    fgets(buf, sizeof(buf), fp);
    fgets(buf, sizeof(buf), fp);

    while(fgets(buf , sizeof(buf) , fp)) {
        p = buf;
        p = skip_whitespace(p);
        if((strncmp(p, "eth", 3)) == 0) {
            p = skip_non_whitespace(p);
            p = skip_whitespace(p);

            sscanf(p, "%u", &recved2[0]);

            for(i = 0; i < 8; i++) {
                p = skip_whitespace(p);
                p = skip_non_whitespace(p);
            }
            p = skip_whitespace(p);

            sscanf(p, "%u", &sent2[0]);
        } else if ((strncmp(p, "wlan", 4)) == 0) {
            p = skip_non_whitespace(p);
            p = skip_whitespace(p);

            sscanf(p, "%u", &recved2[1]);

            for(i = 0; i < 8; i++) {
                p = skip_whitespace(p);
                p = skip_non_whitespace(p);
            }
            p = skip_whitespace(p);
            sscanf(p, "%u", &sent2[1]);
        }
    }

    fclose(fp);
    sprintf(net_buff, "%u/%uK %u/%uK",
            ((recved2[0] - recved1[0]) >> 10),
            ((sent2[0] - sent1[0]) >> 10),
            ((recved2[1] - recved1[1]) >> 10),
            ((sent2[1] - sent1[1])) >> 10);
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
    FILE *fpi, *fps;
    char tmp_buf1[LONG_BUFF_SIZE * 2];
    char tmp_buf2[LONG_BUFF_SIZE * 2];
    int rem, total;

    fps = fopen("/proc/acpi/battery/BAT0/state", "r");
    if(fps == NULL) {
        fclose(fps);
        return 0;
    }
    fgets(tmp_buf1, sizeof(tmp_buf1), fps);
    fgets(tmp_buf1, sizeof(tmp_buf1), fps);
    fgets(tmp_buf1, sizeof(tmp_buf1), fps);

    if(strcmp(tmp_buf1, "charging state:          charged\n") == 0){
        fclose(fps);
        return 0;
    }

    fpi = fopen("/proc/acpi/battery/BAT0/info", "r");
    if(fpi == NULL) {
        fclose(fps);
        fclose(fpi);
        return 0;
    }

    fgets(tmp_buf1, sizeof(tmp_buf1), fps);
    fgets(tmp_buf1, sizeof(tmp_buf1), fps);

    fgets(tmp_buf2, sizeof(tmp_buf2), fpi);
    fgets(tmp_buf2, sizeof(tmp_buf2), fpi);
    fgets(tmp_buf2, sizeof(tmp_buf2), fpi);


    sscanf(tmp_buf1, "remaining capacity:      %d mAh", &rem);
    sscanf(tmp_buf2, "last full capacity:      %d mAh", &total);

    fclose(fps);
    fclose(fpi);
    return ((rem * 100) / total);
}

int main(void)
{
    mpd();
    loadavg();
    net();
    clk();
    int bat = batt();

    if (bat == 0) {
        printf("音乐.%s | 处理器.%d%%  %s| 内存.%dM | 温度.%d°C | 网络.%s | %s\n", mpd_buff, cpu(), ldavg_buff, men(), temp(), net_buff, clk_buff);
    } else {
        printf("音乐.%s | 处理器.%d%%  %s| 内存.%dM | 温度.%d°C | 电池.%d%% | 网络.%s | %s\n", mpd_buff, cpu(), ldavg_buff, men(), temp(), bat, net_buff, clk_buff);
    }

    return 0;
}

