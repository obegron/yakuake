/*
  SPDX-FileCopyrightText: 2008-2014 Eike Hein <hein@kde.org>
  SPDX-FileCopyrightText: 2009 Juan Carlos Torres <carlosdgtorres@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef SESSIONSTACK_H
#define SESSIONSTACK_H

#include "session.h"

#include <config-yakuake.h>

#include <QHash>
#include <QStackedWidget>

class Session;
class Terminal;
class Browser;
class KActionCollection;
#include "visualeventoverlay.h"

class SessionStack : public QStackedWidget
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.yakuake")

public:
    explicit SessionStack(QWidget *parent = nullptr);
    ~SessionStack() override;

    void closeActive(int sessionId = -1);

    void editProfile(int sessionId = -1);

    void emitTitles();

    bool requiresVisualEventOverlay();

    QList<KActionCollection *> getPartActionCollections();

    bool wantsBlur() const;
    Session *session(int sessionId) const
    {
        return m_sessions.value(sessionId);
    }

public Q_SLOTS:
    int addSessionImpl(Session::SessionContent contentType, Session::SessionType type = Session::Single);
    void addTerminalSession();
    void addBrowserSession();
    Q_SCRIPTABLE int addTerminalSessionTwoHorizontal();
    Q_SCRIPTABLE int addTerminalSessionTwoVertical();
    Q_SCRIPTABLE int addTerminalSessionQuad();

    Q_SCRIPTABLE void raiseSession(int sessionId);

    Q_SCRIPTABLE void removeSession(int sessionId);
    Q_SCRIPTABLE void removeContent(int contentId);

    Q_SCRIPTABLE int splitSessionAuto(int sessionId);
    Q_SCRIPTABLE int splitSessionLeftRight(int sessionId);
    Q_SCRIPTABLE int splitSessionTopBottom(int sessionId);
    Q_SCRIPTABLE int splitContentLeftRight(int contentId);
    Q_SCRIPTABLE int splitContentTopBottom(int contentId);

    Q_SCRIPTABLE int tryGrowRight(int terminalId, uint pixels = 10);
    Q_SCRIPTABLE int tryGrowLeft(int terminalId, uint pixels = 10);
    Q_SCRIPTABLE int tryGrowTop(int id, uint pixels = 10);
    Q_SCRIPTABLE int tryGrowBottom(int id, uint pixels = 10);

    Q_SCRIPTABLE int activeSessionId()
    {
        return m_activeSessionId;
    }
    Q_SCRIPTABLE int activeId();

    Q_SCRIPTABLE const QString sessionIdList();
    Q_SCRIPTABLE const QString contentIdList();
    Q_SCRIPTABLE const QString contentIdsForSessionId(int sessionId);
    Q_SCRIPTABLE int sessionIdForContentId(int contentId);

#if defined(REMOVE_SENDTEXT_RUNCOMMAND_DBUS_METHODS)
    void runCommand(const QString &command);
#else
    Q_SCRIPTABLE void runCommand(const QString &command);
    Q_SCRIPTABLE void runCommandInTerminal(int terminalId, const QString &command);
    Q_SCRIPTABLE void runCommandInContent(int contentId, const QString &command);
#endif

    Q_SCRIPTABLE bool isSessionClosable(int sessionId);
    Q_SCRIPTABLE void setSessionClosable(int sessionId, bool closable);
    Q_SCRIPTABLE bool hasUnclosableSessions() const;

    Q_SCRIPTABLE bool isSessionKeyboardInputEnabled(int sessionId);
    Q_SCRIPTABLE void setSessionKeyboardInputEnabled(int sessionId, bool enabled);
    Q_SCRIPTABLE bool isContentKeyboardInputEnabled(int contentId);
    Q_SCRIPTABLE void setContentKeyboardInputEnabled(int contentId, bool enabled);
    Q_SCRIPTABLE bool hasContentWithKeyboardInputEnabled(int sessionId);
    Q_SCRIPTABLE bool hasContentWithKeyboardInputDisabled(int sessionId);

    Q_SCRIPTABLE bool isSessionMonitorActivityEnabled(int sessionId);
    Q_SCRIPTABLE void setSessionMonitorActivityEnabled(int sessionId, bool enabled);
    Q_SCRIPTABLE bool isContentMonitorActivityEnabled(int contentId);
    Q_SCRIPTABLE void setContentMonitorActivityEnabled(int contentId, bool enabled);
    Q_SCRIPTABLE bool hasContentWithMonitorActivityEnabled(int sessionId);
    Q_SCRIPTABLE bool hasContentWithMonitorActivityDisabled(int sessionId);

    Q_SCRIPTABLE bool isSessionMonitorSilenceEnabled(int sessionId);
    Q_SCRIPTABLE void setSessionMonitorSilenceEnabled(int sessionId, bool enabled);
    Q_SCRIPTABLE bool isContentMonitorSilenceEnabled(int contentId);
    Q_SCRIPTABLE void setContentMonitorSilenceEnabled(int contentId, bool enabled);
    Q_SCRIPTABLE bool hasContentWithMonitorSilenceEnabled(int sessionId);
    Q_SCRIPTABLE bool hasContentWithMonitorSilenceDisabled(int sessionId);

    void handleHighlightRequest(int id);

Q_SIGNALS:
    void sessionAdded(int sessionId, const QString &title);
    void sessionRaised(int sessionId);
    void sessionRemoved(int sessionId);

    void activeTitleChanged(const QString &title);
    void titleChanged(int sessionId, const QString &title);

    void previous();
    void next();

    void activityDetected(Session *session, int id);
    void silenceDetected(Session *session, int id);

    void manageProfiles();

    void removeTerminalHighlight();

    void wantsBlurChanged();

protected:
    void showEvent(QShowEvent *event) override;

private Q_SLOTS:
    void handleCurrentChanged(int index);
    void handleActivity(Terminal *terminal);
    void handleSilence(Terminal *terminal);
    void handleManualActivation(Terminal *terminal);
    void handleManualActivation(Browser *browser);

    void cleanup(int sessionId);

private:
    enum QueryCloseType {
        QueryCloseSession,
        QueryCloseTerminal,
        QueryCloseBrowser,
    };
    bool queryClose(int sessionId, QueryCloseType type);

    VisualEventOverlay *m_visualEventOverlay;

    int m_activeSessionId;

    QHash<int, Session *> m_sessions;
};

#endif
