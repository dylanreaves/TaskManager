// Implements many shared helper functions
#include "Utility.h"

#include <algorithm>

#include <QVector>
#include <QList>
#include <QLocale>

#include "EventManager.h"

namespace {

struct CategoryColorSetting {
    QString name;
    QColor color;
};

const QVector<CategoryColorSetting> &categoryColorSettings() {
    static const QVector<CategoryColorSetting> settings = {
        {QStringLiteral("school"), QColor(QStringLiteral("#2563EB"))},
        {QStringLiteral("personal"), QColor(QStringLiteral("#DC2626"))},
        {QStringLiteral("work"), QColor(QStringLiteral("#0F766E"))},
        {QStringLiteral("social"), QColor(QStringLiteral("#BE185D"))},
        {QStringLiteral("health"), QColor(QStringLiteral("#C2410C"))},
        {QStringLiteral("fitness"), QColor(QStringLiteral("#15803D"))},
        {QStringLiteral("finance"), QColor(QStringLiteral("#B45309"))},
        {QStringLiteral("travel"), QColor(QStringLiteral("#6D28D9"))},
        {QStringLiteral("family"), QColor(QStringLiteral("#0284C7"))},
        {QStringLiteral("home"), QColor(QStringLiteral("#4D7C0F"))},
        {QStringLiteral("errands"), QColor(QStringLiteral("#64748B"))},
        {QStringLiteral("meeting"), QColor(QStringLiteral("#0369A1"))},
    };
    return settings;
}

const QList<QColor> &categoryPalette() {
    static const QList<QColor> palette = {
        QColor(QStringLiteral("#2563EB")),
        QColor(QStringLiteral("#0F766E")),
        QColor(QStringLiteral("#C2410C")),
        QColor(QStringLiteral("#DC2626")),
        QColor(QStringLiteral("#0284C7")),
        QColor(QStringLiteral("#6D28D9")),
        QColor(QStringLiteral("#4D7C0F")),
        QColor(QStringLiteral("#BE185D")),
        QColor(QStringLiteral("#7C2D12")),
        QColor(QStringLiteral("#0369A1")),
        QColor(QStringLiteral("#15803D")),
        QColor(QStringLiteral("#B45309")),
    };

    return palette;
}

QString joinedRepeatDays(const QList<Qt::DayOfWeek> &repeatDays) {
    QStringList labels;
    labels.reserve(repeatDays.size());

    const QLocale locale;
    for (Qt::DayOfWeek day : repeatDays) {
        labels.append(locale.dayName(static_cast<int>(day), QLocale::ShortFormat));
    }

    return labels.join(QStringLiteral(", "));
}

QString invalidScheduleText(const EventOccurrence &occurrence) {
    return occurrence.eventType == EventType::Task
        ? QStringLiteral("No due date")
        : QStringLiteral("No date");
}

} // namespace

// Converts the prority enum to a string literal
QString priorityToString(Priority priority) {
    QString returnVal = QStringLiteral("Unknown");
    switch (priority) {
        case Priority::VeryLow:
            returnVal = QStringLiteral("Very Low");
            break;
        case Priority::Low:
            returnVal = QStringLiteral("Low");
            break;
        case Priority::Medium:
            returnVal = QStringLiteral("Medium");
            break;
        case Priority::High:
            returnVal = QStringLiteral("High");
            break;
        case Priority::VeryHigh:
            returnVal = QStringLiteral("Very High");
            break;
        }
    return returnVal;
}

// Converts the prority enum to a string literal thats better formated for files
QString priorityToStorageString(Priority priority) {
    QString returnVal = QStringLiteral("VeryLow");
    switch (priority) {
        case Priority::VeryLow:
            returnVal = QStringLiteral("VeryLow");
            break;
        case Priority::Low:
            returnVal = QStringLiteral("Low");
            break;
        case Priority::Medium:
            returnVal = QStringLiteral("Medium");
            break;
        case Priority::High:
            returnVal = QStringLiteral("High");
            break;
        case Priority::VeryHigh:
            returnVal = QStringLiteral("VeryHigh");
            break;
        }
    return returnVal;
}

