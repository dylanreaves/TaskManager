#include "MonthCalendarWidget.h"

#include <algorithm>

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "Utility.h"

namespace {

const int DAYS_PER_WEEK = 7;
const int WEEK_ROWS = 6;
const int MAX_VISIBLE_LANES = 3;
const int HEADER_HEIGHT = 26;
const int LEGEND_HEIGHT = 30;
const int CELL_PADDING = 5;
const int CELL_BOTTOM_PADDING = 3;
const int LANE_GAP = 2;
const int MORE_ROW_HEIGHT = 12;
const int MORE_TOP_INSET = 4;
const int MORE_RIGHT_RESERVE = 24;
const int DAY_BAND_HEIGHT = 16;
const int MIN_LANE_HEIGHT = 10;
const int MAX_LANE_HEIGHT = 16;
const int MONTH_WIDGET_MIN_HEIGHT = 0;
const int EVENT_AREA_TOP_GAP = 3;
const int PREFERRED_WEEK_ROW_HEIGHT = 47;

QDate monthStartDate(const QDate &date) {
    return date.isValid() ? QDate(date.year(), date.month(), 1) : QDate();
}

QDate monthGridStartDate(const QDate &visibleMonth) {
    if (!visibleMonth.isValid()) {
        return QDate();
    }

    const QDate firstOfMonth(visibleMonth.year(), visibleMonth.month(), 1);
    const int daysBack = firstOfMonth.dayOfWeek() % 7; // Sunday-first grid
    return firstOfMonth.addDays(-daysBack);
}

bool occurrenceIsSpan(const EventOccurrence &occurrence) {
    return occurrence.startDateTime.isValid()
        && occurrence.endDateTime.isValid()
        && occurrence.startDateTime.date() != occurrence.endDateTime.date();
}

QColor stateAdjustedColor(const QString &category, EventVisualState visualState) {
    QColor color = categoryColor(category);
    switch (visualState) {
    case EventVisualState::Completed:
        color = color.lighter(110);
        color.setAlpha(150);
        break;
    case EventVisualState::PastIncomplete:
        color = color.toHsl();
        color.setHsl(color.hslHue(), color.hslSaturation() / 3, qMin(255, color.lightness() + 30), 185);
        break;
    case EventVisualState::Active:
        color.setAlpha(220);
        break;
    }

    return color;
}

QColor rowTextColorForState(EventVisualState visualState) {
    switch (visualState) {
    case EventVisualState::Completed:
        return QColor(QStringLiteral("#D1FAE5"));
    case EventVisualState::PastIncomplete:
        return QColor(QStringLiteral("#CBD5E1"));
    case EventVisualState::Active:
        return QColor(QStringLiteral("#E5E7EB"));
    }

    return QColor(Qt::white);
}

QString popoverLineText(const MonthPopoverItem &item) {
    const QString timeText = occurrenceScheduleText(item.occurrence, item.visibleDate);
    return timeText.trimmed().isEmpty()
        ? item.occurrence.name
        : QStringLiteral("%1  %2").arg(timeText, item.occurrence.name);
}

QIcon categoryDotIcon(const QString &category, EventVisualState visualState) {
    QPixmap pixmap(12, 12);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(stateAdjustedColor(category, visualState));
    painter.drawEllipse(QRectF(1.5, 1.5, 9, 9));
    return QIcon(pixmap);
}

void styleHeaderButton(QToolButton *button, const QString &text = QString()) {
    button->setCursor(Qt::PointingHandCursor);
    button->setText(text);
    button->setMinimumHeight(26);
    button->setAutoRaise(false);
    button->setToolButtonStyle(text.isEmpty() ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextOnly);
}

struct CellMetrics {
    int visibleLanes = 1;
    int laneHeight = 12;
    int rowTop = 0;
};

CellMetrics cellMetrics(const QRect &cellRect) {
    CellMetrics metrics;
    const int topZoneBottom = cellRect.top() + CELL_PADDING + DAY_BAND_HEIGHT + EVENT_AREA_TOP_GAP;
    const int usableBottom = cellRect.bottom() - CELL_BOTTOM_PADDING + 1;
    const int laneAreaBottom = usableBottom;
    const int availableLaneHeight = qMax(0, laneAreaBottom - topZoneBottom);

    int visibleLanes = MAX_VISIBLE_LANES;
    while (visibleLanes > 1) {
        const int candidateLaneHeight = (availableLaneHeight - (visibleLanes - 1) * LANE_GAP) / visibleLanes;
        if (candidateLaneHeight >= MIN_LANE_HEIGHT) {
            break;
        }
        --visibleLanes;
    }

    int candidateLaneHeight = visibleLanes > 0
        ? (availableLaneHeight - (visibleLanes - 1) * LANE_GAP) / visibleLanes
        : 0;
    if (candidateLaneHeight < MIN_LANE_HEIGHT) {
        visibleLanes = 1;
        candidateLaneHeight = qMax(MIN_LANE_HEIGHT, availableLaneHeight);
    }

    metrics.visibleLanes = qMax(1, visibleLanes);
    metrics.laneHeight = qBound(MIN_LANE_HEIGHT, candidateLaneHeight, MAX_LANE_HEIGHT);

    const int totalLaneHeight = metrics.visibleLanes * metrics.laneHeight
        + (metrics.visibleLanes - 1) * LANE_GAP;
    metrics.rowTop = qMax(topZoneBottom, laneAreaBottom - totalLaneHeight + 1);
    return metrics;
}

struct SpanPlacement {
    int spanIndex = -1;
    int weekIndex = -1;
    int lane = -1;
    int startCol = -1;
    int endCol = -1;
    QRect rect;
};

struct CellItemPlacement {
    int cellIndex = -1;
    int itemIndex = -1;
    int lane = -1;
    QRect rect;
};

struct MoreIndicatorPlacement {
    int cellIndex = -1;
    int hiddenCount = 0;
    QRect rect;
};

struct MonthGridLayout {
    QVector<QRect> weekdayHeaderRects;
    QVector<QRect> cellRects;
    QVector<SpanPlacement> spanPlacements;
    QVector<CellItemPlacement> itemPlacements;
    QVector<MoreIndicatorPlacement> morePlacements;
    QVector<int> hiddenCounts;
};

MonthGridLayout buildMonthGridLayout(const MonthViewModel &model, const QRect &bounds) {
    MonthGridLayout layout;
    layout.weekdayHeaderRects.resize(DAYS_PER_WEEK);
    layout.cellRects.resize(DAYS_PER_WEEK * WEEK_ROWS);
    layout.hiddenCounts.fill(0, DAYS_PER_WEEK * WEEK_ROWS);

    if (!model.visibleMonth.isValid() || !model.gridStartDate.isValid() || bounds.width() <= 0 || bounds.height() <= 0) {
        return layout;
    }

    const int usableHeaderHeight = HEADER_HEIGHT;
    const int gridTop = bounds.top() + usableHeaderHeight;
    const int gridHeight = qMax(0, bounds.height() - usableHeaderHeight);
    const int baseColumnWidth = bounds.width() / DAYS_PER_WEEK;
    const int columnRemainder = bounds.width() % DAYS_PER_WEEK;
    const int baseRowHeight = gridHeight / WEEK_ROWS;
    const int rowRemainder = gridHeight % WEEK_ROWS;

    int x = bounds.left();
    QVector<int> columnWidths;
    columnWidths.reserve(DAYS_PER_WEEK);
    for (int col = 0; col < DAYS_PER_WEEK; ++col) {
        const int width = baseColumnWidth + (col < columnRemainder ? 1 : 0);
        columnWidths.append(width);
        layout.weekdayHeaderRects[col] = QRect(x, bounds.top(), width, usableHeaderHeight);
        x += width;
    }

    int y = gridTop;
    for (int week = 0; week < WEEK_ROWS; ++week) {
        const int rowHeight = baseRowHeight + (week < rowRemainder ? 1 : 0);
        int cellX = bounds.left();
        for (int col = 0; col < DAYS_PER_WEEK; ++col) {
            const int cellIndex = week * DAYS_PER_WEEK + col;
            layout.cellRects[cellIndex] = QRect(cellX, y, columnWidths[col], rowHeight);
            cellX += columnWidths[col];
        }

        QVector<int> spansForWeek;
        const QDate weekStart = model.gridStartDate.addDays(week * DAYS_PER_WEEK);
        const QDate weekEnd = weekStart.addDays(DAYS_PER_WEEK - 1);
        for (int spanIndex = 0; spanIndex < model.spans.size(); ++spanIndex) {
            const MonthSpanItem &span = model.spans.at(spanIndex);
            if (span.occurrence.startDateTime.date() <= weekEnd
                && span.occurrence.endDateTime.date() >= weekStart) {
                spansForWeek.append(spanIndex);
            }
        }

        // Multi-day spans claim lanes first so single-day dots can fill whatever vertical space remains.
        std::sort(spansForWeek.begin(), spansForWeek.end(), [&](int leftIndex, int rightIndex) {
            const MonthSpanItem &left = model.spans.at(leftIndex);
            const MonthSpanItem &right = model.spans.at(rightIndex);
            const QDate leftStart = left.occurrence.startDateTime.date();
            const QDate rightStart = right.occurrence.startDateTime.date();
            if (leftStart != rightStart) {
                return leftStart < rightStart;
            }

            const int leftDuration = left.occurrence.startDateTime.daysTo(left.occurrence.endDateTime);
            const int rightDuration = right.occurrence.startDateTime.daysTo(right.occurrence.endDateTime);
            if (leftDuration != rightDuration) {
                return leftDuration > rightDuration;
            }

            if (left.occurrence.priority != right.occurrence.priority) {
                return static_cast<int>(left.occurrence.priority) > static_cast<int>(right.occurrence.priority);
            }

            return left.occurrence.name.localeAwareCompare(right.occurrence.name) < 0;
        });

        const CellMetrics weekMetrics = cellMetrics(layout.cellRects.at(week * DAYS_PER_WEEK));
        const int laneLimit = weekMetrics.visibleLanes;
        bool laneOccupied[MAX_VISIBLE_LANES][DAYS_PER_WEEK] = {};
        for (int spanIndex : spansForWeek) {
            const MonthSpanItem &span = model.spans.at(spanIndex);
            const QDate visibleStartDate = std::max(weekStart, span.occurrence.startDateTime.date());
            const QDate visibleEndDate = std::min(weekEnd, span.occurrence.endDateTime.date());
            const int startCol = weekStart.daysTo(visibleStartDate);
            const int endCol = weekStart.daysTo(visibleEndDate);

            int lane = -1;
            for (int candidateLane = 0; candidateLane < laneLimit; ++candidateLane) {
                bool collides = false;
                for (int col = startCol; col <= endCol; ++col) {
                    if (laneOccupied[candidateLane][col]) {
                        collides = true;
                        break;
                    }
                }
                if (!collides) {
                    lane = candidateLane;
                    break;
                }
            }

            if (lane < 0) {
                for (int col = startCol; col <= endCol; ++col) {
                    ++layout.hiddenCounts[week * DAYS_PER_WEEK + col];
                }
                continue;
            }

            for (int col = startCol; col <= endCol; ++col) {
                laneOccupied[lane][col] = true;
            }

            const QRect startCellRect = layout.cellRects[week * DAYS_PER_WEEK + startCol];
            const QRect endCellRect = layout.cellRects[week * DAYS_PER_WEEK + endCol];
            const CellMetrics metrics = cellMetrics(startCellRect);
            const int rowY = metrics.rowTop + lane * (metrics.laneHeight + LANE_GAP);
            const int rowHeight = metrics.laneHeight;
            const QRect spanRect(startCellRect.left() + CELL_PADDING,
                                 rowY,
                                 endCellRect.right() - startCellRect.left() - (CELL_PADDING * 2) + 1,
                                 rowHeight);
            layout.spanPlacements.append({spanIndex, week, lane, startCol, endCol, spanRect});
        }

        for (int col = 0; col < DAYS_PER_WEEK; ++col) {
            const int cellIndex = week * DAYS_PER_WEEK + col;
            const MonthCellState &cell = model.cells.at(cellIndex);
            const QRect &cellRect = layout.cellRects.at(cellIndex);
            const CellMetrics metrics = cellMetrics(cellRect);

            bool usedLane[MAX_VISIBLE_LANES] = {};
            for (int lane = 0; lane < laneLimit; ++lane) {
                usedLane[lane] = laneOccupied[lane][col];
            }

            for (int itemIndex = 0; itemIndex < cell.dayItems.size(); ++itemIndex) {
                int lane = -1;
                for (int candidateLane = 0; candidateLane < laneLimit; ++candidateLane) {
                    if (!usedLane[candidateLane]) {
                        lane = candidateLane;
                        break;
                    }
                }

                if (lane < 0) {
                    ++layout.hiddenCounts[cellIndex];
                    continue;
                }

                usedLane[lane] = true;
                const int rowY = metrics.rowTop + lane * (metrics.laneHeight + LANE_GAP);
                const QRect itemRect(cellRect.left() + CELL_PADDING,
                                     rowY,
                                     cellRect.width() - (CELL_PADDING * 2),
                                     metrics.laneHeight);
                layout.itemPlacements.append({cellIndex, itemIndex, lane, itemRect});
            }

            if (layout.hiddenCounts.at(cellIndex) > 0) {
                const int moreWidth = qMax(38,
                                           cellRect.width() - (CELL_PADDING * 2) - MORE_RIGHT_RESERVE);
                layout.morePlacements.append({cellIndex,
                                              layout.hiddenCounts.at(cellIndex),
                                              QRect(cellRect.left() + CELL_PADDING,
                                                    cellRect.top() + MORE_TOP_INSET,
                                                    moreWidth,
                                                    MORE_ROW_HEIGHT)});
            }
        }

        y += rowHeight;
    }

    return layout;
}

} // namespace

