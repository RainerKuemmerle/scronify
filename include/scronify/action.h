#pragma once

#include <qstringlist.h>

namespace scronify {

class Action {
 public:
  Action() = default;

  QStringList commands;
  double delay = 0.;
};

}  // namespace scronify
