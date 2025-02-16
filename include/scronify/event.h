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

class Event : public QObject {
  Q_OBJECT
 public:
  explicit Event(QObject* parent = nullptr);
  EventType type = EventType::kUnknown;
  QTimer* timer;
  bool timer_connected;
  OutputConnection connection;
};

}  // namespace scronify
