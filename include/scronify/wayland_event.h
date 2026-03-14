#pragma once

#include <qmutex.h>
#include <qobject.h>
#include <qthread.h>
#include <qtimer.h>

#include <cstdint>
#include <unordered_map>

#include "scronify/event.h"

namespace scronify {

class WaylandEvent : public QThread {
  Q_OBJECT
 public:
  explicit WaylandEvent(QObject* parent = nullptr);

 signals:
  void ScreenAdded();
  void ScreenRemoved();

  // Public helpers used by Wayland listeners to update state.
 public:
  void Connect(std::uint64_t output);
  void Removed(std::uint64_t output);
  void UpdateMetadata(std::uint64_t output, const OutputConnection& conn);

 protected:
  std::unordered_map<std::uint64_t, Event> cached_output_;
  QMutex cached_output_mutex_;

  std::unordered_map<std::uint64_t, int> debounced_event_;
  QMutex debounced_event_mutex_;

  void run() override;

  void UpdateCache(std::uint64_t output, EventType type);
  void SetupDebounce(std::uint64_t output);
  void Debounced(std::uint64_t output);
  void TickDebounce();
};

}  // namespace scronify
