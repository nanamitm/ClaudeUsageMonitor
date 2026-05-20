#pragma once

#include "UsageClient.h"
#include "UsageModel.h"

#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QProgressBar>
#include <QSettings>
#include <QTimer>
#include <QWidget>

class AccessoryWindow : public QWidget
{
    Q_OBJECT

public:
    explicit AccessoryWindow(QWidget *parent = nullptr);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void loadSettings();
    void saveSettings();
    void setExpanded(bool expanded);
    void setAlwaysOnTop(bool enabled);
    void setOpacity(qreal opacity);
    void setShowResetDateTime(bool enabled);
    void setBusy(bool busy);
    void applyUsage(const UsageStatus &status);
    void applyFailure(const UsageFailure &failure);
    void updateWindowRow(QLabel *label, QProgressBar *bar, QLabel *resetLabel, const QString &name, const UsageWindow &window);
    QString resetText(const UsageWindow &window) const;
    QString resetDateTimeText(const UsageWindow &window) const;
    QString retryText(int seconds) const;
    QColor colorFor(double utilization) const;
    QIcon dotIcon(const QColor &color) const;

    UsageClient m_usageClient;
    QTimer m_refreshTimer;
    QSettings m_settings;

    QLabel *m_title = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_percent = nullptr;
    QLabel *m_lastUpdated = nullptr;
    QWidget *m_details = nullptr;
    QWidget *m_opusRow = nullptr;
    QLabel *m_fiveHourLabel = nullptr;
    QLabel *m_fiveHourReset = nullptr;
    QProgressBar *m_fiveHourBar = nullptr;
    QLabel *m_weekLabel = nullptr;
    QLabel *m_weekReset = nullptr;
    QProgressBar *m_weekBar = nullptr;
    QLabel *m_opusLabel = nullptr;
    QLabel *m_opusReset = nullptr;
    QProgressBar *m_opusBar = nullptr;

    QPoint m_dragOffset;
    bool m_dragging = false;
    bool m_expanded = true;
    bool m_alwaysOnTop = true;
    bool m_showResetDateTime = false;
    bool m_hasLastStatus = false;
    UsageStatus m_lastStatus;
    qreal m_opacity = 0.92;
};
