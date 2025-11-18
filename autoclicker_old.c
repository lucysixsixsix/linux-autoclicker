#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/input.h>
#include <linux/uinput.h>

// variables
static volatile bool mouseDown=false;
static volatile bool toggled=false;
static volatile bool firstClick=false;
static volatile long lastClick=0;
static volatile bool programRunning=true;
static volatile bool consoleVisible=true;
static int uinput_fd=-1;

// utilities
long getCurrentTimeMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);

    return ts.tv_sec*1000L+ts.tv_nsec/1000000L;
}

int RandomIntRange(int min,int max) {
    if(max<=min) return min;
    return(rand()%(max-min+1))+min;
}

// uinput setup
int initUinput(void) {
    int fd=open("/dev/uinput",O_WRONLY | O_NONBLOCK);

    if(fd<0) {
        perror("open(/dev/uinput)");
        return -1;
    }

    // enable key/button events
    if(ioctl(fd,UI_SET_EVBIT,EV_KEY)<0) { perror("UI_SET_EVBIT EV_KEY"); close(fd); return -1; }
    if(ioctl(fd,UI_SET_KEYBIT,BTN_LEFT)<0) { perror("UI_SET_KEYBIT BTN_LEFT"); close(fd); return -1; }

    // enable sync events
    if(ioctl(fd,UI_SET_EVBIT,EV_SYN) < 0) { perror("UI_SET_EVBIT EV_SYN"); close(fd); return -1; }

    struct uinput_setup usetup={0};
    snprintf(usetup.name,sizeof(usetup.name),"AUTOCLICKER");
    //memset(&usetup,0,sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor  = 0x1234;
    usetup.id.product = 0x5678;
    usetup.id.version = 1;

    if(ioctl(fd,UI_DEV_SETUP,&usetup)<0) { perror("UI_DEV_SETUP"); close(fd); return -1; }
    if(ioctl(fd,UI_DEV_CREATE)<0) { perror("UI_DEV_CREATE"); close(fd); return -1; }

    // give kernel time to create the device node
    usleep(500*1000);
    return fd;
}

void destroyUinput(int fd) {
    if(fd>=0) {
        ioctl(fd,UI_DEV_DESTROY);
        close(fd);
    }
}

ssize_t write_event(int fd,int type,int code,int value) {
    struct input_event ev;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    ev.time=tv;
    ev.type=type;
    ev.code=code;
    ev.value=value;
    return write(fd,&ev,sizeof(ev));
}

void simulateMouseClickUinput(int fd,bool press) {
    if(fd<0) return;

    if(write_event(fd,EV_KEY,BTN_LEFT,press?1:0)<0) {
        perror("write_event BTN_LEFT");
    }

    if(write_event(fd,EV_SYN,SYN_REPORT,0)<0) {
        perror("write_event SYN_REPORT");
    }
}

// input device detection (find a device that reports BTN_LEFT)
static bool device_has_button(int fd,int button_code) {
    unsigned long bits[EV_MAX/(sizeof(unsigned long)*8)+1];
    memset(bits,0,sizeof(bits));

    if(ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(bits)),bits)<0) {
        return false;
    }

    int idx = button_code/(8*sizeof(unsigned long));
    int bit = button_code%(8*sizeof(unsigned long));

    return(bits[idx]&(1UL<<bit))!=0;
}

int find_mouse_event_device(void) {
    char path[64];
    for(int i=0;i<64;++i) {
        snprintf(path,sizeof(path),"/dev/input/event4",i);
        int fd=open(path,O_RDONLY | O_NONBLOCK);
        if(fd<0) continue;

        // check if device exposes BTN_LEFT
        if(device_has_button(fd,BTN_LEFT)) {
            // keep the fd open in caller or return index; we return fd
            return fd; // caller must not close this fd(we'll return fd)
        }

        close(fd);
    }
    return -1;
}

// mouse monitor thread
void *MouseMonitorThread(void *arg) {
(void)arg;
    int mouse_fd=find_mouse_event_device();
    if(mouse_fd<0) {
        fprintf(stderr,"Warning: could not find mouse event device(/dev/input/event*). Mouse monitoring disabled.\n");
        return NULL;
    }

    struct input_event ev;
    while(programRunning) {
        ssize_t n=read(mouse_fd,&ev,sizeof(ev));
        if(n==(ssize_t)sizeof(ev)) {
            if(ev.type==EV_KEY&&ev.code==BTN_LEFT) {
                if(ev.value==1) { // press
                    mouseDown=true;
                    firstClick=true;
                } else if(ev.value==0) { // release
                    mouseDown=false;
                }
            }
        } else {
            // no event, sleep a tiny bit to avoid busy loop
            usleep(2000);
        }
    }

    close(mouse_fd);
    return NULL;
}

