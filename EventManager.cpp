// Implements event interactions like storage, filtering, sorting and occurence generation
#include "EventManager.h"

#include <QSet>
#include <QTime>

#include <algorithm>

#include "Utility.h"

namespace {

QDateTime startOfDay(const QDate &date) {
    return QDateTime(date, QTime(0, 0, 0));
}

QDateTime endOfDay(const QDate &date) {
    return QDateTime(date, QTime(23, 59, 59));
}

QDate monthGridStartDateForMonth(const QDate &visibleMonth) {
    QDate returnVal;

    if (visibleMonth.isValid()) {
        const QDate monthStart(visibleMonth.year(), visibleMonth.month(), 1);
        const int daysBack = monthStart.dayOfWeek() % 7; // Sunday-first grid.
        returnVal = monthStart.addDays(-daysBack);
    }

    return returnVal;
}

bool eventHasConcreteSchedule(const Event &event) {
    return event.getStartDateTime().isValid() && event.getEndDateTime().isValid();
}

bool eventUsesGeneratedRecurrence(const Event &event) {
    return eventHasConcreteSchedule(event) && event.hasRecurrence() && event.getRecurrenceType() != RecurrenceType::Custom;
}

QDate recurrenceListRangeStart(const QDateTime &now) {
    const int RECURRING_PAST_DAYS = 30;
    return now.date().addDays(-RECURRING_PAST_DAYS);
}

QDate recurrenceListRangeEnd(const QDateTime &now) {
    const int RECURRING_FUTURE_DAYS = 180;
    return now.date().addDays(RECURRING_FUTURE_DAYS);
}

int occurrenceDaySpan(const Event &event) {
    return event.getStartDateTime().date().daysTo(event.getEndDateTime().date());
}

qint64 occurrenceDurationSeconds(const Event &event) {
    return std::max<qint64>(0, event.getStartDateTime().secsTo(event.getEndDateTime()));
}

QDateTime occurrenceEndForGeneratedStart(const Event &event, const QDateTime &occurrenceStartDateTime) {
    QDateTime returnVal = QDateTime();
    if (occurrenceStartDateTime.isValid()) {
        returnVal = occurrenceStartDateTime.addSecs(occurrenceDurationSeconds(event));
    }
    return returnVal;
}

int sundayFirstDayOffset(Qt::DayOfWeek day) {
    return static_cast<int>(day) % 7;
}

// Normalize repeat-day settings so weekly recurrence always works with a predictable weekday list.
QList<Qt::DayOfWeek> normalizedRepeatDays(const Event &event) {
    QList<Qt::DayOfWeek> repeatDays = event.getRepeatDays();
    if (repeatDays.isEmpty() && event.getStartDateTime().isValid()) {
        repeatDays.append(static_cast<Qt::DayOfWeek>(event.getStartDateTime().date().dayOfWeek()));
    }

    std::sort(repeatDays.begin(), repeatDays.end(), [](Qt::DayOfWeek left, Qt::DayOfWeek right) {
        return sundayFirstDayOffset(left) < sundayFirstDayOffset(right);
    });
    repeatDays.erase(std::unique(repeatDays.begin(), repeatDays.end()), repeatDays.end());
    return repeatDays;
}

bool eventSpansDate(const Event &event, const QDate &date) {
    bool returnVal = false;

    if (event.getStartDateTime().isValid() && event.getEndDateTime().isValid() && date.isValid()) {
        returnVal = event.getStartDateTime().date() <= date && event.getEndDateTime().date() >= date;
    }
    return returnVal;
}

bool eventOverlapsDateRange(const Event &event, const QDate &rangeStart, const QDate &rangeEnd) {
    bool returnVal = false;

    if (event.getStartDateTime().isValid()
        && event.getEndDateTime().isValid()
        && rangeStart.isValid()
        && rangeEnd.isValid()) {
        returnVal = event.getEndDateTime().date() >= rangeStart
            && event.getStartDateTime().date() <= rangeEnd;
    }
    return returnVal;
}

bool eventIsBacklogCandidate(const Event &event) {
    return !event.getStartDateTime().isValid() && !event.getEndDateTime().isValid();
}

int visualStateSortRank(EventVisualState visualState) {
    int returnVal = 0;

    switch (visualState) {
        case EventVisualState::Active:
            returnVal = 0;
            break;
        case EventVisualState::PastIncomplete:
            returnVal = 1;
            break;
        case EventVisualState::Completed:
            returnVal = 2;
            break;
    }
    return returnVal;
}

int monthItemSortBucket(const EventOccurrence &occurrence) {
    int returnVal = occurrence.allDay ? 1 : 2;
    if (occurrence.startDateTime.isValid() && occurrence.endDateTime.isValid()
        && occurrence.startDateTime.date() != occurrence.endDateTime.date()) {
        returnVal = 0;
    }
    return returnVal;
}

QTime monthItemSortTime(const EventOccurrence &occurrence) {
    QTime returnVal(23, 59, 59);
    if (occurrence.eventType == EventType::Task && occurrence.endDateTime.isValid()) {
        returnVal = occurrence.endDateTime.time();
    } else if (occurrence.startDateTime.isValid()) {
        returnVal = occurrence.startDateTime.time();
    }
    return returnVal;
}

QDateTime visibleSortAnchor(const EventOccurrence &occurrence, const QDate &visibleDate) {
    QDateTime returnVal;

    if (occurrence.startDateTime.isValid()) {
        returnVal = occurrence.startDateTime;

        if (visibleDate.isValid()
            && occurrence.startDateTime.date() < visibleDate
            && occurrence.endDateTime.date() >= visibleDate) {
            returnVal = startOfDay(visibleDate);
        }
    }

    return returnVal;
}

bool occurrenceIsVisibleInRange(const QDate &rangeStart, const QDate &rangeEnd, const QDateTime &occurrenceStartDateTime, const QDateTime &occurrenceEndDateTime) {
    bool returnVal = false;

    if (occurrenceStartDateTime.isValid() && occurrenceEndDateTime.isValid()) {
        returnVal = occurrenceEndDateTime.date() >= rangeStart
            && occurrenceStartDateTime.date() <= rangeEnd;
    }
    return returnVal;
}

void updateMonthCellJumpDate(MonthCellState &cell, const QDate &clickedDate, const QDate &candidateJumpDate) {
    if (!candidateJumpDate.isValid()) {
        return;
    }

    if (candidateJumpDate == clickedDate) {
        cell.actionableJumpDate = clickedDate;
        return;
    }

    if (!cell.actionableJumpDate.isValid() || (cell.actionableJumpDate != clickedDate && candidateJumpDate < cell.actionableJumpDate)) {
        cell.actionableJumpDate = candidateJumpDate;
    }
}

bool compareMonthCellItems(const MonthCellItem &left, const MonthCellItem &right) {
    if (left.occurrence.priority != right.occurrence.priority) {
        return static_cast<int>(left.occurrence.priority) > static_cast<int>(right.occurrence.priority);
    }

    const int leftBucket = monthItemSortBucket(left.occurrence);
    const int rightBucket = monthItemSortBucket(right.occurrence);
    if (leftBucket != rightBucket) {
        return leftBucket < rightBucket;
    }

    const QTime leftTime = monthItemSortTime(left.occurrence);
    const QTime rightTime = monthItemSortTime(right.occurrence);
    if (leftTime != rightTime) {
        return leftTime < rightTime;
    }

    return left.occurrence.name.localeAwareCompare(right.occurrence.name) < 0;
}

bool compareMonthPopoverItems(const MonthPopoverItem &left, const MonthPopoverItem &right) {
    const int leftBucket = monthItemSortBucket(left.occurrence);
    const int rightBucket = monthItemSortBucket(right.occurrence);
    if (leftBucket != rightBucket) {
        return leftBucket < rightBucket;
    }

    const QTime leftTime = monthItemSortTime(left.occurrence);
    const QTime rightTime = monthItemSortTime(right.occurrence);
    if (leftTime != rightTime) {
        return leftTime < rightTime;
    }

    if (left.occurrence.priority != right.occurrence.priority) {
        return static_cast<int>(left.occurrence.priority) > static_cast<int>(right.occurrence.priority);
    }

    return left.occurrence.name.localeAwareCompare(right.occurrence.name) < 0;
}

bool compareMonthSpans(const MonthSpanItem &left, const MonthSpanItem &right) {
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
}

} // namespace

