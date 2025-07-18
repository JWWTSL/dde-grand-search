// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "grandsearchinterface.h"
#include "grandsearchinterface_p.h"
#include "maincontroller/maincontroller.h"
#include "utils/searchpluginprotocol.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QFileInfo>
#include <QThread>
#include <QCoreApplication>
#include <QDebug>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logDaemon)
#define CHECKINVOKER(ret)            \
    if (!d->isAccessable(message())) \
    return ret

using namespace GrandSearch;

GrandSearchInterfacePrivate::GrandSearchInterfacePrivate(GrandSearchInterface *parent)
    : QObject(parent),
      q(parent)
{
    qCDebug(logDaemon) << "GrandSearchInterfacePrivate constructor - Setting up DBus interface private";
    // 允许调用dbus接口的可执行程序名单
    m_permit.insert("/usr/bin/dde-grand-search", true);
}

GrandSearchInterfacePrivate::~GrandSearchInterfacePrivate()
{
    qCDebug(logDaemon) << "GrandSearchInterfacePrivate destructor - Cleaning up main controller";
    if (m_main) {
        delete m_main;
        m_main = nullptr;
    }
}

bool GrandSearchInterfacePrivate::isAccessable(const QDBusMessage &msg) const
{
    QDBusConnectionInterface *ifs = QDBusConnection::sessionBus().interface();
    Q_ASSERT(ifs);

    // 获取调用方的进程ID
    uint invokerPid = ifs->servicePid(msg.service()).value();

    // 读取进程的可执行程序路径
    QFileInfo fileInfo(QString("/proc/%1/exe").arg(invokerPid));
    if (!fileInfo.exists()) {
        qCWarning(logDaemon) << "Process executable not found - PID:" << invokerPid;
        return false;
    }
    auto invokerPath = fileInfo.canonicalFilePath();

    bool accessable = m_permit.value(invokerPath, false);
    if (!accessable)
        qCWarning(logDaemon) << "Access denied: Invalid invoker path:" << invokerPath << "PID:" << invokerPid;

// 调试使用
#ifdef QT_DEBUG
    return true;
#endif
    return accessable;
}

void GrandSearchInterfacePrivate::terminate()
{
    qCDebug(logDaemon) << "Terminating search operation - Session:" << m_session;
    m_deadline.stop();

    // 停止搜索
    if (m_main) {
        m_main->terminate();
        qCDebug(logDaemon) << "Main controller terminated";
    }
}

void GrandSearchInterfacePrivate::onMatched()
{
    // 控制在主线程
    qCInfo(logDaemon) << "Search results matched for session:" << m_session;
    Q_ASSERT(QThread::currentThread() == qApp->thread());
    if (!m_session.isEmpty()) {
        emit q->Matched(m_session);
    }
}

void GrandSearchInterfacePrivate::onSearchCompleted()
{
    // 控制在主线程
    qCInfo(logDaemon) << "Search completed for session:" << m_session;
    Q_ASSERT(QThread::currentThread() == qApp->thread());
    if (!m_session.isEmpty()) {
        emit q->SearchCompleted(m_session);
    }
}

GrandSearchInterface::GrandSearchInterface(QObject *parent)
    : QDBusService(parent),
      d(new GrandSearchInterfacePrivate(this))
{
    qCDebug(logDaemon) << "GrandSearchInterface constructor - Creating DBus interface";
    initPolicy(QDBusConnection::SessionBus, QString(SERVICE_CONFIG_DIR) + "other/grand-search-daemon.json");
}

GrandSearchInterface::~GrandSearchInterface()
{
    qCDebug(logDaemon) << "GrandSearchInterface destructor - Cleaning up DBus interface";
}

bool GrandSearchInterface::init()
{
    qCDebug(logDaemon) << "Initializing GrandSearchInterface";
    Q_ASSERT(d->m_main == nullptr);

    // 超时后强制结束任务
    d->m_deadline.setInterval(40000);
    d->m_deadline.setSingleShot(true);
    connect(&d->m_deadline, &QTimer::timeout, d, &GrandSearchInterfacePrivate::terminate);

    d->m_main = new MainController;
    // 直连，防止被事件循环打乱时序
    connect(d->m_main, &MainController::matched, d, &GrandSearchInterfacePrivate::onMatched, Qt::DirectConnection);
    connect(d->m_main, &MainController::searchCompleted, d, &GrandSearchInterfacePrivate::onSearchCompleted, Qt::DirectConnection);

    return d->m_main->init();
}

