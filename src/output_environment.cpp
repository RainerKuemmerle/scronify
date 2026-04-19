#include "scronify/output_environment.h"

namespace scronify {

QProcessEnvironment OutputEnvironment::FromOutputConnections(
    const QString& action_name, const std::vector<OutputConnection>& outputs) {
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("SCRONIFY_ACTION", action_name);
  env.insert("SCRONIFY_OUTPUT_COUNT", QString::number(outputs.size()));

  for (int index = 0; index < static_cast<int>(outputs.size()); ++index) {
    const OutputConnection& output = outputs[index];
    const QString prefix = QStringLiteral("SCRONIFY_OUTPUT_%1_").arg(index);

    env.insert(prefix + "NAME", output.output_name);
    env.insert(prefix + "DESCRIPTION", output.description);
    env.insert(prefix + "WIDTH", QString::number(output.width));
    env.insert(prefix + "HEIGHT", QString::number(output.height));
    env.insert(prefix + "VENDOR", output.vendor);
    env.insert(prefix + "PRODUCT", output.product);
    env.insert(prefix + "SERIAL", output.serial);
  }

  return env;
}

}  // namespace scronify
