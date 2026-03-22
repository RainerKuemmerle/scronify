#pragma once

#include <qmutex.h>
#include <qobject.h>
#include <qthread.h>

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "scronify/event.h"

namespace scronify {

class DisplayEvent : public QThread {
  Q_OBJECT
 public:
  explicit DisplayEvent(QObject* parent = nullptr) : QThread(parent) {}
  ~DisplayEvent() override = default;

 signals:
  void ScreenAdded();
  void ScreenRemoved();

 public:
  // Helper to cleanly stop the thread.
  void Shutdown() {
    requestInterruption();
    wait();
  }

 protected:
  void run() override = 0;

  std::unordered_map<std::uint64_t, Event> cached_output_;
  QMutex cached_output_mutex_;

  std::unordered_map<std::uint64_t, int> debounced_event_;
  QMutex debounced_event_mutex_;

  void UpdateCache(std::uint64_t output, EventType type,
                   const std::function<void(Event&)>& modifier = {});
  void SetupDebounce(std::uint64_t output);
  void Debounced(std::uint64_t output);
  void TickDebounce();
};

}  // namespace scronify
