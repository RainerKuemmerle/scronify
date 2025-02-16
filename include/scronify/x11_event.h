#pragma once

#include <qmutex.h>
#include <qobject.h>
#include <qthread.h>
#include <qtimer.h>

#include <cstdint>
#include <unordered_map>

#include "scronify/event.h"

namespace scronify {

struct ScreenHandle;

class X11Event : public QThread {
  Q_OBJECT
 public:
  explicit X11Event(QObject* parent = nullptr);

 signals:
  void ScreenAdded();
  void ScreenRemoved();

 protected:
  std::unordered_map<std::uint64_t, Event> cached_output_;
  QMutex cached_output_mutex_;

  std::unordered_map<std::uint64_t, int> debounced_event_;
  QMutex debounced_event_mutex_;

  void run() override;
  void Connect(std::uint64_t output, const ScreenHandle& handle);
  void Removed(std::uint64_t output, const ScreenHandle& handle);

  void SetupDebounce(std::uint64_t output);
  void Debounced(std::uint64_t output);
  void TickDebounce();
};

}  // namespace scronify
