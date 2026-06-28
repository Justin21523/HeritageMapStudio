#include "ui/TimelinePanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>

namespace {
constexpr int kPlaybackIntervalMs = 260;
constexpr int kDefaultMinYear = 1600;
constexpr int kDefaultMaxYear = 2026;
}

TimelinePanel::TimelinePanel(QWidget *parent)
    : QWidget(parent),
      m_playTimer(new QTimer(this))
{
    createLayout();

    connect(m_playTimer, &QTimer::timeout,
            this, &TimelinePanel::advancePlayback);

    // 初始狀態先給一個安全範圍；真正資料載入後會由 setHeritageSites() 更新。
    configureYearControls(kDefaultMinYear, kDefaultMaxYear);
    setRangeValues(kDefaultMinYear, kDefaultMaxYear, false, false);
    updateLabels();
}

void TimelinePanel::setHeritageSites(const QVector<HeritageSite> &sites)
{
    stopPlayback();

    m_rangeInfo = TimelineRangeInfo::fromSites(sites);
    m_hasDatedSites = m_rangeInfo.hasDatedSites;

    if (!m_hasDatedSites) {
        configureYearControls(kDefaultMinYear, kDefaultMaxYear);
        setRangeValues(kDefaultMinYear, kDefaultMaxYear, false, false);

        m_enableCheckBox->setChecked(false);
        m_enableCheckBox->setEnabled(false);
        m_showUndatedCheckBox->setEnabled(false);
        m_presetComboBox->setEnabled(false);
        m_startSlider->setEnabled(false);
        m_endSlider->setEnabled(false);
        m_startSpinBox->setEnabled(false);
        m_endSpinBox->setEnabled(false);
        m_playButton->setEnabled(false);
        m_resetButton->setEnabled(false);

        updateLabels();
        emitCurrentState();
        return;
    }

    configureYearControls(m_rangeInfo.minYear, m_rangeInfo.maxYear);

    m_enableCheckBox->setEnabled(true);
    m_enableCheckBox->setChecked(true);
    m_showUndatedCheckBox->setEnabled(true);
    m_showUndatedCheckBox->setChecked(true);
    m_presetComboBox->setEnabled(true);
    m_startSlider->setEnabled(true);
    m_endSlider->setEnabled(true);
    m_startSpinBox->setEnabled(true);
    m_endSpinBox->setEnabled(true);
    m_playButton->setEnabled(true);
    m_resetButton->setEnabled(true);

    // 資料更新後，預設顯示完整資料集。
    setRangeValues(m_rangeInfo.minYear, m_rangeInfo.maxYear, false, false);

    const int fullDatasetIndex = m_presetComboBox->findData("full_dataset");
    if (fullDatasetIndex >= 0) {
        m_presetComboBox->setCurrentIndex(fullDatasetIndex);
    }

    updateLabels();
    emitCurrentState();
}

TimelineFilterState TimelinePanel::currentState() const
{
    if (!m_hasDatedSites || !m_enableCheckBox->isChecked()) {
        TimelineFilterState state = TimelineFilterState::createDisabled();
        state.showUndatedSites = m_showUndatedCheckBox->isChecked();
        return state;
    }

    return TimelineFilterState::createForRange(m_startSpinBox->value(),
                                               m_endSpinBox->value(),
                                               m_showUndatedCheckBox->isChecked());
}