class MonthCalendarGridWidget : public QWidget
{
public:
    explicit MonthCalendarGridWidget(MonthCalendarWidget *owner)
        : QWidget(owner)
        , m_owner(owner) {
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        if (m_owner == nullptr) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        painter.fillRect(rect(), QColor(QStringLiteral("#2B2B2B")));
        const MonthGridLayout layout = buildMonthGridLayout(m_owner->m_model, rect());

        painter.setPen(QPen(QColor(QStringLiteral("#3F3F46")), 1));
        painter.setBrush(QColor(QStringLiteral("#333333")));
        for (int col = 0; col < layout.weekdayHeaderRects.size(); ++col) {
            painter.drawRect(layout.weekdayHeaderRects.at(col).adjusted(0, 0, -1, -1));
        }

        static const QStringList weekdayLabels = {
            QStringLiteral("Sun"),
            QStringLiteral("Mon"),
            QStringLiteral("Tue"),
            QStringLiteral("Wed"),
            QStringLiteral("Thu"),
            QStringLiteral("Fri"),
            QStringLiteral("Sat"),
        };

        QFont weekdayFont = painter.font();
        weekdayFont.setBold(true);
        weekdayFont.setPointSizeF(9.5);
        painter.setFont(weekdayFont);
        for (int col = 0; col < layout.weekdayHeaderRects.size(); ++col) {
            const QColor labelColor = (col == 0 || col == 6)
                ? QColor(QStringLiteral("#FF5B5B"))
                : QColor(QStringLiteral("#F3F4F6"));
            painter.setPen(labelColor);
            painter.drawText(layout.weekdayHeaderRects.at(col),
                             Qt::AlignCenter,
                             weekdayLabels.at(col));
        }

        painter.setPen(QPen(QColor(QStringLiteral("#3F3F46")), 1));
        painter.setBrush(Qt::NoBrush);
        for (const QRect &cellRect : layout.cellRects) {
            painter.drawRect(cellRect.adjusted(0, 0, -1, -1));
        }

        const QDate today = QDate::currentDate();
        for (int cellIndex = 0; cellIndex < layout.cellRects.size() && cellIndex < m_owner->m_model.cells.size(); ++cellIndex) {
            const MonthCellState &cell = m_owner->m_model.cells.at(cellIndex);
            const QRect &cellRect = layout.cellRects.at(cellIndex);

            const bool isToday = cell.date == today;
            const bool isSelected = cell.date == m_owner->m_selectedDate;
            if (isToday) {
                QColor todayFill(QStringLiteral("#DC2626"));
                todayFill.setAlpha(18);
                painter.setPen(QPen(QColor(QStringLiteral("#EF4444")), 2));
                painter.setBrush(todayFill);
                painter.drawRect(cellRect.adjusted(2, 2, -2, -2));
            }

            if (isSelected) {
                QColor selectedFill(QStringLiteral("#2563EB"));
                selectedFill.setAlpha(isToday ? 16 : 24);
                const QRect selectedRect = isToday
                    ? cellRect.adjusted(6, 6, -6, -6)
                    : cellRect.adjusted(4, 4, -4, -4);
                painter.setPen(QPen(QColor(QStringLiteral("#3B82F6")), 2));
                painter.setBrush(selectedFill);
                painter.drawRect(selectedRect);
            }

            QFont dayFont = painter.font();
            dayFont.setBold(true);
            dayFont.setPointSizeF(11.0);
            painter.setFont(dayFont);

            QColor dayNumberColor = QColor(QStringLiteral("#F8FAFC"));
            if (!cell.inCurrentMonth) {
                dayNumberColor = QColor(QStringLiteral("#7C8797"));
            } else if (cell.date.dayOfWeek() == 7 || cell.date.dayOfWeek() == 6) {
                dayNumberColor = QColor(QStringLiteral("#FF4D4D"));
            }

            painter.setPen(dayNumberColor);
            painter.drawText(cellRect.adjusted(0, 6, -8, 0),
                             Qt::AlignTop | Qt::AlignRight,
                             QString::number(cell.date.day()));
        }

        for (const SpanPlacement &placement : layout.spanPlacements) {
            const MonthSpanItem &span = m_owner->m_model.spans.at(placement.spanIndex);
            QColor fillColor = stateAdjustedColor(span.occurrence.category, span.visualState);
            QColor borderColor = fillColor.lighter(112);

            painter.setPen(QPen(borderColor, 1));
            painter.setBrush(fillColor);
            painter.drawRoundedRect(placement.rect, placement.rect.height() / 2.0, placement.rect.height() / 2.0);

            QFont spanFont = painter.font();
            spanFont.setBold(true);
            spanFont.setPointSizeF(8.0);
            painter.setFont(spanFont);
            painter.setPen(QColor(QStringLiteral("#F8FAFC")));

            const QDate weekStart = m_owner->m_model.gridStartDate.addDays(placement.weekIndex * DAYS_PER_WEEK);
            const QDate segmentStartDate = weekStart.addDays(placement.startCol);
            const bool isStartSegment = segmentStartDate == span.occurrence.startDateTime.date();
            const QString elidedText = QFontMetrics(spanFont).elidedText(
                span.occurrence.name,
                Qt::ElideRight,
                placement.rect.width() - 12);
            if (isStartSegment || placement.rect.width() >= 110) {
                painter.drawText(placement.rect.adjusted(10, 0, -6, 0),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 elidedText);
            }
        }

        for (const CellItemPlacement &placement : layout.itemPlacements) {
            const MonthCellState &cell = m_owner->m_model.cells.at(placement.cellIndex);
            const MonthCellItem &item = cell.dayItems.at(placement.itemIndex);
            const QRect &itemRect = placement.rect;
            const QColor dotColor = stateAdjustedColor(item.occurrence.category, item.visualState);

            painter.setPen(Qt::NoPen);
            painter.setBrush(dotColor);
            painter.drawEllipse(QRectF(itemRect.left() + 2, itemRect.center().y() - 3, 6, 6));

            QFont rowFont = painter.font();
            rowFont.setPointSizeF(8.0);
            if (item.visualState != EventVisualState::Active) {
                rowFont.setItalic(true);
            }
            painter.setFont(rowFont);
            painter.setPen(rowTextColorForState(item.visualState));
            const QString title = QFontMetrics(rowFont).elidedText(
                item.occurrence.name,
                Qt::ElideRight,
                itemRect.width() - 16);
            painter.drawText(itemRect.adjusted(14, 0, -2, 0),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             title);
        }

        QFont moreFont = painter.font();
        moreFont.setPointSizeF(7.5);
        moreFont.setBold(true);
        for (const MoreIndicatorPlacement &placement : layout.morePlacements) {
            QFont linkFont = moreFont;
            const bool hovered = placement.cellIndex == m_owner->m_hoveredMoreCellIndex;
            linkFont.setUnderline(hovered);
            painter.setFont(linkFont);
            painter.setPen(hovered
                               ? QColor(QStringLiteral("#BFDBFE"))
                               : QColor(QStringLiteral("#93C5FD")));
            painter.drawText(placement.rect,
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QStringLiteral("+%1 more").arg(placement.hiddenCount));
        }
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (m_owner != nullptr && event->button() == Qt::LeftButton) {
            m_owner->handleGridClick(event->pos());
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (m_owner != nullptr) {
            m_owner->handleGridHover(event->pos());
        }
        QWidget::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent *event) override {
        if (m_owner != nullptr) {
            m_owner->clearGridHover();
        }
        QWidget::leaveEvent(event);
    }

private:
    MonthCalendarWidget *m_owner = nullptr;
};

MonthCalendarWidget::MonthCalendarWidget(QWidget *parent)
    : QWidget(parent)
    , m_gridWidget(new MonthCalendarGridWidget(this))
    , m_leftHeaderControls(new QWidget(this))
    , m_previousMonthButton(new QToolButton(m_leftHeaderControls))
    , m_nextMonthButton(new QToolButton(m_leftHeaderControls))
    , m_todayButton(new QToolButton(m_leftHeaderControls))
    , m_titleLabel(new QLabel(this))
    , m_legendContainer(new QWidget(this))
    , m_legendLayout(new QVBoxLayout(m_legendContainer))
    , m_visibleMonth(monthStartDate(QDate::currentDate()))
    , m_selectedDate(QDate::currentDate()) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    const QSize preferredMinimumSize = minimumSizeHint();
    setMinimumWidth(preferredMinimumSize.width());
    setMinimumHeight(MONTH_WIDGET_MIN_HEIGHT);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(5, 3, 5, 4);
    rootLayout->setSpacing(1);

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(3);

