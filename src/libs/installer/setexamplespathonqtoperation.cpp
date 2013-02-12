/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "setexamplespathonqtoperation.h"

#include "qtpatch.h"

#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QDebug>

using namespace QInstaller;

SetExamplesPathOnQtOperation::SetExamplesPathOnQtOperation()
{
    setName(QLatin1String("SetExamplesPathOnQt"));
}

void SetExamplesPathOnQtOperation::backup()
{
}

bool SetExamplesPathOnQtOperation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() != 2) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("exactly 2"), QLatin1String("")));
        return false;
    }

    const QString qtDir = args.at(0);
    QByteArray newValue = QDir::toNativeSeparators(args.at(1)).toUtf8();

    QString qmakePath = qtDir + QLatin1String("/bin/qmake");
#ifdef Q_OS_WIN
    qmakePath = qmakePath + QLatin1String(".exe");
#endif

    QByteArray qmakeOutput;
    QHash<QString, QByteArray> qmakeValueHash = QtPatch::qmakeValues(qmakePath, &qmakeOutput);

    if (qmakeValueHash.isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("The output of \n'%1 -query'\nis not parseable. Please file a bugreport with this "
            "dialog at https://bugreports.qt-project.org.\noutput: %2").arg(QDir::toNativeSeparators(qmakePath),
            QString::fromUtf8(qmakeOutput)));
        return false;
    }

    QByteArray oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_EXAMPLES"));
    bool oldQtPathFromQMakeIsEmpty = oldValue.isEmpty();
    if (oldQtPathFromQMakeIsEmpty) {
        qDebug() << "qpatch: warning: It was not possible to get the old values from" << qmakePath;
    }

    if (newValue.size() >= 255) {
        setError(UserDefinedError);
        setErrorString(tr("Qt patch error: new Qt example path '%1'\nneeds to be less than 255 characters.")
            .arg(QString::fromLocal8Bit(newValue)));
        return false;
    }

    QString qtConfPath = qtDir + QLatin1String("/bin/qt.conf");

    if (QFile::exists(qtConfPath)) {
        QSettings settings(qtConfPath, QSettings::IniFormat);
        settings.setValue( QLatin1String("Paths/Examples"), QString::fromUtf8(newValue));
    }

    oldValue = QByteArray("qt_xmplpath=%1").replace("%1", oldValue);
    newValue = QByteArray("qt_xmplpath=%1").replace("%1", newValue);

    bool isPatched = QtPatch::patchBinaryFile(qmakePath, oldValue, newValue);
    if (!isPatched) {
        qDebug() << "qpatch: warning: could not patch the example path in" << qmakePath;
    }

    return true;
}

bool SetExamplesPathOnQtOperation::undoOperation()
{
    return true;
}

bool SetExamplesPathOnQtOperation::testOperation()
{
    return true;
}

Operation *SetExamplesPathOnQtOperation::clone() const
{
    return new SetExamplesPathOnQtOperation();
}