// terminal raw mode for keyboard
static struct termios orig_termios;
void disableRawMode(void) {
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_termios);
}

void enableRawMode(void) {
    if(tcgetattr(STDIN_FILENO,&orig_termios)==-1) return;
    atexit(disableRawMode);
    struct termios raw=orig_termios;
    raw.c_lflag&=~(ECHO | ICANON | ISIG);
    raw.c_iflag&=~(IXON | ICRNL);
    raw.c_cc[VMIN]=0;
    raw.c_cc[VTIME]=1; // 0.1s
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw);
}

// keyboard thread - monitor ` key and 7
void *KeyboardThread(void *arg) {
(void)arg;
    enableRawMode();

    unsigned char c;
    while(programRunning) {
        ssize_t r=read(STDIN_FILENO,&c,1);
        if(r==1) {
            if(c=='`') {
                toggled=!toggled;
                fprintf(stderr,"Toggled: %s\n",toggled?"ON":"OFF");
            } else if(c=='7') { // exit
                programRunning=false;
            }
        } else {
            // ignore other keys
            usleep(5000);
        }
    }
    return NULL;
}

// click thread
void *ClickThread(void *arg) {
(void)arg;
    while(programRunning) {
        if(toggled&&mouseDown) {
            if(firstClick) {
                // on first click,release to avoid stuck state then continue
                simulateMouseClickUinput(uinput_fd,false);
                firstClick=false;
                // small wait
                struct timespec ts={0,30*1000000L};
                nanosleep(&ts,NULL);
            } else {
                int RandomWaitDelay=RandomIntRange(70,100); // ms
                long now=getCurrentTimeMs();
                if((now-lastClick)>=RandomWaitDelay) {
                    // press
                    simulateMouseClickUinput(uinput_fd,true);
                    lastClick=now;
                    // hold for 30-50 ms
                    int hold = RandomIntRange(30,50);
                    struct timespec ts={0,hold*1000000L};
                    nanosleep(&ts,NULL);
                    // release
                    simulateMouseClickUinput(uinput_fd,false);
                } else {
                    // small sleep to avoid busy loop
                    usleep(2000);
                }
            }
        } else {
            // not toggled or mouse not down
            usleep(5000);
        }
    }
    return NULL;
}

int main(void) {
    // must be root to access /dev/uinput and /dev/input/event*
    if(geteuid()!=0) {
        fprintf(stderr,"ERROR: program must be run as root(sudo) to access input devices.\n");
        return 1;
    }

    srand((unsigned int)time(NULL));
    printf("autoclicker is starting\n");

    uinput_fd=initUinput();
    if(uinput_fd<0) {
        fprintf(stderr,"Failed to initialize uinput device. Exiting.\n");
        return 1;
    }

    printf("uinput device initialized(fd=%d)\n",uinput_fd);

    pthread_t keyboardThread,mouseThread,clickThread;

    if(pthread_create(&keyboardThread,NULL,KeyboardThread,NULL)!=0) {
        perror("pthread_create keyboardThread");
        destroyUinput(uinput_fd);
        return 1;
    }

    if(pthread_create(&mouseThread,NULL,MouseMonitorThread,NULL)!=0) {
        perror("pthread_create mouseThread");
        programRunning=false;
        pthread_join(keyboardThread,NULL);
        destroyUinput(uinput_fd);
        return 1;
    }

    if(pthread_create(&clickThread,NULL,ClickThread,NULL)!=0) {
        perror("pthread_create clickThread");
        programRunning=false;
        pthread_join(keyboardThread,NULL);
        pthread_join(mouseThread,NULL);
        destroyUinput(uinput_fd);
        return 1;
    }

    // main UI loop: print status if consoleVisible
    while(programRunning) {
        if(consoleVisible) {
            printf("\033[H\033[J");
            printf("═══════════════════════════════════════════════\n");
            printf("                  Autoclicker                     \n");
            printf("═══════════════════════════════════════════════\n\n");
            printf("Shortcuts:\n\n");
            printf("`: Toggle Auto Clicker\n\n");
            printf("7: Exit Program\n");
            printf("───────────────────────────────────────────────\n");
            printf("Status: %s\n",(toggled?"ON ":"OFF"));
            printf("Mouse held: %s\n",(mouseDown?"YES":"NO"));
            printf("═══════════════════════════════════════════════\n");
        }
        sleep(1);
    }

    // shutdown
    fprintf(stderr,"shutting down...\n");
    // join threads
    pthread_join(keyboardThread,NULL);
    pthread_join(mouseThread,NULL);
    pthread_join(clickThread,NULL);

    destroyUinput(uinput_fd);
    printf("program terminated.\n");
    return 0;
}
