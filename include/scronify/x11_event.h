#pragma once

#include <qthread.h>

#include <cstdint>
#include <unordered_map>

#include "scronify/output_connection.h"

namespace scronify {

struct ScreenHandle;

class X11Event : public QThread {
  Q_OBJECT
 public:
  explicit X11Event(QObject* parent = nullptr);

 signals:
  void ScreenAdded();
  void ScreenRemoved();

 protected:
  std::unordered_map<std::uint64_t, OutputConnection> cached_output_;

  void run() override;
  void Connect(std::uint64_t output, const ScreenHandle& handle);
  void Removed(std::uint64_t output, const ScreenHandle& handle);
};

}  // namespace scronify