// Converts the priority string back into a an Enum
Priority priorityFromString(const QString &value) {
    // Removes the space from the string and makes it lowercase for easy comparison
    const QString normalized = value.trimmed().remove(QLatin1Char(' ')).toLower();
    Priority returnVal = Priority::VeryLow;

    if (normalized == QStringLiteral("verylow")) {
        returnVal = Priority::VeryLow;
    } else if (normalized == QStringLiteral("low")) {
        returnVal = Priority::Low;
    } else if (normalized == QStringLiteral("medium")) {
        returnVal = Priority::Medium;
    } else if (normalized == QStringLiteral("high")) {
        returnVal = Priority::High;
    } else if (normalized == QStringLiteral("veryhigh")) {
        returnVal = Priority::VeryHigh;
    }
    return returnVal;
}

// Converts the EventType into a string literal
QString eventTypeToString(EventType eventType) {
    QString returnVal = QStringLiteral("Event");
    switch (eventType) {
        case EventType::Task:
            returnVal = QStringLiteral("Task");
            break;
        case EventType::Event:
            returnVal = QStringLiteral("Event");
            break;
        case EventType::Reminder:
            returnVal = QStringLiteral("Reminder");
            break;
        case EventType::ScheduleBlock:
            returnVal = QStringLiteral("ScheduleBlock");
            break;
    }
    return returnVal;
}

// Converts a string literal into an EventType Enum
EventType eventTypeFromString(const QString &value) {
    const QString normalized = value.trimmed().remove(QLatin1Char(' ')).toLower();
    EventType returnVal = EventType::Event;
    if (normalized == QStringLiteral("task")) {
        returnVal = EventType::Task;
    } else if (normalized == QStringLiteral("reminder")) {
        returnVal = EventType::Reminder;
    } else if (normalized == QStringLiteral("scheduleblock")) {
        returnVal = EventType::ScheduleBlock;
    }
    return returnVal;
}

// Converts the EventType Enum into a string that will be displayed.
QString eventTypeDisplayText(EventType eventType) {
    QString returnVal = QStringLiteral("Event");
    switch (eventType) {
        case EventType::Task:
            returnVal = QStringLiteral("Task");
            break;
        case EventType::Event:
            returnVal = QStringLiteral("Event");
            break;
        case EventType::Reminder:
            returnVal = QStringLiteral("Reminder");
            break;
        case EventType::ScheduleBlock:
            returnVal = QStringLiteral("Schedule");
            break;
    }
    return returnVal;
}

// Converts the EventType Enum into a string that will be displayed.
QString eventTypeChipText(EventType eventType) {
    QString returnVal = QStringLiteral("Event");
    switch (eventType) {
        case EventType::Task:
            returnVal = QStringLiteral("Task");
            break;
        case EventType::Event:
            returnVal = QStringLiteral("Event");
            break;
        case EventType::Reminder:
            returnVal = QStringLiteral("Reminder");
            break;
        case EventType::ScheduleBlock:
            returnVal = QStringLiteral("Schedule");
            break;
    }
    return returnVal;
}

// Returns a QColor value from EventType enum
QColor eventTypeAccentColor(EventType eventType) {
    QColor returnVal = QColor(QStringLiteral("#7C3AED"));
    switch (eventType) {
        case EventType::Task:
            returnVal = QColor(QStringLiteral("#EA580C"));
            break;
        case EventType::Event:
            returnVal = QColor(QStringLiteral("#2563EB"));
            break;
        case EventType::Reminder:
            returnVal = QColor(QStringLiteral("#DC2626"));
            break;
        case EventType::ScheduleBlock:
            returnVal = QColor(QStringLiteral("#7C3AED"));
            break;
    }
    return returnVal;
}

// Displays the category field as empty if no category exists
QString categoryDisplayText(const QString &category) {
    const QString trimmedCategory = category.trimmed();
    return trimmedCategory.isEmpty() ? QStringLiteral("No category") : trimmedCategory;
}

// Displays the category field if a category exists
QString categoryTypeDisplayText(const QString &category, EventType eventType) {
    return QStringLiteral("%1 / %2")
        .arg(categoryDisplayText(category), eventTypeDisplayText(eventType));
}

QString priorityMetadataText(Priority priority) {
    return QStringLiteral("Priority: %1").arg(priorityToString(priority));
}