void TimelinePanel::createLayout()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    m_titleLabel = new QLabel("Timeline Filter", this);
    m_titleLabel->setObjectName("TimelinePanelTitleLabel");

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);

    m_currentRangeLabel = new QLabel(this);
    m_currentRangeLabel->setWordWrap(true);

    m_enableCheckBox = new QCheckBox("Enable timeline filter", this);
    m_enableCheckBox->setChecked(true);

    m_showUndatedCheckBox = new QCheckBox("Show undated sites", this);
    m_showUndatedCheckBox->setChecked(true);

    connect(m_enableCheckBox, &QCheckBox::stateChanged,
            this, &TimelinePanel::handleEnabledChanged);
    connect(m_showUndatedCheckBox, &QCheckBox::stateChanged,
            this, &TimelinePanel::handleShowUndatedChanged);

    m_presetComboBox = new QComboBox(this);
    populatePresets();

    connect(m_presetComboBox, &QComboBox::currentIndexChanged,
            this, &TimelinePanel::handlePresetChanged);

    m_startSlider = new QSlider(Qt::Horizontal, this);
    m_endSlider = new QSlider(Qt::Horizontal, this);

    m_startSlider->setTickPosition(QSlider::TicksBelow);
    m_endSlider->setTickPosition(QSlider::TicksBelow);

    m_startSpinBox = new QSpinBox(this);
    m_endSpinBox = new QSpinBox(this);

    connect(m_startSlider, &QSlider::valueChanged,
            this, &TimelinePanel::handleStartYearChanged);
    connect(m_endSlider, &QSlider::valueChanged,
            this, &TimelinePanel::handleEndYearChanged);

    connect(m_startSpinBox, qOverload<int>(&QSpinBox::valueChanged),
            this, &TimelinePanel::handleStartYearChanged);
    connect(m_endSpinBox, qOverload<int>(&QSpinBox::valueChanged),
            this, &TimelinePanel::handleEndYearChanged);

    QFormLayout *rangeForm = new QFormLayout();
    rangeForm->setLabelAlignment(Qt::AlignRight);

    rangeForm->addRow("Start Year", m_startSpinBox);
    rangeForm->addRow("", m_startSlider);
    rangeForm->addRow("End Year", m_endSpinBox);
    rangeForm->addRow("", m_endSlider);

    QHBoxLayout *buttonLayout = new QHBoxLayout();

    m_playButton = new QPushButton("Play Timeline", this);
    m_resetButton = new QPushButton("Reset Timeline", this);

    connect(m_playButton, &QPushButton::clicked,
            this, &TimelinePanel::togglePlayback);
    connect(m_resetButton, &QPushButton::clicked,
            this, &TimelinePanel::resetTimeline);

    buttonLayout->addWidget(m_playButton);
    buttonLayout->addWidget(m_resetButton);

    mainLayout->addWidget(m_titleLabel);
    mainLayout->addWidget(m_summaryLabel);
    mainLayout->addWidget(m_enableCheckBox);
    mainLayout->addWidget(m_showUndatedCheckBox);
    mainLayout->addWidget(new QLabel("Period Preset", this));
    mainLayout->addWidget(m_presetComboBox);
    mainLayout->addLayout(rangeForm);
    mainLayout->addWidget(m_currentRangeLabel);
    mainLayout->addLayout(buttonLayout);
}

void TimelinePanel::populatePresets()
{
    // userData 儲存 preset key，handlePresetChanged() 再依 key 套用年份範圍。
    m_presetComboBox->addItem("Full Dataset", "full_dataset");
    m_presetComboBox->addItem("Qing / Early Modern", "qing");
    m_presetComboBox->addItem("Japanese Period", "japanese_period");
    m_presetComboBox->addItem("Post-war Development", "post_war");
    m_presetComboBox->addItem("Contemporary", "contemporary");
    m_presetComboBox->addItem("Custom Range", "custom");
}

void TimelinePanel::configureYearControls(int minYear, int maxYear)
{
    m_minYear = std::min(minYear, maxYear);
    m_maxYear = std::max(minYear, maxYear);

    // 如果資料集中只有單一年份，slider 仍然可以正常顯示。
    if (m_minYear == m_maxYear) {
        m_maxYear = m_minYear + 1;
    }

    m_isUpdating = true;

    m_startSlider->setRange(m_minYear, m_maxYear);
    m_endSlider->setRange(m_minYear, m_maxYear);

    m_startSpinBox->setRange(m_minYear, m_maxYear);
    m_endSpinBox->setRange(m_minYear, m_maxYear);

    const int span = std::max(1, m_maxYear - m_minYear);
    const int tickInterval = std::max(1, span / 8);

    m_startSlider->setTickInterval(tickInterval);
    m_endSlider->setTickInterval(tickInterval);

    m_isUpdating = false;
}

void TimelinePanel::setRangeValues(int startYear,
                                   int endYear,
                                   bool emitSignal,
                                   bool markAsCustomPreset)
{
    const int clampedStart = std::clamp(startYear, m_minYear, m_maxYear);
    const int clampedEnd = std::clamp(endYear, m_minYear, m_maxYear);

    const int normalizedStart = std::min(clampedStart, clampedEnd);
    const int normalizedEnd = std::max(clampedStart, clampedEnd);

    m_isUpdating = true;

    m_startSlider->setValue(normalizedStart);
    m_endSlider->setValue(normalizedEnd);

    m_startSpinBox->setValue(normalizedStart);
    m_endSpinBox->setValue(normalizedEnd);

    if (markAsCustomPreset) {
        setPresetToCustom();
    }

    m_isUpdating = false;

    updateLabels();

    if (emitSignal) {
        emitCurrentState();
    }
}

void TimelinePanel::handleStartYearChanged(int value)
{
    if (m_isUpdating) {
        return;
    }

    const int endYear = std::max(value, m_endSpinBox->value());
    setRangeValues(value, endYear, true, true);
}

void TimelinePanel::handleEndYearChanged(int value)
{
    if (m_isUpdating) {
        return;
    }

    const int startYear = std::min(value, m_startSpinBox->value());
    setRangeValues(startYear, value, true, true);
}

