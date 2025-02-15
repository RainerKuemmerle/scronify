#include "scronify/x11_event.h"

#include <qdebug.h>
#include <qlogging.h>

#include <memory>
#include <string_view>

#include "scronify/moc_x11_event.cpp"  // NOLINT

// must be last
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <qobject.h>

namespace {
std::string_view ConnectionToStr(int c) {
  switch (c) {
    case 0:
      return "connected";
    case 1:
      return "disconnected";
    default:
      return "unknown";
  }
}
}  // namespace

namespace scronify {

X11Event::X11Event(QObject* parent) : QThread(parent) {}

void X11Event::run() {
  Display* display = XOpenDisplay(nullptr);
  XRRSelectInput(display, DefaultRootWindow(display), RROutputChangeNotifyMask);
  XSync(display, False);

  while (true) {
    if (QThread::currentThread()->isInterruptionRequested()) {
      return;
    }

    const int num_events = XEventsQueued(display, QueuedAfterFlush);
    if (num_events <= 0) {
      msleep(500);  // NOLINT
      continue;
    }

    for (int i = 0; i < num_events; ++i) {
      XEvent ev;
      if (!XNextEvent(display, &ev)) {
        auto* output_change_notify =
            reinterpret_cast<XRROutputChangeNotifyEvent*>(&ev);  // NOLINT

        std::unique_ptr<XRRScreenResources, void (*)(XRRScreenResources*)>
            screen_resource(XRRGetScreenResources(output_change_notify->display,
                                                  output_change_notify->window),
                            XRRFreeScreenResources);
        if (screen_resource == nullptr) {
          qWarning() << "Could not get screen resources";
          continue;
        }

        std::unique_ptr<XRROutputInfo, void (*)(XRROutputInfo*)> output_info(
            XRRGetOutputInfo(output_change_notify->display,
                             screen_resource.get(),
                             output_change_notify->output),
            XRRFreeOutputInfo);
        if (output_info == nullptr) {
          qWarning() << "Could not get output info";
          continue;
        }

        QString output_name = QString(output_info->name).trimmed();
        qDebug() << "Event for output " << output_change_notify->output << " "
                 << output_name << " "
                 << ConnectionToStr(output_info->connection);

        switch (output_info->connection) {
          case 0:  // connected
            emit ScreenAdded();
            break;
          case 1:  // disconnected
            emit ScreenRemoved();
            break;
          default:
            qWarning() << "Unknown connection type";
        }
      }
    }
  }
}

}  // namespace scronify
