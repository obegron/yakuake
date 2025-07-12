/*
  SPDX-FileCopyrightText: 2025 Eike Hein <hein@kde.org>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "browser.h"

#include <KActionCollection>
#include <KColorScheme>
#include <KLocalizedString>
#include <KParts/PartLoader>
#include <KParts/ReadOnlyPart>
#include <KXMLGUIBuilder>
#include <KXMLGUIFactory>

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <QKeyEvent>

int Browser::m_availableBrowserId = 0;

Browser::Browser(QWidget *parent)
    : QObject(nullptr)
{
    m_browserId = m_availableBrowserId;
    m_availableBrowserId++;
    m_parentSplitter = parent;

    KPluginMetaData part(QStringLiteral("kf6/parts/webenginepart"));

    m_part = KParts::PartLoader::instantiatePart<KParts::Part>(part, parent).plugin;
    if (!m_part) {
        displayKPartLoadError();
        return;
    }

    connect(m_part, SIGNAL(setWindowCaption(QString)), this, SLOT(setTitle(QString)));
    connect(m_part, &KParts::Part::destroyed, this, [this] {
        m_part = nullptr;

        if (!m_destroying) {
            Q_EMIT closeRequested(m_browserId);
        }
    });

    m_partWidget = new QWidget(parent);
    m_layout = new QVBoxLayout(m_partWidget);
    m_browserWidget = m_part->widget();
    m_urlBar = new QLineEdit(m_partWidget);

    m_layout->addWidget(m_browserWidget);
    m_layout->addWidget(m_urlBar);

    connect(m_urlBar, &QLineEdit::returnPressed, this, &Browser::openUrl);

    if (m_browserWidget) {
        m_browserWidget->setFocusPolicy(Qt::WheelFocus);
        m_browserWidget->installEventFilter(this);

        if (!m_part->factory() && m_partWidget) {
            if (!m_part->clientBuilder()) {
                m_part->setClientBuilder(new KXMLGUIBuilder(m_partWidget));
            }

            auto factory = new KXMLGUIFactory(m_part->clientBuilder(), this);
            factory->addClient(m_part);

            // Prevents the KXMLGui warning about removing the client
            connect(m_partWidget, &QObject::destroyed, this, [factory, this] {
                factory->removeClient(m_part);
            });
        }
    }
}

Browser::~Browser()
{
    m_destroying = true;
    if (m_part) {
        delete m_part;
    }
}

bool Browser::eventFilter(QObject * /* watched */, QEvent *event)
{
    if (event->type() == QEvent::FocusIn) {
        Q_EMIT activated(m_browserId);

        QFocusEvent *focusEvent = static_cast<QFocusEvent *>(event);

        if (focusEvent->reason() == Qt::MouseFocusReason || focusEvent->reason() == Qt::OtherFocusReason || focusEvent->reason() == Qt::BacktabFocusReason)
            Q_EMIT manuallyActivated(this);
    }

    return false;
}

void Browser::displayKPartLoadError()
{
    qDebug() << "Available parts in kf6/parts:" << KPluginMetaData::findPlugins(QStringLiteral("kf6/parts")).size();
    KColorScheme colorScheme(QPalette::Active);
    QColor warningColor = colorScheme.background(KColorScheme::NeutralBackground).color();
    QColor warningColorLight = KColorScheme::shade(warningColor, KColorScheme::LightShade, 0.1);
    QString gradient = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1,stop: 0 %1, stop: 0.6 %1, stop: 1.0 %2)");
    gradient = gradient.arg(warningColor.name(), warningColorLight.name());
    QString styleSheet = QStringLiteral("QLabel { background: %1; }");

    QWidget *widget = new QWidget(m_parentSplitter);
    widget->setStyleSheet(styleSheet.arg(gradient));
    m_partWidget = widget;
    m_browserWidget = widget;
    m_browserWidget->setFocusPolicy(Qt::WheelFocus);
    m_browserWidget->installEventFilter(this);

    QLabel *label = new QLabel(widget);
    label->setContentsMargins(10, 10, 10, 10);
    label->setWordWrap(false);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QString availableParts;
    const auto plugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/parts"));
    if (!plugins.isEmpty()) {
        QStringList partNames;
        for (const auto &plugin : plugins) {
            partNames << plugin.pluginId();
        }
        availableParts = QStringLiteral("<br/>Available KParts: %1").arg(partNames.join(QStringLiteral(", ")));
    }

    label->setText(xi18nc("@info",
                          "<application>Yakuake</application> was unable to load the <application>WebEnginePart</application> part.<nl/>A "
                          "<application>WebEnginePart</application> installation is required to use Yakuake.%1")
                       .arg(availableParts));

    QLabel *icon = new QLabel(widget);
    icon->setContentsMargins(10, 10, 10, 10);
    icon->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-warning")).pixmap(QSize(48, 48)));
    icon->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->addWidget(icon);
    layout->addWidget(label);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setStretchFactor(icon, 1);
    layout->setStretchFactor(label, 5);
}

void Browser::setTitle(const QString &title)
{
    m_title = title;

    Q_EMIT titleChanged(m_browserId, m_title);
}

void Browser::openUrl()
{
    KParts::ReadOnlyPart *readOnlyPart = qobject_cast<KParts::ReadOnlyPart *>(m_part);
    if (readOnlyPart) {
        readOnlyPart->openUrl(QUrl(m_urlBar->text()));
    }
}

KActionCollection *Browser::actionCollection()
{
    if (m_part->factory()) {
        const auto guiClients = m_part->childClients();
        for (auto *client : guiClients) {
            if (client->actionCollection()->associatedWidgets().contains(m_browserWidget)) {
                return client->actionCollection();
            }
        }
    }

    return nullptr;
}

#include "moc_browser.cpp"
