#pragma once

#include <qprocess.h>

#include <QString>
#include <vector>

#include "scronify/output_connection.h"

namespace scronify {

class OutputEnvironment {
 public:
  static QProcessEnvironment FromOutputConnections(
      const QString& action_name, const std::vector<OutputConnection>& outputs);
};

}  // namespace scronify