// Classify one visible occurrence as active, overdue, or completed.
EventVisualState visualStateForOccurrence(const EventOccurrence &occurrence, const QDateTime &now) {
    EventVisualState returnVal = EventVisualState::Active;
    if (occurrence.completed) {
        returnVal = EventVisualState::Completed;
    } else if (occurrence.endDateTime.isValid() && occurrence.endDateTime < now) {
        returnVal = EventVisualState::PastIncomplete;
    }
    return returnVal;
}


bool occurrenceIsBacklog(const EventOccurrence &occurrence) {
    return !occurrenceHasSchedule(occurrence);
}

// Constructor that automatically increments Event IDs
EventManager::EventManager()
    : m_nextId(1) {
}

// Sets the next ID after an Event is created
int EventManager::generateNextId() {
    return m_nextId++;
}

void EventManager::setEvents(const QVector<Event> &events) {
    m_events = events;
    recalculateNextId();
}

// Function for adding an event
void EventManager::addEvent(Event event) {
    if (event.getId() <= 0) {
        event.setId(generateNextId());
    } else if (event.getId() >= m_nextId) {
        m_nextId = event.getId() + 1;
    }

    m_events.append(event);
}

// Function for deleting an Event
bool EventManager::deleteEvent(int id) {
    const auto it = std::find_if(m_events.begin(), m_events.end(), [id](const Event &event) {
        return event.getId() == id;
    });

    if (it == m_events.end()) {
        return false;
    }

    m_events.erase(it);
    return true;
}

