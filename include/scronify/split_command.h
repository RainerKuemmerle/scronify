#pragma once

#include <qstring.h>
#include <qstringlist.h>

namespace scronify::util {
QStringList SplitCommand(const QString& command);
}
