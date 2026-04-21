/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#define private public
#include "deskflow/ClientApp.h"
#undef private

#include "base/EventQueue.h"

#include <QTest>

namespace
{
class CountingEventQueue : public EventQueue
{
public:
  EventQueueTimer *newOneShotTimer(double duration, void *target) override
  {
    ++oneShotTimerCount;
    lastDuration = duration;
    return EventQueue::newOneShotTimer(duration, target);
  }

  int oneShotTimerCount = 0;
  int deleteTimerCount = 0;
  double lastDuration = 0.0;

  void deleteTimer(EventQueueTimer *timer) override
  {
    ++deleteTimerCount;
    EventQueue::deleteTimer(timer);
  }
};
} // namespace

class ClientAppTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void handleClientDisconnected_schedulesRetry_whenAwake();
  void handleClientDisconnected_skipsRetry_whenSuspended();
  void handleSuspend_and_handleResume_toggleSuspendedState();
  void handleSuspend_cancelsPendingRetry();
  void handleResume_reschedulesDeferredRetry();
  void scheduleClientRestart_replacesPendingRetryTimer();
};

void ClientAppTests::handleClientDisconnected_schedulesRetry_whenAwake()
{
  CountingEventQueue events;
  ClientApp app(&events);

  app.m_suspended = false;
  app.handleClientDisconnected();

  QCOMPARE(events.oneShotTimerCount, 1);
  QCOMPARE(events.lastDuration, 1.0);
}

void ClientAppTests::handleClientDisconnected_skipsRetry_whenSuspended()
{
  CountingEventQueue events;
  ClientApp app(&events);

  app.m_suspended = true;
  app.handleClientDisconnected();

  QCOMPARE(events.oneShotTimerCount, 0);
}

void ClientAppTests::handleSuspend_and_handleResume_toggleSuspendedState()
{
  CountingEventQueue events;
  ClientApp app(&events);

  QVERIFY(!app.m_suspended);

  app.handleSuspend();
  QVERIFY(app.m_suspended);

  app.handleResume();
  QVERIFY(!app.m_suspended);
}

void ClientAppTests::handleSuspend_cancelsPendingRetry()
{
  CountingEventQueue events;
  ClientApp app(&events);

  app.scheduleClientRestart(5.0);
  QVERIFY(app.m_retryTimer != nullptr);

  app.handleSuspend();

  QVERIFY(app.m_suspended);
  QVERIFY(app.m_retryTimer == nullptr);
  QVERIFY(app.m_retryOnResume);
  QCOMPARE(events.deleteTimerCount, 1);
}

void ClientAppTests::handleResume_reschedulesDeferredRetry()
{
  CountingEventQueue events;
  ClientApp app(&events);

  app.m_suspended = true;
  app.m_retryOnResume = true;

  app.handleResume();

  QVERIFY(!app.m_suspended);
  QVERIFY(!app.m_retryOnResume);
  QVERIFY(app.m_retryTimer != nullptr);
  QCOMPARE(events.oneShotTimerCount, 1);
  QCOMPARE(events.lastDuration, 1.0);
}

void ClientAppTests::scheduleClientRestart_replacesPendingRetryTimer()
{
  CountingEventQueue events;
  ClientApp app(&events);

  app.scheduleClientRestart(5.0);
  const auto *firstTimer = app.m_retryTimer;
  QVERIFY(firstTimer != nullptr);

  app.scheduleClientRestart(3.0);

  QVERIFY(app.m_retryTimer != nullptr);
  QVERIFY(app.m_retryTimer != firstTimer);
  QCOMPARE(events.oneShotTimerCount, 2);
  QCOMPARE(events.deleteTimerCount, 1);
  QCOMPARE(events.lastDuration, 3.0);
}

QTEST_MAIN(ClientAppTests)