    auto *leftControlsLayout = new QHBoxLayout(m_leftHeaderControls);
    leftControlsLayout->setContentsMargins(0, 0, 0, 0);
    leftControlsLayout->setSpacing(3);

    m_previousMonthButton->setArrowType(Qt::LeftArrow);
    styleHeaderButton(m_previousMonthButton);

    m_nextMonthButton->setArrowType(Qt::RightArrow);
    styleHeaderButton(m_nextMonthButton);

    styleHeaderButton(m_todayButton, tr("Today"));

    const QString buttonStyle = QStringLiteral(
        "QToolButton {"
        "background-color: #353535;"
        "border: 1px solid #4A4A4A;"
        "border-radius: 6px;"
        "padding: 0 10px;"
        "color: #F8FAFC;"
        "font-weight: 600;"
        "}"
        "QToolButton:hover { background-color: #404040; }"
        "QToolButton:pressed { background-color: #2A2A2A; }");
    m_previousMonthButton->setStyleSheet(buttonStyle);
    m_nextMonthButton->setStyleSheet(buttonStyle);
    m_todayButton->setStyleSheet(buttonStyle);
    m_previousMonthButton->setFixedSize(28, 26);
    m_nextMonthButton->setFixedSize(28, 26);
    m_todayButton->setMinimumWidth(60);
    m_todayButton->setFixedHeight(26);

