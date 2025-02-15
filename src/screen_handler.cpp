#include "scronify/screen_handler.h"

#include <QtConcurrent/qtconcurrentrun.h>
#include <qaction.h>
#include <qapplication.h>
#include <qcoreapplication.h>
#include <qdebug.h>
#include <qdialog.h>
#include <qlogging.h>
#include <qmenu.h>
#include <qobjectdefs.h>
#include <qprocess.h>
#include <qscreen.h>
#include <qsettings.h>
#include <qstringlist.h>
#include <qsystemtrayicon.h>
#include <qtabwidget.h>
#include <qtimer.h>

#include <QCloseEvent>
#include <QVBoxLayout>

#include "scronify/action.h"
#include "scronify/action_widget.h"
#include "scronify/moc_screen_handler.cpp"  // NOLINT
#include "scronify/split_command.h"
#include "scronify/x11_event.h"

namespace {
constexpr double kSecToMs = 1000.;
const QString kSettingsOrg = "scronify";
const QString kSettingsApp = "scronify";
}  // namespace

namespace scronify {

ScreenHandler::ScreenHandler(QWidget* parent, Qt::WindowFlags f)
    : QDialog(parent, f), x11event_(new X11Event(this)) {
  setWindowIcon(QIcon(":/doc/scronify-icon.png"));
  setModal(true);
  CreateTrayIcon();
  CreateWidgets();

  ReadSettings();

  qDebug() << "Execute Startup";
  Run(startup_);

  // Signals
  connect(x11event_, SIGNAL(ScreenAdded()), this, SLOT(ScreenAdded()));
  connect(x11event_, SIGNAL(ScreenRemoved()), this, SLOT(ScreenRemoved()));
  connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(AppQuit()));

  x11event_->start();
}

void ScreenHandler::AppQuit() {
  qDebug() << "About to quit";
  x11event_->requestInterruption();
}

void ScreenHandler::ScreenAdded() {
  qDebug() << "Added Screen";
  Run(connect_);
}

void ScreenHandler::ScreenRemoved() {
  qDebug() << "Removed Screen";
  Run(remove_);
}

void ScreenHandler::CreateWidgets() {
  startup_widget_ = new ActionWidget(nullptr, &startup_);
  connect_widget_ = new ActionWidget(nullptr, &connect_);
  remove_widget_ = new ActionWidget(nullptr, &remove_);

  auto* tab_widget = new QTabWidget(this);
  tab_widget->addTab(startup_widget_, tr("Startup"));
  tab_widget->addTab(connect_widget_, tr("Connect Screen"));
  tab_widget->addTab(remove_widget_, tr("Remove Screen"));

  auto* main_layout = new QVBoxLayout(this);
  main_layout->addWidget(tab_widget);
  setLayout(main_layout);
}

void ScreenHandler::CreateTrayIcon() {
  // Actions
  quit_action_ = new QAction(tr("&Quit"), this);
  connect(quit_action_, &QAction::triggered, qApp, &QCoreApplication::quit);
  rerun_startup_action_ = new QAction(tr("&Rerun Startup"), this);
  connect(rerun_startup_action_, &QAction::triggered, this,
          [this]() { RunInstant(startup_); });
  show_action_ = new QAction(tr("&Configuration"), this);
  connect(show_action_, &QAction::triggered, this, &ScreenHandler::show);

  // Menu
  tray_icon_menu_ = new QMenu(this);
  tray_icon_menu_->addAction(rerun_startup_action_);
  tray_icon_menu_->addAction(show_action_);
  tray_icon_menu_->addSeparator();
  // TODO(Rainer): Add about dialog
  // TODO(Rainer): Add run option for screen tool like arandr
  // TODO(Rainer): Add pause button to not execute actions
  tray_icon_menu_->addAction(quit_action_);

  tray_icon_ = new QSystemTrayIcon(this);
  tray_icon_->setContextMenu(tray_icon_menu_);
  tray_icon_->setIcon(QIcon(":/doc/scronify-icon.png"));
  tray_icon_->show();
}

void ScreenHandler::Run(const Action& action) {
  auto run_helper = [&action] { ScreenHandler::RunInstant(action); };

  if (action.commands.isEmpty()) {
    return;
  }
  if (action.delay > 0) {
    QTimer::singleShot(action.delay * kSecToMs, this, run_helper);
  } else {
    run_helper();
  }
}

void ScreenHandler::RunInstant(const Action& action) {
  // TODO(Rainer): Add environment variables such as action, screen info
  if (action.commands.isEmpty()) {
    qDebug() << "Empty command.";
    return;
  }

  for (const auto& cmd : action.commands) {
    if (cmd.isEmpty()) {
      continue;
    }
    qDebug() << "Running " << cmd;
    auto run_process = [&cmd] {
      QStringList command_list = util::SplitCommand(cmd);
      const QString command = command_list.first();
      command_list.removeFirst();
      QProcess::execute(command, command_list);
    };
    QFuture<void> future = QtConcurrent::run(run_process);
  }

  qDebug() << "Done.";
}

void ScreenHandler::closeEvent(QCloseEvent* e) {
  if (!e->spontaneous() || !isVisible()) {
    return;
  }
  qDebug() << "Updating Actions";
  startup_widget_->UpdateAction();
  connect_widget_->UpdateAction();
  remove_widget_->UpdateAction();
  WriteSettings();
  if (tray_icon_->isVisible()) {
    hide();
    e->ignore();
  }
}

void ScreenHandler::showEvent(QShowEvent* e) {
  if (!e->spontaneous()) {
    qDebug() << "Update Widget";
    startup_widget_->UpdateWidget();
    connect_widget_->UpdateWidget();
    remove_widget_->UpdateWidget();
  }
}

void ScreenHandler::ReadSettings() {
  QSettings settings(kSettingsOrg, kSettingsApp);

  auto read_action_config = [&settings](const QString& group, Action& action) {
    settings.beginGroup(group);
    action.delay = settings.value("delay", action.delay).toDouble();
    action.commands =
        settings.value("commands", action.commands).toStringList();
    settings.endGroup();
  };

  read_action_config("Startup", startup_);
  read_action_config("Connect", connect_);
  read_action_config("Remove", remove_);
}

void ScreenHandler::WriteSettings() {
  QSettings settings(kSettingsOrg, kSettingsApp);

  auto write_action_config = [&settings](const QString& group, Action& action) {
    settings.beginGroup(group);
    settings.setValue("delay", action.delay);
    settings.setValue("commands", action.commands);
    settings.endGroup();
  };

  write_action_config("Startup", startup_);
  write_action_config("Connect", connect_);
  write_action_config("Remove", remove_);
}

}  // namespace scronify
