# Autoclicker

### required packages for Debian based systems
```
$ sudo apt install evtest
```

### required packages for Arch based systems
```
$ sudo pacman -S evtest
```

### find your mouse input
```
$ sudo evtest
```
replace on line 74 and 75 ``/dev/input/event4`` whichever number your mouse/keyboard is

### compile

```
$ gcc -O2 -o autoclicker autoclicker.c -pthread
```
#
