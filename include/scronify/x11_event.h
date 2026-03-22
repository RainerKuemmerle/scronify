#pragma once

#include <qmutex.h>
#include <qobject.h>
#include <qthread.h>
#include <qtimer.h>

#include <cstdint>
#include <unordered_map>

#include "scronify/display_event.h"
#include "scronify/event.h"

namespace scronify {

struct ScreenHandle;

class X11Event : public DisplayEvent {
  Q_OBJECT
 public:
  explicit X11Event(QObject* parent = nullptr);

 protected:
  void run() override;
  void Connect(std::uint64_t output, const ScreenHandle& handle);
  void Removed(std::uint64_t output, const ScreenHandle& handle);

  // X11-specific helper to update cache and extract metadata from
  // the provided ScreenHandle.
  void UpdateCache(std::uint64_t output, EventType type,
                   const ScreenHandle* handle = nullptr);
};

}  // namespace scronify
