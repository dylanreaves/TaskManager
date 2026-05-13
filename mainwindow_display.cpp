// Mainly renders the agenda, list, details panel and visual styling for the mainWindow
#include "mainwindow.h"

#include "./ui_mainwindow.h"

#include <algorithm>

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "MonthCalendarWidget.h"
#include "Utility.h"

namespace {

const int OCCURRENCE_KEY_ROLE = Qt::UserRole;
const int EVENT_ID_ROLE = Qt::UserRole + 1;
const int CATEGORY_ROLE = Qt::UserRole + 2;
const int ROW_KIND_ROLE = Qt::UserRole + 3;
const int FOLDER_ID_ROLE = Qt::UserRole + 4;
const int FOLDER_DEPTH_ROLE = Qt::UserRole + 5;
const int PARENT_FOLDER_ID_ROLE = Qt::UserRole + 6;
const QString TODAY_SUFFIX = QStringLiteral(" <Today>"); // String that labels the folder that represents Today.

// Simple enum to differentiate folders from events in the list table
enum class ListTableRowKind {
    Occurrence = 0,
    Folder = 1,
};

struct ListDisplayRow {
    bool isFolder = false;
    QString folderLabel;
    QString folderId;
    QString parentFolderId;
    QStringList ancestorFolderIds;
    int depth = 0;
    EventOccurrence occurrence;
};

struct BoardTheme {
    QColor pageBackground;
    QColor laneBackground;
    QColor headerBackground;
    QColor cardBackground;
    QColor buttonBackground;
    QColor buttonHoverBackground;
    QColor buttonPressedBackground;
    QColor borderColor;
    QColor primaryText;
    QColor secondaryText;
    QColor mutedText;
    QColor selectionOutline;
    QColor todayOutline;
    QColor completedTintBase;
    QColor overdueTintBase;
};

QTableWidgetItem *createReadOnlyItem(const QString &text, const QString &occurrenceKey = QString(), int eventId = -1) {
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setData(OCCURRENCE_KEY_ROLE, occurrenceKey);
    item->setData(EVENT_ID_ROLE, eventId);
    return item;
}

bool isDateFolderId(const QString &folderId) {
    return folderId.startsWith(QStringLiteral("actionable:date:"));
}

// Background color of certain folders
QColor listFolderBackground(const QString &folderId) {
    QColor returnVal = QColor(QStringLiteral("#39424D"));
    if (folderId == QStringLiteral("actionable:noduedate")) {
        returnVal = QColor(QStringLiteral("#2F3540"));
    } else if (folderId.startsWith(QStringLiteral("history:"))) {
        returnVal = QColor(QStringLiteral("#2C313A"));
    } else if (isDateFolderId(folderId)) {
        returnVal = QColor(QStringLiteral("#39424D"));
    }
    return returnVal;
}

// Foreground color of folder
QColor listFolderForeground(const QString &folderId) {
    QColor returnVal = QColor(QStringLiteral("#E5E7EB"));
    if (isDateFolderId(folderId)) {
        returnVal = QColor(QStringLiteral("#F8FAFC"));
    }
    return returnVal;
}

int listFolderRowHeight(const QString &folderId) {
    return isDateFolderId(folderId) ? 32 : 34;
}

QIcon listFolderIcon(bool expanded) {
    return QApplication::style()->standardIcon(expanded ? QStyle::SP_ArrowDown : QStyle::SP_ArrowRight);
}

void applyFolderItemIndicator(QTableWidgetItem *item, bool expanded) {
    const bool canUpdate = item != nullptr;
    if (canUpdate) {
        item->setIcon(listFolderIcon(expanded));
    }
}

QTableWidgetItem *createFolderItem(const QString &label, const QString &folderId, const QString &parentFolderId, int depth, bool expanded) {
    auto *item = createReadOnlyItem(label);
    item->setFlags(Qt::ItemIsEnabled);
    item->setData(ROW_KIND_ROLE, static_cast<int>(ListTableRowKind::Folder));
    item->setData(FOLDER_ID_ROLE, folderId);
    item->setData(FOLDER_DEPTH_ROLE, depth);
    item->setData(PARENT_FOLDER_ID_ROLE, parentFolderId);

    QFont font = item->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() + (isDateFolderId(folderId) ? 0.25 : 0.75));
    font.setLetterSpacing(QFont::AbsoluteSpacing, isDateFolderId(folderId) ? 0.2 : 0.4);
    item->setFont(font);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    applyFolderItemIndicator(item, expanded);

    item->setBackground(listFolderBackground(folderId));
    item->setForeground(listFolderForeground(folderId));
    return item;
}

QString formattedListFolderDate(const QDate &date) {
    return date.isValid()
        ? date.toString(QStringLiteral("dddd - MMMM d, yyyy"))
              + (date == QDate::currentDate() ? TODAY_SUFFIX : QString())
        : QStringLiteral("No Date");
}

bool isPinnedAgendaListEntry(const EventOccurrence &occurrence, const QDate &today) {
    const bool isTask = occurrence.eventType == EventType::Task;
    const bool hasSchedule = occurrenceHasSchedule(occurrence);
    bool returnVal = true;

    if (isTask) {
        returnVal = occurrence.allDay;
    } else if (hasSchedule) {
        const bool startsToday = occurrence.startDateTime.date() == today;
        const bool endsToday = occurrence.endDateTime.date() == today;
        returnVal = occurrence.allDay || !startsToday || !endsToday;
    }

    return returnVal;
}

QTime agendaListTimedAnchor(const EventOccurrence &occurrence, const QDate &today) {
    QTime returnVal = QTime(23, 59, 59);
    if (occurrence.eventType == EventType::Task && occurrence.endDateTime.isValid()) {
        returnVal = occurrence.endDateTime.time();
    } else if (occurrence.endDateTime.isValid() && occurrence.endDateTime.date() == today) {
        returnVal = occurrence.endDateTime.time();
    } else if (occurrence.eventType == EventType::Reminder && occurrence.startDateTime.isValid()) {
        returnVal = occurrence.startDateTime.time();
    } else if (occurrence.startDateTime.isValid() && occurrence.startDateTime.date() == today) {
        returnVal = occurrence.startDateTime.time();
    }
    return returnVal;
}

