/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "../util.h"
#include <QtCore/qbuffer.h>
#include <QtTest/QtTest>
#include <QtWebEngineCore/qwebengineurlrequestjob.h>
#include <QtWebEngineCore/qwebengineurlschemehandler.h>
#include <QtWebEngineWidgets/qwebengineview.h>
#include <qwebengineprofile.h>
#include <qwebenginepage.h>

class tst_QWebEngineProfile : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultProfile();
    void profileConstructors();
    void clearDataFromCache();
    void disableCache();
    void urlSchemeHandlers();
    void urlSchemeHandlerFailRequest();
};

void tst_QWebEngineProfile::defaultProfile()
{
    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    QVERIFY(profile);
    QVERIFY(!profile->isOffTheRecord());
    QCOMPARE(profile->storageName(), QStringLiteral("Default"));
    QCOMPARE(profile->httpCacheType(), QWebEngineProfile::DiskHttpCache);
    QCOMPARE(profile->persistentCookiesPolicy(), QWebEngineProfile::AllowPersistentCookies);
}

void tst_QWebEngineProfile::profileConstructors()
{
    QWebEngineProfile otrProfile;
    QWebEngineProfile diskProfile(QStringLiteral("Test"));

    QVERIFY(otrProfile.isOffTheRecord());
    QVERIFY(!diskProfile.isOffTheRecord());
    QCOMPARE(diskProfile.storageName(), QStringLiteral("Test"));
    QCOMPARE(otrProfile.httpCacheType(), QWebEngineProfile::MemoryHttpCache);
    QCOMPARE(diskProfile.httpCacheType(), QWebEngineProfile::DiskHttpCache);
    QCOMPARE(otrProfile.persistentCookiesPolicy(), QWebEngineProfile::NoPersistentCookies);
    QCOMPARE(diskProfile.persistentCookiesPolicy(), QWebEngineProfile::AllowPersistentCookies);
}

void tst_QWebEngineProfile::clearDataFromCache()
{
    QWebEnginePage page;

    QDir cacheDir("./tst_QWebEngineProfile_cacheDir");
    cacheDir.makeAbsolute();
    if (cacheDir.exists())
        cacheDir.removeRecursively();
    cacheDir.mkpath(cacheDir.path());

    QWebEngineProfile *profile = page.profile();
    profile->setCachePath(cacheDir.path());
    profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);

    QSignalSpy loadFinishedSpy(&page, SIGNAL(loadFinished(bool)));
    page.load(QUrl("http://qt-project.org"));
    if (!loadFinishedSpy.wait(10000) || !loadFinishedSpy.at(0).at(0).toBool())
        QSKIP("Couldn't load page from network, skipping test.");

    cacheDir.refresh();
    QVERIFY(cacheDir.entryList().contains("Cache"));
    cacheDir.cd("./Cache");
    int filesBeforeClear = cacheDir.entryList().count();

    QFileSystemWatcher fileSystemWatcher;
    fileSystemWatcher.addPath(cacheDir.path());
    QSignalSpy directoryChangedSpy(&fileSystemWatcher, SIGNAL(directoryChanged(const QString &)));

    // It deletes most of the files, but not all of them.
    profile->clearHttpCache();
    QTest::qWait(1000);
    QTRY_VERIFY(directoryChangedSpy.count() > 0);

    cacheDir.refresh();
    QVERIFY(filesBeforeClear > cacheDir.entryList().count());

    cacheDir.removeRecursively();
}

void tst_QWebEngineProfile::disableCache()
{
    QWebEnginePage page;
    QDir cacheDir("./tst_QWebEngineProfile_cacheDir");
    if (cacheDir.exists())
        cacheDir.removeRecursively();
    cacheDir.mkpath(cacheDir.path());

    QWebEngineProfile *profile = page.profile();
    profile->setCachePath(cacheDir.path());
    QVERIFY(!cacheDir.entryList().contains("Cache"));

    profile->setHttpCacheType(QWebEngineProfile::NoCache);
    QSignalSpy loadFinishedSpy(&page, SIGNAL(loadFinished(bool)));
    page.load(QUrl("http://qt-project.org"));
    if (!loadFinishedSpy.wait(10000) || !loadFinishedSpy.at(0).at(0).toBool())
        QSKIP("Couldn't load page from network, skipping test.");

    cacheDir.refresh();
    QVERIFY(!cacheDir.entryList().contains("Cache"));

    profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    page.load(QUrl("http://qt-project.org"));
    if (!loadFinishedSpy.wait(10000) || !loadFinishedSpy.at(1).at(0).toBool())
        QSKIP("Couldn't load page from network, skipping test.");

    cacheDir.refresh();
    QVERIFY(cacheDir.entryList().contains("Cache"));

    cacheDir.removeRecursively();
}

