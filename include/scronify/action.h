#pragma once

#include <qstring.h>

namespace scronify {

class Action {
 public:
  Action() = default;

  QString command;
  double delay = 0.;
};

}  // namespace scronify
