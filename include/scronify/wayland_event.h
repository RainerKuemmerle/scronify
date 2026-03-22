#pragma once

#include <qmutex.h>
#include <qobject.h>
#include <qthread.h>
#include <qtimer.h>

#include <cstdint>

#include "scronify/display_event.h"

namespace scronify {

class WaylandEvent : public DisplayEvent {
  Q_OBJECT
 public:
  explicit WaylandEvent(QObject* parent = nullptr);

  // Public helpers used by Wayland listeners to update state.
  void Connect(std::uint64_t output);
  void Removed(std::uint64_t output);
  void UpdateMetadata(std::uint64_t output, const OutputConnection& conn);

 protected:
  void run() override;
};

}  // namespace scronify
