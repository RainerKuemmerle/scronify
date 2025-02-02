#include "scronify/split_command.h"

#include <qstring.h>
#include <qstringlist.h>

namespace scronify::util {

/**
 * Splits the string \a command into a list of tokens, and returns
 * the list.
 *
 * Tokens with spaces can be surrounded by double quotes; three
 * consecutive double quotes represent the quote character itself.
 *
 * Original code:
 * https://codebrowser.dev/qt6/qtbase/src/corelib/io/qprocess.cpp.html
 * Here to support Ubuntu Focal with older Qt 5.
 */
QStringList SplitCommand(const QString& command) {
  // TODO(Rainer): Replace with QProcess::splitCommand after Ubuntu Focal
  // transition
  QStringList args;
  QString tmp;
  int quote_count = 0;
  bool in_quote = false;
  // handle quoting. tokens can be surrounded by double quotes
  // "hello world". three consecutive double quotes represent
  // the quote character itself.
  for (int i = 0; i < command.size(); ++i) {
    if (command.at(i) == u'"') {
      ++quote_count;
      if (quote_count == 3) {
        // third consecutive quote
        quote_count = 0;
        tmp += command.at(i);
      }
      continue;
    }
    if (quote_count) {
      if (quote_count == 1) {
        in_quote = !in_quote;
      }
      quote_count = 0;
    }
    if (!in_quote && command.at(i).isSpace()) {
      if (!tmp.isEmpty()) {
        args += tmp;
        tmp.clear();
      }
    } else {
      tmp += command.at(i);
    }
  }
  if (!tmp.isEmpty()) {
    args += tmp;
  }
  return args;
}

}  // namespace scronify::util
