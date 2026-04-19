#pragma once

#include <qaction.h>
#include <qdialog.h>
#include <qlineedit.h>
#include <qmenu.h>
#include <qscreen.h>
#include <qsystemtrayicon.h>

#include "scronify/action.h"
#include "scronify/action_widget.h"
#include "scronify/display_event.h"

namespace scronify {

class DisplayEvent;
class LidEvent;

class ScreenHandler : public QDialog {
  Q_OBJECT
 public:
  explicit ScreenHandler(QWidget* parent = nullptr,
                         Qt::WindowFlags f = Qt::WindowFlags());

 public slots:  // NOLINT
  void ScreenAdded();
  void ScreenRemoved();
  void LidChanged();
  void AppQuit();
  void LaunchScreenTool();
  void PauseToggled(bool checked);

 protected:
  void closeEvent(QCloseEvent* e) override;
  void showEvent(QShowEvent* e) override;

  void CreateTrayIcon();
  void CreateWidgets();

  void Run(const Action& action, const QString& action_name = QString());
  void RunInstant(const Action& action, const QString& action_name = QString());

  void ReadSettings();
  void WriteSettings();

  QAction* quit_action_ = nullptr;
  QAction* rerun_startup_action_ = nullptr;
  QAction* show_action_ = nullptr;
  QAction* pause_action_ = nullptr;

  QMenu* tray_icon_menu_ = nullptr;
  QSystemTrayIcon* tray_icon_ = nullptr;

  Action startup_;
  ActionWidget* startup_widget_ = nullptr;

  Action connect_;
  ActionWidget* connect_widget_ = nullptr;

  Action remove_;
  ActionWidget* remove_widget_ = nullptr;

  Action lid_changed_;
  ActionWidget* lid_changed_widget_ = nullptr;

  QLineEdit* screen_tool_edit_ = nullptr;

  DisplayEvent* event_ = nullptr;
  LidEvent* lid_event_ = nullptr;
  bool paused_ = false;
};

}  // namespace scronify
