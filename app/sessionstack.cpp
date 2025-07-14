/*
  SPDX-FileCopyrightText: 2008-2014 Eike Hein <hein@kde.org>
  SPDX-FileCopyrightText: 2009 Juan Carlos Torres <carlosdgtorres@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "sessionstack.h"
#include "browser.h"
#include "session.h"
#include "settings.h"
#include "terminal.h"
#include "visualeventoverlay.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KNotification>

#include <QDBusConnection>
#include <QLineEdit>

#include <algorithm>

static bool show_disallow_certain_dbus_methods_message = true;

SessionStack::SessionStack(QWidget *parent)
    : QStackedWidget(parent)
    , m_visualEventOverlay(new VisualEventOverlay(this))
{
    connect(m_visualEventOverlay, &VisualEventOverlay::clicked, this, &SessionStack::removeTerminalHighlight);

    connect(this, SIGNAL(currentChanged(int)), this, SLOT(handleCurrentChanged(int)));

    connect(this, SIGNAL(activityDetected(Session *, int)), this, SLOT(handleActivity(Session *, int)));
    connect(this, SIGNAL(silenceDetected(Session *, int)), this, SLOT(handleSilence(Session *, int)));

    m_visualEventOverlay->hide();
}

SessionStack::~SessionStack() = default;

int SessionStack::addSessionImpl(Session::SessionContent contentType, Session::SessionType type)
{
    Session *currentSession = m_sessions.value(activeSessionId());
    QString workingDir;
    if (currentSession && currentSession->contentType() == Session::TerminalType) {
        Terminal *currentTerminal = currentSession->getTerminal(currentSession->activeId());
        workingDir = currentTerminal ? currentTerminal->currentWorkingDirectory() : QString();
    }

    Session *session = new Session(workingDir, contentType, type, this);
    // clang-format off
    connect(session, SIGNAL(titleChanged(int,QString)), this, SIGNAL(titleChanged(int,QString)));
    connect(session, SIGNAL(destroyed(int)), this, SLOT(cleanup(int)));
    connect(session, &Session::wantsBlurChanged, this, &SessionStack::wantsBlurChanged);

    if (contentType == Session::TerminalType) {
        connect(session, SIGNAL(terminalManuallyActivated(Terminal*)), this, SLOT(handleManualActivation(Terminal*)));
        connect(session, SIGNAL(keyboardInputBlocked(Terminal*)), m_visualEventOverlay, SLOT(indicateKeyboardInputBlocked(QWidget*)));
        connect(session, SIGNAL(activityDetected(Terminal*)), parentWidget(), SLOT(handleActivity(Session*,int)));
        connect(session, SIGNAL(silenceDetected(Terminal*)), parentWidget(), SLOT(handleSilence(Session*,int)));
        connect(parentWidget(), SIGNAL(windowClosed()), session, SLOT(reconnectMonitorActivitySignals()));
    } else if (contentType == Session::BrowserType) {
        connect(session, SIGNAL(browserManuallyActivated(Browser*)), this, SLOT(handleManualActivation(Browser*)));
    }
    // clang-format on

    addWidget(session->widget());
    session->widget()->updateGeometry();

    m_sessions.insert(session->id(), session);

    Q_EMIT wantsBlurChanged();

    if (Settings::dynamicTabTitles())
        Q_EMIT sessionAdded(session->id(), session->title());
    else
        Q_EMIT sessionAdded(session->id(), QString());

    return session->id();
}

void SessionStack::addTerminalSession()
{
    addSessionImpl(Session::TerminalType, Session::Single);
}

void SessionStack::addBrowserSession()
{
    addSessionImpl(Session::BrowserType, Session::Single);
}

int SessionStack::addTerminalSessionTwoHorizontal()
{
    return addSessionImpl(Session::TerminalType, Session::TwoHorizontal);
}

int SessionStack::addTerminalSessionTwoVertical()
{
    return addSessionImpl(Session::TerminalType, Session::TwoVertical);
}

int SessionStack::addTerminalSessionQuad()
{
    return addSessionImpl(Session::TerminalType, Session::Quad);
}

void SessionStack::raiseSession(int sessionId)
{
    if (sessionId == -1 || !m_sessions.contains(sessionId))
        return;
    Session *session = m_sessions.value(sessionId);

    if (!m_visualEventOverlay->isHidden())
        m_visualEventOverlay->hide();

    if (m_activeSessionId != -1 && m_sessions.contains(m_activeSessionId)) {
        Session *oldActiveSession = m_sessions.value(m_activeSessionId);

        disconnect(oldActiveSession, SLOT(focusPrevious()));
        disconnect(oldActiveSession, SLOT(focusNext()));
        disconnect(oldActiveSession, SLOT(manageProfiles()));
        disconnect(oldActiveSession, SIGNAL(titleChanged(QString)), this, SIGNAL(activeTitleChanged(QString)));

        if (oldActiveSession->contentType() == Session::TerminalType) {
            oldActiveSession->reconnectMonitorActivitySignals();
        }
    }

    m_activeSessionId = sessionId;

    setCurrentWidget(session->widget());

    if (session->widget()->focusWidget())
        session->widget()->focusWidget()->setFocus();

    if (session->contentType() == Session::BrowserType) {
        session->getBrowser(session->activeId())->urlBar()->setFocus();
    }

    if (session->contentType() == Session::TerminalType && session->hasTerminalsWithKeyboardInputDisabled())
        m_visualEventOverlay->show();

    connect(this, SIGNAL(previous()), session, SLOT(focusPrevious()));
    connect(this, SIGNAL(next()), session, SLOT(focusNext()));
    connect(this, SIGNAL(manageProfiles()), session, SLOT(manageProfiles()));
    connect(session, SIGNAL(titleChanged(QString)), this, SIGNAL(activeTitleChanged(QString)));

    Q_EMIT sessionRaised(sessionId);

    Q_EMIT activeTitleChanged(session->title());
}

void SessionStack::removeSession(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return;
    if (!m_sessions.contains(sessionId))
        return;

    if (queryClose(sessionId, QueryCloseSession)) {
        Session *session = m_sessions[sessionId];

        m_sessions.remove(sessionId);

        removeWidget(session->widget());

        Q_EMIT sessionRemoved(sessionId);

        delete session;

        if (m_sessions.empty()) {
            addTerminalSession();
        }
    }
}

void SessionStack::removeContent(int contentId)
{
    int sessionId = sessionIdForContentId(contentId);

    if (contentId == -1) {
        if (m_activeSessionId == -1)
            return;
        if (!m_sessions.contains(m_activeSessionId))
            return;

        if (m_sessions.value(m_activeSessionId)->closable())
            m_sessions.value(m_activeSessionId)->closeSession();
    } else {
        if (m_sessions.value(sessionId)->closable())
            m_sessions.value(sessionId)->closeSession(contentId);
    }
}

void SessionStack::closeActive(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return;
    if (!m_sessions.contains(sessionId))
        return;

    Session *session = m_sessions[sessionId];

    if (session->contentType() == Session::TerminalType) {
        if (queryClose(sessionId, QueryCloseTerminal)) {
            session->closeTerminal(session->activeId());
        }
    } else if (session->contentType() == Session::BrowserType) {
        if (queryClose(sessionId, QueryCloseBrowser)) {
            session->closeBrowser(session->activeId());
        }
    }
}

void SessionStack::cleanup(int sessionId)
{
    if (sessionId == m_activeSessionId)
        m_activeSessionId = -1;

    m_sessions.remove(sessionId);

    Q_EMIT wantsBlurChanged();
    Q_EMIT sessionRemoved(sessionId);
}

int SessionStack::activeId()
{
    if (!m_sessions.contains(m_activeSessionId))
        return -1;

    return m_sessions.value(m_activeSessionId)->activeId();
}

const QString SessionStack::sessionIdList()
{
    QList<int> keyList = m_sessions.keys();
    QStringList idList;

    QListIterator<int> i(keyList);

    while (i.hasNext())
        idList << QString::number(i.next());

    return idList.join(QLatin1Char(','));
}

const QString SessionStack::contentIdList()
{
    QStringList idList;

    QHashIterator<int, Session *> it(m_sessions);

    while (it.hasNext()) {
        it.next();

        idList << it.value()->terminalIdList();
    }

    return idList.join(QLatin1Char(','));
}

const QString SessionStack::contentIdsForSessionId(int sessionId)
{
    if (!m_sessions.contains(sessionId))
        return QString::number(-1);

    return m_sessions.value(sessionId)->terminalIdList();
}

int SessionStack::sessionIdForContentId(int contentId)
{
    int sessionId = -1;

    QHashIterator<int, Session *> it(m_sessions);

    while (it.hasNext()) {
        it.next();

        if (it.value()->contentType() == Session::TerminalType) {
            if (it.value()->hasTerminal(contentId)) {
                sessionId = it.key();

                break;
            }
        } else if (it.value()->contentType() == Session::BrowserType) {
            if (it.value()->hasBrowser(contentId)) {
                sessionId = it.key();

                break;
            }
        }
    }

    return sessionId;
}

static void warnAboutDBus()
{
#if !defined(REMOVE_SENDTEXT_RUNCOMMAND_DBUS_METHODS)
    if (show_disallow_certain_dbus_methods_message) {
        KNotification::event(
            KNotification::Warning,
            QStringLiteral("Yakuake D-Bus Warning"),
            i18n("The D-Bus method runCommand was just used.  There are security concerns about allowing these methods to be public.  If desired, these "
                 "methods can be changed to internal use only by re-compiling Yakuake. <p>This warning will only show once for this Yakuake instance.</p>"));
        show_disallow_certain_dbus_methods_message = false;
    }
#endif
}

void SessionStack::runCommand(const QString &command)
{
    warnAboutDBus();

    if (m_activeSessionId == -1)
        return;
    if (!m_sessions.contains(m_activeSessionId))
        return;

    m_sessions.value(m_activeSessionId)->runCommand(command);
}

void SessionStack::runCommandInTerminal(int terminalId, const QString &command)
{
    warnAboutDBus();

    int sessionId = sessionIdForContentId(terminalId);
    if (sessionId == -1)
        return;
    if (!m_sessions.contains(sessionId))
        return;

    m_sessions.value(sessionId)->runCommand(command, terminalId);
}

void SessionStack::runCommandInContent(int contentId, const QString &command)
{
    warnAboutDBus();

    int sessionId = sessionIdForContentId(contentId);
    if (sessionId == -1)
        return;
    if (!m_sessions.contains(sessionId))
        return;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        session->runCommand(command, contentId);
    } else if (session->contentType() == Session::BrowserType) {
        // No runCommand for browser for now
    }
}

bool SessionStack::isSessionClosable(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return false;
    if (!m_sessions.contains(sessionId))
        return false;

    return m_sessions.value(sessionId)->closable();
}

void SessionStack::setSessionClosable(int sessionId, bool closable)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return;
    if (!m_sessions.contains(sessionId))
        return;

    m_sessions.value(sessionId)->setClosable(closable);
}

bool SessionStack::hasUnclosableSessions() const
{
    QHashIterator<int, Session *> it(m_sessions);

    while (it.hasNext()) {
        it.next();

        if (!it.value()->closable())
            return true;
    }

    return false;
}

bool SessionStack::isSessionKeyboardInputEnabled(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return false;
    if (!m_sessions.contains(sessionId))
        return false;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        return session->keyboardInputEnabled();
    }
    return false;
}

void SessionStack::setSessionKeyboardInputEnabled(int sessionId, bool enabled)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return;
    if (!m_sessions.contains(sessionId))
        return;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        session->setKeyboardInputEnabled(enabled);

        if (sessionId == m_activeSessionId) {
            if (enabled)
                m_visualEventOverlay->hide();
            else
                m_visualEventOverlay->show();
        }
    }
}

bool SessionStack::isContentKeyboardInputEnabled(int contentId)
{
    int sessionId = sessionIdForContentId(contentId);
    if (sessionId == -1)
        return false;
    if (!m_sessions.contains(sessionId))
        return false;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        return session->keyboardInputEnabled(contentId);
    }
    return false;
}

void SessionStack::setContentKeyboardInputEnabled(int contentId, bool enabled)
{
    int sessionId = sessionIdForContentId(contentId);
    if (sessionId == -1)
        return;
    if (!m_sessions.contains(sessionId))
        return;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        session->setKeyboardInputEnabled(contentId, enabled);

        if (sessionId == m_activeSessionId) {
            if (enabled)
                m_visualEventOverlay->hide();
            else
                m_visualEventOverlay->show();
        }
    }
}

bool SessionStack::hasContentWithKeyboardInputEnabled(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return false;
    if (!m_sessions.contains(sessionId))
        return false;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        return session->hasTerminalsWithKeyboardInputEnabled();
    }
    return false;
}

bool SessionStack::hasContentWithKeyboardInputDisabled(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return false;
    if (!m_sessions.contains(sessionId))
        return false;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        return session->hasTerminalsWithKeyboardInputDisabled();
    }
    return false;
}

bool SessionStack::isSessionMonitorActivityEnabled(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return false;
    if (!m_sessions.contains(sessionId))
        return false;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        return session->monitorActivityEnabled();
    }
    return false;
}

void SessionStack::setSessionMonitorActivityEnabled(int sessionId, bool enabled)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return;
    if (!m_sessions.contains(sessionId))
        return;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        session->setMonitorActivityEnabled(enabled);
    }
}

bool SessionStack::isContentMonitorActivityEnabled(int contentId)
{
    QHashIterator<int, Session *> i(m_sessions);

    while (i.hasNext()) {
        i.next();

        Session *session = i.value();

        if (session->contentType() == Session::TerminalType) {
            if (session->hasTerminal(contentId)) {
                return session->monitorActivityEnabled(contentId);
            }
        }
    }

    return false;
}

void SessionStack::setContentMonitorActivityEnabled(int contentId, bool enabled)
{
    QHashIterator<int, Session *> i(m_sessions);

    while (i.hasNext()) {
        i.next();

        Session *session = i.value();

        if (session->contentType() == Session::TerminalType) {
            if (session->hasTerminal(contentId)) {
                session->setMonitorActivityEnabled(contentId, enabled);
                return;
            }
        }
    }
}

bool SessionStack::hasContentWithMonitorActivityEnabled(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return false;
    if (!m_sessions.contains(sessionId))
        return false;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        return session->hasTerminalsWithMonitorActivityEnabled();
    }
    return false;
}

bool SessionStack::hasContentWithMonitorActivityDisabled(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return false;
    if (!m_sessions.contains(sessionId))
        return false;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        return session->hasTerminalsWithMonitorActivityDisabled();
    }
    return false;
}

bool SessionStack::isSessionMonitorSilenceEnabled(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return false;
    if (!m_sessions.contains(sessionId))
        return false;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        return session->monitorSilenceEnabled();
    }
    return false;
}

void SessionStack::setSessionMonitorSilenceEnabled(int sessionId, bool enabled)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return;
    if (!m_sessions.contains(sessionId))
        return;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        session->setMonitorSilenceEnabled(enabled);
    }
}

bool SessionStack::isContentMonitorSilenceEnabled(int contentId)
{
    QHashIterator<int, Session *> i(m_sessions);

    while (i.hasNext()) {
        i.next();

        Session *session = i.value();

        if (session->contentType() == Session::TerminalType) {
            if (session->hasTerminal(contentId)) {
                return session->monitorSilenceEnabled(contentId);
            }
        }
    }

    return false;
}

void SessionStack::setContentMonitorSilenceEnabled(int contentId, bool enabled)
{
    QHashIterator<int, Session *> i(m_sessions);

    while (i.hasNext()) {
        i.next();

        Session *session = i.value();

        if (session->contentType() == Session::TerminalType) {
            if (session->hasTerminal(contentId)) {
                session->setMonitorSilenceEnabled(contentId, enabled);
                return;
            }
        }
    }
}

bool SessionStack::hasContentWithMonitorSilenceEnabled(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return false;
    if (!m_sessions.contains(sessionId))
        return false;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        return session->hasTerminalsWithMonitorSilenceEnabled();
    } else if (session->contentType() == Session::BrowserType) {
        // No monitor silence for browser for now
    }
    return false;
}

bool SessionStack::hasContentWithMonitorSilenceDisabled(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return false;
    if (!m_sessions.contains(sessionId))
        return false;

    Session *session = m_sessions.value(sessionId);
    if (session->contentType() == Session::TerminalType) {
        return session->hasTerminalsWithMonitorSilenceDisabled();
    } else if (session->contentType() == Session::BrowserType) {
        // No monitor silence for browser for now
    }
    return false;
}

void SessionStack::editProfile(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return;
    if (!m_sessions.contains(sessionId))
        return;

    m_sessions.value(sessionId)->editProfile();
}

int SessionStack::splitSessionLeftRight(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return -1;

    Session *session = m_sessions[sessionId];

    if (session->contentType() == Session::TerminalType) {
        return session->splitLeftRight();
    } else if (session->contentType() == Session::BrowserType) {
        return session->splitLeftRight();
    }
    return -1;
}

int SessionStack::splitSessionAuto(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return -1;

    Session *session = m_sessions[sessionId];

    if (session->contentType() == Session::TerminalType) {
        return session->splitAuto();
    } else if (session->contentType() == Session::BrowserType) {
        return session->splitAuto();
    }
    return -1;
}

int SessionStack::splitSessionTopBottom(int sessionId)
{
    if (sessionId == -1)
        sessionId = m_activeSessionId;
    if (sessionId == -1)
        return -1;

    Session *session = m_sessions[sessionId];

    if (session->contentType() == Session::TerminalType) {
        return session->splitTopBottom();
    } else if (session->contentType() == Session::BrowserType) {
        return session->splitTopBottom();
    }
    return -1;
}

int SessionStack::splitContentLeftRight(int contentId)
{
    QHashIterator<int, Session *> i(m_sessions);

    while (i.hasNext()) {
        i.next();

        Session *session = i.value();

        if (session->contentType() == Session::TerminalType) {
            if (session->hasTerminal(contentId)) {
                return session->splitLeftRight(contentId);
            }
        } else if (session->contentType() == Session::BrowserType) {
            if (session->hasBrowser(contentId)) {
                return session->splitLeftRight(contentId);
            }
        }
    }

    return -1;
}

int SessionStack::splitContentTopBottom(int contentId)
{
    QHashIterator<int, Session *> i(m_sessions);

    while (i.hasNext()) {
        i.next();

        Session *session = i.value();

        if (session->contentType() == Session::TerminalType) {
            if (session->hasTerminal(contentId)) {
                return session->splitTopBottom(contentId);
            }
        } else if (session->contentType() == Session::BrowserType) {
            if (session->hasBrowser(contentId)) {
                return session->splitTopBottom(contentId);
            }
        }
    }

    return -1;
}

int SessionStack::tryGrowRight(int id, uint pixels)
{
    QHashIterator<int, Session *> i(m_sessions);

    while (i.hasNext()) {
        i.next();

        Session *session = i.value();

        if (session->contentType() == Session::TerminalType) {
            if (session->hasTerminal(id)) {
                return session->tryGrow(id, Session::Right, pixels);
            }
        } else if (session->contentType() == Session::BrowserType) {
            if (session->hasBrowser(id)) {
                return session->tryGrow(id, Session::Right, pixels);
            }
        }
    }

    return -1;
}

int SessionStack::tryGrowLeft(int id, uint pixels)
{
    QHashIterator<int, Session *> i(m_sessions);

    while (i.hasNext()) {
        i.next();

        Session *session = i.value();

        if (session->contentType() == Session::TerminalType) {
            if (session->hasTerminal(id)) {
                return session->tryGrow(id, Session::Left, pixels);
            }
        } else if (session->contentType() == Session::BrowserType) {
            if (session->hasBrowser(id)) {
                return session->tryGrow(id, Session::Left, pixels);
            }
        }
    }

    return -1;
}

int SessionStack::tryGrowTop(int id, uint pixels)
{
    QHashIterator<int, Session *> i(m_sessions);

    while (i.hasNext()) {
        i.next();

        Session *session = i.value();

        if (session->contentType() == Session::TerminalType) {
            if (session->hasTerminal(id)) {
                return session->tryGrow(id, Session::Up, pixels);
            }
        } else if (session->contentType() == Session::BrowserType) {
            if (session->hasBrowser(id)) {
                return session->tryGrow(id, Session::Up, pixels);
            }
        }
    }

    return -1;
}

int SessionStack::tryGrowBottom(int id, uint pixels)
{
    QHashIterator<int, Session *> i(m_sessions);

    while (i.hasNext()) {
        i.next();

        Session *session = i.value();

        if (session->contentType() == Session::TerminalType) {
            if (session->hasTerminal(id)) {
                return session->tryGrow(id, Session::Down, pixels);
            }
        } else if (session->contentType() == Session::BrowserType) {
            if (session->hasBrowser(id)) {
                return session->tryGrow(id, Session::Down, pixels);
            }
        }
    }

    return -1;
}

void SessionStack::emitTitles()
{
    QString title;

    QHashIterator<int, Session *> it(m_sessions);

    while (it.hasNext()) {
        it.next();

        title = it.value()->title();

        if (!title.isEmpty())
            Q_EMIT titleChanged(it.value()->id(), title);
    }
}

bool SessionStack::requiresVisualEventOverlay()
{
    if (m_activeSessionId == -1)
        return false;
    if (!m_sessions.contains(m_activeSessionId))
        return false;

    Session *session = m_sessions.value(m_activeSessionId);
    if (session->contentType() == Session::TerminalType) {
        return session->hasTerminalsWithKeyboardInputDisabled();
    }
    return false;
}

void SessionStack::handleHighlightRequest(int id)
{
    QHashIterator<int, Session *> i(m_sessions);

    while (i.hasNext()) {
        i.next();

        Session *session = i.value();

        if (session->contentType() == Session::TerminalType) {
            if (session->hasTerminal(id)) {
                m_visualEventOverlay->highlightContent(session->getTerminal(id)->partWidget(), true);
                m_visualEventOverlay->show();
                return;
            }
        } else if (session->contentType() == Session::BrowserType) {
            if (session->hasBrowser(id)) {
                m_visualEventOverlay->highlightContent(session->getBrowser(id)->partWidget(), true);
                m_visualEventOverlay->show();
                return;
            }
        }
    }
}

void SessionStack::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)

    if (m_activeSessionId == -1)
        return;
    if (!m_sessions.contains(m_activeSessionId))
        return;

    Session *session = m_sessions.value(m_activeSessionId);
    if (session->contentType() == Session::TerminalType) {
        if (session->activeId() != -1) {
            session->getTerminal(session->activeId())->terminalWidget()->setFocus();
        }
    } else if (session->contentType() == Session::BrowserType) {
        if (session->activeId() != -1) {
            session->getBrowser(session->activeId())->browserWidget()->setFocus();
        }
    }
}

void SessionStack::handleCurrentChanged(int index)
{
    Q_UNUSED(index);
    // TODO: Implement logic for when the current session changes
}

void SessionStack::handleActivity(Terminal *terminal)
{
    Q_UNUSED(terminal);
    // TODO: Implement logic for activity detection
}

void SessionStack::handleSilence(Terminal *terminal)
{
    Q_UNUSED(terminal);
    // TODO: Implement logic for silence detection
}

void SessionStack::handleManualActivation(Terminal *terminal)
{
    if (!Settings::terminalHighlightOnManualActivation())
        return;

    Session *session = qobject_cast<Session *>(QObject::sender());

    if (session->terminalCount() > 1)
        m_visualEventOverlay->highlightContent(terminal->partWidget(), false);
}

void SessionStack::handleManualActivation(Browser *browser)
{
    Q_UNUSED(browser);
    // No highlight for browser for now
}

bool SessionStack::queryClose(int sessionId, QueryCloseType type)
{
    if (!m_sessions.contains(sessionId))
        return false;

    Session *session = m_sessions[sessionId];

    if (type == QueryCloseSession) {
        bool confirmQuit = Settings::confirmQuit();
        bool hasUnclosableSessions = !session->closable();

        QString closeQuestion = xi18nc("@info", "Are you sure you want to close this session?");
        QString warningMessage;

        if (session->contentType() == Session::TerminalType) {
            if ((confirmQuit && session->terminalCount() > 1) || hasUnclosableSessions) {
                if (confirmQuit && session->terminalCount() > 1) {
                    if (hasUnclosableSessions)
                        warningMessage = xi18nc(
                            "@info",
                            "<warning>There are multiple open terminals in this session, <emphasis>some of which you have locked to prevent closing them "
                            "accidentally.</emphasis> These will be killed if you continue.</warning>");
                    else
                        warningMessage =
                            xi18nc("@info", "<warning>There are multiple open terminals in this session. These will be killed if you continue.</warning>");
                } else if (hasUnclosableSessions) {
                    warningMessage = xi18nc("@info",
                                            "<warning>There are one or more open terminals in this session that you have locked to prevent closing them "
                                            "accidentally. These will be "
                                            "killed if you continue.</warning>");
                }
            }
        } else if (session->contentType() == Session::BrowserType) {
            if ((confirmQuit && session->browserCount() > 1) || hasUnclosableSessions) {
                if (confirmQuit && session->browserCount() > 1) {
                    if (hasUnclosableSessions)
                        warningMessage = xi18nc(
                            "@info",
                            "<warning>There are multiple open browser tabs in this session, <emphasis>some of which you have locked to prevent closing them "
                            "accidentally.</emphasis> These will be killed if you continue.</warning>");
                    else
                        warningMessage =
                            xi18nc("@info", "<warning>There are multiple open browser tabs in this session. These will be killed if you continue.</warning>");
                } else if (hasUnclosableSessions) {
                    warningMessage = xi18nc("@info",
                                            "<warning>There are one or more open browser tabs in this session that you have locked to prevent closing them "
                                            "accidentally. These will be "
                                            "killed if you continue.</warning>");
                }
            }
        }

        if (!warningMessage.isEmpty()) {
            int result = KMessageBox::warningContinueCancel(this,
                                                            warningMessage + QStringLiteral("<br /><br />") + closeQuestion,
                                                            xi18nc("@title:window", "Really Close Session?"),
                                                            KStandardGuiItem::close(),
                                                            KStandardGuiItem::cancel());

            return result != KMessageBox::Cancel;
        }
    } else if (type == QueryCloseTerminal) {
        bool confirmQuit = Settings::confirmQuit();
        bool isClosable = true;
        if (session->contentType() == Session::TerminalType) {
            isClosable = session->getTerminal(session->activeId())->closable();
        }

        QString closeQuestion = xi18nc("@info", "Are you sure you want to close this terminal?");
        QString warningMessage;

        if (confirmQuit || !isClosable) {
            if (!isClosable) {
                warningMessage =
                    xi18nc("@info", "<warning>This terminal is locked to prevent closing it accidentally. It will be killed if you continue.</warning>");
            }

            int result = KMessageBox::warningContinueCancel(this,
                                                            warningMessage + QStringLiteral("<br /><br />") + closeQuestion,
                                                            xi18nc("@title:window", "Really Close Terminal?"),
                                                            KStandardGuiItem::close(),
                                                            KStandardGuiItem::cancel());

            return result != KMessageBox::Cancel;
        }
    } else if (type == QueryCloseBrowser) {
        bool confirmQuit = Settings::confirmQuit();
        bool isClosable = true;
        if (session->contentType() == Session::BrowserType) {
            isClosable = session->getBrowser(session->activeId())->closable();
        }

        QString closeQuestion = xi18nc("@info", "Are you sure you want to close this browser tab?");
        QString warningMessage;

        if (confirmQuit || !isClosable) {
            if (!isClosable) {
                warningMessage =
                    xi18nc("@info", "<warning>This browser tab is locked to prevent closing it accidentally. It will be killed if you continue.</warning>");
            }

            int result = KMessageBox::warningContinueCancel(this,
                                                            warningMessage + QStringLiteral("<br /><br />") + closeQuestion,
                                                            xi18nc("@title:window", "Really Close Browser Tab?"),
                                                            KStandardGuiItem::close(),
                                                            KStandardGuiItem::cancel());

            return result != KMessageBox::Cancel;
        }
    }

    return true;
}

QList<KActionCollection *> SessionStack::getPartActionCollections()
{
    QList<KActionCollection *> actionCollections;

    const auto sessions = m_sessions.values();
    for (auto *session : sessions) {
        const auto terminalIds = session->terminalIdList().split(QStringLiteral(","));

        for (const auto &terminalID : terminalIds) {
            auto *terminal = session->getTerminal(terminalID.toInt());
            if (terminal) {
                auto *collection = terminal->actionCollection();
                if (collection) {
                    actionCollections.append(collection);
                }
            }
        }
    }

    return actionCollections;
}

bool SessionStack::wantsBlur() const
{
    return std::any_of(m_sessions.cbegin(), m_sessions.cend(), [](Session *session) {
        return session->wantsBlur();
    });
}

#include "moc_sessionstack.cpp"
