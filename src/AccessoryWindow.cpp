#include "AccessoryWindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QStyle>
#include <QTime>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
constexpr int RefreshIntervalMs = 5 * 60 * 1000;
constexpr auto CodexUsageUrl = "https://chatgpt.com/codex/cloud/settings/analytics#usage";
constexpr auto ClaudeUsageUrl = "https://claude.ai/settings/usage";

QString cssColor(const QColor &color)
{
    return color.name(QColor::HexRgb);
}
}

AccessoryWindow::AccessoryWindow(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    loadSettings();

    connect(&m_usageClient, &UsageClient::usageReady, this, &AccessoryWindow::applyUsage);
    connect(&m_usageClient, &UsageClient::usageFailed, this, &AccessoryWindow::applyFailure);
    connect(&m_refreshTimer, &QTimer::timeout, &m_usageClient, &UsageClient::refresh);

    m_refreshTimer.start(RefreshIntervalMs);
    setBusy(true);
    m_usageClient.refresh();
}

void AccessoryWindow::buildUi()
{
    setWindowTitle("Claude Usage Monitor");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumSize(260, 124);

    setStyleSheet(R"(
        QLabel {
            color: #eef2f3;
            font-family: "Segoe UI";
        }
        QLabel#title {
            font-size: 13px;
            font-weight: 700;
        }
        QLabel#percent {
            font-size: 28px;
            font-weight: 700;
        }
        QLabel#muted {
            color: #9ca8ad;
            font-size: 11px;
        }
        QProgressBar {
            height: 8px;
            border: 0;
            border-radius: 4px;
            background: rgba(255, 255, 255, 26);
            text-align: center;
        }
        QProgressBar::chunk {
            border-radius: 4px;
            background: #44cc66;
        }
        QPushButton {
            color: #eef2f3;
            background: rgba(255, 255, 255, 24);
            border: 1px solid rgba(255, 255, 255, 34);
            border-radius: 6px;
            padding: 6px 9px;
        }
        QPushButton:hover {
            background: rgba(255, 255, 255, 38);
        }
        QPushButton#closeButton {
            min-width: 24px;
            max-width: 24px;
            min-height: 24px;
            max-height: 24px;
            padding: 0;
            border-radius: 12px;
            font-weight: 700;
        }
        QPushButton#closeButton:hover {
            background: rgba(255, 85, 85, 120);
            border-color: rgba(255, 120, 120, 150);
        }
    )");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *top = new QHBoxLayout();
    auto *titleStack = new QVBoxLayout();
    titleStack->setSpacing(1);
    m_title = new QLabel("Claude Usage");
    m_title->setObjectName("title");
    m_status = new QLabel("Updating...");
    m_status->setObjectName("muted");
    titleStack->addWidget(m_title);
    titleStack->addWidget(m_status);
    top->addLayout(titleStack, 1);
    m_percent = new QLabel("--%");
    m_percent->setObjectName("percent");
    top->addWidget(m_percent, 0, Qt::AlignRight | Qt::AlignVCenter);
    auto *closeButton = new QPushButton("X");
    closeButton->setObjectName("closeButton");
    closeButton->setToolTip("Quit");
    top->addWidget(closeButton, 0, Qt::AlignTop | Qt::AlignRight);
    layout->addLayout(top);

    m_fiveHourLabel = new QLabel("5h");
    m_fiveHourReset = new QLabel("--");
    m_fiveHourReset->setObjectName("muted");
    m_fiveHourBar = new QProgressBar();
    m_fiveHourBar->setRange(0, 100);
    m_fiveHourBar->setTextVisible(false);

    m_weekLabel = new QLabel("Week");
    m_weekReset = new QLabel("--");
    m_weekReset->setObjectName("muted");
    m_weekBar = new QProgressBar();
    m_weekBar->setRange(0, 100);
    m_weekBar->setTextVisible(false);

    m_opusLabel = new QLabel("Opus");
    m_opusReset = new QLabel("--");
    m_opusReset->setObjectName("muted");
    m_opusBar = new QProgressBar();
    m_opusBar->setRange(0, 100);
    m_opusBar->setTextVisible(false);

    m_details = new QWidget();
    auto *detailLayout = new QVBoxLayout(m_details);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(8);

    auto addRow = [detailLayout](QLabel *label, QProgressBar *bar, QLabel *reset) {
        auto *container = new QWidget();
        auto *row = new QVBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        auto *caption = new QHBoxLayout();
        caption->addWidget(label);
        caption->addStretch();
        caption->addWidget(reset);
        row->addLayout(caption);
        row->addWidget(bar);
        container->setLayout(row);
        detailLayout->addWidget(container);
        return container;
    };

    addRow(m_fiveHourLabel, m_fiveHourBar, m_fiveHourReset);
    addRow(m_weekLabel, m_weekBar, m_weekReset);
    m_opusRow = addRow(m_opusLabel, m_opusBar, m_opusReset);
    layout->addWidget(m_details);

    auto *buttons = new QHBoxLayout();
    auto *refresh = new QPushButton("Update");
    auto *claude = new QPushButton("Claude");
    auto *codex = new QPushButton("Codex");
    buttons->addWidget(refresh);
    buttons->addWidget(claude);
    buttons->addWidget(codex);
    layout->addLayout(buttons);

    m_lastUpdated = new QLabel("Double-click to collapse. Right-click for options.");
    m_lastUpdated->setObjectName("muted");
    layout->addWidget(m_lastUpdated);

    connect(refresh, &QPushButton::clicked, this, [this] {
        setBusy(true);
        m_usageClient.refresh();
    });
    connect(claude, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(ClaudeUsageUrl));
    });
    connect(codex, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(CodexUsageUrl));
    });
    connect(closeButton, &QPushButton::clicked, this, [this] {
        saveSettings();
        close();
        qApp->quit();
    });
}