bool compareAgendaListEntries(const EventOccurrence &left, const EventOccurrence &right, const QDate &today) {
    const bool leftPinned = isPinnedAgendaListEntry(left, today);
    const bool rightPinned = isPinnedAgendaListEntry(right, today);
    bool returnVal = false;

    if (leftPinned != rightPinned) {
        returnVal = leftPinned;
    } else if (leftPinned) {
        if (left.priority != right.priority) {
            returnVal = static_cast<int>(left.priority) > static_cast<int>(right.priority);
        } else {
            returnVal = left.name.localeAwareCompare(right.name) < 0;
        }
    } else {
        const QTime leftTime = agendaListTimedAnchor(left, today);
        const QTime rightTime = agendaListTimedAnchor(right, today);
        if (leftTime != rightTime) {
            returnVal = leftTime < rightTime;
        } else if (left.priority != right.priority) {
            returnVal = static_cast<int>(left.priority) > static_cast<int>(right.priority);
        } else {
            returnVal = left.name.localeAwareCompare(right.name) < 0;
        }
    }

    return returnVal;
}

bool compareNoDueTaskEntries(const EventOccurrence &left, const EventOccurrence &right) {
    bool returnVal = false;
    if (left.priority != right.priority) {
        returnVal = static_cast<int>(left.priority) > static_cast<int>(right.priority);
    } else {
        returnVal = left.name.localeAwareCompare(right.name) < 0;
    }
    return returnVal;
}

void appendListOccurrenceRow(QVector<ListDisplayRow> &targetRows,
                             QHash<QString, QStringList> &ancestorMap,
                             const EventOccurrence &occurrence,
                             int depth,
                             const QString &parentFolderId,
                             const QStringList &ancestorFolderIds) {
    ListDisplayRow row;
    row.isFolder = false;
    row.depth = depth;
    row.parentFolderId = parentFolderId;
    row.ancestorFolderIds = ancestorFolderIds;
    row.occurrence = occurrence;
    targetRows.append(row);
    ancestorMap.insert(occurrence.occurrenceKey, ancestorFolderIds);
}

void appendListFolderRow(QVector<ListDisplayRow> &targetRows,
                         const QString &label,
                         const QString &folderId,
                         const QString &parentFolderId,
                         const QStringList &ancestorFolderIds,
                         int depth) {
    ListDisplayRow row;
    row.isFolder = true;
    row.folderLabel = label;
    row.folderId = folderId;
    row.parentFolderId = parentFolderId;
    row.ancestorFolderIds = ancestorFolderIds;
    row.depth = depth;
    targetRows.append(row);
}

void appendFlatListSection(QVector<ListDisplayRow> &targetRows,
                           QHash<QString, QStringList> &ancestorMap,
                           const QVector<EventOccurrence> &sectionEntries,
                           const QString &label,
                           const QString &folderId) {
    if (sectionEntries.isEmpty()) {
        return;
    }

    appendListFolderRow(targetRows, label, folderId, QString(), {}, 0);
    const QStringList ancestors = {folderId};
    for (const EventOccurrence &occurrence : sectionEntries) {
        appendListOccurrenceRow(targetRows, ancestorMap, occurrence, 1, folderId, ancestors);
    }
}

void applyListOccurrenceRowMeta(QTableWidgetItem *item, int depth, const QString &parentFolderId) {
    if (item == nullptr) {
        return;
    }
    item->setData(ROW_KIND_ROLE, static_cast<int>(ListTableRowKind::Occurrence));
    item->setData(FOLDER_DEPTH_ROLE, depth);
    item->setData(PARENT_FOLDER_ID_ROLE, parentFolderId);
}

QDate listGroupingDate(const EventOccurrence &occurrence, EventVisualState visualState) {
    QDate returnVal; // Returns a null date not undefined if empty.
    if (visualState == EventVisualState::Completed && occurrence.completedAt.isValid()) {
        returnVal = occurrence.completedAt.date();
    } else if (occurrence.eventType == EventType::Task && occurrence.endDateTime.isValid()) {
        returnVal = occurrence.endDateTime.date();
    } else if (occurrence.startDateTime.isValid()) {
        returnVal = occurrence.startDateTime.date();
    } else if (occurrence.endDateTime.isValid()) {
        returnVal = occurrence.endDateTime.date();
    }
    return returnVal;
}

QTime listGroupingTime(const EventOccurrence &occurrence) {
    const bool hasSchedule = occurrenceHasSchedule(occurrence);
    QTime returnVal = QTime(23, 59, 59);
    if (hasSchedule) {
        const bool taskAndValid = occurrence.eventType == EventType::Task && occurrence.startDateTime.isValid();
        const bool reminderAndValid = occurrence.eventType == EventType::Reminder && occurrence.startDateTime.isValid();
        if (taskAndValid) {
            if (occurrence.allDay) {
                returnVal = QTime(0, 0);
            } else {
                returnVal = occurrence.endDateTime.time();
            }
        } else if (reminderAndValid) {
            returnVal = occurrence.startDateTime.time();
        } else if (occurrence.allDay) {
            returnVal = QTime(0, 0);
        } else if (occurrence.startDateTime.isValid()) {
            returnVal = occurrence.startDateTime.time();
        }
    }
    return returnVal;
}

void sortListSectionEntries(QVector<EventOccurrence> &entries, const QDateTime &now) {
    std::sort(entries.begin(), entries.end(), [&](const EventOccurrence &left, const EventOccurrence &right) {
        const EventVisualState leftState = visualStateForOccurrence(left, now);
        const EventVisualState rightState = visualStateForOccurrence(right, now);
        const QDate leftDate = listGroupingDate(left, leftState);
        const QDate rightDate = listGroupingDate(right, rightState);
        if (leftDate != rightDate) {
            if (!leftDate.isValid()) {
                return false;
            }
            if (!rightDate.isValid()) {
                return true;
            }
            return leftDate < rightDate;
        }

        const QTime leftTime = listGroupingTime(left);
        const QTime rightTime = listGroupingTime(right);
        if (leftTime != rightTime) {
            return leftTime < rightTime;
        }

        if (left.priority != right.priority) {
            return static_cast<int>(left.priority) > static_cast<int>(right.priority);
        }

        return left.name.localeAwareCompare(right.name) < 0;
    });
}

