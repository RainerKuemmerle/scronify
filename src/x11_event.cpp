#include "scronify/x11_event.h"

#include <qcoreapplication.h>
#include <qdebug.h>
#include <qlogging.h>
#include <qobject.h>
#include <qtimer.h>

#include <memory>

#include "scronify/event.h"
#include "scronify/moc_x11_event.cpp"  // NOLINT
#include "scronify/output_connection.h"

// must be last
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

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

constexpr int kSleepMs = 500;
constexpr int kDebounceTicks = 2;

}  // namespace

namespace scronify {

using ScreenResourcePtr =
    std::unique_ptr<XRRScreenResources, void (*)(XRRScreenResources*)>;
using OutputInfoPtr = std::unique_ptr<XRROutputInfo, void (*)(XRROutputInfo*)>;

struct ScreenHandle {
  ScreenHandle(Display* d, ScreenResourcePtr sr, OutputInfoPtr oi)
      : display(d),
        screen_resource(std::move(sr)),
        output_info(std::move(oi)) {}
  Display* display;
  ScreenResourcePtr screen_resource;
  OutputInfoPtr output_info;
};

namespace {
void GetConnectionDetails(RROutput out, const ScreenHandle& handle,
                          OutputConnection& connection) {
  Bool has_edid_prop = False;
  int props = 0;
  Atom* properties = XRRListOutputProperties(handle.display, out, &props);
  Atom atom_edid = XInternAtom(handle.display, RR_PROPERTY_RANDR_EDID, False);
  for (int i = 0; i < props; i++) {
    if (properties[i] == atom_edid) {  // NOLINT
      has_edid_prop = True;
      break;
    }
  }
  XFree(properties);

  if (!has_edid_prop) {
    return;
  }

  // NOLINTBEGIN
  int format = 0;
  unsigned long n = 0;
  unsigned long extra = 0;
  unsigned char* p = nullptr;
  Atom real = 0;
  if (XRRGetOutputProperty(handle.display, out, atom_edid, 0L, 128L, False,
                           False, AnyPropertyType, &real, &format, &n, &extra,
                           &p) == Success) {
    if (n >= 127) {
      uint16_t vendor = (p[9] << 8) | p[8];
      uint16_t product = (p[11] << 8) | p[10];
      uint32_t serial = p[15] << 24 | p[14] << 16 | p[13] << 8 | p[12];
      connection.vendor = QString::asprintf("%04X", vendor);
      connection.product = QString::asprintf("%04X", product);
      connection.serial = QString::asprintf("%08X", serial);
    }
    free(p);
  }
  // NOLINTEND
}
}  // namespace

X11Event::X11Event(QObject* parent) : QThread(parent) {}

void X11Event::run() {
  Display* display = XOpenDisplay(nullptr);
  XRRSelectInput(display, DefaultRootWindow(display), RROutputChangeNotifyMask);
  XSync(display, False);

  while (!QThread::currentThread()->isInterruptionRequested()) {
    TickDebounce();
    const int num_events = XEventsQueued(display, QueuedAfterFlush);
    if (num_events <= 0) {
      msleep(kSleepMs);
      continue;
    }

    for (int i = 0; i < num_events; ++i) {
      XEvent ev;
      if (XNextEvent(display, &ev)) {
        qWarning("Error while fetching XNextEvent");
        continue;
      }

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
          XRRGetOutputInfo(output_change_notify->display, screen_resource.get(),
                           output_change_notify->output),
          XRRFreeOutputInfo);
      if (output_info == nullptr) {
        qWarning() << "Could not get output info";
        continue;
      }

      qDebug() << "Event for output " << output_change_notify->output << " "
               << QString(output_info->name).trimmed() << " "
               << ConnectionToStr(output_info->connection);

      const int connection = output_info->connection;
      ScreenHandle screen_handle(display, std::move(screen_resource),
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

void X11Event::Connect(std::uint64_t output, const ScreenHandle& handle) {
  QMutexLocker locker(&cached_output_mutex_);
  cached_output_[output].type = EventType::kConnected;
  cached_output_[output].connection.output_name =
      QString(handle.output_info->name).trimmed();
  GetConnectionDetails(output, handle, cached_output_[output].connection);
  SetupDebounce(output);
}

void X11Event::Removed(std::uint64_t output, const ScreenHandle& /*handle*/) {
  QMutexLocker locker(&cached_output_mutex_);
  cached_output_[output].type = EventType::kRemoved;
  SetupDebounce(output);
}

void X11Event::SetupDebounce(std::uint64_t output) {
  QMutexLocker locker(&debounced_event_mutex_);
  debounced_event_[output] = kDebounceTicks;
}

void X11Event::TickDebounce() {
  QMutexLocker locker(&debounced_event_mutex_);
  if (debounced_event_.empty()) {
    return;
  }

  std::vector<std::uint64_t> debounced;
  for (auto& p : debounced_event_) {
    p.second--;
    if (p.second <= 0) {
      debounced.push_back(p.first);
      Debounced(p.first);
    }
  }

  for (auto output : debounced) {
    debounced_event_.erase(output);
  }
}

void X11Event::Debounced(std::uint64_t output) {
  QMutexLocker locker(&cached_output_mutex_);

  auto found_it = cached_output_.find(output);
  if (found_it == cached_output_.end()) {
    qCritical() << "Logic error: Unable to find output's cache";
    return;
  }

  switch (found_it->second.type) {
    case EventType::kConnected:
      emit ScreenAdded();
      break;
    case EventType::kRemoved:
      emit ScreenRemoved();
      break;
    case EventType::kUnknown:
      qCritical() << "Logic error: Unknown event type";
      break;
  }
}

}  // namespace scronify