// Converts ReccurenceType Enum to a string
QString recurrenceTypeToString(RecurrenceType recurrenceType) {
    QString returnVal = QStringLiteral("None");
    switch (recurrenceType) {
        case RecurrenceType::None:
            returnVal = QStringLiteral("None");
            break;
        case RecurrenceType::Daily:
            returnVal = QStringLiteral("Daily");
            break;
        case RecurrenceType::Weekly:
            returnVal = QStringLiteral("Weekly");
            break;
        case RecurrenceType::Monthly:
            returnVal = QStringLiteral("Monthly");
            break;
        case RecurrenceType::Yearly:
            returnVal = QStringLiteral("Yearly");
            break;
        case RecurrenceType::Custom:
            returnVal = QStringLiteral("Custom");
            break;
    }
    return returnVal;
}

// Converts a string to RecurrenceType Enum
RecurrenceType recurrenceTypeFromString(const QString &value) {
    const QString normalized = value.trimmed();
    RecurrenceType returnVal = RecurrenceType::None;

    if (normalized.isEmpty()
        || normalized.compare(QStringLiteral("None"), Qt::CaseInsensitive) == 0
        || normalized.compare(QStringLiteral("Don't repeat"), Qt::CaseInsensitive) == 0) {
        returnVal = RecurrenceType::None;
    } else if (normalized.compare(QStringLiteral("Daily"), Qt::CaseInsensitive) == 0) {
        returnVal = RecurrenceType::Daily;
    } else if (normalized.compare(QStringLiteral("Weekly"), Qt::CaseInsensitive) == 0) {
        returnVal = RecurrenceType::Weekly;
    } else if (normalized.compare(QStringLiteral("Monthly"), Qt::CaseInsensitive) == 0) {
        returnVal = RecurrenceType::Monthly;
    } else if (normalized.compare(QStringLiteral("Yearly"), Qt::CaseInsensitive) == 0) {
        returnVal = RecurrenceType::Yearly;
    } else if (normalized.compare(QStringLiteral("Custom"), Qt::CaseInsensitive) == 0) {
        returnVal = RecurrenceType::Custom;
    }
    return returnVal;
}

// Formats QDateTime into a more readable format
QString formatDateTime(const QDateTime &dateTime) {
    QString returnVal = QStringLiteral("--");
    if (dateTime.isValid()) {
        returnVal = dateTime.toString(QStringLiteral("MMM d, yyyy h:mm AP"));
    }
    return returnVal;
}

// Formats QDateTime range into a more readable format i.e (XX:xx - XX:xx)
QString formatDateTimeRange(const QDateTime &startDateTime, const QDateTime &endDateTime) {
    QString returnVal = QStringLiteral("--");

    if (startDateTime.isValid() && endDateTime.isValid()) {
        if (startDateTime.date() == endDateTime.date()) {
            returnVal = QStringLiteral("%1, %2 - %3")
                            .arg(startDateTime.toString(QStringLiteral("MMM d, yyyy")))
                            .arg(startDateTime.toString(QStringLiteral("h:mm AP")))
                            .arg(endDateTime.toString(QStringLiteral("h:mm AP")));
        } else {
            returnVal = QStringLiteral("%1 - %2")
                            .arg(formatDateTime(startDateTime), formatDateTime(endDateTime));
        }
    }
    return returnVal;
}

// Simple check to see if an event high priority
bool isHighPriority(Priority priority) {
    return priority == Priority::High || priority == Priority::VeryHigh;
}

// Gets category color from CategoryColorSettings vector
QColor categoryColor(const QString &category) {
    const QString normalized = category.trimmed().toLower();
    QColor returnVal(QStringLiteral("#64748B"));

    if (!normalized.isEmpty()) {
        const QVector<CategoryColorSetting> &settings = categoryColorSettings();
        bool foundOverride = false;

        for (const CategoryColorSetting &setting : settings) {
            if (setting.name == normalized) {
                returnVal = setting.color;
                foundOverride = true;
                break;
            }
        }

        if (!foundOverride) {
            const QList<QColor> &palette = categoryPalette();
            const uint paletteIndex = qHash(normalized) % static_cast<uint>(palette.size());
            returnVal = palette.at(static_cast<int>(paletteIndex));
        }
    }
    return returnVal;
}

// Slightly changes the color of the categories
QColor translucentCategoryColor(const QString &category, int alpha) {
    QColor color = categoryColor(category);
    color.setAlpha(qBound(0, alpha, 255));
    return color;
}

