#include "scronify/screen_handler.h"

#include <QtConcurrent/qtconcurrentrun.h>
#include <qaction.h>
#include <qapplication.h>
#include <qdebug.h>
#include <qdialog.h>
#include <qlogging.h>
#include <qmenu.h>
#include <qprocess.h>
#include <qscreen.h>
#include <qstringlist.h>
#include <qsystemtrayicon.h>
#include <qtimer.h>

#include "scronify/moc_screen_handler.cpp"  // NOLINT

namespace {
constexpr double kSecToMs = 1000.;
const QString kShell = "/bin/sh";
}  // namespace

namespace scronify {

ScreenHandler::ScreenHandler(QWidget* parent, Qt::WindowFlags f)
    : QDialog(parent, f) {
  CreateTrayIcon();

  startup_.command = R"(notify-send "Hello World")";
  startup_.delay = 2;

  // TODO(Rainer): Read configuration file

  qDebug() << "Execute Startup";
  Run(startup_);
}

void ScreenHandler::ScreenAdded(QScreen* screen) {
  qDebug() << "Added " << screen->manufacturer() << " " << screen->model();
  Run(connect_);
}

void ScreenHandler::ScreenRemoved(QScreen* screen) {
  qDebug() << "Removed " << screen->manufacturer() << " " << screen->model();
  Run(remove_);
}

void ScreenHandler::CreateTrayIcon() {
  // Actions
  quit_action_ = new QAction(tr("&Quit"), this);
  connect(quit_action_, &QAction::triggered, qApp, &QCoreApplication::quit);
  rerun_startup_action_ = new QAction(tr("&Rerun Startup"), this);
  connect(rerun_startup_action_, &QAction::triggered, this,
          [this]() { RunInstant(startup_); });

  // Menu
  tray_icon_menu_ = new QMenu(this);
  tray_icon_menu_->addAction(rerun_startup_action_);
  tray_icon_menu_->addSeparator();
  // TODO(Rainer): Add about dialog
  tray_icon_menu_->addAction(quit_action_);

  tray_icon_ = new QSystemTrayIcon(this);
  tray_icon_->setContextMenu(tray_icon_menu_);
  tray_icon_->show();
}

void ScreenHandler::Run(const Action& action) {
  auto run_helper = [&action] { ScreenHandler::RunInstant(action); };

  if (action.command.isEmpty()) {
    return;
  }
  if (action.delay > 0) {
    QTimer::singleShot(action.delay * kSecToMs, this, run_helper);
  } else {
    run_helper();
  }
}

void ScreenHandler::RunInstant(const Action& action) {
  qDebug() << "Running " << action.command;
  if (action.command.isEmpty()) {
    qDebug() << "Empty command.";
    return;
  }
  auto run_process = [&action] {
    QStringList command_list = QProcess::splitCommand(action.command);
    const QString command = command_list.first();
    command_list.removeFirst();
    QProcess::execute(command, command_list);
  };
  QFuture<void> future = QtConcurrent::run(run_process);
  qDebug() << "Done.";
}

}  // namespace scronify