void AccessoryWindow::loadSettings()
{
    restoreGeometry(m_settings.value("geometry").toByteArray());
    m_expanded = m_settings.value("expanded", true).toBool();
    m_alwaysOnTop = m_settings.value("alwaysOnTop", true).toBool();
    m_showResetDateTime = m_settings.value("showResetDateTime", false).toBool();
    m_opacity = m_settings.value("opacity", 0.92).toDouble();

    if (geometry().isNull()) {
        const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
        move(screen.right() - width() - 42, screen.top() + 72);
    }

    setExpanded(m_expanded);
    setOpacity(m_opacity);
    setAlwaysOnTop(m_alwaysOnTop);
}

void AccessoryWindow::saveSettings()
{
    m_settings.setValue("geometry", saveGeometry());
    m_settings.setValue("expanded", m_expanded);
    m_settings.setValue("alwaysOnTop", m_alwaysOnTop);
    m_settings.setValue("showResetDateTime", m_showResetDateTime);
    m_settings.setValue("opacity", m_opacity);
}

void AccessoryWindow::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_details->setVisible(expanded);
    adjustSize();
}

void AccessoryWindow::setAlwaysOnTop(bool enabled)
{
    m_alwaysOnTop = enabled;
    Qt::WindowFlags flags = windowFlags();
    flags.setFlag(Qt::WindowStaysOnTopHint, enabled);
    setWindowFlags(flags);
    show();
}

void AccessoryWindow::setOpacity(qreal opacity)
{
    m_opacity = qBound(0.45, opacity, 1.0);
    setWindowOpacity(m_opacity);
}

void AccessoryWindow::setShowResetDateTime(bool enabled)
{
    m_showResetDateTime = enabled;
}

void AccessoryWindow::setBusy(bool busy)
{
    if (busy) {
        m_status->setText("Updating...");
    }
}

void AccessoryWindow::applyUsage(const UsageStatus &status)
{
    m_lastStatus = status;
    m_hasLastStatus = true;

    const double maxUtil = qMax(status.fiveHour.utilization, status.sevenDay.utilization);
    const QColor color = colorFor(maxUtil);
    m_percent->setText(QString::number(status.fiveHour.utilization, 'f', 0) + "%");
    m_percent->setStyleSheet(QString("color: %1;").arg(cssColor(color)));
    m_status->setText(maxUtil >= 85.0 ? "High usage" : maxUtil >= 60.0 ? "Moderate usage" : "Available");
    setWindowIcon(dotIcon(color));

    updateWindowRow(m_fiveHourLabel, m_fiveHourBar, m_fiveHourReset, "5h", status.fiveHour);
    updateWindowRow(m_weekLabel, m_weekBar, m_weekReset, "Week", status.sevenDay);
    updateWindowRow(m_opusLabel, m_opusBar, m_opusReset, "Opus", status.sevenDayOpus);
    m_opusRow->setVisible(status.hasOpus);

    m_lastUpdated->setText("Updated " + QTime::currentTime().toString("HH:mm"));
}

void AccessoryWindow::applyFailure(const UsageFailure &failure)
{
    const QColor red("#ff5555");
    m_percent->setText("--%");
    m_percent->setStyleSheet(QString("color: %1;").arg(cssColor(red)));
    setWindowIcon(dotIcon(red));

    switch (failure.error) {
    case UsageError::MissingCredentials:
        m_status->setText("Claude Code login not found");
        m_lastUpdated->setText("Run Claude Code login, then update.");
        break;
    case UsageError::RateLimited:
        m_status->setText("Rate limited");
        m_lastUpdated->setText("Retry " + retryText(failure.retryAfterSeconds));
        break;
    case UsageError::Parse:
        m_status->setText("Unexpected response");
        m_lastUpdated->setText(failure.message);
        break;
    case UsageError::Network:
        m_status->setText("Network error");
        m_lastUpdated->setText(failure.message);
        break;
    }
}

