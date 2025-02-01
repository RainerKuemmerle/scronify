#pragma once

#include <qcursor.h>
#include <qobject.h>

namespace scronify {

class ScreenHandler : public QObject {
  Q_OBJECT
 public:
  explicit ScreenHandler(QObject* parent = nullptr);

 public slots:  // NOLINT
  void ScreenAdded(QScreen* screen);
  void ScreenRemoved(QScreen* screen);

 protected:
};

}  // namespace scronify
