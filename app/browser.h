/*
  SPDX-FileCopyrightText: 2025 Eike Hein <hein@kde.org>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef BROWSER_H
#define BROWSER_H

#include <KParts/Part>

#include <QPointer>

class QKeyEvent;
class QLineEdit;
class QVBoxLayout;

class Browser : public QObject
{
    Q_OBJECT

public:
    explicit Browser(QWidget *parent = nullptr);
    ~Browser() override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    int id()
    {
        return m_browserId;
    }
    const QString title()
    {
        return m_title;
    }

    QWidget *partWidget()
    {
        return m_partWidget;
    }
    QWidget *browserWidget()
    {
        return m_browserWidget;
    }

    QLineEdit *urlBar()
    {
        return m_urlBar;
    }

    QWidget *splitter()
    {
        return m_parentSplitter;
    }
    void setSplitter(QWidget *splitter)
    {
        m_parentSplitter = splitter;
    }

    KActionCollection *actionCollection();

    bool closable() const
    {
        return m_closable;
    }
    void setClosable(bool closable)
    {
        m_closable = closable;
    }

    bool wantsBlur() const
    {
        return m_wantsBlur;
    }

Q_SIGNALS:
    void titleChanged(int browserId, const QString &title);
    void activated(int browserId);
    void manuallyActivated(Browser *browser);
    void destroyed(int browserId);
    void closeRequested(int browserId);

private Q_SLOTS:
    void setTitle(const QString &title);
    void openUrl();

private:
    void displayKPartLoadError();

    static int m_availableBrowserId;
    int m_browserId;

    KParts::Part *m_part = nullptr;
    QPointer<QWidget> m_partWidget = nullptr;
    QPointer<QWidget> m_browserWidget = nullptr;
    QWidget *m_parentSplitter;
    QVBoxLayout *m_layout;
    QLineEdit *m_urlBar;

    QString m_title;

    bool m_wantsBlur = false;

    bool m_closable = true;

    bool m_destroying = false;
    bool m_urlBarVisible = true;
};

#endif