// Function for editing an Event
bool EventManager::editEvent(int id, const Event &updatedEvent) {
    bool returnVal = false;
    for (Event &event : m_events) {
        if (event.getId() == id) {
            Event replacement = updatedEvent;
            replacement.setId(id);
            if (replacement.getEndDateTime().isValid()
                && replacement.getEndDateTime() >= QDateTime::currentDateTime()) {
                replacement.setAutoCompleteSuppressed(false);
            } else {
                replacement.setAutoCompleteSuppressed(event.isAutoCompleteSuppressed());
            }
            event = replacement;
            returnVal = true;
            break;
        }
    }

    return returnVal;
}

// Function for marking an event as complete
bool EventManager::markEventCompleted(int id, const QDateTime &completedAt) {
    Event *event = findEventById(id);
    if (event == nullptr) {
        return false;
    }

    event->setCompleted(true);
    event->setCompletedAt(completedAt);
    event->setAutoCompleteSuppressed(false);
    return true;
}

// Function for unmarking an event as complete
bool EventManager::reopenEvent(int id) {
    Event *event = findEventById(id);
    if (event == nullptr) {
        return false;
    }

    event->setCompleted(false);
    event->setCompletedAt(QDateTime());
    event->setAutoCompleteSuppressed(true);
    return true;
}

bool EventManager::autoCompleteOverdueEvents(const QDateTime &now) {
    bool changed = false;

    for (Event &event : m_events) {
        if (event.isCompleted() || event.isAutoCompleteSuppressed() || event.hasRecurrence()) {
            continue;
        }

        if (!event.getEndDateTime().isValid() || event.getEndDateTime() >= now) {
            continue;
        }

        event.setCompleted(true);
        event.setCompletedAt(event.getEndDateTime());
        event.setAutoCompleteSuppressed(false);
        changed = true;
    }

    return changed;
}

const QVector<Event> &EventManager::getEvents() const {
    return m_events;
}

Event *EventManager::findEventById(int id) {
    for (Event &event : m_events) {
        if (event.getId() == id) {
            return &event;
        }
    }
    return nullptr;
}

const Event *EventManager::findEventById(int id) const {
    for (const Event &event : m_events) {
        if (event.getId() == id) {
            return &event;
        }
    }
    return nullptr;
}

QVector<EventOccurrence> EventManager::listEntries(const EventQuery &query, const QDateTime &now) const {
    QVector<EventOccurrence> results;

    for (const Event &event : m_events) {
        if (!matchesBaseFacetFilters(event, query)) {
            continue;
        }

        const QVector<EventOccurrence> visibleOccurrences = visibleOccurrencesForFilter(event, query.timeFilter, now);
        for (const EventOccurrence &occurrence : visibleOccurrences) {
            if (matchesFacetFilters(occurrence, query)) {
                results.append(occurrence);
            }
        }
    }

    sortOccurrences(results, query.timeFilter, now);
    return results;
}

QVector<EventOccurrence> EventManager::occurrencesForDate(const QDate &date, const EventQuery &query, const QDateTime &now) const {
    QVector<EventOccurrence> results;

    for (const Event &event : m_events) {
        if (!matchesTimeFilter(event, query.timeFilter, now) || !matchesBaseFacetFilters(event, query)) {
            continue;
        }

        if (query.timeFilter == EventTimeFilter::Completed) {
            if (effectiveDate(event, query.timeFilter) != date) {
                continue;
            }
            results.append(buildOccurrence(event));
            continue;
        }

        const QVector<EventOccurrence> visibleOccurrences = expandOccurrencesForRange(event, date, date);
        for (const EventOccurrence &occurrence : visibleOccurrences) {
            if (matchesFacetFilters(occurrence, query)) {
                results.append(occurrence);
            }
        }
    }

    sortOccurrences(results, query.timeFilter, now, date);
    return results;
}

QVector<EventOccurrence> EventManager::backlogEntries(const EventQuery &query, const QDateTime &now) const {
    QVector<EventOccurrence> results;

    for (const Event &event : m_events) {
        if (!eventIsBacklogCandidate(event)
            || !matchesBacklogTimeFilter(event, query.timeFilter)
            || !matchesBaseFacetFilters(event, query)) {
            continue;
        }

        results.append(buildOccurrence(event));
    }

    sortOccurrences(results, query.timeFilter, now);
    return results;
}

// Gets the event objects that will be added to the agenda section
QVector<EventOccurrence> EventManager::agendaListEntries(const QDateTime &now) const {
    QVector<EventOccurrence> results;
    const QDate today = now.date();

    for (const Event &event : m_events) {
        if (event.isCompleted()) {
            continue;
        }

        if (event.getEventType() == EventType::Task && !event.getEndDateTime().isValid()) {
            results.append(buildOccurrence(event));
            continue;
        }

        const QVector<EventOccurrence> visibleOccurrences = expandOccurrencesForRange(event, today, today);
        results += visibleOccurrences;
    }

    return results;
}

