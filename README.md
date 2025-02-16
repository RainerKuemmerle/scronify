# Scronify

<img src="doc/scronify.png"/>

## Usage

scronify notifies in case a monitor is added or removed.
Upon such an event, the configured commands are run.
This can for example be `notify-send` calls to inform the user about the connected monitor.
Another typical use-case is to adjust the screen layout.

## Compilation

`scronify` depends on Qt either in version 5 or 6. In particular, we require the modules core, gui, and concurrent.

Furthermore, for the connection events using X11, we depend on:
* libx11-dev
* libxrandr-dev


On Debian/Ubuntu you can install the required Qt development files with
```
# Qt6
sudo apt install qt6-base-dev libx11-dev libxrandr-dev
# Qt5
sudo apt install qtbase5-dev libx11-dev libxrandr-dev
```

Building follows the standard pattern of a CMake based project. For example, on Linux with the default Makefile generator.
```
mkdir build
cd build
cmake ..
make
```

You can use `cmake -DSCRONIFY_QTVERSION=5 ..` to use Qt5 instead of the default Qt6.

## The name

`scronify` got its name roughly following the scheme below:
```
Screen Cron Notify
│      ─┬──    ┌──
│       │      │
│       ▼      │
└────►scronify◄┘
```
