#!/usr/bin/perl

my $batt = &battery;
my $statline;

&check_x;

if ($batt eq "100%") {
    $statline = sprintf("音乐.%s | 处理器.%2d%%  %s | 内存.%4dM | 温度.%3d°C | 网络.%s | 音量.%s | %s", &musicinfo, &cpu_calc, &load_avg, &mem_calc, &temp_read, &net_calc, &volume, &timeinfo);
} else {
    $statline = sprintf("音乐.%s | 处理器.%2d%%  %s | 内存.%4dM | 温度.%3d°C | 网络.%s | 音量.%s | 电池.%s | %s", &musicinfo, &cpu_calc, &load_avg, &mem_calc, &temp_read, &net_calc, $batt, &volume, &timeinfo);
}
print $statline;
1;

sub check_x {
    my $x = `pidof X`;
    unless ($x =~ /\d/) {
        print "NO X\n";
        exit;
    }
}

sub cpu_read {
    my $cpu_file = '/proc/stat';
    my ($cpu_name, $cpu_usr, $cpu_nice, $cpu_system, $cpu_idle, $cpu_lines);
    my @ret;

    open F, "$cpu_file" or die "Can't open $cpu_file for read.\n";
    $cpu_lines = <F>;
    chomp ($cpu_lines);
    ($cpu_name, $cpu_usr, $cpu_nice, $cpu_system, $cpu_idle) = split / +/, $cpu_lines;
    ($cpu_usr + $cpu_system), ($cpu_usr + $cpu_nice + $cpu_system + $cpu_idle);
}

sub cpu_calc {
    my @cpu_info1 = &cpu_read;
    sleep 1;
    my @cpu_info2 = &cpu_read;

    my $cpu_useage = (($cpu_info2[0] - $cpu_info1[0]) * 100 / ($cpu_info2[1] - $cpu_info1[1]));
}

sub load_avg {
    my $loadstr = `uptime`;
    chomp ($loadstr);
    $loadstr =~ s/.*load average: //;
    $loadstr;
}

sub mem_calc {
    my $mem_file = '/proc/meminfo';
    my ($mem_total, $mem_free, $mem_buf, $mem_cache);

    open F, "$mem_file" or die "Can't open $mem_file for read.\n";
    $mem_total = <F>;
    chomp($mem_total);
    $mem_total =~ s/MemTotal: +(\d+).*/$1/;
    $mem_free = <F>;
    chomp($mem_free);
    $mem_free =~ s/MemFree: +(\d+).*/$1/;
    $mem_buf = <F>;
    chomp($mem_buf);
    $mem_buf =~ s/Buffers: +(\d+).*/$1/;
    $mem_cache = <F>;
    chomp($mem_cache);
    $mem_cache =~ s/Cached: +(\d+).*/$1/;

    ($mem_total - $mem_free - $mem_buf - $mem_cache) / 1024
}

sub net_calc {
    my @a = `ifstat -i eth0,wlan0 -q 1 1`;
    #my @a = `ifstat -q 1 1`;
    $a[0] =~ m/(\w+) *(\w+)/;
    my $ifname1 = $1;
    my $ifname2 = $2;

    return unless($ifname1);
    
    if($ifname2) {
        chomp($a[2]);
        $a[2] =~ s/^ +(\d+)\.\d+ +(\d+).\d+ +(\d+).\d+ +(\d+).\d+/$1\/$2kb $3\/$4kb/;
    } else {
        chomp($a[2]);
        chomp($a[0]);
        $a[0] =~ s/ //g;
        $a[2] =~ s/^ +(\d+)\.\d+ +(\d+).\d+/$a[0]: $1\/$2kb/;
    }

    #if ($a[0] =~ /wlan0/) {
    #    $a[2] .= " " . &wireless_read;
    #}

    $a[2];
}

sub temp_read {
    my @acpit = `acpi -t`;
    $acpit[0] =~ s/.*(\d\d+\.\d) degrees C.*/$1/;
    chomp ($acpit[0]);
    $acpit[0];
}

sub battery {
    my @bat = split / +/, `acpi -b`;
    $bat[3] .= $bat[4] if ($bat[4]);
    chomp($bat[3]);

    $bat[3];
}

sub volume {
    my @vol = `amixer get Master`;
    $vol[5] =~ s/.*\[(\d+\%)\].*/$1/;
    chomp($vol[5]);
    $vol[5];
}

sub timeinfo {
    my ($sec,$min,$hour,$day,$mon,$year,$weekday,$yeardate,$savinglightday, $ret, @cn_weekday);
    ($sec,$min,$hour,$day,$mon,$year,$weekday,$yeardate,$savinglightday) = (localtime(time));
    $sec = ($sec < 10) ? "0$sec" : $sec;
    $min = ($min < 10) ? "0$min" : $min;
    $hour = ($hour < 10) ? "0$hour" : $hour;
    $day  = ($day < 10) ? "0$day" : $day;
    $mon = ($mon < 9) ? "0".($mon+1) : ($mon+1);
    $year += 1900;
    @cn_weekday = qw(周日 周一 周二 周三 周四 周五 周六);
    $ret = "$cn_weekday[$weekday] $year-$mon-$day  $hour:$min "; 
    $ret;
}

sub musicinfo {
#    my $name = `mocp -i|head -2|tail -1`;
#    chomp($name);
#    $name =~ s/\b.*\///g;
#    $name =~ s/(\.mp3)|(\.wma)|(\.ogg)//g;
#    $name =~ s/\b(.{0,25}).*\b/$1/;
#    $name;

    my $name = `mpc current`;
    chomp($name);
    $name =~ s/.*\///;
    $name =~ s/\b(.{0,35}).*\b/$1/;
    $name;
}