// Marks tasks as upcoming if its endDate is valid and its startDate
bool isUpcomingDueTaskOccurrence(const EventOccurrence &occurrence, const QDateTime &now) {
    const bool isDueTask = occurrence.eventType == EventType::Task && occurrence.endDateTime.isValid();
    bool returnVal = false;
    if (isDueTask) {
        returnVal = visualStateForOccurrence(occurrence, now) == EventVisualState::Active;
    }
    return returnVal;
}

void sortUpcomingTaskEntries(QVector<EventOccurrence> &entries) {
    std::sort(entries.begin(), entries.end(), [](const EventOccurrence &left, const EventOccurrence &right) {
        if (left.endDateTime != right.endDateTime) {
            return left.endDateTime < right.endDateTime;
        }

        if (left.priority != right.priority) {
            return static_cast<int>(left.priority) > static_cast<int>(right.priority);
        }

        return left.name.localeAwareCompare(right.name) < 0;
    });
}

// Builds the short time text shown in the main list table.
QString compactListTimeText(const EventOccurrence &occurrence) {
    const bool isTask = occurrence.eventType == EventType::Task;
    const bool hasDueDate = occurrence.endDateTime.isValid();
    const bool hasSchedule = occurrenceHasSchedule(occurrence);
    const bool isMultiDay = occurrence.startDateTime.isValid()
                            && occurrence.endDateTime.isValid()
                            && occurrence.startDateTime.date() != occurrence.endDateTime.date();
    QString returnVal = QStringLiteral("--");

    if (isTask && !hasDueDate) {
        returnVal = QStringLiteral("No due date");
    } else if (!hasSchedule) {
        returnVal = QStringLiteral("No date");
    } else if (isTask) {
        if (occurrence.allDay) {
            returnVal = QStringLiteral("All day");
        } else {
            returnVal = QStringLiteral("Due %1")
                .arg(occurrence.endDateTime.toString(QStringLiteral("h:mm AP")));
        }
    } else if (occurrence.allDay) {
        returnVal = QStringLiteral("All day");
    } else if (isMultiDay) {
        returnVal = QStringLiteral("Starts %1")
            .arg(occurrence.startDateTime.toString(QStringLiteral("h:mm AP")));
    } else if (occurrence.startDateTime.isValid()) {
        returnVal = occurrence.startDateTime.toString(QStringLiteral("h:mm AP"));
    }
    return returnVal;
}

// Builds the relative due text shown for tasks in the Upcoming folder.
QString upcomingTaskTimeText(const EventOccurrence &occurrence, const QDateTime &now) {
    QString returnVal = compactListTimeText(occurrence);

    if (occurrence.endDateTime.isValid()) {
        const int daysUntilDue = now.date().daysTo(occurrence.endDateTime.date());
        if (daysUntilDue <= 0) {
            if (occurrence.allDay) {
                returnVal = QObject::tr("Due today");
            } else {
                returnVal = QObject::tr("Due today %1")
                    .arg(occurrence.endDateTime.toString(QStringLiteral("h:mm AP")));
            }
        } else if (daysUntilDue == 1) {
            returnVal = QObject::tr("Due tomorrow");
        } else {
            returnVal = QObject::tr("Due in %1 days").arg(daysUntilDue);
        }
    }

    return returnVal;
}
QString formatOccurrenceTimeForDate(const EventOccurrence &occurrence, const QDate &date) {
    return occurrenceScheduleText(occurrence, date);
}

QString formatOccurrenceDateTimeRange(const EventOccurrence &occurrence) {
    return occurrenceDetailsDateText(occurrence);
}

QString occurrenceNameDisplayText(const EventOccurrence &occurrence) {
    return QStringLiteral("%1 / %2").arg(eventTypeDisplayText(occurrence.eventType), occurrence.name);
}

QString agendaListItemText(const EventOccurrence &occurrence, const QDateTime &now) {
    return QStringLiteral("%1\n%2").arg(occurrence.name, agendaListStatusText(occurrence, now));
}

QString listOccurrenceNameText(const EventOccurrence &occurrence) {
    return occurrence.name;
}

QString calendarOccurrenceNameText(const EventOccurrence &occurrence) {
    return QStringLiteral("%1 / %2").arg(eventTypeDisplayText(occurrence.eventType), occurrence.name);
}

QColor rowBackgroundForState(EventVisualState visualState) {
    QColor returnVal;
    switch (visualState) {
        case EventVisualState::Completed:
            returnVal = QColor(QStringLiteral("#1D3A2A"));
            break;
        case EventVisualState::PastIncomplete:
            returnVal = QColor(QStringLiteral("#343A43"));
            break;
        case EventVisualState::Active:
            break;
    }
    return returnVal;
}

QColor rowTextColorForState(EventVisualState visualState) {
    QColor returnVal;
    switch (visualState) {
        case EventVisualState::Completed:
            returnVal = QColor(QStringLiteral("#ECFDF5"));
            break;
        case EventVisualState::PastIncomplete:
            returnVal = QColor(QStringLiteral("#B8C0CC"));
            break;
        case EventVisualState::Active:
            break;
    }
    return returnVal;
}

BoardTheme taskTrackerBoardTheme() {
    BoardTheme theme;
    theme.pageBackground = QColor(QStringLiteral("#2B2B2B"));
    theme.laneBackground = QColor(QStringLiteral("#232323"));
    theme.headerBackground = QColor(QStringLiteral("#4A4A4A"));
    theme.cardBackground = QColor(QStringLiteral("#303030"));
    theme.buttonBackground = QColor(QStringLiteral("#404040"));
    theme.buttonHoverBackground = QColor(QStringLiteral("#4A4A4A"));
    theme.buttonPressedBackground = QColor(QStringLiteral("#363636"));
    theme.borderColor = QColor(QStringLiteral("#4F4F4F"));
    theme.primaryText = QColor(QStringLiteral("#F5F7FA"));
    theme.secondaryText = QColor(QStringLiteral("#C9D1DA"));
    theme.mutedText = QColor(QStringLiteral("#8A94A3"));
    theme.selectionOutline = QColor(QStringLiteral("#2563EB"));
    theme.todayOutline = QColor(QStringLiteral("#2563EB"));
    theme.completedTintBase = QColor(QStringLiteral("#22C55E"));
    theme.overdueTintBase = QColor(QStringLiteral("#6B7280"));
    return theme;
}

