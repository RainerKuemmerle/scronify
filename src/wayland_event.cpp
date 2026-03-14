#include "scronify/wayland_event.h"

#include <qdebug.h>
#include <qlogging.h>

#include "scronify/moc_wayland_event.cpp"  // NOLINT

#if defined(HAVE_WAYLAND)
#include <wayland-client.h>

#include <algorithm>
#include <cstring>
#include <map>
#endif

namespace {
constexpr int kSleepMs = 500;
constexpr int kDebounceTicks = 2;
}  // namespace

#if defined(HAVE_WAYLAND)
namespace {
struct WaylandContext {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  std::map<uint32_t, wl_output*> outputs;
  std::map<uint32_t, scronify::OutputConnection> outputs_info;
  std::map<wl_output*, uint32_t> ptr_to_name;
};

struct ListenerData {
  scronify::WaylandEvent* self;
  WaylandContext* ctx;
};

void OutputHandleGeometry(void* data, wl_output* wl_output_ptr, int /*x*/,
                          int /*y*/, int /*physical_width*/,
                          int /*physical_height*/, int /*subpixel*/,
                          const char* make, const char* model,
                          int /*transform*/) {
  auto* ld = static_cast<ListenerData*>(data);
  auto it = ld->ctx->ptr_to_name.find(wl_output_ptr);
  if (it == ld->ctx->ptr_to_name.end()) {
    return;
  }
  uint32_t name = it->second;
  if (make) {
    ld->ctx->outputs_info[name].vendor = QString::fromUtf8(make);
  }
  if (model) {
    ld->ctx->outputs_info[name].product = QString::fromUtf8(model);
  }
}

void OutputHandleMode(void* data, wl_output* wl_output_ptr, uint32_t /*flags*/,
                      int width, int height, int /*refresh*/) {
  auto* ld = static_cast<ListenerData*>(data);
  auto it = ld->ctx->ptr_to_name.find(wl_output_ptr);
  if (it == ld->ctx->ptr_to_name.end()) {
    return;
  }
  uint32_t name = it->second;
  ld->ctx->outputs_info[name].width = width;
  ld->ctx->outputs_info[name].height = height;

  // Build a connection object and hand it to the WaylandEvent instance.
  scronify::OutputConnection conn;
  conn.width = width;
  conn.height = height;
  conn.output_name = ld->ctx->outputs_info[name].output_name;
  conn.vendor = ld->ctx->outputs_info[name].vendor;
  conn.product = ld->ctx->outputs_info[name].product;
  ld->self->UpdateMetadata(name, conn);
}

void OutputHandleDone(void* /*data*/, wl_output* /*wl_output_ptr*/) {}
void OutputHandleScale(void* /*data*/, wl_output* /*wl_output_ptr*/,
                       int /*scale*/) {}

const wl_output_listener kOutputListener = {
    OutputHandleGeometry, OutputHandleMode, OutputHandleDone,
    OutputHandleScale,    nullptr,          nullptr};

void RegistryGlobal(void* data, wl_registry* registry, uint32_t name,
                    const char* interface, uint32_t version) {
  auto* ld = static_cast<ListenerData*>(data);
  if (std::strcmp(interface, "wl_output") == 0) {
    uint32_t ver = std::min<uint32_t>(version, 2);
    auto* out = static_cast<wl_output*>(
        wl_registry_bind(registry, name, &wl_output_interface, ver));
    if (!out) {
      return;
    }
    ld->ctx->outputs[name] = out;
    ld->ctx->ptr_to_name[out] = name;
    ld->ctx->outputs_info[name] = scronify::OutputConnection();
    wl_output_add_listener(out, &kOutputListener, ld);

    // Mark connected and schedule debounce
    ld->self->Connect(name);
  }
}

void RegistryGlobalRemove(void* data, wl_registry* /*registry*/,
                          uint32_t name) {
  auto* ld = static_cast<ListenerData*>(data);
  auto oit = ld->ctx->outputs.find(name);
  if (oit != ld->ctx->outputs.end()) {
    wl_output* out = oit->second;
    // destroy the wl_output and erase mappings
    ld->ctx->ptr_to_name.erase(out);
    wl_output_destroy(out);
    ld->ctx->outputs.erase(oit);
  }

  // Mark removed and schedule debounce
  ld->self->Removed(name);
}

const wl_registry_listener kRegistryListener = {RegistryGlobal,
                                                RegistryGlobalRemove};
}  // namespace
#endif

namespace scronify {

WaylandEvent::WaylandEvent(QObject* parent) : QThread(parent) {}

void WaylandEvent::run() {
  qDebug() << "WaylandEvent thread started";
#if defined(HAVE_WAYLAND)
  WaylandContext* ctx = new WaylandContext();
  ListenerData* ld = new ListenerData{this, ctx};

  ctx->display = wl_display_connect(nullptr);
  if (!ctx->display) {
    qWarning() << "Failed to connect to Wayland display";
    delete ld;
    delete ctx;
    return;
  }

  ctx->registry = wl_display_get_registry(ctx->display);
  wl_registry_add_listener(ctx->registry, &kRegistryListener, ld);
  // Do an initial roundtrip to populate existing globals
  wl_display_roundtrip(ctx->display);
#else
  qWarning() << "Wayland support not available at build time.";
#endif

  while (!QThread::currentThread()->isInterruptionRequested()) {
    TickDebounce();
#if defined(HAVE_WAYLAND)
    // Dispatch pending Wayland events without blocking.
    if (ctx && ctx->display) {
      wl_display_dispatch_pending(ctx->display);
      wl_display_flush(ctx->display);
    }
#endif
    msleep(kSleepMs);
  }

  qDebug() << "WaylandEvent thread stopping";

#if defined(HAVE_WAYLAND)
  // Cleanup Wayland resources
  if (ctx) {
    if (ctx->registry) {
      wl_registry_destroy(ctx->registry);
    }
    for (auto& p : ctx->outputs) {
      if (p.second) {
        wl_output_destroy(p.second);
      }
    }
    if (ctx->display) {
      wl_display_disconnect(ctx->display);
    }
    delete ld;
    delete ctx;
  }
#endif
}

void WaylandEvent::UpdateCache(std::uint64_t output, EventType type) {
  QMutexLocker locker(&cached_output_mutex_);
  cached_output_[output].type = type;
  // Wayland-specific metadata extraction will be implemented later.
  SetupDebounce(output);
}

void WaylandEvent::SetupDebounce(std::uint64_t output) {
  QMutexLocker locker(&debounced_event_mutex_);
  debounced_event_[output] = kDebounceTicks;
}

void WaylandEvent::TickDebounce() {
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

void WaylandEvent::Debounced(std::uint64_t output) {
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

void WaylandEvent::Connect(std::uint64_t output) {
  UpdateCache(output, EventType::kConnected);
}

void WaylandEvent::Removed(std::uint64_t output) {
  UpdateCache(output, EventType::kRemoved);
}

void WaylandEvent::UpdateMetadata(std::uint64_t output,
                                  const OutputConnection& conn) {
  QMutexLocker locker(&cached_output_mutex_);
  auto& evt = cached_output_[output];
  evt.connection = conn;
}

}  // namespace scronify