// Gets the event objects that are happening in the current week
QMap<QDate, QVector<EventOccurrence>> EventManager::occurrencesForWeek(const QDate &referenceDate, const EventQuery &query, const QDateTime &now) const {
    QMap<QDate, QVector<EventOccurrence>> grouped;
    const QDate weekStart = weekStartForDate(referenceDate);

    for (int offset = 0; offset < 7; ++offset) {
        grouped.insert(weekStart.addDays(offset), {});
    }

    for (const Event &event : m_events) {
        if (!matchesTimeFilter(event, query.timeFilter, now) || !matchesBaseFacetFilters(event, query)) {
            continue;
        }

        if (query.timeFilter == EventTimeFilter::Completed) {
            const QDate date = effectiveDate(event, query.timeFilter);
            if (date < weekStart || date > weekStart.addDays(6)) {
                continue;
            }

            grouped[date].append(buildOccurrence(event));
            continue;
        }

        const QDate weekEnd = weekStart.addDays(6);
        const QVector<EventOccurrence> visibleOccurrences = expandOccurrencesForRange(event, weekStart, weekEnd);
        for (const EventOccurrence &occurrence : visibleOccurrences) {
            const QDate firstVisibleDate = std::max(weekStart, occurrence.startDateTime.date());
            const QDate lastVisibleDate = std::min(weekEnd, occurrence.endDateTime.date());
            for (QDate visibleDate = firstVisibleDate; visibleDate <= lastVisibleDate; visibleDate = visibleDate.addDays(1)) {
                grouped[visibleDate].append(occurrence);
            }
        }
    }

    for (auto it = grouped.begin(); it != grouped.end(); ++it) {
        sortOccurrences(it.value(), query.timeFilter, now, it.key());
    }

    return grouped;
}

// Returns a QStringList of the categories
QStringList EventManager::categories() const {
    QSet<QString> seenCategories;
    QStringList categories;

    for (const Event &event : m_events) {
        const QString category = event.getCategory().trimmed();
        if (category.isEmpty() || seenCategories.contains(category)) {
            continue;
        }

        seenCategories.insert(category);
        categories.append(category);
    }

    std::sort(categories.begin(), categories.end(), [](const QString &left, const QString &right) {
        return left.localeAwareCompare(right) < 0;
    });
    return categories;
}

QStringList EventManager::distinctCategoriesForDate(const QDate &date, const EventQuery &query, const QDateTime &now) const {
    QSet<QString> seenCategories;
    QStringList categoriesForDate;
    const QVector<EventOccurrence> entries = occurrencesForDate(date, query, now);

    for (const EventOccurrence &entry : entries) {
        const QString category = entry.category.trimmed();
        if (category.isEmpty() || seenCategories.contains(category)) {
            continue;
        }

        seenCategories.insert(category);
        categoriesForDate.append(category);
    }

    std::sort(categoriesForDate.begin(), categoriesForDate.end(),
               [](const QString &left, const QString &right) {
                   return left.localeAwareCompare(right) < 0;
               });
    return categoriesForDate;
}

// Build the fixed 6-week month model used by the custom month calendar widget.
MonthViewModel EventManager::monthViewModel(const QDate &visibleMonth, const EventQuery &query, const QDateTime &now) const {
    MonthViewModel model;
    model.visibleMonth = QDate(visibleMonth.year(), visibleMonth.month(), 1);
    model.gridStartDate = monthGridStartDateForMonth(model.visibleMonth);
    model.legendCategories = categories();

    if (!model.visibleMonth.isValid() || !model.gridStartDate.isValid()) {
        return model;
    }

    model.cells.resize(42);
    for (int index = 0; index < model.cells.size(); ++index) {
        MonthCellState &cell = model.cells[index];
        cell.date = model.gridStartDate.addDays(index);
        cell.inCurrentMonth = cell.date.month() == model.visibleMonth.month()
            && cell.date.year() == model.visibleMonth.year();
    }

    const QDate gridEndDate = model.gridStartDate.addDays(model.cells.size() - 1);

    auto cellForDate = [&](const QDate &date) -> MonthCellState * {
        const int index = model.gridStartDate.daysTo(date);
        return index >= 0 && index < model.cells.size() ? &model.cells[index] : nullptr;
    };

    for (const Event &event : m_events) {
        if (!matchesTimeFilter(event, query.timeFilter, now) || !matchesBaseFacetFilters(event, query)) {
            continue;
        }

        if (query.timeFilter == EventTimeFilter::Completed) {
            const QDate completedDate = effectiveDate(event, query.timeFilter);
            if (!completedDate.isValid() || completedDate < model.gridStartDate || completedDate > gridEndDate) {
                continue;
            }

            MonthCellState *cell = cellForDate(completedDate);
            if (cell == nullptr) {
                continue;
            }

            const EventOccurrence occurrence = buildOccurrence(event);
            const EventVisualState visualState = visualStateForOccurrence(occurrence, now);
            cell->dayItems.append({occurrence, QDate(), visualState});
            cell->popoverItems.append({occurrence, completedDate, QDate(), visualState});
            continue;
        }

        const QVector<EventOccurrence> visibleOccurrences = expandOccurrencesForRange(event, model.gridStartDate, gridEndDate);
        for (const EventOccurrence &occurrence : visibleOccurrences) {
            const EventVisualState visualState = visualStateForOccurrence(occurrence, now);
            const QDate occurrenceStartDate = occurrence.startDateTime.date();
            const QDate occurrenceEndDate = occurrence.endDateTime.date();
            const bool isSpan = occurrenceStartDate != occurrenceEndDate;
            const QDate firstVisibleDate = std::max(model.gridStartDate, occurrenceStartDate);
            const QDate lastVisibleDate = std::min(gridEndDate, occurrenceEndDate);

            if (!isSpan) {
                MonthCellState *cell = cellForDate(occurrenceStartDate);
                if (cell == nullptr) {
                    continue;
                }

                const QDate jumpDate = visualState == EventVisualState::Active ? occurrenceStartDate : QDate();
                cell->dayItems.append({occurrence, jumpDate, visualState});
                cell->popoverItems.append({occurrence, occurrenceStartDate, jumpDate, visualState});
                updateMonthCellJumpDate(*cell, occurrenceStartDate, jumpDate);
                continue;
            }

            model.spans.append({occurrence,
                                visualState == EventVisualState::Active ? occurrenceStartDate : QDate(),
                                visualState});

            for (QDate visibleDate = firstVisibleDate; visibleDate <= lastVisibleDate; visibleDate = visibleDate.addDays(1)) {
                MonthCellState *cell = cellForDate(visibleDate);
                if (cell == nullptr) {
                    continue;
                }

                const QDate jumpDate = visualState == EventVisualState::Active
                    ? (visibleDate == occurrenceStartDate ? visibleDate : occurrenceStartDate)
                    : QDate();
                cell->popoverItems.append({occurrence, visibleDate, jumpDate, visualState});
                updateMonthCellJumpDate(*cell, visibleDate, jumpDate);
            }
        }
    }

    for (MonthCellState &cell : model.cells) {
        std::sort(cell.dayItems.begin(), cell.dayItems.end(), compareMonthCellItems);
        std::sort(cell.popoverItems.begin(), cell.popoverItems.end(), compareMonthPopoverItems);
    }

    std::sort(model.spans.begin(), model.spans.end(), compareMonthSpans);
    return model;
}