    leftControlsLayout->addWidget(m_previousMonthButton);
    leftControlsLayout->addWidget(m_nextMonthButton);
    leftControlsLayout->addWidget(m_todayButton);
    leftControlsLayout->addStretch(1);

    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(QStringLiteral("QLabel { color: #F8FAFC; font-size: 15px; font-weight: 700; }"));

    headerLayout->addWidget(m_leftHeaderControls, 0);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_titleLabel, 0, Qt::AlignCenter);
    headerLayout->addStretch(1);
    headerLayout->addSpacing(m_leftHeaderControls->sizeHint().width());
    rootLayout->addLayout(headerLayout);

    m_gridWidget->setStyleSheet(QStringLiteral("QWidget { background: transparent; }"));
    rootLayout->addWidget(m_gridWidget, 1);

    m_legendContainer->setMinimumHeight(LEGEND_HEIGHT);
    m_legendContainer->setMaximumHeight(LEGEND_HEIGHT);
    m_legendLayout->setContentsMargins(0, 0, 0, 0);
    m_legendLayout->setSpacing(2);
    rootLayout->addWidget(m_legendContainer, 0);

    setStyleSheet(QStringLiteral(
        "MonthCalendarWidget {"
        "background-color: #2B2B2B;"
        "border: 1px solid #3F3F46;"
        "border-radius: 12px;"
        "}"));

    connect(m_previousMonthButton, &QToolButton::clicked, this, [this] {
        shiftMonth(-1);
    });
    connect(m_nextMonthButton, &QToolButton::clicked, this, [this] {
        shiftMonth(1);
    });
    connect(m_todayButton, &QToolButton::clicked, this, [this] {
        closeOverflowPopover();
        setSelectedDate(QDate::currentDate());
        showSelectedDate();
        emit dayActivated(m_selectedDate, m_selectedDate);
    });

    updateHeaderLabel();
    rebuildLegend();
}

