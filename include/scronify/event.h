#pragma once

#include <qobject.h>
#include <qtimer.h>

#include "scronify/output_connection.h"

namespace scronify {

enum class EventType : std::uint8_t {
  kConnected,
  kRemoved,
  kUnknown,
};

struct Event {
  EventType type = EventType::kUnknown;
  OutputConnection connection;
};

}  // namespace scronify