EventOccurrence EventManager::masterEntryForEvent(int id) const {
    const Event *event = findEventById(id);
    return event == nullptr ? EventOccurrence() : buildOccurrence(*event, true);
}

EventOccurrence EventManager::occurrenceForEvent(int id, const QDateTime &originalStartDateTime) const {
    const Event *event = findEventById(id);
    if (event == nullptr) {
        return EventOccurrence();
    }

    if (eventUsesGeneratedRecurrence(*event) && !event->isCompleted()) {
        if (originalStartDateTime.isValid()) {
            return buildOccurrence(*event,
                                   originalStartDateTime,
                                   occurrenceEndForGeneratedStart(*event, originalStartDateTime));
        }

        QVector<EventOccurrence> visibleOccurrences = visibleOccurrencesForFilter(
            *event,
            EventTimeFilter::AllEvents,
            QDateTime::currentDateTime());
        if (!visibleOccurrences.isEmpty()) {
            sortOccurrences(visibleOccurrences, EventTimeFilter::AllEvents, QDateTime::currentDateTime());
            return visibleOccurrences.first();
        }
    }

    return buildOccurrence(*event);
}

int EventManager::totalEvents() const {
    return m_events.size();
}

int EventManager::eventsTodayCount(const QDateTime &now) const {
    int count = 0;
    for (const Event &event : m_events) {
        count += visibleOccurrencesForFilter(event, EventTimeFilter::Today, now).size();
    }
    return count;
}

int EventManager::upcomingEventsCount(const QDateTime &now) const {
    int count = 0;
    for (const Event &event : m_events) {
        count += visibleOccurrencesForFilter(event, EventTimeFilter::Upcoming, now).size();
    }
    return count;
}

int EventManager::highPriorityEventsCount() const {
    int count = 0;
    for (const Event &event : m_events) {
        if (isHighPriority(event.getPriority())) {
            ++count;
        }
    }
    return count;
}

bool EventManager::isEmpty() const {
    return m_events.isEmpty();
}

QString EventManager::occurrenceKey(int eventId, const QDateTime &originalStartDateTime) {
    return QStringLiteral("%1|%2").arg(eventId).arg(originalStartDateTime.isValid()
                 ? originalStartDateTime.toString(Qt::ISODate)
                 : QStringLiteral("invalid"));
}

QString EventManager::seriesMasterKey(int eventId) {
    return QStringLiteral("series:%1").arg(eventId);
}

bool EventManager::isSeriesMasterKey(const QString &key) {
    return key.startsWith(QStringLiteral("series:"));
}

QDate EventManager::weekStartForDate(const QDate &date) {
    return date.addDays(-(date.dayOfWeek() % 7));
}