// Replace the visible month data and refresh every dependent surface together.
void MonthCalendarWidget::setMonthViewModel(const MonthViewModel &model) {
    m_model = model;
    if (model.visibleMonth.isValid()) {
        m_visibleMonth = monthStartDate(model.visibleMonth);
    }

    if (!m_selectedDate.isValid()) {
        m_selectedDate = m_visibleMonth.isValid() ? m_visibleMonth : QDate::currentDate();
    }

    updateHeaderLabel();
    rebuildLegend();
    closeOverflowPopover();
    m_gridWidget->update();
}

QSize MonthCalendarWidget::minimumSizeHint() const {
    const int ROOT_TOP_MARGIN = 3;
    const int ROOT_BOTTOM_MARGIN = 4;
    const int ROOT_VERTICAL_SPACING = 2;
    const int HEADER_CONTROLS_HEIGHT = 26;

    const int preferredHeight = ROOT_TOP_MARGIN
        + ROOT_BOTTOM_MARGIN
        + ROOT_VERTICAL_SPACING
        + HEADER_CONTROLS_HEIGHT
        + HEADER_HEIGHT
        + (WEEK_ROWS * PREFERRED_WEEK_ROW_HEIGHT)
        + LEGEND_HEIGHT;

    return QSize(560, preferredHeight);
}

