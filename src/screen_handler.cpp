#include "scronify/screen_handler.h"

#include <QtConcurrent/qtconcurrentrun.h>
#include <qaction.h>
#include <qapplication.h>
#include <qcoreapplication.h>
#include <qdebug.h>
#include <qdialog.h>
#include <qlabel.h>
#include <qlineedit.h>
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
#include "scronify/display_event.h"
#include "scronify/lid_event.h"
#include "scronify/moc_screen_handler.cpp"  // NOLINT
#include "scronify/output_environment.h"
#include "scronify/split_command.h"
#if defined(HAVE_WAYLAND)
#include "scronify/wayland_event.h"
#endif
#if defined(HAVE_X11)
#include "scronify/x11_event.h"
#endif

namespace {
constexpr double kSecToMs = 1000.;
const QString kSettingsOrg = "scronify";
const QString kSettingsApp = "scronify";

bool IsWayland() {
  const QString platform = QGuiApplication::platformName().toLower();
  return platform.contains("wayland");
}
}  // namespace

namespace scronify {

ScreenHandler::ScreenHandler(QWidget* parent, Qt::WindowFlags f)
    : QDialog(parent, f) {
  setWindowIcon(QIcon(":/doc/scronify-icon.png"));
  setModal(true);
  CreateTrayIcon();
  CreateWidgets();

  ReadSettings();

  qDebug() << "Execute Startup";
  Run(startup_, QStringLiteral("Startup"));

  // Signals
  // Choose backend based on platform; prefer Wayland when available.
  if (IsWayland()) {
#if defined(HAVE_WAYLAND)
    event_ = new WaylandEvent(this);
    qDebug() << "Using WaylandEvent backend";
#else
    qWarning() << "Wayland support not available at build time.";
#endif
  } else {
#if defined(HAVE_X11)
    event_ = new X11Event(this);
    qDebug() << "Using X11Event backend";
#else
    qWarning() << "X11 support not available at build time.";
#endif
  }

  if (event_) {
    connect(event_, SIGNAL(ScreenAdded()), this, SLOT(ScreenAdded()));
    connect(event_, SIGNAL(ScreenRemoved()), this, SLOT(ScreenRemoved()));
    event_->start();
  }

  lid_event_ = new LidEvent(this);
  connect(lid_event_, SIGNAL(LidClosed()), this, SLOT(LidChanged()));
  connect(lid_event_, SIGNAL(LidOpened()), this, SLOT(LidChanged()));
  connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(AppQuit()));
}

void ScreenHandler::AppQuit() {
  qDebug() << "About to quit, waiting for thread";
  if (event_) {
    event_->Shutdown();
  }
  if (lid_event_) {
    lid_event_->Shutdown();
  }
  qDebug() << "Byebye";
}

void ScreenHandler::ScreenAdded() {
  qDebug() << "Added Screen";
  if (paused_) {
    qDebug() << "Paused; skipping Added Screen actions";
    return;
  }
  Run(connect_, QStringLiteral("Connect"));
}

void ScreenHandler::ScreenRemoved() {
  qDebug() << "Removed Screen";
  if (paused_) {
    qDebug() << "Paused; skipping Removed Screen actions";
    return;
  }
  Run(remove_, QStringLiteral("Remove"));
}

void ScreenHandler::LidChanged() {
  qDebug() << "Lid toggled";
  if (paused_) {
    qDebug() << "Paused; skipping Lid actions";
    return;
  }
  Run(lid_changed_, QStringLiteral("Lid"));
}

