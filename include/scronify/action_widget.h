#pragma once
#include <qobject.h>
#include <qtextedit.h>
#include <qwidget.h>

#include <QDoubleSpinBox>
#include <QPlainTextEdit>

#include "scronify/action.h"

namespace scronify {

class ActionWidget : public QWidget {
  Q_OBJECT
 public:
  explicit ActionWidget(QWidget* parent, Action* action);

  void UpdateWidget();
  void UpdateAction();

 protected:
  Action* action_;
  QDoubleSpinBox* delay_spinner_;
  QPlainTextEdit* command_edit_;
};

}  // namespace scronify