void AccessoryWindow::updateWindowRow(QLabel *label, QProgressBar *bar, QLabel *resetLabel, const QString &name, const UsageWindow &window)
{
    const int value = qBound(0, qRound(window.utilization), 100);
    const QColor color = colorFor(window.utilization);
    label->setText(QString("%1  %2%").arg(name, QString::number(window.utilization, 'f', 1)));
    resetLabel->setText(m_showResetDateTime ? resetDateTimeText(window) : resetText(window));
    bar->setValue(value);
    bar->setStyleSheet(QString("QProgressBar::chunk { background: %1; border-radius: 4px; }").arg(cssColor(color)));
}

QString AccessoryWindow::resetText(const UsageWindow &window) const
{
    if (!window.hasReset) {
        return "--";
    }

    const qint64 seconds = QDateTime::currentDateTimeUtc().secsTo(window.resetsAt.toUTC());
    return retryText(static_cast<int>(seconds));
}

QString AccessoryWindow::resetDateTimeText(const UsageWindow &window) const
{
    if (!window.hasReset) {
        return "--";
    }

    return window.resetsAt.toLocalTime().toString("MM/dd HH:mm");
}

QString AccessoryWindow::retryText(int seconds) const
{
    if (seconds <= 0) {
        return "soon";
    }

    const int minutes = seconds / 60;
    const int hours = minutes / 60;
    const int days = hours / 24;
    if (days > 0) {
        return QString("%1d %2h").arg(days).arg(hours % 24);
    }
    if (hours > 0) {
        return QString("%1h %2m").arg(hours).arg(minutes % 60);
    }
    return QString("%1m").arg(qMax(1, minutes));
}

QColor AccessoryWindow::colorFor(double utilization) const
{
    if (utilization >= 85.0) {
        return QColor("#ff5555");
    }
    if (utilization >= 60.0) {
        return QColor("#ffb020");
    }
    return QColor("#44cc66");
}

QIcon AccessoryWindow::dotIcon(const QColor &color) const
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(8, 8, 48, 48);
    return QIcon(pixmap);
}

void AccessoryWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setExpanded(!m_expanded);
        saveSettings();
    }
}

void AccessoryWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
}

void AccessoryWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
    }
}

void AccessoryWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        saveSettings();
    }
}

void AccessoryWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF panel = rect().adjusted(1.0, 1.0, -1.0, -1.0);
    painter.setPen(QColor(255, 255, 255, 42));
    painter.setBrush(QColor(24, 28, 32, 228));
    painter.drawRoundedRect(panel, 10.0, 10.0);
}

void AccessoryWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *update = menu.addAction("Update now");
    QAction *expand = menu.addAction(m_expanded ? "Collapse" : "Expand");
    QAction *alwaysOnTop = menu.addAction("Always on top");
    alwaysOnTop->setCheckable(true);
    alwaysOnTop->setChecked(m_alwaysOnTop);
    QAction *resetDateTime = menu.addAction("Show reset date/time");
    resetDateTime->setCheckable(true);
    resetDateTime->setChecked(m_showResetDateTime);

    QMenu *opacityMenu = menu.addMenu("Opacity");
    const QList<QPair<QString, qreal>> opacityValues = {
        {"100%", 1.0},
        {"90%", 0.9},
        {"75%", 0.75},
        {"60%", 0.6},
    };
    for (const auto &item : opacityValues) {
        QAction *action = opacityMenu->addAction(item.first);
        action->setData(item.second);
        action->setCheckable(true);
        action->setChecked(qAbs(m_opacity - item.second) < 0.02);
    }

    menu.addSeparator();
    QAction *openClaude = menu.addAction("Open Claude usage");
    QAction *openCodex = menu.addAction("Open Codex usage");
    menu.addSeparator();
    QAction *quit = menu.addAction("Quit");

    QAction *chosen = menu.exec(event->globalPos());
    if (!chosen) {
        return;
    }

    if (chosen == update) {
        setBusy(true);
        m_usageClient.refresh();
    } else if (chosen == expand) {
        setExpanded(!m_expanded);
    } else if (chosen == alwaysOnTop) {
        setAlwaysOnTop(alwaysOnTop->isChecked());
    } else if (chosen == resetDateTime) {
        setShowResetDateTime(resetDateTime->isChecked());
        if (m_hasLastStatus) {
            applyUsage(m_lastStatus);
        }
    } else if (chosen == openClaude) {
        QDesktopServices::openUrl(QUrl(ClaudeUsageUrl));
    } else if (chosen == openCodex) {
        QDesktopServices::openUrl(QUrl(CodexUsageUrl));
    } else if (chosen == quit) {
        close();
        qApp->quit();
    } else if (chosen->data().isValid()) {
        setOpacity(chosen->data().toDouble());
    }
    saveSettings();
}

void AccessoryWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QWidget::closeEvent(event);
}
