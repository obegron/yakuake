/*
  SPDX-FileCopyrightText: 2008-2014 Eike Hein <hein@kde.org>
  SPDX-FileCopyrightText: 2009 Juan Carlos Torres <carlosdgtorres@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "session.h"
#include "browser.h"
#include "terminal.h"

#include <algorithm>

int Session::m_availableSessionId = 0;

Session::Session(const QString &workingDir, SessionContent contentType, SessionType type, QWidget *parent)
    : QObject(parent)
{
    m_workingDir = workingDir;
    m_sessionId = m_availableSessionId;
    m_availableSessionId++;
    m_contentType = contentType;

    m_activeId = -1;

    m_closable = true;

    m_baseSplitter = new Splitter(Qt::Horizontal, parent);
    connect(m_baseSplitter, SIGNAL(destroyed()), this, SLOT(prepareShutdown()));

    setupSession(type);
}

Session::~Session()
{
    if (m_baseSplitter)
        delete m_baseSplitter;

    for (auto pair : m_terminals) {
        delete pair.second;
    }
    m_terminals.clear();

    for (auto pair : m_browsers) {
        delete pair.second;
    }
    m_browsers.clear();

    Q_EMIT destroyed(m_sessionId);
}

void Session::setupSession(SessionType type)
{
    switch (m_contentType) {
    case TerminalType: {
        switch (type) {
        case Single: {
            Terminal *terminal = addTerminal(m_baseSplitter);
            setActiveId(terminal->id());
            m_baseSplitter->setSizes(QList<int>() << 100);

            break;
        }

        case TwoHorizontal: {
            int splitterWidth = m_baseSplitter->width();

            Terminal *terminal = addTerminal(m_baseSplitter);
            addTerminal(m_baseSplitter);

            QList<int> newSplitterSizes;
            newSplitterSizes << (splitterWidth / 2) << (splitterWidth / 2);
            m_baseSplitter->setSizes(newSplitterSizes);

            QWidget *terminalWidget = terminal->terminalWidget();

            if (terminalWidget) {
                terminalWidget->setFocus();
                setActiveId(terminal->id());
            }

            break;
        }

        case TwoVertical: {
            m_baseSplitter->setOrientation(Qt::Vertical);

            int splitterHeight = m_baseSplitter->height();

            Terminal *terminal = addTerminal(m_baseSplitter);
            addTerminal(m_baseSplitter);

            QList<int> newSplitterSizes;
            newSplitterSizes << (splitterHeight / 2) << (splitterHeight / 2);
            m_baseSplitter->setSizes(newSplitterSizes);

            QWidget *terminalWidget = terminal->terminalWidget();

            if (terminalWidget) {
                terminalWidget->setFocus();
                setActiveId(terminal->id());
            }

            break;
        }

        case Quad: {
            int splitterWidth = m_baseSplitter->width();
            int splitterHeight = m_baseSplitter->height();

            m_baseSplitter->setOrientation(Qt::Vertical);

            Splitter *upperSplitter = new Splitter(Qt::Horizontal, m_baseSplitter);
            connect(upperSplitter, SIGNAL(destroyed()), this, SLOT(cleanup()));

            Splitter *lowerSplitter = new Splitter(Qt::Horizontal, m_baseSplitter);
            connect(lowerSplitter, SIGNAL(destroyed()), this, SLOT(cleanup()));

            Terminal *terminal = addTerminal(upperSplitter);
            addTerminal(upperSplitter);

            addTerminal(lowerSplitter);
            addTerminal(lowerSplitter);

            QList<int> newSplitterSizes;
            newSplitterSizes << (splitterHeight / 2) << (splitterHeight / 2);
            m_baseSplitter->setSizes(newSplitterSizes);

            newSplitterSizes.clear();
            newSplitterSizes << (splitterWidth / 2) << (splitterWidth / 2);
            upperSplitter->setSizes(newSplitterSizes);
            lowerSplitter->setSizes(newSplitterSizes);

            QWidget *terminalWidget = terminal->terminalWidget();

            if (terminalWidget) {
                terminalWidget->setFocus();
                setActiveId(terminal->id());
            }

            break;
        }

        default: {
            addTerminal(m_baseSplitter);

            break;
        }
        }
        break;
    }
    case BrowserType: {
        Browser *browser = addBrowser(m_baseSplitter);
        setActiveId(browser->id());
        m_baseSplitter->setSizes(QList<int>() << 100);
        break;
    }
    }
}

Terminal *Session::addTerminal(QSplitter *parent, QString workingDir)
{
    if (workingDir.isEmpty()) {
        // fallback to session's default working dir
        workingDir = m_workingDir;
    }

    Terminal *terminal = new Terminal(workingDir, parent);
    connect(terminal, SIGNAL(activated(int)), this, SLOT(setActiveId(int)));
    connect(terminal, SIGNAL(manuallyActivated(Terminal *)), this, SIGNAL(terminalManuallyActivated(Terminal *)));
    connect(terminal, SIGNAL(titleChanged(int, QString)), this, SLOT(setTitle(int, QString)));
    connect(terminal, SIGNAL(keyboardInputBlocked(Terminal *)), this, SIGNAL(keyboardInputBlocked(Terminal *)));
    connect(terminal, SIGNAL(silenceDetected(Terminal *)), this, SIGNAL(silenceDetected(Session *, int)));
    connect(terminal, &Terminal::closeRequested, this, QOverload<int>::of(&Session::cleanup));

    m_terminals[terminal->id()] = terminal;

    Q_EMIT wantsBlurChanged();

    parent->addWidget(terminal->partWidget());
    QWidget *terminalWidget = terminal->terminalWidget();
    if (terminalWidget)
        terminalWidget->setFocus();

    return terminal;
}

Browser *Session::addBrowser(QSplitter *parent)
{
    Browser *browser = new Browser(parent);
    connect(browser, SIGNAL(activated(int)), this, SLOT(setActiveId(int)));
    connect(browser, SIGNAL(manuallyActivated(Browser *)), this, SIGNAL(browserManuallyActivated(Browser *)));
    connect(browser, SIGNAL(titleChanged(int, QString)), this, SLOT(setTitle(int, QString)));
    connect(browser, &Browser::closeRequested, this, QOverload<int>::of(&Session::cleanup));

    m_browsers[browser->id()] = browser;

    Q_EMIT wantsBlurChanged();

    parent->addWidget(browser->partWidget());
    QWidget *browserWidget = browser->browserWidget();
    if (browserWidget)
        browserWidget->setFocus();

    return browser;
}

void Session::closeSession(int id)
{
    if (id == -1)
        id = m_activeId;
    if (id == -1)
        return;

    if (m_contentType == TerminalType) {
        if (!m_terminals.contains(id))
            return;
    } else if (m_contentType == BrowserType) {
        if (!m_browsers.contains(id))
            return;
    }

    cleanup(id);
}

void Session::focusPrevious()
{
    if (m_activeId == -1)
        return;

    if (m_contentType == Session::TerminalType) {
        if (!m_terminals.contains(m_activeId))
            return;

        std::map<int, Terminal *>::iterator current = m_terminals.find(m_activeId);

        std::map<int, Terminal *>::iterator previous;

        if (current == m_terminals.begin()) {
            previous = std::prev(m_terminals.end());
        } else {
            previous = std::prev(current);
        }

        QWidget *widget = previous->second->terminalWidget();
        if (widget) {
            widget->setFocus();
        }
    } else if (m_contentType == Session::BrowserType) {
        if (!m_browsers.contains(m_activeId))
            return;

        std::map<int, Browser *>::iterator current = m_browsers.find(m_activeId);

        std::map<int, Browser *>::iterator previous;

        if (current == m_browsers.begin()) {
            previous = std::prev(m_browsers.end());
        } else {
            previous = std::prev(current);
        }

        QWidget *widget = previous->second->browserWidget();
        if (widget) {
            widget->setFocus();
        }
    }
}

void Session::focusNext()
{
    if (m_activeId == -1)
        return;

    if (m_contentType == TerminalType) {
        if (!m_terminals.contains(m_activeId))
            return;

        std::map<int, Terminal *>::iterator current = m_terminals.find(m_activeId);

        std::map<int, Terminal *>::iterator next = std::next(current);

        if (next == m_terminals.end()) {
            next = m_terminals.begin();
        }

        QWidget *widget = next->second->terminalWidget();
        if (widget) {
            widget->setFocus();
        }
    } else if (m_contentType == BrowserType) {
        if (!m_browsers.contains(m_activeId))
            return;

        std::map<int, Browser *>::iterator current = m_browsers.find(m_activeId);

        std::map<int, Browser *>::iterator next = std::next(current);

        if (next == m_browsers.end()) {
            next = m_browsers.begin();
        }

        QWidget *widget = next->second->browserWidget();
        if (widget) {
            widget->setFocus();
        }
    }
}

int Session::splitLeftRight(int id)
{
    if (id == -1)
        id = m_activeId;
    if (id == -1)
        return -1;

    if (m_contentType == TerminalType) {
        if (!m_terminals.contains(id))
            return -1;

        Terminal *terminal = m_terminals[id];

        if (terminal)
            return split(terminal, Qt::Horizontal);
        else
            return -1;
    } else if (m_contentType == BrowserType) {
        if (!m_browsers.contains(id))
            return -1;

        Browser *browser = m_browsers[id];

        if (browser)
            return split(browser, Qt::Horizontal);
        else
            return -1;
    }
    return -1;
}

int Session::splitTopBottom(int id)
{
    if (id == -1)
        id = m_activeId;
    if (id == -1)
        return -1;

    if (m_contentType == TerminalType) {
        if (!m_terminals.contains(id))
            return -1;

        Terminal *terminal = m_terminals[id];

        if (terminal)
            return split(terminal, Qt::Vertical);
        else
            return -1;
    } else if (m_contentType == BrowserType) {
        if (!m_browsers.contains(id))
            return -1;

        Browser *browser = m_browsers[id];

        if (browser)
            return split(browser, Qt::Vertical);
        else
            return -1;
    }
    return -1;
}

int Session::splitAuto(int id)
{
    if (id == -1)
        id = m_activeId;
    if (id == -1)
        return -1;

    if (m_contentType == TerminalType) {
        if (!m_terminals.contains(id))
            return -1;

        Terminal *terminal = m_terminals[id];

        if (terminal) {
            if (terminal->partWidget()->width() > terminal->partWidget()->height())
                return split(terminal, Qt::Horizontal);
            else
                return split(terminal, Qt::Vertical);
        } else
            return -1;
    } else if (m_contentType == BrowserType) {
        if (!m_browsers.contains(id))
            return -1;

        Browser *browser = m_browsers[id];

        if (browser) {
            if (browser->partWidget()->width() > browser->partWidget()->height())
                return split(browser, Qt::Horizontal);
            else
                return split(browser, Qt::Vertical);
        } else
            return -1;
    }
    return -1;
}

int Session::split(Terminal *terminal, Qt::Orientation orientation)
{
    Splitter *splitter = static_cast<Splitter *>(terminal->splitter());

    if (splitter->count() == 1) {
        int splitterWidth = splitter->width();

        if (splitter->orientation() != orientation)
            splitter->setOrientation(orientation);

        terminal = addTerminal(splitter, terminal->currentWorkingDirectory());

        QList<int> newSplitterSizes;
        newSplitterSizes << (splitterWidth / 2) << (splitterWidth / 2);
        splitter->setSizes(newSplitterSizes);

        QWidget *partWidget = terminal->partWidget();
        if (partWidget)
            partWidget->show();

        m_activeId = terminal->id();
    } else {
        QList<int> splitterSizes = splitter->sizes();

        Splitter *newSplitter = new Splitter(orientation, splitter);
        connect(newSplitter, SIGNAL(destroyed()), this, SLOT(cleanup()));

        if (splitter->indexOf(terminal->partWidget()) == 0)
            splitter->insertWidget(0, newSplitter);

        QWidget *partWidget = terminal->partWidget();
        if (partWidget)
            partWidget->setParent(newSplitter);

        terminal->setSplitter(newSplitter);

        terminal = addTerminal(newSplitter, terminal->currentWorkingDirectory());

        splitter->setSizes(splitterSizes);
        QList<int> newSplitterSizes;
        newSplitterSizes << (splitterSizes[1] / 2) << (splitterSizes[1] / 2);
        newSplitter->setSizes(newSplitterSizes);

        newSplitter->show();

        partWidget = terminal->partWidget();
        if (partWidget)
            partWidget->show();

        m_activeId = terminal->id();
    }

    return m_activeId;
}

int Session::split(Browser *browser, Qt::Orientation orientation)
{
    Splitter *splitter = static_cast<Splitter *>(browser->splitter());

    if (splitter->count() == 1) {
        int splitterWidth = splitter->width();

        if (splitter->orientation() != orientation)
            splitter->setOrientation(orientation);

        browser = addBrowser(splitter);

        QList<int> newSplitterSizes;
        newSplitterSizes << (splitterWidth / 2) << (splitterWidth / 2);
        splitter->setSizes(newSplitterSizes);

        QWidget *partWidget = browser->partWidget();
        if (partWidget)
            partWidget->show();

        m_activeId = browser->id();
    } else {
        QList<int> splitterSizes = splitter->sizes();

        Splitter *newSplitter = new Splitter(orientation, splitter);
        connect(newSplitter, SIGNAL(destroyed()), this, SLOT(cleanup()));

        if (splitter->indexOf(browser->partWidget()) == 0)
            splitter->insertWidget(0, newSplitter);

        QWidget *partWidget = browser->partWidget();
        if (partWidget)
            partWidget->setParent(newSplitter);

        browser->setSplitter(newSplitter);

        browser = addBrowser(newSplitter);

        splitter->setSizes(splitterSizes);
        QList<int> newSplitterSizes;
        newSplitterSizes << (splitterSizes[1] / 2) << (splitterSizes[1] / 2);
        newSplitter->setSizes(newSplitterSizes);

        newSplitter->show();

        partWidget = browser->partWidget();
        if (partWidget)
            partWidget->show();

        m_activeId = browser->id();
    }

    return m_activeId;
}

int Session::tryGrow(int id, GrowthDirection direction, uint pixels)
{
    QWidget *child = nullptr;
    if (m_contentType == TerminalType) {
        Terminal *terminal = getTerminal(id);
        if (!terminal)
            return -1;
        child = terminal->partWidget();
    } else if (m_contentType == BrowserType) {
        Browser *browser = getBrowser(id);
        if (!browser)
            return -1;
        child = browser->partWidget();
    }

    if (!child)
        return -1;

    Splitter *splitter = static_cast<Splitter *>(child->parentWidget());

    while (splitter) {
        bool isHorizontal = (direction == Right || direction == Left);
        bool isForward = (direction == Down || direction == Right);

        // Detecting correct orientation.
        if ((splitter->orientation() == Qt::Horizontal && isHorizontal) || (splitter->orientation() == Qt::Vertical && !isHorizontal)) {
            int currentPos = splitter->indexOf(child);

            if (currentPos != -1 // Next/Prev movable element detection.
                && (currentPos != 0 || isForward) && (currentPos != splitter->count() - 1 || !isForward)) {
                QList<int> currentSizes = splitter->sizes();
                int oldSize = currentSizes[currentPos];

                int affected = isForward ? currentPos + 1 : currentPos - 1;
                currentSizes[currentPos] += pixels;
                currentSizes[affected] -= pixels;
                splitter->setSizes(currentSizes);

                return splitter->sizes().at(currentPos) - oldSize;
            }
        }
        // Try with a higher level.
        child = splitter;
        splitter = static_cast<Splitter *>(splitter->parentWidget());
    }

    return -1;
}

void Session::setActiveId(int id)
{
    m_activeId = id;

    if (m_contentType == TerminalType) {
        setTitle(m_activeId, m_terminals[m_activeId]->title());
    } else if (m_contentType == BrowserType) {
        setTitle(m_activeId, m_browsers[m_activeId]->title());
    }
}

void Session::setTitle(int id, const QString &title)
{
    if (id == m_activeId) {
        m_title = title;

        Q_EMIT titleChanged(m_title);
        Q_EMIT titleChanged(m_sessionId, m_title);
    }
}

void Session::cleanup(int id)
{
    if (m_contentType == TerminalType) {
        if (m_activeId == id && m_terminals.size() > 1)
            focusPrevious();

        delete m_terminals[id];
        m_terminals.erase(id);
    } else if (m_contentType == BrowserType) {
        if (m_activeId == id && m_browsers.size() > 1)
            focusPrevious();

        delete m_browsers[id];
        m_browsers.erase(id);
    }
    Q_EMIT wantsBlurChanged();

    cleanup();
}

void Session::cleanup()
{
    if (!m_baseSplitter)
        return;

    m_baseSplitter->recursiveCleanup();

    if (m_terminals.empty() && m_browsers.empty())
        m_baseSplitter->deleteLater();
}

void Session::prepareShutdown()
{
    m_baseSplitter = nullptr;

    deleteLater();
}

const QString Session::terminalIdList()
{
    QStringList idList;
    if (m_contentType == TerminalType) {
        for (auto &[id, terminal] : m_terminals) {
            idList << QString::number(id);
        }
    } else if (m_contentType == BrowserType) {
        for (auto &[id, browser] : m_browsers) {
            idList << QString::number(id);
        }
    }

    return idList.join(QLatin1Char(','));
}

bool Session::hasTerminal(int terminalId)
{
    return m_terminals.contains(terminalId);
}

Terminal *Session::getTerminal(int terminalId)
{
    if (!m_terminals.contains(terminalId))
        return nullptr;

    return m_terminals[terminalId];
}

void Session::closeTerminal(int terminalId)
{
    if (!m_terminals.contains(terminalId))
        return;

    delete m_terminals[terminalId];
    m_terminals.erase(terminalId);

    cleanup();
}

bool Session::hasBrowser(int browserId)
{
    return m_browsers.contains(browserId);
}

Browser *Session::getBrowser(int browserId)
{
    if (!m_browsers.contains(browserId))
        return nullptr;

    return m_browsers[browserId];
}

void Session::closeBrowser(int browserId)
{
    if (!m_browsers.contains(browserId))
        return;

    delete m_browsers[browserId];
    m_browsers.erase(browserId);

    cleanup();
}

void Session::runCommand(const QString &command, int id)
{
    if (id == -1)
        id = m_activeId;
    if (id == -1)
        return;

    if (m_contentType == TerminalType) {
        if (!m_terminals.contains(id))
            return;
        m_terminals[id]->runCommand(command);
    }
}

void Session::manageProfiles()
{
    if (m_activeId == -1)
        return;

    if (m_contentType == TerminalType) {
        if (!m_terminals.contains(m_activeId))
            return;
        m_terminals[m_activeId]->manageProfiles();
    }
}

void Session::editProfile()
{
    if (m_activeId == -1)
        return;

    if (m_contentType == TerminalType) {
        if (!m_terminals.contains(m_activeId))
            return;
        m_terminals[m_activeId]->editProfile();
    }
}

bool Session::keyboardInputEnabled()
{
    return std::all_of(m_terminals.cbegin(), m_terminals.cend(), [](auto &it) {
        auto &[id, terminal] = it;
        return terminal->keyboardInputEnabled();
    });
}

void Session::setKeyboardInputEnabled(bool enabled)
{
    for (auto &[id, terminal] : m_terminals) {
        terminal->setKeyboardInputEnabled(enabled);
    }
}

bool Session::keyboardInputEnabled(int terminalId)
{
    if (!m_terminals.contains(terminalId))
        return false;

    return m_terminals[terminalId]->keyboardInputEnabled();
}

void Session::setKeyboardInputEnabled(int terminalId, bool enabled)
{
    if (!m_terminals.contains(terminalId))
        return;

    m_terminals[terminalId]->setKeyboardInputEnabled(enabled);
}

bool Session::hasTerminalsWithKeyboardInputEnabled()
{
    return std::any_of(m_terminals.cbegin(), m_terminals.cend(), [](auto &it) {
        auto &[id, terminal] = it;
        return terminal->keyboardInputEnabled();
    });
}

bool Session::hasTerminalsWithKeyboardInputDisabled()
{
    return std::any_of(m_terminals.cbegin(), m_terminals.cend(), [](auto &it) {
        auto &[id, terminal] = it;
        return !terminal->keyboardInputEnabled();
    });
}

bool Session::monitorActivityEnabled()
{
    return std::all_of(m_terminals.cbegin(), m_terminals.cend(), [](auto &it) {
        auto &[id, terminal] = it;
        return terminal->monitorActivityEnabled();
    });
}

void Session::setMonitorActivityEnabled(bool enabled)
{
    for (auto &[id, terminal] : m_terminals) {
        setMonitorActivityEnabled(id, enabled);
    }
}

bool Session::monitorActivityEnabled(int terminalId)
{
    if (!m_terminals.contains(terminalId))
        return false;

    return m_terminals[terminalId]->monitorActivityEnabled();
}

void Session::setMonitorActivityEnabled(int terminalId, bool enabled)
{
    if (!m_terminals.contains(terminalId))
        return;

    Terminal *terminal = m_terminals[terminalId];

    connect(terminal, SIGNAL(activityDetected(Terminal *)), this, SIGNAL(activityDetected(Session *, int)), Qt::UniqueConnection);

    terminal->setMonitorActivityEnabled(enabled);
}

bool Session::hasTerminalsWithMonitorActivityEnabled()
{
    return std::any_of(m_terminals.cbegin(), m_terminals.cend(), [](auto &it) {
        auto &[id, terminal] = it;
        return terminal->monitorActivityEnabled();
    });
}

bool Session::hasTerminalsWithMonitorActivityDisabled()
{
    return std::any_of(m_terminals.cbegin(), m_terminals.cend(), [](auto &it) {
        auto &[id, terminal] = it;
        return !terminal->monitorActivityEnabled();
    });
}

void Session::reconnectMonitorActivitySignals()
{
    for (auto &[id, terminal] : m_terminals) {
        // clang-format off
        connect(terminal, SIGNAL(activityDetected(Terminal*)), this, SIGNAL(activityDetected(Session*,int)), Qt::UniqueConnection);
        // clang-format on
    }
}

bool Session::monitorSilenceEnabled()
{
    return std::all_of(m_terminals.cbegin(), m_terminals.cend(), [](auto &it) {
        auto &[id, terminal] = it;
        return terminal->monitorSilenceEnabled();
    });
}

void Session::setMonitorSilenceEnabled(bool enabled)
{
    for (auto &[id, terminal] : m_terminals) {
        terminal->setMonitorSilenceEnabled(enabled);
    }
}

bool Session::monitorSilenceEnabled(int terminalId)
{
    if (!m_terminals.contains(terminalId))
        return false;

    return m_terminals[terminalId]->monitorSilenceEnabled();
}

void Session::setMonitorSilenceEnabled(int terminalId, bool enabled)
{
    if (!m_terminals.contains(terminalId))
        return;

    m_terminals[terminalId]->setMonitorSilenceEnabled(enabled);
}

bool Session::hasTerminalsWithMonitorSilenceDisabled()
{
    return std::any_of(m_terminals.cbegin(), m_terminals.cend(), [](auto &it) {
        auto &[id, terminal] = it;
        return !terminal->monitorSilenceEnabled();
    });
}

bool Session::hasTerminalsWithMonitorSilenceEnabled()
{
    return std::any_of(m_terminals.cbegin(), m_terminals.cend(), [](auto &it) {
        auto &[id, terminal] = it;
        return terminal->monitorSilenceEnabled();
    });
}

bool Session::wantsBlur() const
{
    if (m_contentType == Session::TerminalType) {
        return std::all_of(m_terminals.cbegin(), m_terminals.cend(), [](auto &it) {
            auto &[id, terminal] = it;
            return terminal->wantsBlur();
        });
    } else if (m_contentType == Session::BrowserType) {
        return std::all_of(m_browsers.cbegin(), m_browsers.cend(), [](auto &it) {
            auto &[id, browser] = it;
            return browser->wantsBlur();
        });
    }
    return false;
}

#include "moc_session.cpp"
