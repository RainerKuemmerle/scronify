#include "scronify/wayland_event.h"

#include <poll.h>
#include <qdebug.h>
#include <qlogging.h>
#include <wayland-client.h>

#if defined(HAVE_XDG_OUTPUT)
#include "xdg-output-unstable-v1-client-protocol.h"
#endif

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>

#include "scronify/moc_wayland_event.cpp"  // NOLINT

namespace {
constexpr int kSleepMs = 500;
}  // namespace

namespace {
struct WaylandContext {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  std::map<uint32_t, wl_output*> outputs;
  std::map<uint32_t, scronify::OutputConnection> outputs_info;
  std::map<wl_output*, uint32_t> ptr_to_name;
#if defined(HAVE_XDG_OUTPUT)
  zxdg_output_manager_v1* xdg_output_manager = nullptr;
  std::map<wl_output*, zxdg_output_v1*> xdg_outputs;
  std::map<zxdg_output_v1*, uint32_t> xdg_to_name;
#endif
};

struct ListenerData {
  scronify::WaylandEvent* self;
  std::unique_ptr<WaylandContext> ctx;
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
  conn.description = ld->ctx->outputs_info[name].description;
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

#if defined(HAVE_XDG_OUTPUT)
void XdgOutputHandleLogicalPosition(void* /*data*/,
                                    zxdg_output_v1* /*xdg_output*/, int /*x*/,
                                    int /*y*/) {}
void XdgOutputHandleLogicalSize(void* /*data*/, zxdg_output_v1* /*xdg_output*/,
                                int /*width*/, int /*height*/) {}
void XdgOutputHandleDone(void* /*data*/, zxdg_output_v1* /*xdg_output*/) {}
void XdgOutputHandleName(void* data, zxdg_output_v1* xdg_output,
                         const char* name) {
  if (!name) {
    return;
  }
  auto* ld = static_cast<ListenerData*>(data);
  auto it = ld->ctx->xdg_to_name.find(xdg_output);
  if (it == ld->ctx->xdg_to_name.end()) {
    return;
  }
  uint32_t output_id = it->second;
  ld->ctx->outputs_info[output_id].output_name = QString::fromUtf8(name);
  scronify::OutputConnection conn = ld->ctx->outputs_info[output_id];
  ld->self->UpdateMetadata(output_id, conn);
}
void XdgOutputHandleDescription(void* data, zxdg_output_v1* xdg_output,
                                const char* description) {
  if (!description) {
    return;
  }
  auto* ld = static_cast<ListenerData*>(data);
  auto it = ld->ctx->xdg_to_name.find(xdg_output);
  if (it == ld->ctx->xdg_to_name.end()) {
    return;
  }
  uint32_t output_id = it->second;
  ld->ctx->outputs_info[output_id].description = QString::fromUtf8(description);
  scronify::OutputConnection conn = ld->ctx->outputs_info[output_id];
  ld->self->UpdateMetadata(output_id, conn);
}

const zxdg_output_v1_listener kXdgOutputListener = {
    XdgOutputHandleLogicalPosition, XdgOutputHandleLogicalSize,
    XdgOutputHandleDone, XdgOutputHandleName, XdgOutputHandleDescription};

void CreateXdgOutput(ListenerData* ld, uint32_t id, wl_output* output) {
  if (!ld->ctx->xdg_output_manager || output == nullptr) {
    return;
  }
  zxdg_output_v1* xdg_output = zxdg_output_manager_v1_get_xdg_output(
      ld->ctx->xdg_output_manager, output);
  if (!xdg_output) {
    return;
  }
  ld->ctx->xdg_outputs[output] = xdg_output;
  ld->ctx->xdg_to_name[xdg_output] = id;
  zxdg_output_v1_add_listener(xdg_output, &kXdgOutputListener, ld);
}
#endif

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
#if defined(HAVE_XDG_OUTPUT)
    CreateXdgOutput(ld, name, out);
#endif

