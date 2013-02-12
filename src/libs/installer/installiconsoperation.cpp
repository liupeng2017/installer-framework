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
#include "installiconsoperation.h"

#include "fileutils.h"
#include "packagemanagercore.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>

#if QT_VERSION >= 0x040600
#   include <QProcessEnvironment>
#else
#   include <QProcess>
#endif

using namespace QInstaller;

QString InstallIconsOperation::targetDirectory()
{
    // we're not searching for the first time, let's re-use the old value
    if (hasValue(QLatin1String("targetdirectory")))
        return value(QLatin1String("targetdirectory")).toString();

#if QT_VERSION >= 0x040600
    const QProcessEnvironment env;
    QStringList XDG_DATA_DIRS = env.value(QLatin1String("XDG_DATA_DIRS")).split(QLatin1Char(':'),
        QString::SkipEmptyParts);
#else
    QStringList XDG_DATA_DIRS;
    const QStringList env = QProcess::systemEnvironment();
    for (QStringList::const_iterator it = env.begin(); it != env.end(); ++it) {
        if (it->startsWith(QLatin1String("XDG_DATA_DIRS=")))
            XDG_DATA_DIRS = it->mid(it->indexOf(QLatin1Char('=')) + 1).split(QLatin1Char(':'));
     }
#endif

    XDG_DATA_DIRS.push_back(QLatin1String("/usr/share/pixmaps")); // default path
    XDG_DATA_DIRS.push_back(QDir::home().absoluteFilePath(QLatin1String(".local/share/icons"))); // default path
    XDG_DATA_DIRS.push_back(QDir::home().absoluteFilePath(QLatin1String(".icons"))); // default path

    QString directory;
    const QStringList& directories = XDG_DATA_DIRS;
    for (QStringList::const_iterator it = directories.begin(); it != directories.end(); ++it) {
        if (it->isEmpty())
            continue;

        // our default dirs are correct, XDG_DATA_DIRS set via env need "icon" at the end
        if ((it + 1 == directories.end()) || (it + 2 == directories.end()) || (it + 3 == directories.end()))
            directory = QDir(*it).absolutePath();
        else
            directory = QDir(*it).absoluteFilePath(QLatin1String("icons"));

        QDir dir(directory);
        // let's see if this dir exists or we're able to create it
        if (!dir.exists() && !QDir().mkpath(directory))
            continue;

        // we just try if we're able to open the file in ReadWrite
        QFile file(QDir(directory).absoluteFilePath(QLatin1String("tmpfile")));
        const bool existed = file.exists();
        if (!file.open(QIODevice::ReadWrite))
            continue;

        file.close();
        if (!existed)
            file.remove();
        break;
    }

    if (!QDir(directory).exists())
        QDir().mkpath(directory);

    setValue(QLatin1String("directory"), directory);
    return directory;
}

InstallIconsOperation::InstallIconsOperation()
{
    setName(QLatin1String("InstallIcons"));
}

InstallIconsOperation::~InstallIconsOperation()
{
    const QStringList backupFiles = value(QLatin1String("backupfiles")).toStringList();
    for (QStringList::const_iterator it = backupFiles.begin(); it != backupFiles.end(); it += 2) {
        const QString& backup = *(it + 1);
        deleteFileNowOrLater(backup);
    }
}

void InstallIconsOperation::backup()
{
    // we backup on the fly
}