// Returns a pointer for to an occurence
const EventOccurrence *findOccurrenceByKey(const QVector<EventOccurrence> &occurrences, const QString &occurrenceKey) {
    const EventOccurrence *returnVal = nullptr;
    for (const EventOccurrence &occurrence : occurrences) {
        if (returnVal == nullptr && occurrence.occurrenceKey == occurrenceKey) {
            returnVal = &occurrence;
        }
    }
    return returnVal;
}

bool useItalicStatusFont(EventVisualState visualState) {
    return visualState == EventVisualState::PastIncomplete
        or visualState == EventVisualState::Completed;
}

bool useStrikeOutStatusFont(EventVisualState visualState) {
    return visualState == EventVisualState::Completed;
}

QFont styledStatusFont(const QWidget *widget, int pixelSize, int fontWeight, EventVisualState visualState, bool allowStrikeOut = false) {
    QFont font = widget != nullptr ? widget->font() : QFont();
    font.setPixelSize(pixelSize);
    font.setWeight(static_cast<QFont::Weight>(fontWeight));
    font.setItalic(useItalicStatusFont(visualState));
    font.setStrikeOut(allowStrikeOut && useStrikeOutStatusFont(visualState));
    return font;
}

QColor agendaListTitleTextColor(EventVisualState visualState, const BoardTheme &theme) {
    QColor returnVal = theme.primaryText;
    if (visualState == EventVisualState::PastIncomplete) {
        returnVal = QColor(QStringLiteral("#D1D5DB"));
    } else if (visualState == EventVisualState::Completed) {
        returnVal = QColor(QStringLiteral("#ECFDF5"));
    }
    return returnVal;
}

QColor agendaListSecondaryTextColor(EventVisualState visualState, const BoardTheme &theme) {
    QColor returnVal = theme.secondaryText;
    if (visualState == EventVisualState::PastIncomplete) {
        returnVal = QColor(QStringLiteral("#9CA3AF"));
    } else if (visualState == EventVisualState::Completed) {
        returnVal = QColor(QStringLiteral("#CFF7DD"));
    }
    return returnVal;
}

QString agendaListCardStyle(const EventOccurrence &occurrence, bool selected, const BoardTheme &theme, const QDateTime &now) {
    const EventVisualState visualState = visualStateForOccurrence(occurrence, now);
    QColor borderColor = theme.borderColor;
    QColor backgroundColor = theme.cardBackground;
    QString borderWidth = QStringLiteral("1");

    if (selected) {
        borderColor = theme.selectionOutline;
        borderWidth = QStringLiteral("2");
    } else if (visualState == EventVisualState::Completed) {
        borderColor = theme.completedTintBase;
    }

    if (visualState == EventVisualState::Completed) {
        backgroundColor = blendColors(theme.cardBackground, theme.completedTintBase, 0.22);
    } else if (visualState == EventVisualState::PastIncomplete) {
        backgroundColor = blendColors(theme.cardBackground, theme.overdueTintBase, 0.24);
    }

    QString returnVal = QStringLiteral(
                            "QFrame#agendaListCardFrame {"
                            "background-color: %1;"
                            "border: %2px solid %3;"
                            "border-radius: 10px;"
                            "}")
                            .arg(colorCss(backgroundColor), borderWidth, colorCss(borderColor));
    return returnVal;
}

QString categoryDotStyle(const QString &category) {
    return QStringLiteral("QFrame { background-color: %1; border: none; border-radius: 4px; }")
        .arg(colorCss(categoryColor(category)));
}

QFrame *createAgendaListCard(const EventOccurrence &occurrence, bool selected, const QDateTime &now, QWidget *parent) {
    const BoardTheme theme = taskTrackerBoardTheme();
    const EventVisualState visualState = visualStateForOccurrence(occurrence, now);
    const QColor titleColor = agendaListTitleTextColor(visualState, theme);
    const QColor secondaryColor = agendaListSecondaryTextColor(visualState, theme);

    auto *cardFrame = new QFrame(parent);
    cardFrame->setObjectName(QStringLiteral("agendaListCardFrame"));
    cardFrame->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    cardFrame->setStyleSheet(agendaListCardStyle(occurrence, selected, theme, now));

    auto *cardLayout = new QVBoxLayout(cardFrame);
    cardLayout->setContentsMargins(8, 7, 8, 7);
    cardLayout->setSpacing(3);

    auto *titleLabel = new QLabel(occurrence.name, cardFrame);
    titleLabel->setWordWrap(true);
    titleLabel->setStyleSheet(fontStyleCss(titleColor, 11, 700));
    titleLabel->setFont(styledStatusFont(titleLabel, 11, 700, visualState, true));
    cardLayout->addWidget(titleLabel);

    auto *typeRow = new QWidget(cardFrame);
    typeRow->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    auto *typeRowLayout = new QHBoxLayout(typeRow);
    typeRowLayout->setContentsMargins(0, 0, 0, 0);
    typeRowLayout->setSpacing(6);

    auto *dotFrame = new QFrame(typeRow);
    dotFrame->setFixedSize(8, 8);
    dotFrame->setStyleSheet(categoryDotStyle(occurrence.category));
    typeRowLayout->addWidget(dotFrame, 0, Qt::AlignVCenter);

    auto *typeLabel = new QLabel(categoryTypeDisplayText(occurrence.category, occurrence.eventType), typeRow);
    typeLabel->setStyleSheet(fontStyleCss(secondaryColor, 9, 600));
    typeLabel->setFont(styledStatusFont(typeLabel, 9, 600, visualState));
    typeRowLayout->addWidget(typeLabel, 0, Qt::AlignVCenter);
    typeRowLayout->addStretch(1);
    cardLayout->addWidget(typeRow);

    auto *dueLabel = new QLabel(agendaListStatusText(occurrence, now), cardFrame);
    dueLabel->setWordWrap(true);
    dueLabel->setStyleSheet(fontStyleCss(secondaryColor, 10, 400));
    dueLabel->setFont(styledStatusFont(dueLabel, 10, 400, visualState));
    cardLayout->addWidget(dueLabel);

    auto *priorityLabel = new QLabel(priorityMetadataText(occurrence.priority), cardFrame);
    priorityLabel->setWordWrap(true);
    priorityLabel->setStyleSheet(fontStyleCss(secondaryColor, 9, 500));
    priorityLabel->setFont(styledStatusFont(priorityLabel, 9, 500, visualState));
    cardLayout->addWidget(priorityLabel);

    return cardFrame;
}

