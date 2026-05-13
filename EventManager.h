// Header file for interacting with "Event" objects
#ifndef EVENTMANAGER_H
#define EVENTMANAGER_H

#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include "Event.h"

enum class EventTimeFilter {
    AllEvents,
    Today,
    Upcoming,
    ThisWeek,
    Recurring,
    Completed,
};

struct EventQuery {
    QString searchText;
    bool priorityEnabled = false;
    Priority priority = Priority::VeryLow;
    QString category;
    EventTimeFilter timeFilter = EventTimeFilter::AllEvents;
};

struct EventOccurrence {
    int eventId = -1;
    QString occurrenceKey;
    QDateTime originalStartDateTime;
    QDateTime originalEndDateTime;
    QDateTime startDateTime;
    QDateTime endDateTime;
    QString name;
    QString description;
    QString location;
    Priority priority = Priority::VeryLow;
    QString category;
    EventType eventType = EventType::Event;
    bool allDay = false;
    bool recurring = false;
    RecurrenceType recurrenceType = RecurrenceType::None;
    bool seriesMaster = false;
    bool completed = false;
    QDateTime completedAt;
    QString recurrenceSummary;

    bool isValid() const {
        return eventId >= 0;
    }
};

enum class EventVisualState {
    Active,
    PastIncomplete,
    Completed,
};

EventVisualState visualStateForOccurrence(const EventOccurrence &occurrence, const QDateTime &now = QDateTime::currentDateTime());
bool occurrenceHasSchedule(const EventOccurrence &occurrence);
bool occurrenceIsBacklog(const EventOccurrence &occurrence);
QString occurrenceScheduleText(const EventOccurrence &occurrence, const QDate &visibleDate = QDate());
QString occurrenceDetailsDateText(const EventOccurrence &occurrence);
QString occurrenceListStartText(const EventOccurrence &occurrence);
QString occurrenceListEndText(const EventOccurrence &occurrence);
QString agendaListStatusText(const EventOccurrence &occurrence, const QDateTime &now = QDateTime::currentDateTime());
QString agendaListDueSummaryText(const EventOccurrence &occurrence, const QDateTime &now = QDateTime::currentDateTime());

struct MonthCellItem {
    EventOccurrence occurrence;
    QDate jumpDate;
    EventVisualState visualState = EventVisualState::Active;
};

struct MonthSpanItem {
    EventOccurrence occurrence;
    QDate jumpDate;
    EventVisualState visualState = EventVisualState::Active;
};

struct MonthPopoverItem {
    EventOccurrence occurrence;
    QDate visibleDate;
    QDate jumpDate;
    EventVisualState visualState = EventVisualState::Active;
};

struct MonthCellState {
    QDate date;
    bool inCurrentMonth = false;
    QDate actionableJumpDate;
    QVector<MonthCellItem> dayItems;
    QVector<MonthPopoverItem> popoverItems;
};

struct MonthViewModel {
    QDate visibleMonth;
    QDate gridStartDate;
    QVector<MonthCellState> cells;
    QVector<MonthSpanItem> spans;
    QStringList legendCategories;
};

class EventManager {
    public:
        EventManager();

        // Main application functions add, edit, delete
        int generateNextId();
        void setEvents(const QVector<Event> &events);
        void addEvent(Event event);
        bool deleteEvent(int id);
        bool editEvent(int id, const Event &updatedEvent);
        bool markEventCompleted(int id, const QDateTime &completedAt);
        bool reopenEvent(int id);
        bool autoCompleteOverdueEvents(const QDateTime &now);

        // Event search functions
        const QVector<Event> &getEvents() const;
        const Event *findEventById(int id) const;
        Event *findEventById(int id);

        // Event list entry functions
        QVector<EventOccurrence> listEntries(const EventQuery &query, const QDateTime &now) const;
        QVector<EventOccurrence> occurrencesForDate(const QDate &date, const EventQuery &query, const QDateTime &now) const;
        QVector<EventOccurrence> backlogEntries(const EventQuery &query, const QDateTime &now) const;
        QVector<EventOccurrence> agendaListEntries(const QDateTime &now) const;
        QMap<QDate, QVector<EventOccurrence>> occurrencesForWeek(const QDate &referenceDate, const EventQuery &query, const QDateTime &now) const;
        QStringList categories() const;
        QStringList distinctCategoriesForDate(const QDate &date, const EventQuery &query, const QDateTime &now) const;
        MonthViewModel monthViewModel(const QDate &visibleMonth, const EventQuery &query, const QDateTime &now) const;

        // Special reccurence series functions
        EventOccurrence masterEntryForEvent(int id) const;
        EventOccurrence occurrenceForEvent(int id, const QDateTime &originalStartDateTime) const;

        // Simple specific use-case functions
        int totalEvents() const;
        int eventsTodayCount(const QDateTime &now) const;
        int upcomingEventsCount(const QDateTime &now) const;
        int highPriorityEventsCount() const;
        bool isEmpty() const;

        static QString occurrenceKey(int eventId, const QDateTime &originalStartDateTime);
        static QString seriesMasterKey(int eventId);
        static bool isSeriesMasterKey(const QString &key);

    private:
        static QDate weekStartForDate(const QDate &date);
        static void sortOccurrences(QVector<EventOccurrence> &occurrences, EventTimeFilter filter, const QDateTime &now, const QDate &visibleDate = QDate());
        static bool matchesBacklogTimeFilter(const Event &event, EventTimeFilter filter);
        static bool matchesFacetFilters(const EventOccurrence &occurrence, const EventQuery &query);
        static bool matchesBaseFacetFilters(const Event &event, const EventQuery &query);
        QVector<EventOccurrence> expandOccurrencesForRange(const Event &event, const QDate &rangeStart, const QDate &rangeEnd) const;
        QVector<EventOccurrence> visibleOccurrencesForFilter(const Event &event, EventTimeFilter filter, const QDateTime &now) const;
        EventOccurrence buildOccurrence(const Event &event, bool seriesMaster = false) const;
        EventOccurrence buildOccurrence(const Event &event, const QDateTime &occurrenceStartDateTime,const QDateTime &occurrenceEndDateTime, bool seriesMaster = false) const;
        bool matchesTimeFilter(const Event &event, EventTimeFilter filter, const QDateTime &now) const;
        QDate effectiveDate(const Event &event, EventTimeFilter filter) const;
        void recalculateNextId();

        // Stores all event objects & the next event id
        QVector<Event> m_events;
        int m_nextId;
};

#endif // EVENTMANAGER_H



