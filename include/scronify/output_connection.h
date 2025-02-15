#pragma once

#include <qobject.h>

namespace scronify {

struct OutputConnection {
  QString output_name;
  QString vendor;
  QString product;
  QString serial;
  int height = -1;
  int width = -1;
};

}  // namespace scronify
