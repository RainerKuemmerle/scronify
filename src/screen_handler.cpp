#include "scronify/screen_handler.h"

#include <qdebug.h>
#include <qscreen.h>

#include <iostream>

#include "scronify/moc_screen_handler.cpp"  // NOLINT

namespace scronify {

ScreenHandler::ScreenHandler(QObject* parent) : QObject(parent) {}

void ScreenHandler::ScreenAdded(QScreen* screen) {
  qDebug() << "Added " << screen->manufacturer() << " " << screen->model();
}
void ScreenHandler::ScreenRemoved(QScreen* screen) {
  qDebug() << "Removed " << screen->manufacturer() << " " << screen->model();
}

}  // namespace scronify
