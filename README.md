# Scronify

<img src="doc/scronify.png"/>

## Usage

scronify notifies in case a monitor is added or removed.
Upon such an event, the configured commands are run.
This can for example be `notify-send` calls to inform the user about the connected monitor.
Another typical use-case is to adjust the screen layout.

## Compilation

`scronify` depends on Qt either in version 5 or 6. In particular, we require the modules core, gui, and concurrent.

For the connection events using X11, we depend on (optional):
* libx11-dev
* libxrandr-dev

For the connection events using wayland, we depend on:
* libwayland-dev

On Debian/Ubuntu you can install the required Qt development files with
```
# Qt6
sudo apt install qt6-base-dev libx11-dev libxrandr-dev libwayland-dev
# Qt5
sudo apt install qtbase5-dev libx11-dev libxrandr-dev libwayland-dev
```

Building follows the standard pattern of a CMake based project. For example, on Linux with the default Makefile generator.
```
mkdir build
cd build
cmake ..
make
```

You can use `cmake -DSCRONIFY_QTVERSION=5 ..` to use Qt5 instead of the default Qt6.

If you don't want to build X11 support, pass `-DENABLE_X11=OFF` to CMake.
The default is `ON`.

Similarly, if you do not want to build the Wayland support, pass `-DENABLE_WAYLAND=OFF` to CMake. The default is `ON`.

## scronify-wayland-placer

`scronify-wayland-placer` is a small Wayland helper binary that enumerates available outputs, computes a simple stacked layout, and applies it through the `wlr-output-management` protocol.

It is built when Wayland support is enabled and the Wayland client libraries are available. To actually apply the computed layout, the build also needs `wayland-scanner` and the `wlr-output-management-unstable-v1` protocol XML.

Example usage:
```
scronify-wayland-placer --dry-run
scronify-wayland-placer --primary-name eDP-1
```

Options:
* `--dry-run` - print the planned output positions without applying them.
* `--primary-name NAME` - treat the named output as the internal/bottom display.

This tool is one example of a command to be called from the scronify actions.
It is useful when you want to compute or apply a default stacked layout for Wayland outputs, especially for laptop+external monitor setups.

## The name

`scronify` got its name roughly following the scheme below:
```
Screen Cron Notify
│      ─┬──    ┌──
│       │      │
│       ▼      │
└────►scronify◄┘
```