QSize MonthCalendarWidget::sizeHint() const {
    return minimumSizeHint();
}

QDate MonthCalendarWidget::selectedDate() const {
    return m_selectedDate;
}

QDate MonthCalendarWidget::visibleMonth() const {
    return m_visibleMonth;
}

void MonthCalendarWidget::setSelectedDate(const QDate &date) {
    if (!date.isValid() || date == m_selectedDate) {
        return;
    }

    m_selectedDate = date;
    closeOverflowPopover();
    m_gridWidget->update();
    emit selectedDateChanged(m_selectedDate);
}

void MonthCalendarWidget::showSelectedDate() {
    if (!m_selectedDate.isValid()) {
        return;
    }

    setVisibleMonthInternal(monthStartDate(m_selectedDate), true);
}

// Rebuild the wrapped legend when the widget width changes so the two-row layout stays tidy.
void MonthCalendarWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    rebuildLegend();
    if (m_gridWidget != nullptr) {
        m_gridWidget->update();
    }
}

void MonthCalendarWidget::shiftMonth(int months) {
    if (!m_visibleMonth.isValid()) {
        setVisibleMonthInternal(monthStartDate(QDate::currentDate()), true);
        return;
    }

    setVisibleMonthInternal(m_visibleMonth.addMonths(months), true);
}

void MonthCalendarWidget::setVisibleMonthInternal(const QDate &month, bool emitSignal) {
    const QDate normalizedMonth = monthStartDate(month);
    if (!normalizedMonth.isValid()) {
        return;
    }

    if (normalizedMonth == m_visibleMonth) {
        if (emitSignal) {
            emit currentPageChanged(m_visibleMonth.year(), m_visibleMonth.month());
        }
        return;
    }

    m_visibleMonth = normalizedMonth;
    updateHeaderLabel();
    closeOverflowPopover();
    if (emitSignal) {
        emit currentPageChanged(m_visibleMonth.year(), m_visibleMonth.month());
    }
}

void MonthCalendarWidget::updateHeaderLabel() {
    const QDate labelMonth = m_visibleMonth.isValid() ? m_visibleMonth : monthStartDate(QDate::currentDate());
    m_titleLabel->setText(labelMonth.toString(QStringLiteral("MMMM yyyy")));
}

