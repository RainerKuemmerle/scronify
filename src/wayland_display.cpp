#include "scronify/wayland_display.h"

#include <algorithm>
#include <cstring>
#include <iostream>

#ifdef HAVE_OUTPUT_MGMT
namespace {

struct HeadData {
  zwlr_output_head_v1* head;
  WaylandDisplay* ctx;
};

struct ModeData {
  zwlr_output_head_v1* head;
  WaylandDisplay* ctx;
};

// head listeners
void HeadName(void* data, zwlr_output_head_v1* /*head*/, const char* name) {
  auto* hd = static_cast<HeadData*>(data);
  hd->ctx->Heads()[hd->head].name = name ? name : std::string();
}
void HeadDescription(void* data, zwlr_output_head_v1* /*head*/,
                     const char* desc) {
  auto* hd = static_cast<HeadData*>(data);
  hd->ctx->Heads()[hd->head].product = desc ? desc : std::string();
}
void HeadMake(void* data, zwlr_output_head_v1* /*head*/, const char* make) {
  auto* hd = static_cast<HeadData*>(data);
  hd->ctx->Heads()[hd->head].vendor = make ? make : std::string();
}
void HeadModel(void* data, zwlr_output_head_v1* /*head*/, const char* model) {
  auto* hd = static_cast<HeadData*>(data);
  hd->ctx->Heads()[hd->head].product = model ? model : std::string();
}
void HeadSerial(void* data, zwlr_output_head_v1* /*head*/, const char* serial) {
  auto* hd = static_cast<HeadData*>(data);
  hd->ctx->Heads()[hd->head].serial_number = serial ? serial : std::string();
}
void HeadPhysicalSize(void* /*data*/, zwlr_output_head_v1* /*head*/,
                      int32_t /*w*/, int32_t /*h*/) {}
void HeadMode(void* data, zwlr_output_head_v1* head,
              zwlr_output_mode_v1* mode) {
  auto* hd = static_cast<HeadData*>(data);
  auto* md = new ModeData{head, hd->ctx};
  static const zwlr_output_mode_v1_listener kModeListener = {
      // size
      +[](void* data, zwlr_output_mode_v1* /*mode*/, int32_t width,
          int32_t height) {
        auto* md = static_cast<ModeData*>(data);
        for (auto& kv : md->ctx->Heads()) {
          if (kv.first == md->head) {
            if (kv.second.width == 0) {
              kv.second.width = width;
            }
            if (kv.second.height == 0) {
              kv.second.height = height;
            }
            break;
          }
        }
      },
      // refresh
      +[](void* /*data*/, zwlr_output_mode_v1* /*mode*/, int32_t /*refresh*/) {
      },
      // preferred
      +[](void* /*data*/, zwlr_output_mode_v1* /*mode*/) {},
      // finished
      +[](void* data, zwlr_output_mode_v1* /*mode*/) {
        delete static_cast<ModeData*>(data);
      }};
  zwlr_output_mode_v1_add_listener(mode, &kModeListener, md);
}
void HeadEnabled(void* data, zwlr_output_head_v1* head, int enabled) {
  auto* hd = static_cast<HeadData*>(data);
  hd->ctx->Heads()[head].enabled = enabled != 0;
}
void HeadCurrentMode(void* /*data*/, zwlr_output_head_v1* /*head*/,
                     zwlr_output_mode_v1* /*mode*/) {}
void HeadPosition(void* /*data*/, zwlr_output_head_v1* /*head*/, int /*x*/,
                  int /*y*/) {}
void HeadTransform(void* /*data*/, zwlr_output_head_v1* /*head*/, int /*t*/) {}
void HeadScale(void* /*data*/, zwlr_output_head_v1* /*head*/,
               wl_fixed_t /*scale*/) {}
void HeadFinished(void* /*data*/, zwlr_output_head_v1* /*head*/) {}

const zwlr_output_head_v1_listener kHeadListener = {
    HeadName,        HeadDescription, HeadPhysicalSize, HeadMode,  HeadEnabled,
    HeadCurrentMode, HeadPosition,    HeadTransform,    HeadScale, HeadFinished,
    HeadMake,        HeadModel,       HeadSerial};

// manager listeners
void ManagerHandleHead(void* data, zwlr_output_manager_v1* /*manager*/,
                       zwlr_output_head_v1* head) {
  auto* ctx = static_cast<WaylandDisplay*>(data);
  ctx->Heads()[head] = OutputInfo();
  ctx->Heads()[head].head = head;
  auto* hd = new HeadData{head, ctx};
  zwlr_output_head_v1_add_listener(head, &kHeadListener, hd);
}
void ManagerHandleDone(void* data, zwlr_output_manager_v1* /*manager*/,
                       uint32_t serial) {
  auto* ctx = static_cast<WaylandDisplay*>(data);
  ctx->SetLastSerial(serial);
}
const zwlr_output_manager_v1_listener kManagerListener = {ManagerHandleHead,
                                                          ManagerHandleDone};

}  // namespace
#endif  // HAVE_OUTPUT_MGMT