// Keep each view stable by sorting visible occurrences in the same order every refresh.
void EventManager::sortOccurrences(QVector<EventOccurrence> &occurrences, EventTimeFilter filter, const QDateTime &now, const QDate &visibleDate) {
    if (filter == EventTimeFilter::Completed) {
        std::sort(occurrences.begin(), occurrences.end(), [](const EventOccurrence &left, const EventOccurrence &right) {
            if (left.completedAt != right.completedAt) {
                return left.completedAt > right.completedAt;
            }
            return left.name.localeAwareCompare(right.name) < 0;
        });
        return;
    }

    std::sort(occurrences.begin(),
              occurrences.end(),
              [&](const EventOccurrence &left, const EventOccurrence &right) {
                  const EventVisualState leftState = visualStateForOccurrence(left, now);
                  const EventVisualState rightState = visualStateForOccurrence(right, now);
                  const int leftStateRank = visualStateSortRank(leftState);
                  const int rightStateRank = visualStateSortRank(rightState);

                  if (leftStateRank != rightStateRank) {
                      return leftStateRank < rightStateRank;
                  }

                  if (leftState == EventVisualState::Completed
                      && rightState == EventVisualState::Completed
                      && left.completedAt != right.completedAt) {
                      return left.completedAt > right.completedAt;
                  }

                  const bool leftBacklog = occurrenceIsBacklog(left);
                  const bool rightBacklog = occurrenceIsBacklog(right);
                  if (leftBacklog != rightBacklog) {
                      return !leftBacklog;
                  }

                  if (leftBacklog && rightBacklog) {
                      if (left.priority != right.priority) {
                          return static_cast<int>(left.priority) > static_cast<int>(right.priority);
                      }

                      return left.name.localeAwareCompare(right.name) < 0;
                  }

                  const QDateTime leftAnchor = visibleSortAnchor(left, visibleDate);
                  const QDateTime rightAnchor = visibleSortAnchor(right, visibleDate);
                  if (leftAnchor != rightAnchor) {
                      return leftAnchor < rightAnchor;
                  }

                  if (left.endDateTime != right.endDateTime) {
                      return left.endDateTime < right.endDateTime;
                  }

                  return left.name.localeAwareCompare(right.name) < 0;
              });
}

bool EventManager::matchesFacetFilters(const EventOccurrence &occurrence, const EventQuery &query) {
    bool returnVal = true;
    const QString categoryFilter = query.category.trimmed();
    const QString searchText = query.searchText.trimmed();

    if (!searchText.isEmpty() && !occurrence.name.contains(searchText, Qt::CaseInsensitive)) {
        returnVal = false;
    }

    if (returnVal && query.priorityEnabled && occurrence.priority != query.priority) {
        returnVal = false;
    }

    if (returnVal
        && !categoryFilter.isEmpty()
        && categoryFilter.compare(occurrence.category.trimmed(), Qt::CaseInsensitive) != 0) {
        returnVal = false;
    }

    return returnVal;
}

bool EventManager::matchesBaseFacetFilters(const Event &event, const EventQuery &query) {
    bool returnVal = true;
    const QString categoryFilter = query.category.trimmed();
    const QString searchText = query.searchText.trimmed();

    if (!searchText.isEmpty() && !event.getName().contains(searchText, Qt::CaseInsensitive)) {
        returnVal = false;
    }

    if (returnVal && query.priorityEnabled && event.getPriority() != query.priority) {
        returnVal = false;
    }

    if (returnVal
        && !categoryFilter.isEmpty()
        && categoryFilter.compare(event.getCategory().trimmed(), Qt::CaseInsensitive) != 0) {
        returnVal = false;
    }

    return returnVal;
}

bool EventManager::matchesBacklogTimeFilter(const Event &event, EventTimeFilter filter) {
    bool returnVal = false;
    const bool isCompleted = event.isCompleted();

    switch (filter) {
        case EventTimeFilter::Completed:
            returnVal = isCompleted;
            break;
        case EventTimeFilter::Recurring:
            returnVal = false;
            break;
        case EventTimeFilter::AllEvents:
        case EventTimeFilter::Today:
        case EventTimeFilter::Upcoming:
        case EventTimeFilter::ThisWeek:
            returnVal = !isCompleted;
            break;
    }
    return returnVal;
}