QString categoryChipStyle(const QString &category, int verticalPadding, int horizontalPadding, int radius) {
    const QString normalizedCategory = category.trimmed();
    const QColor accentColor = normalizedCategory.isEmpty()
                                   ? QColor(QStringLiteral("#64748B"))
                                   : categoryColor(normalizedCategory);
    const QColor borderColor = accentColor.darker(105);

    return QStringLiteral("QLabel {"
                          "padding: %1px %2px;"
                          "border-radius: %3px;"
                          "font-weight: 600;"
                          "color: #FFFFFF;"
                          "background-color: %4;"
                          "border: 1px solid %5;"
                          "}")
        .arg(verticalPadding)
        .arg(horizontalPadding)
        .arg(radius)
        .arg(accentColor.name(), borderColor.name());
}

QWidget *createAgendaListSectionHeader(const QString &title, QWidget *parent) {
    auto *headerWidget = new QWidget(parent);
    headerWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto *layout = new QHBoxLayout(headerWidget);
    layout->setContentsMargins(4, 4, 4, 2);
    layout->setSpacing(0);

    auto *label = new QLabel(title, headerWidget);
    label->setStyleSheet(QStringLiteral("QLabel { color: #CBD5E1; font-size: 10px; font-weight: 700; letter-spacing: 0.4px; }"));
    layout->addWidget(label);
    layout->addStretch(1);

    return headerWidget;
}

} // namespace

// Populate the agenda section with events listed for today and tasks with no due date.
void MainWindow::populateAgendaList(const QVector<EventOccurrence> &occurrences) {
    QSignalBlocker blocker(ui->tasksListWidget);

    ui->tasksListWidget->clear();
    const QDateTime now = QDateTime::currentDateTime();
    const QDate today = now.date();

    QVector<EventOccurrence> todayEntries;
    QVector<EventOccurrence> noDueTaskEntries;

    for (const EventOccurrence &occurrence : occurrences) {
        if (occurrence.eventType == EventType::Task && !occurrence.endDateTime.isValid()) {
            noDueTaskEntries.append(occurrence);
        } else {
            todayEntries.append(occurrence);
        }
    }

    // Standard library sort
    std::sort(todayEntries.begin(),
              todayEntries.end(),
              [today](const EventOccurrence &left, const EventOccurrence &right) {
                  return compareAgendaListEntries(left, right, today);
              });

    std::sort(noDueTaskEntries.begin(), noDueTaskEntries.end(), compareNoDueTaskEntries);

    // Keep section/header creation close to the QListWidget API, but hide the widget details from the loop below.
    const auto appendSectionHeader = [this](const QString &title) {
        auto *headerItem = new QListWidgetItem(ui->tasksListWidget);
        headerItem->setFlags(Qt::ItemIsEnabled);
        headerItem->setSizeHint(QSize(0, 24));
        ui->tasksListWidget->setItemWidget(
            headerItem,
            createAgendaListSectionHeader(title, ui->tasksListWidget));
    };

    const auto appendOccurrenceItem = [this, now](const EventOccurrence &occurrence) {
        auto *item = new QListWidgetItem(ui->tasksListWidget);
        item->setData(OCCURRENCE_KEY_ROLE, occurrence.occurrenceKey);
        item->setData(EVENT_ID_ROLE, occurrence.eventId);
        item->setToolTip(agendaListStatusText(occurrence, now));
        item->setSizeHint(QSize(0, 96));
        ui->tasksListWidget->setItemWidget(
            item,
            createAgendaListCard(occurrence, false, now, ui->tasksListWidget));
    };

    if (!todayEntries.isEmpty()) {
        //appendSectionHeader(tr("Today")); // the "Today" section in the agenda list
        for (const EventOccurrence &occurrence : todayEntries) {
            appendOccurrenceItem(occurrence);
        }
    }

    if (!noDueTaskEntries.isEmpty()) {
        appendSectionHeader(tr("No Due Date Tasks"));
        for (const EventOccurrence &occurrence : noDueTaskEntries) {
            appendOccurrenceItem(occurrence);
        }
    }

    refreshAgendaListCardStyles();
}

