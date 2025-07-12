/*
  SPDX-FileCopyrightText: 2009 Eike Hein <hein@kde.org>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef VISUALEVENTOVERLAY_H
#define VISUALEVENTOVERLAY_H

#include <QElapsedTimer>
#include <QRect>
#include <QTime>
#include <QWidget>

class SessionStack;
class Terminal;
class Browser;

class QTimer;

class EventRect : public QRect
{
public:
    enum EventType {
        TerminalHighlight,
        KeyboardInputBlocked,
    };

    enum EventFlag {
        NoFlags = 0x00000000,
        Singleton = 0x00000001,
        Exclusive = 0x00000002,
        Persistent = 0x00000004,
    };
    Q_DECLARE_FLAGS(EventFlags, EventFlag)

    EventRect(const QPoint &topLeft, const QPoint &bottomRight, EventType type, EventFlags flags = EventRect::NoFlags);
    ~EventRect();

    EventType eventType() const
    {
        return m_eventType;
    }
    const QElapsedTimer &timeStamp() const
    {
        return m_timeStamp;
    }

    EventFlags eventFlags() const
    {
        return m_eventFlags;
    }
    void setEventFlags(EventFlags flags)
    {
        m_eventFlags = flags;
    }
    inline bool testFlag(EventFlag flag) const
    {
        return m_eventFlags & flag;
    }

    bool operator==(const EventRect &eventRect) const;
    bool operator<(const EventRect &eventRect) const;

private:
    EventType m_eventType;
    EventFlags m_eventFlags;

    QElapsedTimer m_timeStamp;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(EventRect::EventFlags)

class VisualEventOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit VisualEventOverlay(SessionStack *parent = nullptr);
    ~VisualEventOverlay() override;

public Q_SLOTS:
    void highlightContent(QWidget *contentWidget, bool persistent = false);
    void removeTerminalHighlight();

Q_SIGNALS:
    void clicked();

public Q_SLOTS:
    void indicateKeyboardInputBlocked(QWidget *contentWidget);

    void contentEvent(QWidget *contentWidget, EventRect::EventType type, EventRect::EventFlags flags = EventRect::NoFlags);

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
    void paintEvent(QPaintEvent *) override;

private Q_SLOTS:
    void cleanupOverlay();

private:
    void scheduleCleanup(int in);

    QList<EventRect> m_eventRects;

    QTimer *m_cleanupTimer = nullptr;
    QElapsedTimer m_cleanupTimerStarted;
    int m_cleanupTimerCeiling;

    QElapsedTimer m_time;

    SessionStack *m_sessionStack = nullptr;
};

#endif