// Expand one stored event into the concrete visible occurrences that overlap the requested date range.
QVector<EventOccurrence> EventManager::expandOccurrencesForRange(const Event &event, const QDate &rangeStart, const QDate &rangeEnd) const {
    QVector<EventOccurrence> occurrences;

    if (!rangeStart.isValid() || !rangeEnd.isValid() || rangeEnd < rangeStart || !eventHasConcreteSchedule(event)) {
        return occurrences;
    }

    if (!eventUsesGeneratedRecurrence(event) || event.isCompleted()) {
        if (eventOverlapsDateRange(event, rangeStart, rangeEnd)) {
            occurrences.append(buildOccurrence(event));
        }
        return occurrences;
    }

    const QDateTime anchorStartDateTime = event.getStartDateTime();
    const QDateTime anchorEndDateTime = event.getEndDateTime();
    const QDate recurrenceUntil = event.getRecurrenceUntil();
    const int interval = std::max(1, event.getRecurrenceInterval());
    const int daySpan = occurrenceDaySpan(event);
    const QDate earliestRelevantStartDate = rangeStart.addDays(-daySpan);

    switch (event.getRecurrenceType()) {
        case RecurrenceType::Daily: {
            int occurrenceOffsetDays = 0;
            if (anchorStartDateTime.date() < earliestRelevantStartDate) {
                const int daysToRelevantRange = anchorStartDateTime.date().daysTo(earliestRelevantStartDate);
                occurrenceOffsetDays = (daysToRelevantRange / interval) * interval;
                QDateTime candidateStartDateTime = anchorStartDateTime.addDays(occurrenceOffsetDays);
                while (candidateStartDateTime.date() < earliestRelevantStartDate) {
                    occurrenceOffsetDays += interval;
                    candidateStartDateTime = anchorStartDateTime.addDays(occurrenceOffsetDays);
                }
                while (occurrenceOffsetDays >= interval
                       && anchorStartDateTime.addDays(occurrenceOffsetDays - interval).date() >= earliestRelevantStartDate) {
                    occurrenceOffsetDays -= interval;
                }
            }

            for (QDateTime occurrenceStartDateTime = anchorStartDateTime.addDays(occurrenceOffsetDays),
                           occurrenceEndDateTime = anchorEndDateTime.addDays(occurrenceOffsetDays);
                 occurrenceStartDateTime.date() <= rangeEnd;
                  occurrenceStartDateTime = occurrenceStartDateTime.addDays(interval),
                             occurrenceEndDateTime = occurrenceEndDateTime.addDays(interval)) {
                  if (recurrenceUntil.isValid() && occurrenceStartDateTime.date() > recurrenceUntil) {
                      break;
                  }
                  if (occurrenceIsVisibleInRange(rangeStart, rangeEnd, occurrenceStartDateTime,occurrenceEndDateTime)) {
                      occurrences.append(buildOccurrence(event, occurrenceStartDateTime, occurrenceEndDateTime));
                  }
              }
              break;
          }
        case RecurrenceType::Weekly: {
            const QList<Qt::DayOfWeek> repeatDays = normalizedRepeatDays(event);
            if (repeatDays.isEmpty()) {
                break;
            }

            const QDate anchorWeekStart = weekStartForDate(anchorStartDateTime.date());
            const QDate relevantWeekStart = weekStartForDate(earliestRelevantStartDate);
            const int weeksToRelevantRange = std::max<int>(
                0,
                static_cast<int>(anchorWeekStart.daysTo(relevantWeekStart) / 7));
            int cycleOffsetWeeks = (weeksToRelevantRange / interval) * interval;
            QDate currentWeekStart = anchorWeekStart.addDays(cycleOffsetWeeks * 7);
            while (currentWeekStart > relevantWeekStart && cycleOffsetWeeks >= interval) {
                cycleOffsetWeeks -= interval;
                currentWeekStart = anchorWeekStart.addDays(cycleOffsetWeeks * 7);
            }

            for (; currentWeekStart <= rangeEnd; currentWeekStart = currentWeekStart.addDays(interval * 7)) {
                for (Qt::DayOfWeek day : repeatDays) {
                    const QDate occurrenceStartDate = currentWeekStart.addDays(sundayFirstDayOffset(day));
                    if (occurrenceStartDate < anchorStartDateTime.date()) {
                        continue;
                    }

                    const QDateTime occurrenceStartDateTime(occurrenceStartDate, anchorStartDateTime.time());
                      if (recurrenceUntil.isValid() && occurrenceStartDateTime.date() > recurrenceUntil) {
                          continue;
                      }

                      const QDateTime occurrenceEndDateTime =
                          QDateTime(occurrenceStartDateTime).addSecs(occurrenceDurationSeconds(event));
                      if (occurrenceIsVisibleInRange(rangeStart, rangeEnd, occurrenceStartDateTime, occurrenceEndDateTime)) {
                          occurrences.append(buildOccurrence(event, occurrenceStartDateTime, occurrenceEndDateTime));
                      }
                  }
              }
              break;
        }
        case RecurrenceType::Monthly: {
            for (QDateTime occurrenceStartDateTime = anchorStartDateTime, occurrenceEndDateTime = anchorEndDateTime;
                 occurrenceStartDateTime.date() <= rangeEnd;
                 occurrenceStartDateTime = occurrenceStartDateTime.addMonths(interval),
                           occurrenceEndDateTime = occurrenceEndDateTime.addMonths(interval)) {
                  if (recurrenceUntil.isValid() && occurrenceStartDateTime.date() > recurrenceUntil) {
                      break;
                  }

                  if (occurrenceIsVisibleInRange(rangeStart, rangeEnd, occurrenceStartDateTime, occurrenceEndDateTime)) {
                      occurrences.append(buildOccurrence(event, occurrenceStartDateTime, occurrenceEndDateTime));
                  }
              }
              break;
          }
        case RecurrenceType::Yearly: {
            for (QDateTime occurrenceStartDateTime = anchorStartDateTime, occurrenceEndDateTime = anchorEndDateTime;
                 occurrenceStartDateTime.date() <= rangeEnd;
                 occurrenceStartDateTime = occurrenceStartDateTime.addYears(interval),
                           occurrenceEndDateTime = occurrenceEndDateTime.addYears(interval)) {
                  if (recurrenceUntil.isValid() && occurrenceStartDateTime.date() > recurrenceUntil) {
                      break;
                  }

                  if (occurrenceIsVisibleInRange(rangeStart, rangeEnd, occurrenceStartDateTime, occurrenceEndDateTime)) {
                      occurrences.append(buildOccurrence(event, occurrenceStartDateTime, occurrenceEndDateTime));
                  }
              }
              break;
          }
          case RecurrenceType::Custom:
          case RecurrenceType::None:
              if (occurrenceIsVisibleInRange(rangeStart, rangeEnd, anchorStartDateTime,anchorEndDateTime)) {
                  occurrences.append(buildOccurrence(event, anchorStartDateTime, anchorEndDateTime));
              }
              break;
      }

    return occurrences;
}