void TimelinePanel::handleEnabledChanged(int state)
{
    Q_UNUSED(state)

    const bool enabled = m_enableCheckBox->isChecked() && m_hasDatedSites;

    m_startSlider->setEnabled(enabled);
    m_endSlider->setEnabled(enabled);
    m_startSpinBox->setEnabled(enabled);
    m_endSpinBox->setEnabled(enabled);
    m_presetComboBox->setEnabled(enabled);
    m_playButton->setEnabled(enabled);
    m_resetButton->setEnabled(enabled);

    if (!enabled) {
        stopPlayback();
    }

    updateLabels();
    emitCurrentState();
}

void TimelinePanel::handleShowUndatedChanged(int state)
{
    Q_UNUSED(state)

    updateLabels();
    emitCurrentState();
}

void TimelinePanel::handlePresetChanged(int index)
{
    if (m_isUpdating || index < 0) {
        return;
    }

    const QString presetKey = m_presetComboBox->itemData(index).toString();

    if (presetKey == "custom") {
        return;
    }

    applyPreset(presetKey);
}

void TimelinePanel::togglePlayback()
{
    if (!m_hasDatedSites || !m_enableCheckBox->isChecked()) {
        return;
    }

    if (m_playTimer->isActive()) {
        stopPlayback();
        return;
    }

    // 播放時從目前 startYear 開始，逐步推進 endYear。
    // 如果已經跑到最後，就從資料集最早年份重新開始。
    if (m_endSpinBox->value() >= m_maxYear) {
        setRangeValues(m_minYear, m_minYear, true, true);
    }

    m_playButton->setText("Stop Timeline");
    m_playTimer->start(kPlaybackIntervalMs);
}

void TimelinePanel::advancePlayback()
{
    const int nextEndYear = std::min(m_maxYear, m_endSpinBox->value() + playbackStep());

    setRangeValues(m_startSpinBox->value(), nextEndYear, true, true);

    if (nextEndYear >= m_maxYear) {
        stopPlayback();
    }
}

void TimelinePanel::resetTimeline()
{
    stopPlayback();

    if (!m_hasDatedSites) {
        return;
    }

    setRangeValues(m_rangeInfo.minYear, m_rangeInfo.maxYear, true, false);

    const int fullDatasetIndex = m_presetComboBox->findData("full_dataset");
    if (fullDatasetIndex >= 0) {
        m_isUpdating = true;
        m_presetComboBox->setCurrentIndex(fullDatasetIndex);
        m_isUpdating = false;
    }

    m_enableCheckBox->setChecked(true);
    m_showUndatedCheckBox->setChecked(true);

    updateLabels();
    emitCurrentState();
}

void TimelinePanel::updateLabels()
{
    m_summaryLabel->setText(m_rangeInfo.summary());

    if (!m_hasDatedSites) {
        m_currentRangeLabel->setText("No dated records available.");
        return;
    }

    const TimelineFilterState state = currentState();

    if (!state.enabled) {
        m_currentRangeLabel->setText("Timeline filter is disabled.");
        return;
    }

    m_currentRangeLabel->setText(state.displayLabel());
}

void TimelinePanel::emitCurrentState()
{
    emit timelineFilterChanged(currentState());
}

void TimelinePanel::stopPlayback()
{
    if (m_playTimer->isActive()) {
        m_playTimer->stop();
    }

    if (m_playButton) {
        m_playButton->setText("Play Timeline");
    }
}

void TimelinePanel::setPresetToCustom()
{
    const int customIndex = m_presetComboBox->findData("custom");

    if (customIndex < 0 || m_presetComboBox->currentIndex() == customIndex) {
        return;
    }

    m_presetComboBox->setCurrentIndex(customIndex);
}

void TimelinePanel::applyPreset(const QString &presetKey)
{
    stopPlayback();

    if (!m_hasDatedSites) {
        return;
    }

    if (presetKey == "full_dataset") {
        setRangeValues(m_rangeInfo.minYear, m_rangeInfo.maxYear, true, false);
        return;
    }

    if (presetKey == "qing") {
        setRangeValues(1644, 1895, true, false);
        return;
    }

    if (presetKey == "japanese_period") {
        setRangeValues(1895, 1945, true, false);
        return;
    }

    if (presetKey == "post_war") {
        setRangeValues(1945, 1980, true, false);
        return;
    }

    if (presetKey == "contemporary") {
        setRangeValues(1980, m_rangeInfo.maxYear, true, false);
        return;
    }
}

int TimelinePanel::playbackStep() const
{
    const int span = std::max(1, m_maxYear - m_minYear);

    // 讓整段播放大約 80~120 步完成，不會一筆一筆慢慢爬到天荒地老。
    return std::max(1, span / 100);
}