#include "scronify/action_widget.h"

#include <qboxlayout.h>
#include <qfont.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qplaintextedit.h>
#include <qspinbox.h>

#include "scronify/moc_action_widget.cpp"  // NOLINT

namespace scronify {

ActionWidget::ActionWidget(const QString& title, QWidget* parent,
                           Action* action)
    : QGroupBox(title, parent),
      action_(action),
      delay_spinner_(new QDoubleSpinBox(this)),
      command_edit_(new QPlainTextEdit(this)) {
  delay_spinner_->setMinimum(0.);
  delay_spinner_->setMaximum(10.);  // NOLINT

  command_edit_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel(tr("Delay")));
  layout->addWidget(delay_spinner_);
  layout->addWidget(new QLabel(tr("Commands")));
  layout->addWidget(command_edit_);
  setLayout(layout);
}

void ActionWidget::UpdateWidget() {
  delay_spinner_->setValue(action_->delay);
  command_edit_->setPlainText(action_->commands.join('\n'));
}

void ActionWidget::UpdateAction() {
  action_->delay = delay_spinner_->value();
  action_->commands = command_edit_->toPlainText().split('\n');
}

}  // namespace scronify
