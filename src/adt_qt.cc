/**
 * @file adt_qt.cc
 * @brief Minimal Qt-based entry for ADT.
 */

#include <QApplication>
#include <QLabel>

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  QLabel label("ADT Qt version");
  label.show();
  return app.exec();
}
