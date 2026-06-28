#pragma once

#include "models/HeritageSite.h"
#include "models/TimelineFilterState.h"

#include <QWidget>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimer;

// TimelinePanel 是左側時間軸面板。
// 重點：它只負責 UI 控制與產生 TimelineFilterState，不直接操作 MapCanvas。
class TimelinePanel final : public QWidget
{
    Q_OBJECT

public:
    explicit TimelinePanel(QWidget *parent = nullptr);

    // 匯入或更新資料後，根據資料集重新計算年份範圍。
    void setHeritageSites(const QVector<HeritageSite> &sites);

    TimelineFilterState currentState() const;

signals:
    void timelineFilterChanged(const TimelineFilterState &state);

private slots:
    void handleStartYearChanged(int value);
    void handleEndYearChanged(int value);
    void handleEnabledChanged(int state);
    void handleShowUndatedChanged(int state);
    void handlePresetChanged(int index);

    void togglePlayback();
    void advancePlayback();
    void resetTimeline();

private:
    void createLayout();
    void populatePresets();

    void configureYearControls(int minYear, int maxYear);
    void setRangeValues(int startYear,
                        int endYear,
                        bool emitSignal,
                        bool markAsCustomPreset);

    void updateLabels();
    void emitCurrentState();

    void stopPlayback();
    void setPresetToCustom();
    void applyPreset(const QString &presetKey);

    int playbackStep() const;

private:
    bool m_isUpdating = false;
    bool m_hasDatedSites = false;

    int m_minYear = 0;
    int m_maxYear = 0;

    TimelineRangeInfo m_rangeInfo;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_currentRangeLabel = nullptr;

    QCheckBox *m_enableCheckBox = nullptr;
    QCheckBox *m_showUndatedCheckBox = nullptr;

    QComboBox *m_presetComboBox = nullptr;

    QSlider *m_startSlider = nullptr;
    QSlider *m_endSlider = nullptr;

    QSpinBox *m_startSpinBox = nullptr;
    QSpinBox *m_endSpinBox = nullptr;

    QPushButton *m_playButton = nullptr;
    QPushButton *m_resetButton = nullptr;

    QTimer *m_playTimer = nullptr;
};