void ScreenHandler::CreateWidgets() {
  startup_widget_ = new ActionWidget(nullptr, &startup_);
  connect_widget_ = new ActionWidget(nullptr, &connect_);
  remove_widget_ = new ActionWidget(nullptr, &remove_);
  lid_changed_widget_ = new ActionWidget(nullptr, &lid_changed_);

  auto* tab_widget = new QTabWidget(this);
  tab_widget->addTab(startup_widget_, tr("Startup"));
  tab_widget->addTab(connect_widget_, tr("Connect Screen"));
  tab_widget->addTab(remove_widget_, tr("Remove Screen"));
  tab_widget->addTab(lid_changed_widget_, tr("Lid Opened/Closed"));

  // Settings tab: allow overriding the default screen tool command
  auto* settings_tab = new QWidget(this);
  auto* settings_layout = new QVBoxLayout(settings_tab);
  auto* settings_label = new QLabel(
      tr("Screen tool command (leave empty for default):"), settings_tab);
  screen_tool_edit_ = new QLineEdit(settings_tab);
  screen_tool_edit_->setPlaceholderText(tr("wdisplays  OR  arandr"));
  settings_layout->addWidget(settings_label);
  settings_layout->addWidget(screen_tool_edit_);
  settings_layout->addStretch();
  tab_widget->addTab(settings_tab, tr("Settings"));

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
          [this]() { RunInstant(startup_, QStringLiteral("Startup")); });
  show_action_ = new QAction(tr("&Configuration"), this);
  connect(show_action_, &QAction::triggered, this, &ScreenHandler::show);
  auto* screen_tool_action = new QAction(tr("&Screen Tool"), this);
  connect(screen_tool_action, &QAction::triggered, this,
          &ScreenHandler::LaunchScreenTool);
  pause_action_ = new QAction(tr("&Pause"), this);
  pause_action_->setCheckable(true);
  connect(pause_action_, &QAction::toggled, this, &ScreenHandler::PauseToggled);

  // Menu
  tray_icon_menu_ = new QMenu(this);
  tray_icon_menu_->addAction(rerun_startup_action_);
  tray_icon_menu_->addAction(show_action_);
  tray_icon_menu_->addAction(screen_tool_action);
  tray_icon_menu_->addAction(pause_action_);
  tray_icon_menu_->addSeparator();
  // TODO(Rainer): Add about dialog
  tray_icon_menu_->addAction(quit_action_);

  tray_icon_ = new QSystemTrayIcon(this);
  tray_icon_->setContextMenu(tray_icon_menu_);
  tray_icon_->setIcon(QIcon(":/doc/scronify-icon.png"));
  tray_icon_->show();
}

void ScreenHandler::LaunchScreenTool() {
  QString cmd;
  if (screen_tool_edit_) {
    cmd = screen_tool_edit_->text().trimmed();
  }
  if (cmd.isEmpty()) {
    cmd = IsWayland() ? "wdisplays" : "arandr";
  }
  QStringList command_list = util::SplitCommand(cmd);
  if (command_list.isEmpty()) {
    qDebug() << "Empty screen tool command.";
    return;
  }
  const QString program = command_list.first();
  command_list.removeFirst();
  bool ok = QProcess::startDetached(program, command_list);
  qDebug() << "Launched screen tool" << program << command_list << "ok:" << ok;
}

void ScreenHandler::PauseToggled(bool checked) {
  paused_ = checked;
  qDebug() << (paused_ ? "Paused; skipping screen events"
                       : "Resumed; handling screen events");
}

void ScreenHandler::Run(const Action& action, const QString& action_name) {
  auto run_helper = [this, action, action_name] {
    RunInstant(action, action_name);
  };

  if (action.commands.isEmpty()) {
    return;
  }
  if (action.delay > 0) {
    QTimer::singleShot(action.delay * kSecToMs, this, run_helper);
  } else {
    run_helper();
  }
}

void ScreenHandler::RunInstant(const Action& action,
                               const QString& action_name) {
  if (action.commands.isEmpty()) {
    qDebug() << "Empty command.";
    return;
  }

  const QProcessEnvironment env =
      event_ ? OutputEnvironment::FromOutputConnections(
                   action_name, event_->OutputConnections())
             : OutputEnvironment::FromOutputConnections(action_name, {});

  // Debug the environment variables being passed to the commands.
  qDebug() << "Environment for action" << action_name << ":";
  for (const QString& var : env.keys()) {
    if (!var.startsWith(QStringLiteral("SCRONIFY_"))) {
      continue;
    }
    qDebug() << "  " << var << "=" << env.value(var);
  }

  for (const auto& cmd : action.commands) {
    if (cmd.isEmpty()) {
      continue;
    }
    qDebug() << "Running " << cmd;
    auto run_process = [cmd, env] {
      QStringList command_list = util::SplitCommand(cmd);
      if (command_list.isEmpty()) {
        return;
      }
      const QString command = command_list.first();
      command_list.removeFirst();
      QProcess process;
      process.setProcessEnvironment(env);
      process.start(command, command_list);
      process.waitForFinished();
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
  lid_changed_widget_->UpdateAction();
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
    lid_changed_widget_->UpdateWidget();
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
  read_action_config("Lid", lid_changed_);
  settings.beginGroup("Settings");
  if (screen_tool_edit_) {
    screen_tool_edit_->setText(
        settings.value("screen_tool", QString()).toString());
  }
  settings.endGroup();
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
  write_action_config("Lid", lid_changed_);
  settings.beginGroup("Settings");
  if (screen_tool_edit_) {
    settings.setValue("screen_tool", screen_tool_edit_->text());
  }
  settings.endGroup();
}

}  // namespace scronify
