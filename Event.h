#ifndef EVENT_H
#define EVENT_H

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <Qt>

// Priority rating as an enum (1-5)
enum class Priority {
    VeryLow = 1,
    Low = 2,
    Medium = 3,
    High = 4,
    VeryHigh = 5,
};

// EventType rating as an enum (1-5)
enum class EventType {
    Event,
    Task,
    Reminder,
    ScheduleBlock,
};

enum class RecurrenceType {
    None,
    Daily,
    Weekly,
    Monthly,
    Yearly,
    Custom,
};

class Event {
    public:
        Event();
        Event(int id, const QString &name);
        Event(int id,
              const QString &name,
              EventType eventType,
              const QDateTime &startDateTime,
              const QDateTime &endDateTime,
              Priority priority = Priority::VeryLow,
              const QString &category = QString(),
              const QString &location = QString(),
              const QString &description = QString(),
              bool allDay = false);

        // Getters
        int getId() const;
        QString getName() const;
        QString getDescription() const;
        QString getLocation() const;
        QDateTime getStartDateTime() const;
        QDateTime getEndDateTime() const;
        Priority getPriority() const;
        QString getCategory() const;
        EventType getEventType() const;
        bool isAllDay() const;
        RecurrenceType getRecurrenceType() const;
        int getRecurrenceInterval() const;
        QDate getRecurrenceUntil() const;
        QList<Qt::DayOfWeek> getRepeatDays() const;
        bool isNotified() const;
        bool isCompleted() const;
        QDateTime getCompletedAt() const;
        bool isAutoCompleteSuppressed() const;

        // Setters
        void setId(int newId);
        void setName(const QString &newName);
        void setDescription(const QString &newDescription);
        void setLocation(const QString &newLocation);
        void setStartDateTime(const QDateTime &newStartDateTime);
        void setEndDateTime(const QDateTime &newEndDateTime);
        void setPriority(Priority newPriority);
        void setCategory(const QString &newCategory);
        void setEventType(EventType eventType);
        void setAllDay(bool allDay);
        void setRecurrenceType(RecurrenceType recurrenceType);
        void setRecurrenceInterval(int recurrenceInterval);
        void setRecurrenceUntil(const QDate &recurrenceUntil);
        void setRepeatDays(const QList<Qt::DayOfWeek> &repeatDays);
        void setNotified(bool notified);
        void setCompleted(bool completed);
        void setCompletedAt(const QDateTime &completedAt);
        void setAutoCompleteSuppressed(bool autoCompleteSuppressed);

        bool hasRecurrence() const;
        bool isMultiDay() const;
        bool isTaskLike() const;
        bool isScheduleBlock() const;
        bool isValid() const;

    private:
        int m_id;                          // Unique ID for every event object
        QString m_name;                    // Display name for the event object
        QString m_description;             // Optional description field for the event
        QString m_location;                // Optional location field for the event
        QDateTime m_startDateTime;         // Start Date&Time field for the event
        QDateTime m_endDateTime;           // End Date&Time field for the event
        Priority m_priority;               // Enum Priority for the event (VeryLow - VeryHigh)
        QString m_category;                // Category type for the event (Custom)

        bool m_notified;                   // Notification Bool
        bool m_completed;                  // Marked as completed Bool
        QDateTime m_completedAt;           // The Date&Time the event was last marked as completed
        bool m_autoCompleteSuppressed;     // Controls whether the event should be auto completed if its due date passes

        EventType m_eventType;             // Seperates events, tasks, reminders, and recurring schedule.
        bool m_allDay;                     // Marks items such as holidays, birthdays, or full-day deadlines.
        RecurrenceType m_recurrenceType;   // Defines what recurrence type the item has.
        int m_recurrenceInterval;          // Stores the "every X" portion of a recurrence rule.
        QDate m_recurrenceUntil;           // Sets an optional final date for a recurring series.
        QList<Qt::DayOfWeek> m_repeatDays; // Stores the weekdays used by weekly schedule-style recurrences.
};

#endif // EVENT_H