// Recreate the legend rows each time category content or available width changes.
void MonthCalendarWidget::rebuildLegend() {
    if (m_legendLayout == nullptr) {
        return;
    }

    while (QLayoutItem *item = m_legendLayout->takeAt(0)) {
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    const QStringList legendCategories = m_model.legendCategories;
    const int availableWidth = qMax(200, width() - 24);
    int currentRowWidth = 0;
    int currentRowIndex = 0;
    auto *rowWidget = new QWidget(m_legendContainer);
    auto *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(10);
    m_legendLayout->addWidget(rowWidget);

    QFontMetrics metrics(font());
    const auto ensureLegendRowCapacity = [&](int entryWidth) {
        if (currentRowIndex == 0 && currentRowWidth > 0 && currentRowWidth + entryWidth > availableWidth) {
            currentRowIndex = 1;
            currentRowWidth = 0;
            rowWidget = new QWidget(m_legendContainer);
            rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(10);
            m_legendLayout->addWidget(rowWidget);
        }
    };

    // Keep legend row construction close to the wrapping logic so row ownership stays easy to follow.
    const auto appendLegendEntry = [&](const QString &labelText, QWidget *markerWidget, int markerWidth) {
        const int entryWidth = metrics.horizontalAdvance(labelText) + markerWidth + 15;
        ensureLegendRowCapacity(entryWidth);

        auto *entryWidget = new QWidget(rowWidget);
        auto *entryLayout = new QHBoxLayout(entryWidget);
        entryLayout->setContentsMargins(0, 0, 0, 0);
        entryLayout->setSpacing(4);

        entryLayout->addWidget(markerWidget, 0, Qt::AlignVCenter);

        auto *label = new QLabel(labelText, entryWidget);
        label->setStyleSheet(QStringLiteral("QLabel { color: #CBD5E1; font-size: 10px; }"));
        entryLayout->addWidget(label, 0, Qt::AlignVCenter);

        rowLayout->addWidget(entryWidget, 0, Qt::AlignLeft);
        currentRowWidth += entryWidth + 14;
    };

    auto *todaySquare = new QFrame(rowWidget);
    todaySquare->setFixedSize(9, 9);
    todaySquare->setStyleSheet(QStringLiteral(
        "QFrame {"
        "background: transparent;"
        "border: 1px solid #EF4444;"
        "}"));
    appendLegendEntry(tr("Today"), todaySquare, 9);

    auto *selectedSquare = new QFrame(rowWidget);
    selectedSquare->setFixedSize(9, 9);
    selectedSquare->setStyleSheet(QStringLiteral(
        "QFrame {"
        "background: transparent;"
        "border: 1px solid #3B82F6;"
        "}"));
    appendLegendEntry(tr("Selected"), selectedSquare, 9);

    for (const QString &category : legendCategories) {
        auto *dot = new QLabel(rowWidget);
        dot->setFixedSize(7, 7);
        dot->setStyleSheet(QStringLiteral("QLabel { background-color: %1; border-radius: 3px; }")
                               .arg(categoryColor(category).name()));
        appendLegendEntry(category, dot, 7);
    }

    if (QLayoutItem *lastItem = m_legendLayout->itemAt(m_legendLayout->count() - 1)) {
        if (QWidget *lastRow = lastItem->widget()) {
            if (auto *lastRowLayout = qobject_cast<QHBoxLayout *>(lastRow->layout())) {
                lastRowLayout->addStretch(1);
            }
        }
    }
}

void MonthCalendarWidget::closeOverflowPopover() {
    if (m_overflowPopup != nullptr) {
        m_overflowPopup->close();
        m_overflowPopup->deleteLater();
        m_overflowPopup = nullptr;
    }
}

// Show the hidden day items in a popup instead of letting the crowded month cell grow.
void MonthCalendarWidget::showOverflowPopover(const QDate &date, const QPoint &globalAnchor) {
    closeOverflowPopover();
    if (!m_model.gridStartDate.isValid()) {
        return;
    }

    const int cellIndex = m_model.gridStartDate.daysTo(date);
    if (cellIndex < 0 || cellIndex >= m_model.cells.size()) {
        return;
    }

    const MonthCellState &cell = m_model.cells.at(cellIndex);
    if (cell.popoverItems.isEmpty()) {
        return;
    }

    auto *popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    popup->setObjectName(QStringLiteral("monthOverflowPopup"));
    popup->setStyleSheet(QStringLiteral(
        "QFrame#monthOverflowPopup {"
        "background-color: #2B2B2B;"
        "border: 1px solid #3A3A3A;"
        "border-radius: 12px;"
        "}"
        "QScrollArea { border: none; background: transparent; }"
        "QWidget#monthOverflowContent { background: transparent; }"));

    auto *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto *titleLabel = new QLabel(date.toString(QStringLiteral("dddd - MMMM d, yyyy")), popup);
    titleLabel->setStyleSheet(QStringLiteral("QLabel { color: #F8FAFC; font-size: 12px; font-weight: 700; }"));
    layout->addWidget(titleLabel);

    auto *scrollArea = new QScrollArea(popup);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setMaximumHeight(220);
    layout->addWidget(scrollArea);

    auto *contentWidget = new QWidget(scrollArea);
    contentWidget->setObjectName(QStringLiteral("monthOverflowContent"));
    auto *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(4);

    for (const MonthPopoverItem &item : cell.popoverItems) {
        auto *button = new QPushButton(popoverLineText(item), contentWidget);
        button->setFlat(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setIcon(categoryDotIcon(item.occurrence.category, item.visualState));
        button->setIconSize(QSize(12, 12));
        button->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "text-align: left;"
            "padding: 6px 8px;"
            "border: 1px solid transparent;"
            "border-radius: 8px;"
            "background-color: transparent;"
            "color: %1;"
            "font-size: 11px;"
            "%2"
            "}"
            "QPushButton:hover {"
            "background-color: rgba(37, 99, 235, 36);"
            "border-color: rgba(59, 130, 246, 90);"
            "}")
                                    .arg(rowTextColorForState(item.visualState).name(),
                                         item.visualState == EventVisualState::Active
                                             ? QString()
                                             : QStringLiteral("font-style: italic;")));
        contentLayout->addWidget(button);

        connect(button, &QPushButton::clicked, this, [this, date, item] {
            closeOverflowPopover();
            setSelectedDate(date);
            emit occurrenceActivated(item.occurrence.occurrenceKey, item.occurrence.eventId);
        });
    }

    contentLayout->addStretch(1);
    scrollArea->setWidget(contentWidget);

    popup->resize(280, qMin(260, 56 + cell.popoverItems.size() * 32));
    popup->move(globalAnchor);
    popup->show();
    m_overflowPopup = popup;
}

