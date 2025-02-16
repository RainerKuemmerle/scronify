#include "scronify/event.h"

#include <qobject.h>

#include "scronify/moc_event.cpp"  // NOLINT

namespace scronify {

Event::Event(QObject* parent)
    : QObject(parent), timer(new QTimer(this)), timer_connected(false) {
  timer->setSingleShot(true);
}

}  // namespace scronify
