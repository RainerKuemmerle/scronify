#include "scronify/lid_event.h"

#include <qdebug.h>
#include <qdir.h>
#include <qfile.h>
#include <qstring.h>
#include <qvariant.h>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QEventLoop>

#include "scronify/moc_lid_event.cpp"  // NOLINT

namespace {
// NOLINTBEGIN
constexpr char kLogindService[] = "org.freedesktop.login1";
constexpr char kLogindPath[] = "/org/freedesktop/login1";
constexpr char kLogindManagerInterface[] = "org.freedesktop.login1.Manager";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";
constexpr char kLidClosedProperty[] = "LidClosed";
constexpr char kAcpiLidDir[] = "/proc/acpi/button/lid";
constexpr char kStateFile[] = "state";
// NOLINTEND

bool ParseLidState(const QString& data, bool* closed) {
  const auto lower = data.toLower();
  if (lower.contains("closed")) {
    *closed = true;
    return true;
  }
  if (lower.contains("open")) {
    *closed = false;
    return true;
  }
  return false;
}
}  // namespace

namespace scronify {

LidEvent::LidEvent(QObject* parent) : QObject(parent) { Initialize(); }

void LidEvent::Shutdown() {
  QDBusConnection bus = QDBusConnection::systemBus();
  if (!bus.isConnected()) {
    return;
  }

  bus.disconnect(QString::fromLatin1(kLogindService),
                 QString::fromLatin1(kLogindPath),
                 QString::fromLatin1(kPropertiesInterface),
                 QString::fromLatin1("PropertiesChanged"), this,
                 SLOT(PropertiesChanged(QString, QVariantMap, QStringList)));
}

bool LidEvent::QueryLidClosed(bool* closed) {
  const QDBusConnection bus = QDBusConnection::systemBus();
  if (!bus.isConnected()) {
    return false;
  }

  QDBusInterface login1(kLogindService, kLogindPath, kLogindManagerInterface,
                        bus);
  if (!login1.isValid()) {
    return false;
  }

  const QVariant value = login1.property(kLidClosedProperty);
  if (!value.isValid()) {
    return false;
  }

  *closed = value.toBool();
  return true;
}

void LidEvent::Initialize() {
  QDBusConnection bus = QDBusConnection::systemBus();
  bool dbus_available = bus.isConnected();
  bool dbus_subscribed = false;

  if (dbus_available) {
    qDebug() << "LidEvent: connected to system bus";
    if (QueryLidClosed(&last_closed_)) {
      last_state_valid_ = true;
      qDebug() << "LidEvent: initial lid state from logind is"
               << (last_closed_ ? "closed" : "open");
    }

    dbus_subscribed = bus.connect(
        QString::fromLatin1(kLogindService), QString::fromLatin1(kLogindPath),
        QString::fromLatin1(kPropertiesInterface),
        QString::fromLatin1("PropertiesChanged"), this,
        SLOT(PropertiesChanged(QString, QVariantMap, QStringList)));
    if (!dbus_subscribed) {
      qWarning() << "LidEvent: failed to subscribe to logind PropertiesChanged";
    }
  } else {
    qWarning() << "LidEvent: system bus unavailable";
  }

  if (!last_state_valid_) {
    const QString state_file = FindStateFile();
    if (!state_file.isEmpty()) {
      bool closed = false;
      if (ReadLidState(state_file, &closed)) {
        last_closed_ = closed;
        last_state_valid_ = true;
        qDebug() << "LidEvent: initial lid state from file" << state_file
                 << (closed ? "closed" : "open");
      } else {
        qWarning() << "LidEvent: failed to read initial lid state from"
                   << state_file;
      }
    }
  }

  if (!dbus_subscribed) {
    qWarning() << "LidEvent: DBus lid monitoring unavailable; will not emit "
                  "lid change events";
  }
}

void LidEvent::PropertiesChanged(
    const QString& interface, const QVariantMap& changed_properties,
    const QStringList& /*invalidated_properties*/) {
  if (interface != QString::fromLatin1(kLogindManagerInterface)) {
    return;
  }

  if (!changed_properties.contains(QString::fromLatin1(kLidClosedProperty))) {
    return;
  }

  const bool closed =
      changed_properties.value(QString::fromLatin1(kLidClosedProperty))
          .toBool();

  if (!last_state_valid_) {
    last_closed_ = closed;
    last_state_valid_ = true;
    return;
  }

  if (last_closed_ == closed) {
    return;
  }

  last_closed_ = closed;
  if (closed) {
    emit LidClosed();
  } else {
    emit LidOpened();
  }
}

QString LidEvent::FindStateFile() {
  const QDir acpi_dir(QString::fromLatin1(kAcpiLidDir));
  if (!acpi_dir.exists()) {
    return {};
  }

  const auto entries = acpi_dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QString& entry : entries) {
    const QString candidate =
        acpi_dir.filePath(entry + "/" + QString::fromLatin1(kStateFile));
    if (QFile::exists(candidate)) {
      return candidate;
    }
  }

  QString legacy = acpi_dir.filePath(QString::fromLatin1(kStateFile));
  if (QFile::exists(legacy)) {
    return legacy;
  }

  return {};
}

bool LidEvent::ReadLidState(const QString& path, bool* closed) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  const QString contents = QString::fromUtf8(file.readAll());
  return ParseLidState(contents, closed);
}

}  // namespace scronify
