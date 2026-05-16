// Implementation file for creating schedule objects called "Events"
#include "Event.h"

Event::Event()
    : m_id(0)
    , m_priority(Priority::VeryLow)
    , m_notified(false)
    , m_completed(false)
    , m_autoCompleteSuppressed(false)
    , m_eventType(EventType::Event)
    , m_allDay(false)
    , m_recurrenceType(RecurrenceType::None)
    , m_recurrenceInterval(1) {
}

Event::Event(int id, const QString &name)
    : Event() {
    m_id = id;
    m_name = name;
}

Event::Event(int id,
             const QString &name,
             EventType eventType,
             const QDateTime &startDateTime,
             const QDateTime &endDateTime,
             Priority priority,
             const QString &category,
             const QString &location,
             const QString &description,
             bool allDay)
    : Event() {
    m_id = id;
    m_name = name;
    m_eventType = eventType;
    m_startDateTime = startDateTime;
    m_endDateTime = endDateTime;
    m_priority = priority;
    m_category = category;
    m_location = location;
    m_description = description;
    m_allDay = allDay;
}

// Getters
int Event::getId() const {
    return m_id;
}

QString Event::getName() const {
    return m_name;
}

QString Event::getDescription() const {
    return m_description;
}

QString Event::getLocation() const {
    return m_location;
}

QDateTime Event::getStartDateTime() const {
    return m_startDateTime;
}

QDateTime Event::getEndDateTime() const {
    return m_endDateTime;
}

Priority Event::getPriority() const {
    return m_priority;
}

QString Event::getCategory() const {
    return m_category;
}

EventType Event::getEventType() const {
    return m_eventType;
}

bool Event::isAllDay() const {
    return m_allDay;
}

RecurrenceType Event::getRecurrenceType() const {
    return m_recurrenceType;
}

int Event::getRecurrenceInterval() const {
    return m_recurrenceInterval;
}

QDate Event::getRecurrenceUntil() const {
    return m_recurrenceUntil;
}

QList<Qt::DayOfWeek> Event::getRepeatDays() const {
    return m_repeatDays;
}

bool Event::isNotified() const {
    return m_notified;
}

bool Event::isCompleted() const {
    return m_completed;
}

QDateTime Event::getCompletedAt() const {
    return m_completedAt;
}

bool Event::isAutoCompleteSuppressed() const {
    return m_autoCompleteSuppressed;
}

// Setters
void Event::setId(int newId) {
    m_id = newId;
}

void Event::setName(const QString &newName) {
    m_name = newName;
}

void Event::setDescription(const QString &newDescription) {
    m_description = newDescription;
}

void Event::setLocation(const QString &newLocation) {
    m_location = newLocation;
}

void Event::setStartDateTime(const QDateTime &newStartDateTime) {
    m_startDateTime = newStartDateTime;
}

void Event::setEndDateTime(const QDateTime &newEndDateTime) {
    m_endDateTime = newEndDateTime;
}

void Event::setPriority(Priority newPriority) {
    m_priority = newPriority;
}

void Event::setCategory(const QString &newCategory) {
    m_category = newCategory;
}

void Event::setEventType(EventType eventType) {
    m_eventType = eventType;
}

void Event::setAllDay(bool allDay) {
    m_allDay = allDay;
}

void Event::setRecurrenceType(RecurrenceType recurrenceType) {
    m_recurrenceType = recurrenceType;
}

void Event::setRecurrenceInterval(int recurrenceInterval) {
    m_recurrenceInterval = recurrenceInterval;
}

void Event::setRecurrenceUntil(const QDate &recurrenceUntil) {
    m_recurrenceUntil = recurrenceUntil;
}

void Event::setRepeatDays(const QList<Qt::DayOfWeek> &repeatDays) {
    m_repeatDays = repeatDays;
}

void Event::setNotified(bool notified) {
    m_notified = notified;
}

void Event::setCompleted(bool completed) {
    m_completed = completed;
}

void Event::setCompletedAt(const QDateTime &completedAt) {
    m_completedAt = completedAt;
}

void Event::setAutoCompleteSuppressed(bool autoCompleteSuppressed) {
    m_autoCompleteSuppressed = autoCompleteSuppressed;
}

bool Event::hasRecurrence() const {
    return m_recurrenceType != RecurrenceType::None;
}

bool Event::isMultiDay() const {
    return m_startDateTime.isValid() && m_endDateTime.isValid() && m_startDateTime.date() != m_endDateTime.date();
}

bool Event::isTaskLike() const {
    return m_eventType == EventType::Task || m_eventType == EventType::Reminder;
}

bool Event::isScheduleBlock() const {
    return m_eventType == EventType::ScheduleBlock;
}

// Verifies if an event object fields are filled enough to be created & stored
bool Event::isValid() const {
    if (m_name.trimmed().isEmpty()) {
        return false;   // Event is not valid if it has no name
    }

    const bool hasValidStart = m_startDateTime.isValid();
    const bool hasValidEnd = m_endDateTime.isValid();

    if (hasValidStart && hasValidEnd && m_endDateTime < m_startDateTime) {
        return false;   // Event is not valid if it has no startDate and endDate
    }

    switch (m_eventType) {
        case EventType::Task:
            break;
        case EventType::Event:
        case EventType::ScheduleBlock:
            if (!hasValidStart || !hasValidEnd) {
                return false;
            }
            break;
        case EventType::Reminder:
            if (!hasValidStart) {
                return false;
            }
            break;
    }

    if (hasRecurrence()) {
        if (m_recurrenceInterval < 1 || !hasValidStart) {
            return false;
        }

        if (m_recurrenceUntil.isValid() && m_recurrenceUntil < m_startDateTime.date()) {
            return false;
        }
    }

    return true;
}



