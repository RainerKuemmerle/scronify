#pragma once

#include <qobject.h>
#include <qstringlist.h>
#include <qvariant.h>

namespace scronify {

class LidEvent : public QObject {
  Q_OBJECT
 public:
  explicit LidEvent(QObject* parent = nullptr);
  void Shutdown();

 signals:
  void LidClosed();
  void LidOpened();

 private:
  void Initialize();

 private slots:
  void PropertiesChanged(const QString& interface,
                         const QVariantMap& changed_properties,
                         const QStringList& invalidated_properties);

 private:  // NOLINT
  static QString FindStateFile();
  static bool ReadLidState(const QString& path, bool* closed);
  static bool QueryLidClosed(bool* closed);

  bool last_state_valid_ = false;
  bool last_closed_ = false;
};

}  // namespace scronify
