#include <qapplication.h>
#include <qcoreapplication.h>
#include <qdebug.h>
#include <qobject.h>
#include <qsettings.h>

#include <QThread>

#include "scronify/config.h"
#include "scronify/screen_handler.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName("Scronify");
  QCoreApplication::setApplicationVersion(SCRONIFY_VERSION);

  QSettings::setDefaultFormat(QSettings::IniFormat);

  scronify::ScreenHandler screen_handler;

  qDebug() << "Entering main loop";
  return QApplication::exec();
}