    // Mark connected and schedule debounce
    ld->self->Connect(name);
  }
#if defined(HAVE_XDG_OUTPUT)
  else if (std::strcmp(interface, "zxdg_output_manager_v1") == 0) {
    uint32_t ver = std::min<uint32_t>(version, 3);
    ld->ctx->xdg_output_manager =
        static_cast<zxdg_output_manager_v1*>(wl_registry_bind(
            registry, name, &zxdg_output_manager_v1_interface, ver));
    if (ld->ctx->xdg_output_manager) {
      for (const auto& p : ld->ctx->outputs) {
        CreateXdgOutput(ld, p.first, p.second);
      }
    }
  }
#endif
}

void RegistryGlobalRemove(void* data, wl_registry* /*registry*/,
                          uint32_t name) {
  auto* ld = static_cast<ListenerData*>(data);
  auto oit = ld->ctx->outputs.find(name);
  if (oit != ld->ctx->outputs.end()) {
    wl_output* out = oit->second;
#if defined(HAVE_XDG_OUTPUT)
    auto xit = ld->ctx->xdg_outputs.find(out);
    if (xit != ld->ctx->xdg_outputs.end()) {
      zxdg_output_v1_destroy(xit->second);
      ld->ctx->xdg_to_name.erase(xit->second);
      ld->ctx->xdg_outputs.erase(xit);
    }
#endif
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

bool HandleWaylandEvents(WaylandContext* ctx) {
  if (ctx && ctx->display) {
    int fd = wl_display_get_fd(ctx->display);
    if (fd >= 0) {
      struct pollfd pfd{};
      pfd.fd = fd;
      pfd.events = POLLIN;
      int ret = poll(&pfd, 1, kSleepMs);
      if (ret > 0) {
        if (pfd.revents & POLLIN) {
          wl_display_dispatch(ctx->display);
        }
      } else if (ret == 0) {
        // timeout, nothing to do
      } else {
        wl_display_dispatch_pending(ctx->display);
      }
      wl_display_flush(ctx->display);
      return true;
    }
    // No valid fd; dispatch pending events and sleep.
    wl_display_dispatch_pending(ctx->display);
    wl_display_flush(ctx->display);
    scronify::WaylandEvent::msleep(kSleepMs);
    return true;
  }
  return false;
}
}  // namespace

namespace scronify {

WaylandEvent::WaylandEvent(QObject* parent) : DisplayEvent(parent) {}

void WaylandEvent::run() {
  qDebug() << "WaylandEvent thread started";
  auto ld = std::make_unique<ListenerData>(
      ListenerData{this, std::make_unique<WaylandContext>()});

  WaylandContext* ctx = ld->ctx.get();
  ctx->display = wl_display_connect(nullptr);
  if (!ctx->display) {
    qWarning() << "Failed to connect to Wayland display";
    return;
  }

  ctx->registry = wl_display_get_registry(ctx->display);
  wl_registry_add_listener(ctx->registry, &kRegistryListener, ld.get());
  // Do an initial roundtrip to populate existing globals
  wl_display_roundtrip(ctx->display);

  while (!QThread::currentThread()->isInterruptionRequested()) {
    TickDebounce();
    bool handled = HandleWaylandEvents(ctx);
    if (!handled) {
      msleep(kSleepMs);
    }
  }

  qDebug() << "WaylandEvent thread stopping";

  // Cleanup Wayland resources
  if (ctx) {
    if (ctx->registry) {
      wl_registry_destroy(ctx->registry);
    }
#if defined(HAVE_XDG_OUTPUT)
    for (auto& x : ctx->xdg_outputs) {
      if (x.second) {
        zxdg_output_v1_destroy(x.second);
      }
    }
    if (ctx->xdg_output_manager) {
      zxdg_output_manager_v1_destroy(ctx->xdg_output_manager);
    }
#endif
    for (auto& p : ctx->outputs) {
      if (p.second) {
        wl_output_destroy(p.second);
      }
    }
    if (ctx->display) {
      wl_display_disconnect(ctx->display);
    }
  }
}

void WaylandEvent::Connect(std::uint64_t output) {
  DisplayEvent::UpdateCache(output, EventType::kConnected);
}

void WaylandEvent::Removed(std::uint64_t output) {
  DisplayEvent::UpdateCache(output, EventType::kRemoved);
}

void WaylandEvent::UpdateMetadata(std::uint64_t output,
                                  const OutputConnection& conn) {
  QMutexLocker locker(&cached_output_mutex_);
  auto& evt = cached_output_[output];
  evt.connection = conn;
}

}  // namespace scronify