bool GrandSearchInterface::Search(const QString &session, const QString &key)
{
    qCDebug(logDaemon) << "Search request received - Session:" << session;
    CHECKINVOKER(false);

    // 会话ID检查，符合UUID宽度36。
    static int sessionLength = 36;
    // 关键词检查，最大长度不超过512
    static int maxKeyLength = 512;
    if (session.size() != sessionLength || key.isEmpty() || key.size() > maxKeyLength) {
        qCWarning(logDaemon) << "Invalid search parameters - Session length:" << session.size()
                             << "Key length:" << key.size();
        return false;
    }

    // 发起新的搜索
    if (d->m_main->newSearch(key)) {
        d->m_session = session;
        d->m_deadline.start();
        qCDebug(logDaemon) << "Search started successfully - Session:" << session;
        return true;
    }

    qCWarning(logDaemon) << "Failed to start new search - Session:" << session;
    d->m_deadline.stop();
    d->m_session.clear();
    return false;
}

void GrandSearchInterface::Terminate()
{
    qCDebug(logDaemon) << "Terminating search operation";
    CHECKINVOKER();
    d->terminate();
}

QByteArray GrandSearchInterface::MatchedResults(const QString &session)
{
    qCDebug(logDaemon) << "Retrieving matched results for session:" << session;
    QByteArray ret;
    CHECKINVOKER(ret);

    if (d->vaildSession(session)) {
        ret = d->m_main->getResults();
    } else {
        qCWarning(logDaemon) << "Invalid session for results retrieval - Session:" << session
                             << "Current session:" << d->m_session;
    }

    return ret;
}

QByteArray GrandSearchInterface::MatchedBuffer(const QString &session)
{
    qCDebug(logDaemon) << "Reading matched buffer for session:" << session;
    QByteArray ret;
    CHECKINVOKER(ret);

    if (d->vaildSession(session)) {
        if (!d->m_main->isEmptyBuffer()) {
            ret = d->m_main->readBuffer();
            qCDebug(logDaemon) << "Read buffer - Size:" << ret.size() << "bytes";
        } else {
            qCDebug(logDaemon) << "Buffer is empty for session:" << session;
        }
    } else {
        qCWarning(logDaemon) << "Invalid session for buffer reading - Session:" << session
                             << "Current session:" << d->m_session;
    }

    return ret;
}

bool GrandSearchInterface::OpenWithPlugin(const QString &searcher, const QString &item)
{
    qCDebug(logDaemon) << "Opening item with plugin - Searcher:" << searcher << "Item:" << item;
    CHECKINVOKER(false);
    return d->m_main->searcherAction(searcher, PLUGININTERFACE_PROTOCOL_ACTION_OPEN, item);
}

// bool GrandSearchInterface::Configure(const QVariantMap &)
//{
//     CHECKINVOKER(false);

//    return false;
//}

// QVariantMap GrandSearchInterface::Configuration() const
//{
//     return QVariantMap();
// }

// bool GrandSearchInterface::SetFeedBackStrategy(const QVariantMap &)
//{
//     CHECKINVOKER(false);

//    return false;
//}

// QVariantMap GrandSearchInterface::FeedBackStrategy() const
//{
//     return QVariantMap();
// }

bool GrandSearchInterface::KeepAlive(const QString &session)
{
    qCDebug(logDaemon) << "Keep alive request for session:" << session;
    CHECKINVOKER(false);

    // 重新计时
    if (d->vaildSession(session)) {
        d->m_deadline.start();
        qCDebug(logDaemon) << "Keep alive successful - Deadline restarted for session:" << session;
        return true;
    }

    qCWarning(logDaemon) << "Keep alive failed - Invalid session:" << session
                         << "Current session:" << d->m_session;
    return false;
}