// Returns the location field of an event a string
QString locationDisplayText(const QString &location) {
    return location.trimmed().isEmpty() ? QStringLiteral("N/A") : location.trimmed();
}

QColor blendColors(const QColor &base, const QColor &overlay, qreal overlayAmount) {
    const qreal amount = std::clamp(overlayAmount, 0.0, 1.0);
    return QColor(
        static_cast<int>(base.red() * (1.0 - amount) + overlay.red() * amount),
        static_cast<int>(base.green() * (1.0 - amount) + overlay.green() * amount),
        static_cast<int>(base.blue() * (1.0 - amount) + overlay.blue() * amount));
}

QString colorCss(const QColor &color) {
    return color.name(QColor::HexRgb);
}

QString rgbaCss(const QColor &color) {
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

QString fontStyleCss(const QColor &color, int fontSize, int fontWeight) {
    return QStringLiteral("QLabel { color: %1; font-size: %2px; font-weight: %3; }")
        .arg(colorCss(color))
        .arg(fontSize)
        .arg(fontWeight);
}

// Changes recurrence options displayed for each reccurence type
QString recurrenceSummaryForEvent(const Event &event) {
    if (!event.hasRecurrence()) {
        return QStringLiteral("Does not repeat");
    }

    const int interval = std::max(1, event.getRecurrenceInterval());
    QString summary;

    switch (event.getRecurrenceType()) {
        case RecurrenceType::Daily:
            summary = interval == 1
                ? QStringLiteral("Every day")
                : QStringLiteral("Every %1 days").arg(interval);
            break;
        case RecurrenceType::Weekly:
            summary = interval == 1
                ? QStringLiteral("Every week")
                : QStringLiteral("Every %1 weeks").arg(interval);
            if (!event.getRepeatDays().isEmpty()) {
                summary += QStringLiteral(" on %1").arg(joinedRepeatDays(event.getRepeatDays()));
            }
            break;
        case RecurrenceType::Monthly:
            summary = interval == 1
                ? QStringLiteral("Every month")
                : QStringLiteral("Every %1 months").arg(interval);
            break;
        case RecurrenceType::Yearly:
            summary = interval == 1
                ? QStringLiteral("Every year")
                : QStringLiteral("Every %1 years").arg(interval);
            break;
        case RecurrenceType::Custom:
            summary = interval == 1
                ? QStringLiteral("Custom recurrence")
                : QStringLiteral("Custom recurrence every %1 intervals").arg(interval);
            break;
        case RecurrenceType::None:
            summary = QStringLiteral("Does not repeat");
            break;
    }

    if (event.getRecurrenceUntil().isValid()) {
        summary += QStringLiteral(" until %1")
                       .arg(event.getRecurrenceUntil().toString(QStringLiteral("MMM d, yyyy")));
    }

    return summary;
}

// Simple check to see if a an occurence has a startDate and an endDate
bool occurrenceHasSchedule(const EventOccurrence &occurrence) {
    return occurrence.startDateTime.isValid() && occurrence.endDateTime.isValid();
}

// Builds the main schedule label for an occurrence by describing due times, reminder times,
// all-day items, same-day ranges, and multi-day start/end context for the currently visible date.
QString occurrenceScheduleText(const EventOccurrence &occurrence, const QDate &visibleDate) {
    QString returnVal = invalidScheduleText(occurrence);

    if (occurrenceHasSchedule(occurrence)) {
        if (occurrence.eventType == EventType::Task) {
            if (occurrence.allDay) {
                returnVal = QStringLiteral("Due %1")
                                .arg(occurrence.endDateTime.date().toString(QStringLiteral("MMM d, yyyy")));
            } else {
                const bool useShortTime =
                    visibleDate.isValid() && occurrence.endDateTime.date() == visibleDate;
                returnVal = QStringLiteral("Due %1")
                                .arg(useShortTime
                                         ? occurrence.endDateTime.toString(QStringLiteral("h:mm AP"))
                                         : formatDateTime(occurrence.endDateTime));
            }
        } else if (occurrence.eventType == EventType::Reminder) {
            const bool useShortTime =
                visibleDate.isValid() && occurrence.startDateTime.date() == visibleDate;
            returnVal = QStringLiteral("Remind at %1")
                            .arg(useShortTime
                                     ? occurrence.startDateTime.toString(QStringLiteral("h:mm AP"))
                                     : formatDateTime(occurrence.startDateTime));
        } else if (occurrence.allDay
                   && occurrence.startDateTime.date() == occurrence.endDateTime.date()) {
            returnVal = QStringLiteral("All day");
        } else if (visibleDate.isValid()) {
            if (occurrence.startDateTime.date() == occurrence.endDateTime.date()) {
                returnVal = QStringLiteral("%1 - %2")
                                .arg(occurrence.startDateTime.toString(QStringLiteral("h:mm AP")))
                                .arg(occurrence.endDateTime.toString(QStringLiteral("h:mm AP")));
            } else if (occurrence.startDateTime.date() == visibleDate) {
                returnVal = QStringLiteral("Starts %1")
                                .arg(occurrence.allDay
                                         ? occurrence.startDateTime.date().toString(QStringLiteral("MMM d"))
                                         : occurrence.startDateTime.toString(QStringLiteral("h:mm AP")));
            } else if (occurrence.endDateTime.date() == visibleDate) {
                returnVal = QStringLiteral("Ends %1")
                                .arg(occurrence.allDay
                                         ? occurrence.endDateTime.date().toString(QStringLiteral("MMM d"))
                                         : occurrence.endDateTime.toString(QStringLiteral("h:mm AP")));
            } else {
                returnVal = formatDateTimeRange(occurrence.startDateTime, occurrence.endDateTime);
            }
        } else {
            returnVal = formatDateTimeRange(occurrence.startDateTime, occurrence.endDateTime);
        }
    }

    return returnVal;
}

QString occurrenceDetailsDateText(const EventOccurrence &occurrence) {
    return occurrenceScheduleText(occurrence);
}

// Returns a string version of the startDate of an event
QString occurrenceListStartText(const EventOccurrence &occurrence) {
    QString returnVal = invalidScheduleText(occurrence); // Fallback to invalid
    if (occurrenceHasSchedule(occurrence)) {
        switch (occurrence.eventType) {
            case EventType::Task:
                returnVal = QStringLiteral("Due %1").arg(formatDateTime(occurrence.endDateTime));
                break;
            case EventType::Reminder:
                returnVal = QStringLiteral("Remind %1").arg(formatDateTime(occurrence.startDateTime));
                break;
            case EventType::Event:
            case EventType::ScheduleBlock:
                if (occurrence.allDay) {
                    returnVal = occurrence.startDateTime.date().toString(QStringLiteral("MMM d, yyyy"));
                } else {
                    returnVal = formatDateTime(occurrence.startDateTime);
                }
            break;
        }
    }

    return returnVal;
}

QString occurrenceListEndText(const EventOccurrence &occurrence) {
    QString returnVal = QStringLiteral("--");
    if (occurrenceHasSchedule(occurrence)) {
        switch (occurrence.eventType) {
            case EventType::Task:
            case EventType::Reminder:
                returnVal = QStringLiteral("--");
                break;
            case EventType::Event:
            case EventType::ScheduleBlock:
                if (occurrence.allDay) {
                    returnVal = occurrence.endDateTime.date().toString(QStringLiteral("MMM d, yyyy"));
                } else {
                    returnVal = formatDateTime(occurrence.endDateTime);
                }
            break;
        }
    }
    return returnVal;
}

// Summarizes a task deadline for the agenda list by turning the due date into compact overdue,
// today, tomorrow, or future wording based on the current date and time.
QString agendaListDueSummaryText(const EventOccurrence &occurrence, const QDateTime &now) {
    QString returnVal = QStringLiteral("No due date");

    if (occurrence.eventType == EventType::Task && occurrence.endDateTime.isValid()) {
        const QDate dueDate = occurrence.endDateTime.date();
        const QDate today = now.date();

        if (occurrence.endDateTime < now) {
            if (dueDate == today) {
                returnVal = occurrence.allDay
                    ? QStringLiteral("Overdue today")
                    : QStringLiteral("Overdue today at %1")
                          .arg(occurrence.endDateTime.toString(QStringLiteral("h:mm AP")));
            } else if (dueDate == today.addDays(-1)) {
                returnVal = occurrence.allDay
                    ? QStringLiteral("Overdue yesterday")
                    : QStringLiteral("Overdue yesterday at %1")
                          .arg(occurrence.endDateTime.toString(QStringLiteral("h:mm AP")));
            } else {
                returnVal = occurrence.allDay
                    ? QStringLiteral("Overdue %1").arg(dueDate.toString(QStringLiteral("MMM d")))
                    : QStringLiteral("Overdue %1")
                          .arg(occurrence.endDateTime.toString(QStringLiteral("MMM d, h:mm AP")));
            }
        } else if (dueDate == today) {
            returnVal = occurrence.allDay
                ? QStringLiteral("Due today")
                : QStringLiteral("Due today at %1")
                      .arg(occurrence.endDateTime.toString(QStringLiteral("h:mm AP")));
        } else if (dueDate == today.addDays(1)) {
            returnVal = occurrence.allDay
                ? QStringLiteral("Due tomorrow")
                : QStringLiteral("Due tomorrow at %1")
                      .arg(occurrence.endDateTime.toString(QStringLiteral("h:mm AP")));
        } else {
            returnVal = occurrence.allDay
                ? QStringLiteral("Due %1").arg(dueDate.toString(QStringLiteral("MMM d")))
                : QStringLiteral("Due %1")
                      .arg(occurrence.endDateTime.toString(QStringLiteral("MMM d, h:mm AP")));
        }
    }

    return returnVal;
}

// Chooses the short agenda status label that appears beside an occurrence by checking task due
// states, reminder times, missing schedules, multi-day progress, and same-day time ranges.
QString agendaListStatusText(const EventOccurrence &occurrence, const QDateTime &now) {
    const QDate today = now.date();
    QString returnVal = invalidScheduleText(occurrence);

    if (occurrence.eventType == EventType::Task) {
        if (!occurrence.endDateTime.isValid()) {
            returnVal = QStringLiteral("No due date");
        } else if (occurrence.endDateTime.date() == today) {
            if (occurrence.allDay) {
                returnVal = QStringLiteral("Due today");
            } else {
                returnVal = QStringLiteral("Due %1")
                                .arg(occurrence.endDateTime.toString(QStringLiteral("h:mm AP")));
            }
        }
    } else if (occurrence.eventType == EventType::Reminder && occurrence.startDateTime.isValid()) {
        returnVal = QStringLiteral("Remind at %1")
                        .arg(occurrence.startDateTime.toString(QStringLiteral("h:mm AP")));
    } else if (occurrenceHasSchedule(occurrence)) {
        const QDate startDate = occurrence.startDateTime.date();
        const QDate endDate = occurrence.endDateTime.date();
        const bool multiDay = startDate != endDate;

        if (multiDay) {
            if (startDate < today && endDate > today) {
                returnVal = QStringLiteral("Continues");
            } else if (endDate == today && startDate < today) {
                returnVal = occurrence.allDay
                    ? QStringLiteral("Ends %1").arg(endDate.toString(QStringLiteral("MMM d")))
                    : QStringLiteral("Ends %1")
                          .arg(occurrence.endDateTime.toString(QStringLiteral("h:mm AP")));
            } else if (startDate == today && endDate > today) {
                returnVal = occurrence.allDay
                    ? QStringLiteral("Starts %1").arg(startDate.toString(QStringLiteral("MMM d")))
                    : QStringLiteral("Starts %1")
                          .arg(occurrence.startDateTime.toString(QStringLiteral("h:mm AP")));
            } else if (occurrence.allDay) {
                returnVal = QStringLiteral("All day");
            } else if (startDate == today) {
                returnVal = QStringLiteral("%1 - %2")
                                .arg(occurrence.startDateTime.toString(QStringLiteral("h:mm AP")))
                                .arg(occurrence.endDateTime.toString(QStringLiteral("h:mm AP")));
            } else {
                returnVal = occurrenceScheduleText(occurrence, today);
            }
        } else if (occurrence.allDay) {
            returnVal = QStringLiteral("All day");
        } else if (startDate == today) {
            returnVal = QStringLiteral("%1 - %2")
                            .arg(occurrence.startDateTime.toString(QStringLiteral("h:mm AP")))
                            .arg(occurrence.endDateTime.toString(QStringLiteral("h:mm AP")));
        } else {
            returnVal = occurrenceScheduleText(occurrence, today);
        }
    }

    return returnVal;
}



