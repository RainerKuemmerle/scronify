#pragma once

#include <qobject.h>
#include <qthread.h>

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
};

}  // namespace scronify
