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
replace on line 119 ``/dev/input/event4`` whichever number your mouse is

### compile

```
$ gcc -O2 -o autoclicker autoclicker.c -pthread
```
#
