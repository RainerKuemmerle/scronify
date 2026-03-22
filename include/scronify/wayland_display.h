#pragma once

#include <wayland-client.h>
#ifdef HAVE_OUTPUT_MGMT
#include "wlr-output-management-unstable-v1-client-protocol.h"
#endif

#include <cctype>
#include <map>
#include <string>
#include <vector>

struct OutputInfo {
  uint32_t id = 0;
  std::string name;
  std::string vendor;
  std::string product;
  std::string serial_number;
  int width = 0;
  int height = 0;
  bool enabled = false;
#ifdef HAVE_OUTPUT_MGMT
  zwlr_output_head_v1* head = nullptr;
#endif
};

// convert ASCII string to lowercase (safe for signed char)
static inline std::string ToLower(const std::string& s) {
  std::string out = s;
  for (auto& ch : out) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return out;
}

// Small RAII helper to open a display, enumerate heads (wlr manager when
// available), and apply configurations via the zwlr output-manager protocol.
class WaylandDisplay {
 public:
  WaylandDisplay();
  ~WaylandDisplay();

  [[nodiscard]] bool Valid() const { return display_ != nullptr; }

  // Try to enumerate via wlr output-management. Returns empty vector if
  // manager isn't available or on error.
  std::vector<OutputInfo> EnumerateWlrHeads(int max_roundtrips = 8);

  // Apply placements using the bound manager (binds manager if necessary).
  // Returns true on success.
  bool Apply(const std::vector<std::pair<OutputInfo, std::pair<int, int>>>&
                 placements);

  static bool IsInternalDisplay(const OutputInfo& o);

 private:
  wl_display* display_ = nullptr;
  wl_registry* registry_ = nullptr;

#ifdef HAVE_OUTPUT_MGMT
  zwlr_output_manager_v1* manager_ = nullptr;
  uint32_t last_serial_ = 0;
  std::map<zwlr_output_head_v1*, OutputInfo> heads_;
#endif

 public:
#ifdef HAVE_OUTPUT_MGMT
  // accessors for private members (listeners use these)
  [[nodiscard]] zwlr_output_manager_v1* Manager() const { return manager_; }
  void SetManager(zwlr_output_manager_v1* m) { manager_ = m; }

  [[nodiscard]] uint32_t LastSerial() const { return last_serial_; }
  void SetLastSerial(uint32_t s) { last_serial_ = s; }

  std::map<zwlr_output_head_v1*, OutputInfo>& Heads() { return heads_; }
  [[nodiscard]] const std::map<zwlr_output_head_v1*, OutputInfo>& Heads()
      const {
    return heads_;
  }
#endif
};
