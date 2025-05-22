# GTKShell

[gtkshell](https://github.com/dawsers/gtkshell) is a multi-threaded
programmer's Wayland desktop layer shell bar using GTK4 and written in C++.
It is not recommended for general use, only for people who are comfortable
modifying and compiling C++ code and want to experiment. It also serves as
a reference implementation for the [scroll](https://github.com/dawsers/scroll)
modules in case someone wants to port the functionality to other mainstream
layer shell bars. I use gtkshell in my default desktop.

![gtkshell](./gtkshell.png)

*gtkshell* provides most of the services other shells provide, like
notifications, a system tray, mpris audio control, a calendar with events,
a weather module, CPU/Memory/GPU real time graphs and specific modules for
[scroll](https://github.com/dawsers/scroll) and [hyprland](https://github.com/hyprwm/Hyprland).

You can also change your style-sheet or configuration and reload them at
runtime (press the right mouse button on the bar for a menu).

## Building and Installing

There are no packages for any distribution, installation is manual and you
will need to compile the package. You should also make modifications to the
code to adapt it to call your favorite applications on mouse events etc. I
don't have plans to provide external configuration for the different modules
outside of manually editing their code. There is basic json configuration for
the geometry of the bar, monitors, and active modules, though.

These are the dependencies you will need to compile gtkshell:

cmake, a C++ compiler, gtk4-layer-shell, gtk4, gtkmm, libnm (network manager),
nlohmann_json, wireplumber, and curl.

``` bash
git clone https://github.com/dawsers/gtkshell
cd gtkshell
make release
# You will generate an executable `gtkshell-release`
```

Create a configuration directory and copy the configuration files provided:

``` bash
mkdir -p ~/.config/gtkshell
cp -r config ~/.config/gtkshell/
```

Now you can edit those configuration files adapting them to your needs.

`config_scroll.json`: configuration file for scroll.

`config_hyprland.json`: configuration file for Hyprland.

`style.css`: CSS configuration to personalize colors etc.

`calendar/*`: files for the calendar (clock). See the documentation for the
calendar module.

`weathericonds` and `windrose`: icons needed for the weather module.

Now you can call gtkshell from your scroll/hyprland configuration, for
example:

``` config
exec /home/dawsers/work/gtkshell/gtkshell-release -c ~/.config/gtkshell/config_scroll.json
```

## Modules/Widgets

These are the available modules you can add to your `config.json` (see `main.cpp: Bar::create_widget()`):

### Scroll Specific Widgets

`scroll-workspaces`: sway/scroll workspaces

`scroll-scroller`: information about mode modifiers and click for a widget to
modify them.

`scroll-trails`: scroll trails (active, number and number of trailmarks in the active
one).

`scroll-submap`: currently active scroll submap (if any).

`scroll-client-title`: current client title, marks and trailmark.

`scroll-keyboard-layout`: keyboard layout information.


### Hyprland Specific Widgets

`workspaces`: This is a module to manage Hyprland workspaces.

`scroller`: Widget/information for [hyprscroller](https://github.com/dawsers/hyprscroller).

`submap`: Hyprland submap information

`client-title`: Hyprland client title information.

`keyboard-layout`: Information about your current keyboard layout.


### Mpris

`mpris`: Controls mpris sources. You can click on the buttons to play/pause
etc, right click to select source, and left-click to call `kitty ncmpcpp`.
Change the code in `widgets.cpp:MPlayer:MPlayer()` for your favorite player.


### Clock/Calendar

`clock`: This module provides a clock and a calendar with event information
when you click on the clock. Hovering on a date will show information for that
day. You can configure the information in files located in
`~/.config/gtkshell/calendar`. You can modify the files and they will be
reloaded when you click on the clock widget again.

The format is simple:

One line per event. A line starting with `#` is a comment. Each record has six
fields: `year`, `month`, `day`, `repeat` (will be auto-generated from the given date
forward), `compute` (add the number of years to the event description - for
birthdays, for example), and `description`.

Example:

birthdays.txt

```
2025,04,22,1,1,Scroll release
```

### Arch Package Updates

`package-updates`: It shows missing updates for your system, including the
AUR. You need to have [paru](https://github.com/Morganamilo/paru) installed
to see the information. Hovering over the icon will re-query the server, and
when you click the icon, it will try to open `paman-manager`. You can modify
the code in`widgets.cpp:PackageUpdates()` and change it to something else.

### Idle Inhibitor

`idle-inhibitor`: it uses [matcha](https://codeberg.org/QuincePie/matcha/) as
idle inhibitor. Click on the icon to toggle.


### Weather

`weather`: It uses [Open Meteo](https://open-meteo.com/) to query the weather.
It will show two strings, the current weather and a one hour prediction. The
location and prediction period, as well as time zone can be configured in
`weather.cpp:OMInput`. Click on the widget and you will get much more detailed
information.

### Screenshots

`screenshot`: It uses `wl-copy`, `grim` and `slurp`. Left button click saves
the selection to `~/Downloads`, and right button click to the clipboard. See
the code in `widgets.cpp:ScreenShot`.

### CPU/Mem/GPU Graphs

`cpu-graph`: Shows a graph with CPU usage. Hovering shows the top offenders.

`mem-graph`: Shows a graph with memory usage. Hovering over it shows a tooltip
with the top users. 

`gpu-graph`: This is only for Nvidia GPUs. It uses `nvidia-smi`.

Clicking on `cpu-graph` or `mem-graph` will try to call `kitty btop`. You can
change this in `graph.cpp`. Search for that string, it is present in two
places. `gpu-graph` calls `kitty nvtop`.


### Basic Network Information

`network`: This only shows the IP and whether there is access to the Internet.


### Pipewire Audio

`speaker`: Change volume or mute the active speaker.

`microphone`: Change volume or mute the active microphone.

Right clicking calls `pavucontrol`. Change this in `wireplumber.cpp`.


### Notifications

`notifications`: Provides a notification system. Notifications will show when
received. Click on them to dismiss them, but not close (delete) them. You can also
activate them (if the Activate button shows). To close a notification for
good (and delete them from the queue), click on the notifications icon (bell)
which will show all the available ones. When you click on one there, it will be
closed and deleted.


### System Tray

`systray`: Provides a system tray.