// Builds the main ListView by splitting occurrences into top-level sections and date folders.
void MainWindow::populateListTable(const QVector<EventOccurrence> &entries) {
    QSignalBlocker blocker(ui->eventTableWidget);

    ui->eventTableWidget->clearSpans();
    ui->eventTableWidget->clearContents();
    ui->eventTableWidget->setRowCount(0);
    m_listTableState.rows.clear();
    m_listTableState.occurrenceAncestorFolderIds.clear();

    const QDateTime now = QDateTime::currentDateTime();
    QVector<EventOccurrence> activeEntries;      // Vector storing active events (endDate has not passed)
    QVector<EventOccurrence> passedEntries;      // Vector storing passed events (endDate passed)
    QVector<EventOccurrence> completedEntries;   // Vector storing completed events (endDate passed & markedComplete)

    for (const EventOccurrence &entry : entries) {
        // Add each occurence to respective vector store
        switch (visualStateForOccurrence(entry, now)) {
            case EventVisualState::Active:
                activeEntries.append(entry);
                break;
            case EventVisualState::PastIncomplete:
                passedEntries.append(entry);
                break;
            case EventVisualState::Completed:
                completedEntries.append(entry);
                break;
        }
    }

    sortListSectionEntries(activeEntries, now);
    sortListSectionEntries(passedEntries, now);
    sortListSectionEntries(completedEntries, now);

    QVector<ListDisplayRow> rows;

    QVector<EventOccurrence> upcomingTaskEntries; // Vector storing upcoming tasks (startDate passed, endDate hasn't)
    QVector<EventOccurrence> noDueTaskEntries;    // Vector storing tasks with no due date
    QVector<EventOccurrence> datedActiveEntries;
    for (const EventOccurrence &occurrence : activeEntries) {
        if (isUpcomingDueTaskOccurrence(occurrence, now)) {
            upcomingTaskEntries.append(occurrence);
        } else if (occurrence.eventType == EventType::Task && !occurrence.endDateTime.isValid()) {
            noDueTaskEntries.append(occurrence);
        } else if (listGroupingDate(occurrence, EventVisualState::Active).isValid()) {
            datedActiveEntries.append(occurrence);
        }
    }

    sortUpcomingTaskEntries(upcomingTaskEntries);
    sortListSectionEntries(noDueTaskEntries, now);
    sortListSectionEntries(datedActiveEntries, now);

    if (!upcomingTaskEntries.isEmpty()) {
        const QString upcomingFolderId = QStringLiteral("actionable:upcoming");
        appendListFolderRow(rows, tr("Upcoming"), upcomingFolderId, QString(), {}, 0);
        const QStringList ancestors = {upcomingFolderId};
        for (const EventOccurrence &occurrence : upcomingTaskEntries) {
            appendListOccurrenceRow(rows,
                                    m_listTableState.occurrenceAncestorFolderIds,
                                    occurrence,
                                    1,
                                    upcomingFolderId,
                                    ancestors);
        }
    }

    if (!noDueTaskEntries.isEmpty()) {
        const QString noDueFolderId = QStringLiteral("actionable:nodue");
        appendListFolderRow(rows, tr("No Due Date Tasks"), noDueFolderId, QString(), {}, 0);
        const QStringList ancestors = {noDueFolderId};
        for (const EventOccurrence &occurrence : noDueTaskEntries) {
            appendListOccurrenceRow(rows,
                                    m_listTableState.occurrenceAncestorFolderIds,
                                    occurrence,
                                    1,
                                    noDueFolderId,
                                    ancestors);
        }
    }

    QDate currentDate;
    bool haveDateFolder = false;
    QString currentDateFolderId;
    for (const EventOccurrence &occurrence : datedActiveEntries) {
        const QDate groupDate = listGroupingDate(occurrence, EventVisualState::Active);
        if (!haveDateFolder || currentDate != groupDate) {
            currentDate = groupDate;
            currentDateFolderId = QStringLiteral("actionable:date:%1").arg(currentDate.toString(Qt::ISODate));
            appendListFolderRow(rows,
                                formattedListFolderDate(currentDate),
                                currentDateFolderId,
                                QString(),
                                {},
                                0);
            haveDateFolder = true;
        }

        appendListOccurrenceRow(rows,
                                m_listTableState.occurrenceAncestorFolderIds,
                                occurrence,
                                1,
                                currentDateFolderId,
                                {currentDateFolderId});
    }

    appendFlatListSection(rows,
                          m_listTableState.occurrenceAncestorFolderIds,
                          passedEntries,
                          tr("Passed Events"),
                          QStringLiteral("history:passed"));
    appendFlatListSection(rows,
                          m_listTableState.occurrenceAncestorFolderIds,
                          completedEntries,
                          tr("Completed"),
                          QStringLiteral("history:completed"));

    ui->eventTableWidget->setRowCount(rows.size());
    m_listTableState.rows.resize(rows.size());

    for (qsizetype row = 0; row < rows.size(); ++row) {
        const ListDisplayRow &displayRow = rows.at(row);
        ListRowState rowState;
        rowState.isFolder = displayRow.isFolder;
        rowState.folderId = displayRow.folderId;
        rowState.parentFolderId = displayRow.parentFolderId;
        rowState.ancestorFolderIds = displayRow.ancestorFolderIds;
        rowState.depth = displayRow.depth;
        rowState.occurrenceKey = displayRow.isFolder ? QString() : displayRow.occurrence.occurrenceKey;
        m_listTableState.rows[static_cast<int>(row)] = rowState;

        if (displayRow.isFolder) {
            const bool expanded = !m_listTableState.collapsedFolderIds.contains(displayRow.folderId);
            QTableWidgetItem *folderItem = createFolderItem(displayRow.folderLabel,
                                                            displayRow.folderId,
                                                            displayRow.parentFolderId,
                                                            displayRow.depth,
                                                            expanded);
            ui->eventTableWidget->setItem(static_cast<int>(row), 0, folderItem);
            ui->eventTableWidget->setSpan(static_cast<int>(row), 0, 1, ui->eventTableWidget->columnCount());
            ui->eventTableWidget->setRowHeight(static_cast<int>(row), listFolderRowHeight(displayRow.folderId));
            continue;
        }

        const EventOccurrence &entry = displayRow.occurrence;
        const QString timeText = displayRow.parentFolderId == QStringLiteral("actionable:upcoming")
            ? upcomingTaskTimeText(entry, now)
            : compactListTimeText(entry);
        QTableWidgetItem *timeItem = createReadOnlyItem(timeText, entry.occurrenceKey, entry.eventId);
        applyListOccurrenceRowMeta(timeItem, displayRow.depth, displayRow.parentFolderId);
        ui->eventTableWidget->setItem(static_cast<int>(row), 0, timeItem);

        QTableWidgetItem *nameItem = createReadOnlyItem(listOccurrenceNameText(entry), entry.occurrenceKey, entry.eventId);
        applyListOccurrenceRowMeta(nameItem, displayRow.depth, displayRow.parentFolderId);
        ui->eventTableWidget->setItem(static_cast<int>(row), 1, nameItem);

        QTableWidgetItem *typeItem = createReadOnlyItem(eventTypeDisplayText(entry.eventType), entry.occurrenceKey, entry.eventId);
        applyListOccurrenceRowMeta(typeItem, displayRow.depth, displayRow.parentFolderId);
        ui->eventTableWidget->setItem(static_cast<int>(row), 2, typeItem);

        QTableWidgetItem *categoryItem = createReadOnlyItem(
            entry.category.trimmed().isEmpty() ? QStringLiteral("--") : entry.category,
            entry.occurrenceKey,
            entry.eventId);
        categoryItem->setData(CATEGORY_ROLE, entry.category);
        categoryItem->setToolTip(entry.category.trimmed().isEmpty()
                                     ? tr("No category assigned")
                                     : tr("Category: %1").arg(entry.category.trimmed()));
        applyListOccurrenceRowMeta(categoryItem, displayRow.depth, displayRow.parentFolderId);
        ui->eventTableWidget->setItem(static_cast<int>(row), 3, categoryItem);

        QTableWidgetItem *priorityItem = createReadOnlyItem(priorityToString(entry.priority), entry.occurrenceKey, entry.eventId);
        applyListOccurrenceRowMeta(priorityItem, displayRow.depth, displayRow.parentFolderId);
        ui->eventTableWidget->setItem(static_cast<int>(row), 4, priorityItem);

        QTableWidgetItem *locationItem = createReadOnlyItem(
            locationDisplayText(entry.location),
            entry.occurrenceKey,
            entry.eventId);
        applyListOccurrenceRowMeta(locationItem, displayRow.depth, displayRow.parentFolderId);
        ui->eventTableWidget->setItem(static_cast<int>(row), 5, locationItem);
        ui->eventTableWidget->setRowHeight(static_cast<int>(row), 28);
        applyOccurrenceRowState(ui->eventTableWidget, static_cast<int>(row), entry, 3, false);
    }
    applyListFolderVisibility();
}