class RedirectingUrlSchemeHandler : public QWebEngineUrlSchemeHandler
{
public:
    void requestStarted(QWebEngineUrlRequestJob *job)
    {
        job->redirect(QUrl(QStringLiteral("data:text/plain;charset=utf-8,")
                           + job->requestUrl().fileName()));
    }
};

class ReplyingUrlSchemeHandler : public QWebEngineUrlSchemeHandler
{
    QBuffer m_buffer;
    QByteArray m_bufferData;
public:
    ReplyingUrlSchemeHandler(QObject *parent = nullptr)
        : QWebEngineUrlSchemeHandler(parent)
    {
        m_buffer.setBuffer(&m_bufferData);
    }

    void requestStarted(QWebEngineUrlRequestJob *job)
    {
        m_bufferData = job->requestUrl().toString().toUtf8();
        job->reply("text/plain;charset=utf-8", &m_buffer);
    }
};

void tst_QWebEngineProfile::urlSchemeHandlers()
{
    RedirectingUrlSchemeHandler mailtoHandler;
    QWebEngineProfile profile(QStringLiteral("urlSchemeHandlers"));
    profile.installUrlSchemeHandler("mailto", &mailtoHandler);
    QWebEngineView view;
    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.setPage(new QWebEnginePage(&profile, &view));
    QString emailAddress = QStringLiteral("egon@olsen-banden.dk");
    view.load(QUrl(QStringLiteral("mailto:") + emailAddress));
    QVERIFY(loadFinishedSpy.wait());
    QCOMPARE(toPlainTextSync(view.page()), emailAddress);

    // Install a gopher handler after the view has been fully initialized.
    ReplyingUrlSchemeHandler gopherHandler;
    profile.installUrlSchemeHandler("gopher", &gopherHandler);
    QUrl url = QUrl(QStringLiteral("gopher://olsen-banden.dk/benny"));
    view.load(url);
    QVERIFY(loadFinishedSpy.wait());
    QCOMPARE(toPlainTextSync(view.page()), url.toString());

    // Remove the mailto scheme, and check whether it is not handled anymore.
    profile.removeUrlScheme("mailto");
    emailAddress = QStringLiteral("kjeld@olsen-banden.dk");
    view.load(QUrl(QStringLiteral("mailto:") + emailAddress));
    QVERIFY(loadFinishedSpy.wait());
    QVERIFY(toPlainTextSync(view.page()) != emailAddress);

    // Check if gopher is still working after removing mailto.
    url = QUrl(QStringLiteral("gopher://olsen-banden.dk/yvonne"));
    view.load(url);
    QVERIFY(loadFinishedSpy.wait());
    QCOMPARE(toPlainTextSync(view.page()), url.toString());

    // Does removeAll work?
    profile.removeAllUrlSchemeHandlers();
    url = QUrl(QStringLiteral("gopher://olsen-banden.dk/harry"));
    view.load(url);
    QVERIFY(loadFinishedSpy.wait());
    QVERIFY(toPlainTextSync(view.page()) != url.toString());

    // Install a handler that is owned by the view. Make sure this doesn't crash on shutdown.
    profile.installUrlSchemeHandler("aviancarrier", new ReplyingUrlSchemeHandler(&view));
    url = QUrl(QStringLiteral("aviancarrier:inspector.mortensen@politistyrke.dk"));
    view.load(url);
    QVERIFY(loadFinishedSpy.wait());
    QCOMPARE(toPlainTextSync(view.page()), url.toString());
}

class FailingUrlSchemeHandler : public QWebEngineUrlSchemeHandler
{
public:
    void requestStarted(QWebEngineUrlRequestJob *job) override
    {
        job->fail(QWebEngineUrlRequestJob::RequestFailed);
    }
};

void tst_QWebEngineProfile::urlSchemeHandlerFailRequest()
{
    FailingUrlSchemeHandler handler;
    QWebEngineProfile profile;
    profile.installUrlSchemeHandler("foo", &handler);
    QWebEngineView view;
    QSignalSpy loadFinishedSpy(&view, SIGNAL(loadFinished(bool)));
    view.setPage(new QWebEnginePage(&profile, &view));
    view.load(QUrl(QStringLiteral("foo://bar")));
    QVERIFY(loadFinishedSpy.wait());
    QVERIFY(toPlainTextSync(view.page()).isEmpty());
}

QTEST_MAIN(tst_QWebEngineProfile)
#include "tst_qwebengineprofile.moc"