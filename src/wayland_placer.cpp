// Small CLI to enumerate Wayland outputs, compute a stacked layout
// (internal laptop display at the bottom, others stacked above centered),
// and optionally apply it via the wlr output-management protocol.
// Vibe coded by: GitHub Copilot

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QString>
#include <QTextStream>
#include <cstring>
#include <iostream>
#include <string>

#include "scronify/wayland_display.h"

namespace {

void Usage(const char* argv0) {
  std::cout << "Usage: " << argv0 << " [--primary-name NAME] [--dry-run]\n";
  std::cout
      << "  --primary-name NAME : treat output name NAME as internal bottom\n";
  std::cout << "  --dry-run           : print planned positions\n";
}

bool ReadFileContains(const QString& path, const QString& needle) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
    return false;
  }
  const QByteArray content = f.readAll();
  const bool found = content.contains(needle.toUtf8());
  f.close();
  return found;
}

bool LidIsClosed() {
  // Try common ACPI path(s)
  const QString primary = QStringLiteral("/proc/acpi/button/lid/LID/state");
  if (ReadFileContains(primary, QStringLiteral("closed"))) {
    return true;
  }
  // try any /proc/acpi/button/lid/*/state
  QDir d(QStringLiteral("/proc/acpi/button/lid"));
  if (!d.exists()) {
    return false;
  }
  const auto entries = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QString& name : entries) {
    QString path = d.filePath(name + QLatin1String("/state"));
    if (ReadFileContains(path, QStringLiteral("closed"))) {
      return true;
    }
  }
  return false;
}

int DeterminePrimaryIndex(const std::vector<OutputInfo>& outputs,
                          const std::string& primary_name_override) {
  if (!primary_name_override.empty()) {
    for (size_t i = 0; i < outputs.size(); ++i) {
      if (outputs[i].name == primary_name_override) {
        return static_cast<int>(i);
      }
    }
  }

  for (size_t i = 0; i < outputs.size(); ++i) {
    if (WaylandDisplay::IsInternalDisplay(outputs[i])) {
      return static_cast<int>(i);
    }
  }

  int best = 0;
  for (size_t i = 1; i < outputs.size(); ++i) {
    if (outputs[i].width > 0 && outputs[best].width > 0) {
      if (outputs[i].width < outputs[best].width) {
        best = static_cast<int>(i);
      }
    } else if (outputs[i].height < outputs[best].height) {
      best = static_cast<int>(i);
    }
  }
  return best;
}

std::vector<std::pair<OutputInfo, std::pair<int, int>>> ComputeStackedLayout(
    std::vector<OutputInfo> outputs, const std::string& primary_name_override,
    bool lid_closed) {
  // choose primary/internal
  int primary = DeterminePrimaryIndex(outputs, primary_name_override);

  std::vector<std::pair<OutputInfo, std::pair<int, int>>> placements;
  if (primary < 0) {
    return placements;
  }

  const OutputInfo& primary_out = outputs[primary];
  // collect others in original order except primary
  std::vector<OutputInfo> others;
  for (size_t i = 0; i < outputs.size(); ++i) {
    if (static_cast<int>(i) != primary) {
      others.push_back(outputs[i]);
    }
  }

  // compute total height of others stacked from top to just above primary
  int y = 0;
  for (auto& o : others) {
    y += o.height > 0 ? o.height : 0;
  }

  // If the lid is closed, skip stacking the first other so the first other
  // and the primary share the same vertical position (overlap primary).
  if (lid_closed && !others.empty()) {
    int first_h = others[0].height > 0 ? others[0].height : 0;
    y -= first_h;
    y = std::max(y, 0);
  }

  // primary at bottom: y position equals total height above
  int primary_x = 0;
  int primary_y = y;
  placements.push_back({primary_out, {primary_x, primary_y}});

  int cur = 0;
  for (size_t idx = 0; idx < others.size(); ++idx) {
    auto& o = others[idx];
    int ox = primary_x + ((primary_out.width - o.width) / 2);
    int oy = 0;
    if (lid_closed && idx == 0) {
      // place first other at same Y as primary (overlap)
      oy = primary_y;
    } else {
      oy = cur;
    }
    placements.push_back({o, {ox, oy}});
    if (!lid_closed || idx != 0) {
      cur += o.height > 0 ? o.height : 0;
    }
  }

  return placements;
}

}  // namespace

int main(int argc, char* argv[]) {
  bool apply = true;
  std::string primary_name;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--dry-run") == 0) {
      apply = false;
    } else if (std::strcmp(argv[i], "--primary-name") == 0 && i + 1 < argc) {
      primary_name = argv[++i];
    } else if (std::strcmp(argv[i], "--help") == 0) {
      Usage(argv[0]);
      return 0;
    }
  }

#ifndef HAVE_OUTPUT_MGMT
  std::cout << "wlr output-management support is not compiled in; "
               "rebuild with wayland-scanner and protocol XML.\n";
  return 42;
#else

  bool lid_closed = LidIsClosed();
  WaylandDisplay disp;
  if (!disp.Valid()) {
    std::cerr << "Failed to connect to Wayland display\n";
    return 1;
  }
  std::vector<OutputInfo> outputs = disp.EnumerateWlrHeads();
  if (outputs.empty()) {
    std::cerr << "No outputs found.\n";
    return 2;
  }

  auto placements = ComputeStackedLayout(outputs, primary_name, lid_closed);
  if (placements.empty()) {
    std::cerr << "Failed to compute layout\n";
    return 1;
  }

  std::cout << "Lid is " << (lid_closed ? "closed" : "open") << "\n";
  std::cout << "Planned placements:\n";
  for (auto& p : placements) {
    const auto& o = p.first;
    int x = p.second.first;
    int y = p.second.second;
    std::cout << "- id=" << o.id << " name='" << o.name << "' w=" << o.width
              << " h=" << o.height << " -> x=" << x << " y=" << y << "\n";
  }

  if (!apply) {
    return 0;
  }

  if (!disp.Apply(placements)) {
    std::cerr << "Failed to apply configuration\n";
  }
#endif

  return 0;
}
