/*
  SPDX-FileCopyrightText: 2008-2009 Eike Hein <hein@kde.org>
  SPDX-FileCopyrightText: 2009 Juan Carlos Torres <carlosdgtorres@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef SESSION_H
#define SESSION_H

#include "splitter.h"

#include <QObject>

class Terminal;
class Browser;

class Session : public QObject
{
    Q_OBJECT

public:
    enum SessionContent {
        TerminalType,
        BrowserType,
    };

    enum SessionType {
        Single,
        TwoHorizontal,
        TwoVertical,
        Quad,
    };
    enum GrowthDirection {
        Up,
        Right,
        Down,
        Left,
    };

    explicit Session(const QString &workingDir, SessionContent contentType = TerminalType, SessionType type = Single, QWidget *parent = nullptr);
    ~Session() override;

    int id() const
    {
        return m_sessionId;
    }
    const QString title()
    {
        return m_title;
    }
    QWidget *widget()
    {
        return m_baseSplitter;
    }

    int activeId() const
    {
        return m_activeId;
    }
    const QString terminalIdList();
    int terminalCount() const
    {
        return m_terminals.size();
    }
    int browserCount() const
    {
        return m_browsers.size();
    }
    bool hasTerminal(int terminalId);
    Terminal *getTerminal(int terminalId);
    void closeTerminal(int terminalId);
    bool hasBrowser(int browserId);
    Browser *getBrowser(int browserId);
    void closeBrowser(int browserId);

    bool closable() const
    {
        return m_closable;
    }
    void setClosable(bool closable)
    {
        m_closable = closable;
    }

    bool keyboardInputEnabled();
    void setKeyboardInputEnabled(bool enabled);
    bool keyboardInputEnabled(int terminalId);
    void setKeyboardInputEnabled(int terminalId, bool enabled);
    bool hasTerminalsWithKeyboardInputEnabled();
    bool hasTerminalsWithKeyboardInputDisabled();

    bool monitorActivityEnabled();
    void setMonitorActivityEnabled(bool enabled);
    bool monitorActivityEnabled(int terminalId);
    void setMonitorActivityEnabled(int terminalId, bool enabled);
    bool hasTerminalsWithMonitorActivityEnabled();
    bool hasTerminalsWithMonitorActivityDisabled();

    bool monitorSilenceEnabled();
    void setMonitorSilenceEnabled(bool enabled);
    bool monitorSilenceEnabled(int terminalId);
    void setMonitorSilenceEnabled(int terminalId, bool enabled);
    bool hasTerminalsWithMonitorSilenceEnabled();
    bool hasTerminalsWithMonitorSilenceDisabled();

    bool wantsBlur() const;

public Q_SLOTS:
    void closeSession(int id = -1);

    void focusNext();
    void focusPrevious();

    int splitAuto(int id = -1);
    int splitLeftRight(int id = -1);
    int splitTopBottom(int id = -1);

    int tryGrow(int id, GrowthDirection direction, uint pixels);

    void runCommand(const QString &command, int id = -1);

    void manageProfiles();
    void editProfile();

    void reconnectMonitorActivitySignals();

Q_SIGNALS:
    void titleChanged(const QString &title);
    void titleChanged(int sessionId, const QString &title);
    void terminalManuallyActivated(Terminal *terminal);
    void browserManuallyActivated(Browser *browser);
    void keyboardInputBlocked(Terminal *terminal);
    void activityDetected(Session *session, int id);
    void silenceDetected(Session *session, int id);
    void destroyed(int sessionId);
    void wantsBlurChanged();

private Q_SLOTS:
    void setActiveId(int Id);
    void setTitle(int Id, const QString &title);

    void cleanup(int Id);
    void cleanup();
    void prepareShutdown();

public:
    SessionContent contentType() const
    {
        return m_contentType;
    }

private:
    void setupSession(SessionType type);

    Terminal *addTerminal(QSplitter *parent, QString workingDir = QString());
    Browser *addBrowser(QSplitter *parent);
    int split(Terminal *terminal, Qt::Orientation orientation);
    int split(Browser *browser, Qt::Orientation orientation);

    QString m_workingDir;
    static int m_availableSessionId;
    int m_sessionId;

    Splitter *m_baseSplitter = nullptr;

    int m_activeId;
    SessionContent m_contentType;
    std::map<int, Terminal *> m_terminals;
    std::map<int, Browser *> m_browsers;

    QString m_title;

    bool m_closable;
};

#endif
