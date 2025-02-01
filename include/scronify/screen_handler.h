#pragma once

#include <qaction.h>
#include <qdialog.h>
#include <qmenu.h>
#include <qscreen.h>
#include <qsystemtrayicon.h>

#include "scronify/action.h"

namespace scronify {

class ScreenHandler : public QDialog {
  Q_OBJECT
 public:
  explicit ScreenHandler(QWidget* parent = nullptr,
                         Qt::WindowFlags f = Qt::WindowFlags());

 public slots:  // NOLINT
  void ScreenAdded(QScreen* screen);
  void ScreenRemoved(QScreen* screen);

 protected:
  void CreateTrayIcon();

  void Run(const Action& action);
  static void RunInstant(const Action& action);

  QAction* quit_action_ = nullptr;
  QAction* rerun_startup_action_ = nullptr;

  QMenu* tray_icon_menu_ = nullptr;
  QSystemTrayIcon* tray_icon_ = nullptr;

  Action startup_;
  Action connect_;
  Action remove_;
};

}  // namespace scronify