bool InstallIconsOperation::performOperation()
{
    const QStringList args = arguments();
    if ((args.count() != 1) && (args.count() != 2)) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("1 or 2"), tr(" (Sourcepath, [Vendorprefix])")));
        return false;
    }

    const QString source = args.at(0);
    const QString vendor = args.value(1);

    if (source.isEmpty()) {
        setError(InvalidArguments);
        setErrorString(QObject::tr("Invalid Argument: source folder must not be empty."));
        return false;
    }

    const QDir sourceDir = QDir(source);
    const QDir targetDir = QDir(targetDirectory());

    QStringList files;
    QStringList backupFiles;
    QStringList createdDirectories;

    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();

    // iterate a second time to get the actual work done
    QDirIterator it(sourceDir.path(), QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        qApp->processEvents();

        const int status = core->status();
        if (status == PackageManagerCore::Canceled || status == PackageManagerCore::Failure)
            return true;

        const QString source = it.next();
        QString target = targetDir.absoluteFilePath(sourceDir.relativeFilePath(source));

        emit outputTextChanged(target);

        const QFileInfo fi = it.fileInfo();
        if (!fi.isDir()) {

            // exchange prefix with vendor if vendor is set
            if (!vendor.isEmpty()) {
                // path of the target file
                const QString targetPath = QFileInfo(target).absolutePath() + QLatin1String("/");

                // filename with replaced vendor string
                const QString targetFile = vendor
                    + fi.baseName().section(QLatin1Char('-'), 1, -1, QString::SectionIncludeLeadingSep)
                    + QLatin1String(".") + fi.completeSuffix();

                // target is the file with full path and replaced vendor string
                target = targetPath + targetFile;
            }

            if (QFile(target).exists()) {
                // first backup...
                const QString backup = generateTemporaryFileName(target + QLatin1String("XXXXXX"));
                QFile bf(target);
                if (!bf.copy(backup)) {
                    setError(UserDefinedError);
                    setErrorString(QObject::tr("Could not backup file %1: %2").arg(target, bf.errorString()));
                    undoOperation();
                    return false;
                }

                backupFiles.push_back(target);
                backupFiles.push_back(backup);
                setValue(QLatin1String("backupfiles"), backupFiles);

                // then delete it
                QString errStr;
                if (!deleteFileNowOrLater(target, &errStr)) {
                    setError(UserDefinedError);
                    setErrorString(QObject::tr("Failed to overwrite %1: %2").arg(target, errStr));
                    undoOperation();
                    return false;
                }

            }

            // copy the file to its new location
            QFile cf(source);
            if (!cf.copy(target)) {
                setError(UserDefinedError);
                setErrorString(QObject::tr("Failed to copy file %1: %2").arg(target, cf.errorString()));
                undoOperation();
                return false;
            }
            deleteFileNowOrLater(source);
            files.push_back(source);
            files.push_back(target);
            setValue(QLatin1String("files"), files);
        } else if (fi.isDir() && !QDir(target).exists()) {
            if (!QDir().mkpath(target)) {
                setErrorString(QObject::tr("Could not create folder at %1: %2").arg(target, qt_error_string()));
                undoOperation();
                return false;
            }
            createdDirectories.push_front(target);
            setValue(QLatin1String("createddirectories"), createdDirectories);
        }
    }

    // this should work now if not, it's not _that_ problematic...
    try {
        removeDirectory(source);
    } catch(...) {
    }
    return true;
}

bool InstallIconsOperation::undoOperation()
{
    bool success = true;

    // first copy back all files to their origin
    const QStringList files = value(QLatin1String("files")).toStringList();
    for (QStringList::const_iterator it = files.begin(); it != files.end(); it += 2) {
        qApp->processEvents();

        const QString& source = *it;
        const QString& target = *(it + 1);

        // first make sure the "source" path is valid
        QDir().mkpath(QFileInfo(source).absolutePath());

        // now copy target to source (feels weird, I know...)
        success = QFile::copy(target, source) && success;
        // and remove target
        success = QFile::remove(target) && success;
    }

    // then copy back and remove all backuped files
    const QStringList backupFiles = value(QLatin1String("backupfiles")).toStringList();
    for (QStringList::const_iterator it = backupFiles.begin(); it != backupFiles.end(); it += 2) {
        const QString& target = *it;
        const QString& backup = *(it + 1);

        // remove the target
        if (QFile::exists(target))
            success = deleteFileNowOrLater(target) && success;
        // then copy the backup onto the target
        success = QFile::copy(backup, target) && success;
        // finally remove the backp
        success = deleteFileNowOrLater(backup) && success;
    }

    // then remove all directories created by us
    const QStringList createdDirectories = value(QLatin1String("createddirectories")).toStringList();
    for (QStringList::const_iterator it = createdDirectories.begin(); it != createdDirectories.end(); ++it) {
        const QDir dir(*it);
        removeSystemGeneratedFiles(dir.absolutePath());
        success = QDir::root().rmdir(dir.path());
    }

    return success;
}

bool InstallIconsOperation::testOperation()
{
    return true;
}

Operation *InstallIconsOperation::clone() const
{
    return new InstallIconsOperation();
}