WaylandDisplay::WaylandDisplay() : display_(wl_display_connect(nullptr)) {
  if (display_) {
    registry_ = wl_display_get_registry(display_);
  }
}

WaylandDisplay::~WaylandDisplay() {
#ifdef HAVE_OUTPUT_MGMT
  if (manager_) {
    zwlr_output_manager_v1_destroy(manager_);
  }
#endif
  if (registry_) {
    wl_registry_destroy(registry_);
  }
  if (display_) {
    wl_display_disconnect(display_);
  }
}

bool WaylandDisplay::IsInternalDisplay(const OutputInfo& o) {
  const std::string nn = ToLower(o.name);
  return nn.find("edp") != std::string::npos ||
         nn.find("lvds") != std::string::npos ||
         nn.find("internal") != std::string::npos;
}

#ifdef HAVE_OUTPUT_MGMT
std::vector<OutputInfo> WaylandDisplay::EnumerateWlrHeads(int max_roundtrips) {
  std::vector<OutputInfo> outs;
  if (!display_ || !registry_) {
    return outs;
  }

  // registry listener to bind manager
  auto enum_registry_global = [](void* data, wl_registry* registry,
                                 uint32_t name, const char* interface,
                                 uint32_t version) {
    auto* ctx = static_cast<WaylandDisplay*>(data);
    if (std::strcmp(interface, "zwlr_output_manager_v1") == 0) {
      uint32_t ver = std::min<uint32_t>(version, 2u);
      ctx->manager_ = static_cast<zwlr_output_manager_v1*>(wl_registry_bind(
          registry, name, &zwlr_output_manager_v1_interface, ver));
      if (ctx->manager_) {
        zwlr_output_manager_v1_add_listener(ctx->manager_, &kManagerListener,
                                            ctx);
      }
    }
  };

  auto enum_registry_remove = [](void* /*data*/, wl_registry* /*registry*/,
                                 uint32_t /*name*/) {};
  static const wl_registry_listener kEnumRegistryListener = {
      enum_registry_global, enum_registry_remove};
  wl_registry_add_listener(registry_, &kEnumRegistryListener, this);

  for (int i = 0; i < max_roundtrips && last_serial_ == 0; ++i) {
    wl_display_roundtrip(display_);
  }

  outs.reserve(heads_.size());
  for (auto& kv : heads_) {
    outs.push_back(kv.second);
  }

  return outs;
}

