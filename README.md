# Autoclicker

### required packages for Debian based systems
```
sudo apt install ydotool libx11-dev libxtst-dev libgtk-3-dev libappindicator3-dev
```

### required packages for Arch based systems
```
sudo pacman -S ydotool libx11 libxtst gtk3 libappindicator-gtk3
```

### compile

```
gcc -O2 -o autoclicker autoclicker.c -pthread
```
#
