#pragma once

#include <qthread.h>

namespace scronify {

class X11Event : public QThread {
  Q_OBJECT
 public:
  explicit X11Event(QObject* parent = nullptr);

 signals:
  void ScreenAdded();
  void ScreenRemoved();

 protected:
  void run() override;
};

}  // namespace scronify