// Hide or show list rows based on which folders are collapsed.
void MainWindow::applyListFolderVisibility() {
    for (int row = 0; row < ui->eventTableWidget->rowCount(); ++row) {
        const bool hasRowState = row >= 0 && row < m_listTableState.rows.size();
        bool hidden = false;

        if (hasRowState) {
            const ListRowState &rowState = m_listTableState.rows.at(row);
            const int rowHeight = rowState.isFolder ? listFolderRowHeight(rowState.folderId) : 28;
            ui->eventTableWidget->setRowHeight(row, rowHeight);
            for (const QString &ancestorId : rowState.ancestorFolderIds) {
                if (m_listTableState.collapsedFolderIds.contains(ancestorId)) {
                    hidden = true;
                    break;
                }
            }
        }
        ui->eventTableWidget->setRowHidden(row, hidden);
    }
}

bool MainWindow::isListFolderRow(const QTableWidget *table, int row) const {
    const ListTableState *state = listStateForTable(table);
    bool returnVal = false;

    if (state != nullptr) {
        const bool rowInRange = row >= 0 && row < state->rows.size();
        if (rowInRange) {
            returnVal = state->rows.at(row).isFolder;
        }
    }
    return returnVal;
}

void MainWindow::toggleListFolder(QTableWidget *table, int row) {
    ListTableState *state = listStateForTable(table);
    bool canToggle = state != nullptr && isListFolderRow(table, row);

    if (canToggle) {
        const ListRowState &rowState = state->rows.at(row);
        canToggle = !rowState.folderId.isEmpty();

        if (canToggle) {
            const bool shouldExpand = state->collapsedFolderIds.contains(rowState.folderId);
            if (shouldExpand) {
                state->collapsedFolderIds.remove(rowState.folderId);
            } else {
                state->collapsedFolderIds.insert(rowState.folderId);
            }

            QTableWidgetItem *item = table->item(row, 0);
            if (item != nullptr) {
                applyFolderItemIndicator(item, shouldExpand);
            }

            applyListFolderVisibility();
        }
    }
}

int MainWindow::firstSelectableListRow(const QTableWidget *table) const {
    int returnVal = -1;
    if (table != nullptr) {
        for (int row = 0; row < table->rowCount(); ++row) {
            const bool rowHidden = table->isRowHidden(row);
            const bool folderRow = isListFolderRow(table, row);
            if (!rowHidden && !folderRow && returnVal < 0) {
                const QTableWidgetItem *item = table->item(row, 0);
                const bool hasOccurrence = item != nullptr
                                           && !item->data(OCCURRENCE_KEY_ROLE).toString().isEmpty();
                if (hasOccurrence) {
                    returnVal = row;
                }
            }
        }
    }
    return returnVal;
}

void MainWindow::refreshAgendaListCardStyles() {
    const BoardTheme theme = taskTrackerBoardTheme();
    const QDateTime now = QDateTime::currentDateTime();

    for (int row = 0; row < ui->tasksListWidget->count(); ++row) {
        QListWidgetItem *item = ui->tasksListWidget->item(row);
        if (item == nullptr) {
            continue;
        }

        QFrame *cardFrame = qobject_cast<QFrame *>(ui->tasksListWidget->itemWidget(item));
        if (cardFrame == nullptr) {
            continue;
        }

        const QString occurrenceKey = item->data(OCCURRENCE_KEY_ROLE).toString();
        const EventOccurrence *occurrence = findOccurrenceByKey(m_currentAgendaListEntries, occurrenceKey);
        if (occurrence == nullptr) {
            continue;
        }

        cardFrame->setStyleSheet(agendaListCardStyle(*occurrence, item->isSelected(), theme, now));
    }
}

// Refreshes the details panel using the currently selected occurrence.
void MainWindow::refreshDetailsPanel(const EventOccurrence *occurrence) {
    const bool hasOccurrence = occurrence != nullptr;

    if (!hasOccurrence) {
        ui->detailNameValueLabel->setText(QStringLiteral("No Item Selected"));
        ui->detailTypeValueLabel->setText(QStringLiteral("--"));
        ui->detailDateValueLabel->setText(QStringLiteral("--"));
        ui->detailLocationValueLabel->setText(QStringLiteral("N/A"));
        ui->detailPriorityValueLabel->setText(QStringLiteral("--"));
        ui->detailCategoryValueLabel->setText(QStringLiteral("--"));
        ui->detailDescriptionTextEdit->setPlainText(QString());
        {
            QSignalBlocker blocker(ui->completeEventCheckBox);
            ui->completeEventCheckBox->setChecked(false);
        }
        ui->completeEventCheckBox->setToolTip(tr("Select an item to update its completion status."));
        applyCategoryDetailAccent(QString());
    } else {
        const QString descriptionText = occurrence->description.trimmed().isEmpty()
            ? QStringLiteral("No description provided.")
            : occurrence->description;
        const QString completionTip = occurrence->completed
            ? tr("Uncheck to reopen this item and move it back into active views.")
            : tr("Check to mark this item completed and move it into history when applicable.");

        ui->detailNameValueLabel->setText(occurrence->name);
        ui->detailTypeValueLabel->setText(eventTypeDisplayText(occurrence->eventType));
        ui->detailDateValueLabel->setText(formatOccurrenceDateTimeRange(*occurrence));
        ui->detailLocationValueLabel->setText(locationDisplayText(occurrence->location));
        ui->detailPriorityValueLabel->setText(priorityToString(occurrence->priority));
        applyCategoryDetailAccent(occurrence->category);
        ui->detailDescriptionTextEdit->setPlainText(descriptionText);
        {
            QSignalBlocker blocker(ui->completeEventCheckBox);
            ui->completeEventCheckBox->setChecked(occurrence->completed);
        }
        ui->completeEventCheckBox->setToolTip(completionTip);
    }
}

