/**
 * @file adt_qt.cc
 * @brief Minimal Qt-based entry for ADT.
 */

#include "aps.icon"
#include "adtVersion.h"

#include <sys/utsname.h>

#include <epicsVersion.h>
#include <QApplication>
#include <QBitmap>
#include <QImage>
#include <QFont>
#include <QFontMetrics>
#include <QLabel>
#include <QPainter>
#include <QString>
#include <QWidget>

class LogoWidget : public QWidget
{
public:
  explicit LogoWidget(QWidget *parent = nullptr) : QWidget(parent)
  {
    setMinimumSize(400, 200);
  }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    QBitmap bitmap = QBitmap::fromData(QSize(aps_width, aps_height),
      aps_bits, QImage::Format_MonoLSB);
    p.drawPixmap(50, (height() - aps_height) / 2, bitmap);

    QFont font("Monospace", 9);
    p.setFont(font);
    QFontMetrics fm(font);
    int fheight = fm.height();
    int linespacing = static_cast<int>(1.5 * fheight);
    int ystart = (height() - fheight) / 2 - fheight / 2;

    auto drawCentered = [this, &p, &fm](const QString &text, int y)
    {
      int width = fm.horizontalAdvance(text);
      int xstart = (this->width() - width) / 2;
      p.drawText(xstart, y, text);
    };

    drawCentered("Array Display Tool", ystart - linespacing);
    drawCentered("Written by Dwarfs in the Waterfall Glenn", ystart);
    drawCentered(QString("Version %1 %2").arg(ADT_VERSION_STRING)
      .arg(EPICS_VERSION_STRING), ystart + linespacing);

    struct utsname name;
    if (uname(&name) != -1) {
      drawCentered(QString("Running %1 %2 on %3")
        .arg(name.sysname)
        .arg(name.release)
        .arg(name.nodename), ystart + 2 * linespacing);
    }

    drawCentered("Please Send Comments and Bugs to soliday@anl.gov",
      ystart + 3 * linespacing);
  }
};

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  LogoWidget logo;
  logo.show();
  return app.exec();
}
