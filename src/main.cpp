#include <qapplication.h>
#include <qcoreapplication.h>
#include <qlogging.h>
#include <qobject.h>

#include <QThread>

#include "scronify/screen_handler.h"

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  QCoreApplication::setApplicationName("Scronify");
  // QCoreApplication::setApplicationVersion(SCRONIFY_VERSION);

  scronify::ScreenHandler screen_handler;

  QObject::connect(&app, SIGNAL(screenAdded(QScreen*)), &screen_handler,
                   SLOT(ScreenAdded(QScreen*)));
  QObject::connect(&app, SIGNAL(screenRemoved(QScreen*)), &screen_handler,
                   SLOT(ScreenRemoved(QScreen*)));

  qDebug() << "Entering main loop";
  while (true) {
    QGuiApplication::processEvents();
    QThread::usleep(100000);
  }

  return QApplication::exec();
}
