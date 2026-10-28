/**
 * @file adt_help.h
 * @brief Help menu, guide window, and About dialog for the Qt ADT.
 *
 * Follows the Help menus of the companion Qt applications (Qt StripTool,
 * QtProbe, QtEDM, and QtALH).
 *
 * @author Robert Soliday
 * @copyright
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 * Distributed subject to a Software License Agreement found in the file LICENSE that is included with this distribution.
 */

#ifndef ADT_HELP_H
#define ADT_HELP_H

#include <QString>
#include <QUrl>

class QDialog;
class QMenu;
class QWidget;

namespace adt {
/* One reusable, modeless guide per owning window. */
QDialog *showHelp(QWidget *parent, const QString &topic = QString());
void showAbout(QWidget *parent);
QUrl helpManualUrl();
bool openHelpManual(QWidget *parent);
void populateHelpMenu(QMenu *menu, QWidget *owner);
} // namespace adt

#endif
