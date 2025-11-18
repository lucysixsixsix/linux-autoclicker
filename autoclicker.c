#define _GNU_SOURCE
#include <linux/uinput.h>
#include <linux/input.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <time.h>

#define die(str, args...) do { perror(str); exit(EXIT_FAILURE); } while(0)

bool toggled=false;
bool mouse_down=false;
long lastClick=0;

const int hold_min=30;
const int hold_max=50;
const int delay_min=70;
const int delay_max=100;

const int toggle_key=KEY_GRAVE;

int uinput_fd=-1;

void emit(int fd,int type,int code,int val) {
    struct input_event ie;
    ie.type=type;
    ie.code=code;
    ie.value=val;
    gettimeofday(&ie.time,NULL);
    if(write(fd,&ie,sizeof(ie))<0) perror("write uinput");
}

long getCurrentTimeMs(void) {
    struct timeval tv;
    gettimeofday(&tv,NULL);

    return tv.tv_sec*1000L+tv.tv_usec/1000L;
}

int RandomIntRange(int min,int max) {
    if(max<=min) return min;
    return(rand()%(max-min+1))+min;
}

int main(void) {
    if(geteuid()!=0) die("must run as root");

    srand((unsigned int)time(NULL));
    printf("meow");

    uinput_fd=open("/dev/uinput",O_WRONLY | O_NONBLOCK);
    if(uinput_fd<0) die("open uinput");

    ioctl(uinput_fd,UI_SET_EVBIT,EV_KEY);
    ioctl(uinput_fd,UI_SET_KEYBIT,BTN_LEFT);
    ioctl(uinput_fd,UI_SET_EVBIT,EV_SYN);

    struct uinput_setup usetup={0};
    snprintf(usetup.name,UINPUT_MAX_NAME_SIZE,"AUTOCLICKER");
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor  = 0x1234;
    usetup.id.product = 0x5678;
    usetup.id.version = 1;

    ioctl(uinput_fd,UI_DEV_SETUP,&usetup);
    ioctl(uinput_fd,UI_DEV_CREATE,0);
    sleep(1);

    int kb_fd=open("/dev/input/event17",O_RDONLY | O_NONBLOCK);
    int mouse_fd=open("/dev/input/event4",O_RDONLY | O_NONBLOCK);

    if(kb_fd<0||mouse_fd<0) die("open input devices");

    struct pollfd fds[2]={
        { .fd=kb_fd, .events=POLLIN },
        { .fd=mouse_fd, .events=POLLIN }
    };

    struct input_event ev;

    while(1) {
        if(poll(fds,2,10)<0) die("poll");

        if(fds[0].revents&POLLIN) {
            while(read(kb_fd,&ev,sizeof(ev))==sizeof(ev)) {
                if(ev.type==EV_KEY&&ev.code==toggle_key&&ev.value==1) {
                    printf("\033[H\033[J");
                    printf("═══════════════════════════════════════════════\n");
                    printf("                  Autoclicker                     \n");
                    printf("═══════════════════════════════════════════════\n\n");
                    printf("Shortcuts:\n\n");
                    printf("Grave:      Toggle Auto Clicker\n\n");
                    printf("CTRL-C:     Exit Program\n");
                    printf("───────────────────────────────────────────────\n");
                    printf("Status: %s\n",(toggled?"ON":"OFF"));
                    printf("═══════════════════════════════════════════════\n");
                    printf("\033[H\033[J");
                    toggled=!toggled;
                }
            }
        }

        if(fds[1].revents&POLLIN) {
            while(read(mouse_fd,&ev,sizeof(ev))==sizeof(ev)) {
                if(ev.type==EV_KEY&&ev.code==BTN_LEFT) mouse_down=(ev.value!=0);
            }
        }

        if(toggled&&mouse_down) {
            long now=getCurrentTimeMs();
            int delay=RandomIntRange(delay_min,delay_max);

            if(now-lastClick>=delay) {
                emit(uinput_fd,EV_KEY,BTN_LEFT,1);
                emit(uinput_fd,EV_SYN,SYN_REPORT,0);

                // hold
                int hold = RandomIntRange(hold_min,hold_max);
                usleep(hold*1000);

                // release
                emit(uinput_fd,EV_KEY,BTN_LEFT,0);
                emit(uinput_fd,EV_SYN,SYN_REPORT,0);

                lastClick = now;
            }
        }

        usleep(2000);
    }

    ioctl(uinput_fd,UI_DEV_DESTROY);
    close(uinput_fd);

    return 0;
}