QVector<EventOccurrence> EventManager::visibleOccurrencesForFilter(const Event &event, EventTimeFilter filter, const QDateTime &now) const {
    QVector<EventOccurrence> returnVal;
    const bool completed = event.isCompleted();
    const QDate today = now.date();

    switch (filter) {
        case EventTimeFilter::Completed:
            if (completed) {
                returnVal = {buildOccurrence(event)};
            }
            break;
        case EventTimeFilter::Today:
            if (!completed) {
                returnVal = expandOccurrencesForRange(event, today, today);
            }
            break;
        case EventTimeFilter::Upcoming:
            if (!completed) {
                returnVal = expandOccurrencesForRange(event, today, today.addDays(5));
            }
            break;
        case EventTimeFilter::ThisWeek: {
            if (!completed) {
                const QDate weekStart = weekStartForDate(today);
                returnVal = expandOccurrencesForRange(event, weekStart, weekStart.addDays(6));
            }
            break;
        }
        case EventTimeFilter::Recurring:
            if (!completed && event.hasRecurrence()) {
                if (eventUsesGeneratedRecurrence(event)) {
                    returnVal = expandOccurrencesForRange(event, recurrenceListRangeStart(now), recurrenceListRangeEnd(now));
                } else {
                    returnVal = {buildOccurrence(event)};
                }
            }
            break;
        case EventTimeFilter::AllEvents:
            if (completed || eventIsBacklogCandidate(event) || !eventUsesGeneratedRecurrence(event)) {
                returnVal = {buildOccurrence(event)};
            } else {
                returnVal = expandOccurrencesForRange(event, recurrenceListRangeStart(now), recurrenceListRangeEnd(now));
            }
            break;
    }

    return returnVal;
}

EventOccurrence EventManager::buildOccurrence(const Event &event, bool seriesMaster) const {
    return buildOccurrence(event, event.getStartDateTime(), event.getEndDateTime(), seriesMaster);
}

EventOccurrence EventManager::buildOccurrence(const Event &event, const QDateTime &occurrenceStartDateTime, const QDateTime &occurrenceEndDateTime, bool seriesMaster) const {
    EventOccurrence occurrence;
    occurrence.eventId = event.getId();
    occurrence.occurrenceKey = seriesMaster ? seriesMasterKey(event.getId()) : occurrenceKey(event.getId(), occurrenceStartDateTime);
    occurrence.originalStartDateTime = event.getStartDateTime();
    occurrence.originalEndDateTime = event.getEndDateTime();
    occurrence.startDateTime = occurrenceStartDateTime;
    occurrence.endDateTime = occurrenceEndDateTime;
    occurrence.name = event.getName();
    occurrence.description = event.getDescription();
    occurrence.location = event.getLocation();
    occurrence.priority = event.getPriority();
    occurrence.category = event.getCategory();
    occurrence.eventType = event.getEventType();
    occurrence.allDay = event.isAllDay();
    occurrence.recurring = event.hasRecurrence();
    occurrence.recurrenceType = event.getRecurrenceType();
    occurrence.seriesMaster = seriesMaster;
    occurrence.completed = event.isCompleted();
    occurrence.completedAt = event.getCompletedAt();
    occurrence.recurrenceSummary = recurrenceSummaryForEvent(event);
    return occurrence;
}

bool EventManager::matchesTimeFilter(const Event &event, EventTimeFilter filter, const QDateTime &now) const {
    bool returnVal = true;

    switch (filter) {
        case EventTimeFilter::AllEvents:
            returnVal = true;
            break;
        case EventTimeFilter::Today:
        case EventTimeFilter::Upcoming:
        case EventTimeFilter::ThisWeek:
            returnVal = !visibleOccurrencesForFilter(event, filter, now).isEmpty();
            break;
        case EventTimeFilter::Recurring:
            returnVal = !event.isCompleted() && event.hasRecurrence();
            break;
        case EventTimeFilter::Completed:
            returnVal = event.isCompleted();
            break;
    }

    return returnVal;
}


QDate EventManager::effectiveDate(const Event &event, EventTimeFilter filter) const {
    QDate returnVal = event.getStartDateTime().date();

    if (filter == EventTimeFilter::Completed && event.getCompletedAt().isValid()) {
        returnVal = event.getCompletedAt().date();
    }
    return returnVal;
}


void EventManager::recalculateNextId() {
    int maxId = 0;
    for (const Event &event : std::as_const(m_events)) {
        maxId = std::max(maxId, event.getId());
    }
    m_nextId = maxId + 1;
}


