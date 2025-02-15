#pragma once

#include <qaction.h>
#include <qdialog.h>
#include <qmenu.h>
#include <qscreen.h>
#include <qsystemtrayicon.h>

#include "scronify/action.h"
#include "scronify/action_widget.h"
#include "scronify/x11_event.h"

namespace scronify {

class X11Event;

class ScreenHandler : public QDialog {
  Q_OBJECT
 public:
  explicit ScreenHandler(QWidget* parent = nullptr,
                         Qt::WindowFlags f = Qt::WindowFlags());

 public slots:  // NOLINT
  void ScreenAdded();
  void ScreenRemoved();
  void AppQuit();

 protected:
  void closeEvent(QCloseEvent* e) override;
  void showEvent(QShowEvent* e) override;

  void CreateTrayIcon();
  void CreateWidgets();

  void Run(const Action& action);
  static void RunInstant(const Action& action);

  void ReadSettings();
  void WriteSettings();

  QAction* quit_action_ = nullptr;
  QAction* rerun_startup_action_ = nullptr;
  QAction* show_action_ = nullptr;

  QMenu* tray_icon_menu_ = nullptr;
  QSystemTrayIcon* tray_icon_ = nullptr;

  Action startup_;
  ActionWidget* startup_widget_ = nullptr;

  Action connect_;
  ActionWidget* connect_widget_ = nullptr;

  Action remove_;
  ActionWidget* remove_widget_ = nullptr;

  X11Event* x11event_ = nullptr;
};

}  // namespace scronify
