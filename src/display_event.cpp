#include "scronify/display_event.h"

#include <qdebug.h>

#include <vector>

namespace {
constexpr int kDebounceTicks = 2;
}  // namespace

namespace scronify {

void DisplayEvent::UpdateCache(std::uint64_t output, EventType type,
                               const std::function<void(Event&)>& modifier) {
  QMutexLocker locker(&cached_output_mutex_);
  auto& evt = cached_output_[output];
  evt.type = type;
  if (modifier) {
    modifier(evt);
  }
  SetupDebounce(output);
}

void DisplayEvent::SetupDebounce(std::uint64_t output) {
  QMutexLocker locker(&debounced_event_mutex_);
  debounced_event_[output] = kDebounceTicks;
}

void DisplayEvent::TickDebounce() {
  QMutexLocker locker(&debounced_event_mutex_);
  if (debounced_event_.empty()) {
    return;
  }

  std::vector<std::uint64_t> debounced;
  for (auto& p : debounced_event_) {
    p.second--;
    if (p.second <= 0) {
      debounced.push_back(p.first);
      Debounced(p.first);
    }
  }

  for (auto output : debounced) {
    debounced_event_.erase(output);
  }
}

void DisplayEvent::Debounced(std::uint64_t output) {
  QMutexLocker locker(&cached_output_mutex_);

  auto found_it = cached_output_.find(output);
  if (found_it == cached_output_.end()) {
    qCritical() << "Logic error: Unable to find output's cache";
    return;
  }

  switch (found_it->second.type) {
    case EventType::kConnected:
      emit ScreenAdded();
      break;
    case EventType::kRemoved:
      emit ScreenRemoved();
      break;
    case EventType::kUnknown:
      qCritical() << "Logic error: Unknown event type";
      break;
  }
}

}  // namespace scronify

#include "scronify/moc_display_event.cpp"  // NOLINT
