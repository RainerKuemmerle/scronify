#include <qapplication.h>
#include <qcoreapplication.h>
#include <qlogging.h>
#include <qobject.h>

#include <QThread>

#include "scronify/screen_handler.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName("Scronify");
  // QCoreApplication::setApplicationVersion(SCRONIFY_VERSION);

  scronify::ScreenHandler screen_handler;

  // Signals
  QObject::connect(&app, SIGNAL(screenAdded(QScreen*)), &screen_handler,
                   SLOT(ScreenAdded(QScreen*)));
  QObject::connect(&app, SIGNAL(screenRemoved(QScreen*)), &screen_handler,
                   SLOT(ScreenRemoved(QScreen*)));

  qDebug() << "Entering main loop";
  return QApplication::exec();
}
