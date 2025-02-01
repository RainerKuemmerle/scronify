#pragma once
#include <qobject.h>
#include <qtextedit.h>

#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QPlainTextEdit>

#include "scronify/action.h"

namespace scronify {

class ActionWidget : public QGroupBox {
  Q_OBJECT
 public:
  explicit ActionWidget(const QString& title, QWidget* parent, Action* action);

  void UpdateWidget();
  void UpdateAction();

 protected:
  Action* action_;
  QDoubleSpinBox* delay_spinner_;
  QPlainTextEdit* command_edit_;
};

}  // namespace scronify
