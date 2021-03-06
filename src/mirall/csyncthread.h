/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef CSYNCTHREAD_H
#define CSYNCTHREAD_H

#include <stdint.h>

#include <QMutex>
#include <QThread>
#include <QString>
#include <qelapsedtimer.h>

#include <csync.h>

#include "mirall/syncfileitem.h"
#include "mirall/progressdispatcher.h"

class QProcess;

namespace Mirall {

class SyncJournalFileRecord;

class SyncJournalDb;

class OwncloudPropagator;

void csyncLogCatcher(int /*verbosity*/,
                     const char */*function*/,
                     const char *buffer,
                     void */*userdata*/);

class CSyncThread : public QObject
{
    Q_OBJECT
    friend void csyncthread_updater_progress_callback(CSYNC_PROGRESS *progress, void *userdata);
public:
    CSyncThread(CSYNC *, const QString &localPath, const QString &remoteURL, const QString &remotePath, SyncJournalDb *journal);
    ~CSyncThread();

    static QString csyncErrorToString( CSYNC_STATUS);

    Q_INVOKABLE void startSync();
    Q_INVOKABLE void setNetworkLimits();

    /* Abort the sync.  Called from the main thread */
    void abort();

signals:
    void csyncError( const QString& );
    void csyncWarning( const QString& );
    void csyncUnavailable();
    void treeWalkResult(const SyncFileItemVector&);

    void transmissionProgress( const Progress::Info& progress );
    void transmissionProblem( const Progress::SyncProblem& problem );

    void csyncStateDbFile( const QString& );
    void wipeDb();

    void finished();
    void started();

    void aboutToRemoveAllFiles(SyncFileItem::Direction direction, bool *cancel);

private slots:
    void transferCompleted(const SyncFileItem& item);
    void slotFinished();
    void slotProgress(Progress::Kind kind, const SyncFileItem &item, quint64 curr = 0, quint64 total = 0);
    void slotProgressChanged(qint64 change);
    void slotUpdateFinished(int updateResult);

private:


    void handleSyncError(CSYNC *ctx, const char *state);
    void progressProblem(Progress::Kind kind, const SyncFileItem& item);

    static int treewalkLocal( TREE_WALK_FILE*, void *);
    static int treewalkRemote( TREE_WALK_FILE*, void *);
    int treewalkFile( TREE_WALK_FILE*, bool );
    bool checkBlacklisting( SyncFileItem *item );

    static QMutex _syncMutex;
    SyncFileItemVector _syncedItems;

    CSYNC *_csync_ctx;
    bool _needsUpdate;
    QString _localPath;
    QString _remoteUrl;
    QString _remotePath;
    SyncJournalDb *_journal;
    QScopedPointer <OwncloudPropagator> _propagator;
    QElapsedTimer _syncTime;
    QString _lastDeleted; // if the last item was a path and it has been deleted
    QHash <QString, QString> _seenFiles;
    QThread _thread;


    // maps the origin and the target of the folders that have been renamed
    QHash<QString, QString> _renamedFolders;
    QString adjustRenamedPath(const QString &original);

    bool _hasFiles; // true if there is at least one file that is not ignored or removed
    Progress::Info _progressInfo;
    int _downloadLimit;
    int _uploadLimit;
    int _previousIndex;   // store the index of the previous item in progress update slot
    qint64 _currentFileNo;
    qint64 _overallFileCount;
    quint64 _lastOverallBytes;

    friend struct CSyncRunScopeHelper;
};


class UpdateJob : public QObject {
    Q_OBJECT
    CSYNC *_csync_ctx;
    csync_log_callback _log_callback;
    int _log_level;
    void* _log_userdata;
    Q_INVOKABLE void start() {
        csync_set_log_callback(_log_callback);
        csync_set_log_level(_log_level);
        csync_set_log_userdata(_log_userdata);
        emit finished(csync_update(_csync_ctx));
        deleteLater();
    }
public:
    explicit UpdateJob(CSYNC *ctx, QObject* parent = 0)
            : QObject(parent), _csync_ctx(ctx) {
        // We need to forward the log property as csync uses thread local
        // and updates run in another thread
        _log_callback = csync_get_log_callback();
        _log_level = csync_get_log_level();
        _log_userdata = csync_get_log_userdata();
    }
signals:
    void finished(int result);
};


}

#endif // CSYNCTHREAD_H
