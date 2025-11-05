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
gcc autoclicker.c -o autoclicker \
  `pkg-config --cflags --libs gtk+-3.0 appindicator3-0.1 x11 xtst` \
  -lpthread
```
#