bool WaylandDisplay::Apply(
    const std::vector<std::pair<OutputInfo, std::pair<int, int>>>& placements) {
  if (!display_) {
    return false;
  }
  if (!registry_) {
    return false;
  }
  // bind manager if not already
  if (!manager_) {
    // do a roundtrip hoping manager is announced
    wl_display_roundtrip(display_);
  }
  if (!manager_) {
    return false;
  }
  if (last_serial_ == 0) {
    return false;
  }

  // Build lookup maps
  std::map<std::string, zwlr_output_head_v1*> name_map;
  std::map<std::string, zwlr_output_head_v1*> product_map;
  std::map<std::pair<int, int>, zwlr_output_head_v1*> size_map;
  for (auto& hkv : heads_) {
    zwlr_output_head_v1* h = hkv.first;
    OutputInfo& hi = hkv.second;
    if (!hi.name.empty()) {
      name_map[ToLower(hi.name)] = h;
    }
    if (!hi.product.empty()) {
      product_map[ToLower(hi.product)] = h;
    }
    if (hi.width > 0 && hi.height > 0) {
      size_map[{hi.width, hi.height}] = h;
    }
  }

  std::map<zwlr_output_head_v1*, std::pair<int, int>> pos_map;
  for (const auto& p : placements) {
    const OutputInfo& oi = p.first;
    int x = p.second.first;
    int y = p.second.second;
    bool matched = false;
    if (!oi.name.empty()) {
      auto it = name_map.find(ToLower(oi.name));
      if (it != name_map.end()) {
        pos_map[it->second] = {x, y};
        matched = true;
      }
    }
    if (!matched && !oi.product.empty()) {
      auto it = product_map.find(ToLower(oi.product));
      if (it != product_map.end()) {
        pos_map[it->second] = {x, y};
        matched = true;
      }
    }
    if (!matched && oi.width > 0 && oi.height > 0) {
      auto it = size_map.find({oi.width, oi.height});
      if (it != size_map.end()) {
        pos_map[it->second] = {x, y};
        matched = true;
      }
    }
    if (!matched) {
      std::cerr << "Warning: placement for '" << oi.name
                << "' not matched to any head\n";
    }
  }

  std::cout << "Planned configuration actions:\n";
  for (auto& hkv : heads_) {
    zwlr_output_head_v1* h = hkv.first;
    OutputInfo& hi = hkv.second;
    bool internal = IsInternalDisplay(hi);
    auto pit = pos_map.find(h);
    std::cout << "- head='" << hi.name << "' product='" << hi.product
              << "' internal=" << (internal ? "yes" : "no") << " => enable";
    if (pit != pos_map.end()) {
      std::cout << " pos=(" << pit->second.first << "," << pit->second.second
                << ")";
    }
    std::cout << "\n";
  }

  zwlr_output_configuration_v1* cfg =
      zwlr_output_manager_v1_create_configuration(manager_, last_serial_);
  if (!cfg) {
    return false;
  }

  for (auto& hkv : heads_) {
    zwlr_output_head_v1* h = hkv.first;
    OutputInfo& hi = hkv.second;
    auto pit = pos_map.find(h);
    if (!hi.enabled) {
      std::cout << "Requesting enable for head='" << hi.name << "'\n";
      hi.enabled = true;
    }
    zwlr_output_configuration_head_v1* headcfg =
        zwlr_output_configuration_v1_enable_head(cfg, h);
    if (!headcfg) {
      continue;
    }
    if (pit != pos_map.end()) {
      zwlr_output_configuration_head_v1_set_position(headcfg, pit->second.first,
                                                     pit->second.second);
    }
  }

  struct ApplyResult {
    int status = 0;
  } result;
  static const zwlr_output_configuration_v1_listener kCfgListener = {
      +[](void* data, zwlr_output_configuration_v1* /*cfg*/) {
        auto* r = static_cast<ApplyResult*>(data);
        r->status = 1;
      },
      +[](void* data, zwlr_output_configuration_v1* /*cfg*/) {
        auto* r = static_cast<ApplyResult*>(data);
        r->status = -1;
      },
      +[](void* data, zwlr_output_configuration_v1* /*cfg*/) {
        auto* r = static_cast<ApplyResult*>(data);
        r->status = -2;
      }};
  zwlr_output_configuration_v1_add_listener(cfg, &kCfgListener, &result);
  zwlr_output_configuration_v1_apply(cfg);

  for (int i = 0; i < 20 && result.status == 0; ++i) {
    wl_display_roundtrip(display_);
  }

  if (result.status == 1) {
    std::cout << "Apply result: succeeded\n";
  } else if (result.status == -1) {
    std::cout << "Apply result: failed\n";
  } else if (result.status == -2) {
    std::cout << "Apply result: cancelled\n";
  } else {
    std::cout << "Apply result: timeout/no-response\n";
  }

  zwlr_output_configuration_v1_destroy(cfg);
  return result.status == 1;
}
#endif  // HAVE_OUTPUT_MGMT