void MainWindow::refreshTableAccentStates() {
    const auto refreshTable = [this](QTableWidget *table, int accentColumn, const QVector<EventOccurrence> &entries) {
        if (table == nullptr) {
            return;
        }

        for (int row = 0; row < table->rowCount(); ++row) {
            QTableWidgetItem *keyItem = table->item(row, 0);
            if (keyItem == nullptr) {
                continue;
            }

            const QString occurrenceKey = keyItem->data(OCCURRENCE_KEY_ROLE).toString();
            const EventOccurrence *occurrence = findOccurrenceByKey(entries, occurrenceKey);
            if (occurrence == nullptr) {
                continue;
            }

            const bool rowSelected = table->selectionModel() != nullptr
                && table->selectionModel()->isRowSelected(row, QModelIndex());
            applyOccurrenceRowState(table, row, *occurrence, accentColumn, rowSelected);
        }
    };

    refreshTable(ui->eventTableWidget, 3, m_currentListEntries);
    refreshAgendaListCardStyles();
}

void MainWindow::applyCategoryAccent(QTableWidgetItem *item,
                                     const QString &category,
                                     bool selected,
                                     EventVisualState visualState,
                                     bool strikeOutText) const {
    const bool canApply = item != nullptr;

    if (canApply) {
        const QString normalizedCategory = category.trimmed();
        const bool hasCategory = !normalizedCategory.isEmpty();
        const bool clearColors = selected || visualState == EventVisualState::Active;
        QFont categoryFont = item->font();
        categoryFont.setBold(true);
        categoryFont.setItalic(useItalicStatusFont(visualState));
        categoryFont.setStrikeOut(strikeOutText && useStrikeOutStatusFont(visualState));
        item->setFont(categoryFont);

        if (!hasCategory) {
            if (clearColors) {
                item->setData(Qt::ForegroundRole, QVariant());
                item->setData(Qt::BackgroundRole, QVariant());
            } else {
                item->setForeground(rowTextColorForState(visualState));
                item->setBackground(rowBackgroundForState(visualState).darker(108));
            }
            item->setToolTip(tr("No category assigned"));
        } else {
            item->setToolTip(tr("Category: %1").arg(normalizedCategory));

            if (selected) {
                item->setData(Qt::ForegroundRole, QVariant());
                item->setData(Qt::BackgroundRole, QVariant());
            } else {
                QColor accentColor = categoryColor(normalizedCategory).darker(105);
                QColor textColor(Qt::white);
                if (visualState == EventVisualState::Completed) {
                    accentColor = QColor(QStringLiteral("#228B5A"));
                    textColor = QColor(QStringLiteral("#F0FDF4"));
                } else if (visualState == EventVisualState::PastIncomplete) {
                    accentColor = QColor(QStringLiteral("#4B5563"));
                    textColor = QColor(QStringLiteral("#E5E7EB"));
                }

                item->setForeground(textColor);
                item->setBackground(accentColor);
            }
        }
    }
}

// Applies the visual state colors and fonts to a table row.
void MainWindow::applyOccurrenceRowState(QTableWidget *table,
                                         int row,
                                         const EventOccurrence &occurrence,
                                         int accentColumn,
                                         bool selected) const {
    const bool canApply = table != nullptr;

    if (canApply) {
        const EventVisualState visualState = visualStateForOccurrence(occurrence);
        const QColor rowBackground = rowBackgroundForState(visualState);
        const QColor rowText = rowTextColorForState(visualState);
        const int nameColumn = table == ui->eventTableWidget ? 1 : 0;

        for (int column = 0; column < table->columnCount(); ++column) {
            QTableWidgetItem *item = table->item(row, column);
            if (item != nullptr) {
                const bool accentCell = column == accentColumn;
                const bool nameCell = column == nameColumn;
                QFont itemFont = item->font();
                itemFont.setItalic(useItalicStatusFont(visualState));
                itemFont.setStrikeOut(nameCell && useStrikeOutStatusFont(visualState));
                if (!accentCell) {
                    itemFont.setBold(false);
                }
                item->setFont(itemFont);

                if (accentCell) {
                    applyCategoryAccent(item,
                                        occurrence.category,
                                        selected,
                                        visualState,
                                        column == 0);
                } else if (selected || visualState == EventVisualState::Active) {
                    item->setData(Qt::ForegroundRole, QVariant());
                    item->setData(Qt::BackgroundRole, QVariant());
                } else {
                    item->setForeground(rowText);
                    item->setBackground(rowBackground);
                }
            }
        }
    }
}

void MainWindow::applyCategoryDetailAccent(const QString &category) {
    const QString normalizedCategory = category.trimmed();
    const bool hasCategory = !normalizedCategory.isEmpty();

    if (!hasCategory) {
        ui->detailCategoryValueLabel->setText(QStringLiteral("--"));
        ui->detailCategoryValueLabel->setStyleSheet(
            QStringLiteral("QLabel {"
                           "padding: 4px 10px;"
                           "border-radius: 10px;"
                           "background-color: #E2E8F0;"
                           "color: #475569;"
                           "border: 1px solid #CBD5E1;"
                           "}"));
        ui->detailCategoryValueLabel->setToolTip(tr("No category assigned"));
    } else {
        ui->detailCategoryValueLabel->setText(normalizedCategory);
        ui->detailCategoryValueLabel->setStyleSheet(categoryChipStyle(normalizedCategory, 1, 6, 8));
        ui->detailCategoryValueLabel->setToolTip(tr("Category: %1").arg(normalizedCategory));
    }
}