// Route clicks to the right action: open the +more popup, activate an item/span, or jump by day.
void MonthCalendarWidget::handleGridClick(const QPoint &localPos) {
    const MonthGridLayout layout = buildMonthGridLayout(m_model, m_gridWidget->rect());

    for (const MoreIndicatorPlacement &placement : layout.morePlacements) {
        if (!placement.rect.contains(localPos)) {
            continue;
        }

        const QDate selectedDate = m_model.cells.at(placement.cellIndex).date;
        setSelectedDate(selectedDate);
        const QPoint globalAnchor = m_gridWidget->mapToGlobal(placement.rect.bottomLeft() + QPoint(0, 6));
        showOverflowPopover(selectedDate, globalAnchor);
        return;
    }

    for (const CellItemPlacement &placement : layout.itemPlacements) {
        if (!placement.rect.contains(localPos)) {
            continue;
        }

        const MonthCellState &cell = m_model.cells.at(placement.cellIndex);
        const MonthCellItem &item = cell.dayItems.at(placement.itemIndex);
        closeOverflowPopover();
        setSelectedDate(cell.date);
        emit dayActivated(cell.date, item.jumpDate);
        return;
    }

    for (const SpanPlacement &placement : layout.spanPlacements) {
        if (!placement.rect.contains(localPos)) {
            continue;
        }

        const QDate weekStart = m_model.gridStartDate.addDays(placement.weekIndex * DAYS_PER_WEEK);
        QDate clickedDate = weekStart.addDays(placement.startCol);
        for (int col = placement.startCol; col <= placement.endCol; ++col) {
            const int cellIndex = placement.weekIndex * DAYS_PER_WEEK + col;
            if (layout.cellRects.at(cellIndex).contains(localPos)) {
                clickedDate = m_model.cells.at(cellIndex).date;
                break;
            }
        }

        const MonthSpanItem &span = m_model.spans.at(placement.spanIndex);
        const QDate jumpDate = span.jumpDate.isValid()
            ? (clickedDate == span.occurrence.startDateTime.date() ? clickedDate : span.jumpDate)
            : QDate();
        closeOverflowPopover();
        setSelectedDate(clickedDate);
        emit dayActivated(clickedDate, jumpDate);
        return;
    }

    for (int cellIndex = 0; cellIndex < layout.cellRects.size() && cellIndex < m_model.cells.size(); ++cellIndex) {
        if (!layout.cellRects.at(cellIndex).contains(localPos)) {
            continue;
        }

        const MonthCellState &cell = m_model.cells.at(cellIndex);
        closeOverflowPopover();
        setSelectedDate(cell.date);
        emit dayActivated(cell.date, cell.actionableJumpDate);
        return;
    }
}

// Track hover only for the +N more indicator so the rest of the grid stays visually calm.
void MonthCalendarWidget::handleGridHover(const QPoint &localPos) {
    const MonthGridLayout layout = buildMonthGridLayout(m_model, m_gridWidget->rect());

    int hoveredMoreCellIndex = -1;
    for (const MoreIndicatorPlacement &placement : layout.morePlacements) {
        if (placement.rect.contains(localPos)) {
            hoveredMoreCellIndex = placement.cellIndex;
            break;
        }
    }

    if (hoveredMoreCellIndex == m_hoveredMoreCellIndex) {
        return;
    }

    m_hoveredMoreCellIndex = hoveredMoreCellIndex;
    if (m_gridWidget != nullptr) {
        if (m_hoveredMoreCellIndex >= 0) {
            m_gridWidget->setCursor(Qt::PointingHandCursor);
        } else {
            m_gridWidget->unsetCursor();
        }
        m_gridWidget->update();
    }
}

// Clear the lightweight hover state when the pointer leaves the grid.
void MonthCalendarWidget::clearGridHover() {
    if (m_hoveredMoreCellIndex < 0) {
        return;
    }

    m_hoveredMoreCellIndex = -1;
    if (m_gridWidget != nullptr) {
        m_gridWidget->unsetCursor();
        m_gridWidget->update();
    }
}



