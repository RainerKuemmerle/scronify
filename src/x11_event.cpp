#include "scronify/x11_event.h"

#include <qdebug.h>
#include <qlogging.h>

#include <memory>
#include <string_view>

#include "scronify/moc_x11_event.cpp"  // NOLINT
#include "scronify/output_connection.h"

// must be last
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <qobject.h>

namespace {
QString ConnectionToStr(int c) {
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

using ScreenResourcePtr =
    std::unique_ptr<XRRScreenResources, void (*)(XRRScreenResources*)>;
using OutputInfoPtr = std::unique_ptr<XRROutputInfo, void (*)(XRROutputInfo*)>;

struct ScreenHandle {
  ScreenHandle(ScreenResourcePtr sr, OutputInfoPtr oi)
      : screen_resource(std::move(sr)), output_info(std::move(oi)) {}
  ScreenResourcePtr screen_resource;
  OutputInfoPtr output_info;
};

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

        ScreenResourcePtr screen_resource(
            XRRGetScreenResources(output_change_notify->display,
                                  output_change_notify->window),
            XRRFreeScreenResources);
        if (screen_resource == nullptr) {
          qWarning() << "Could not get screen resources";
          continue;
        }

        OutputInfoPtr output_info(
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

        const int connection = output_info->connection;
        ScreenHandle screen_handle(std::move(screen_resource),
                                   std::move(output_info));
        switch (connection) {
          case 0:  // connected
            Connect(output_change_notify->output, screen_handle);
            break;
          case 1:  // disconnected
            Removed(output_change_notify->output, screen_handle);
            break;
          default:
            qWarning() << "Unknown connection type";
        }
      }
    }
  }
}

void X11Event::Connect(std::uint64_t output, const ScreenHandle& handle) {
  auto found_it = cached_output_.find(output);

  bool already_connected = found_it != cached_output_.end();
  const OutputConnection&
      connection =  // TODO(Rainer) generate correct connection
      already_connected ? found_it->second : OutputConnection();

  cached_output_[output] = connection;

  if (!already_connected) {
    // TODO debounce
    emit ScreenAdded();
  }
}

void X11Event::Removed(std::uint64_t output, const ScreenHandle& handle) {
  // TODO remove
  emit ScreenRemoved();
  cached_output_.erase(output);
}

}  // namespace scronify
