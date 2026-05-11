// Header file for Qt's main window framework
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHash>
#include <QMainWindow>
#include <QSet>

#include "EventManager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QListWidgetItem;
class MonthCalendarWidget;
class QResizeEvent;
class QSplitter;
class QTableWidget;
class QTableWidgetItem;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    struct ListRowState {
        bool isFolder = false;
        QString folderId;
        QString parentFolderId;
        QStringList ancestorFolderIds;
        int depth = 0;
        QString occurrenceKey;
    };

    struct ListTableState {
        QVector<ListRowState> rows;
        QHash<QString, QStringList> occurrenceAncestorFolderIds;
        QSet<QString> collapsedFolderIds;
    };

    void configureUi();
    void bindSignals();
    void loadEvents();
    bool persistEvents(const QVector<Event> &events, const QString &actionDescription);
    bool autoCompleteOverdueEventsIfNeeded(const QDateTime &now);
    void refreshCategoryFilter();
    void refreshMonthCalendar(const QDateTime &now);
    void refreshVisibleData();
    void populateAgendaList(const QVector<EventOccurrence> &occurrences);
    void populateListTable(const QVector<EventOccurrence> &entries);
    void refreshAgendaListCardStyles();
    void refreshDetailsPanel(const EventOccurrence *occurrence);
    void refreshActionButtonStates();
    void refreshTableAccentStates();
    void applyListFolderVisibility();
    bool isListFolderRow(const QTableWidget *table, int row) const;
    void toggleListFolder(QTableWidget *table, int row);
    int firstSelectableListRow(const QTableWidget *table) const;
    ListTableState *listStateForTable(QTableWidget *table);
    const ListTableState *listStateForTable(const QTableWidget *table) const;
    void revealActionableDateFolder(const QDate &date);
    void enforceListMonthSplitterBounds();
    void enforceContentVerticalSplitterBounds();
    void finalizeCrudUpdate(const QVector<Event> &updatedEvents, const QString &statusMessage,
                            const QString &preferredOccurrenceKey = QString(),
                            int preferredEventId = -1,
                            bool revealPreferredEvent = false);
    void applyCategoryAccent(QTableWidgetItem *item, const QString &category, bool selected,
                             EventVisualState visualState,
                             bool strikeOutText) const;
    void applyCategoryDetailAccent(const QString &category);
    void applyOccurrenceRowState(QTableWidget *table,
                                 int row,
                                 const EventOccurrence &occurrence,
                                 int accentColumn,
                                 bool selected) const;
    void resetFacetControls();
    void syncTimeFilterButtons(EventTimeFilter filter);
    void clearRevealFilters();
    Event createDefaultAddEvent() const;
    QDate eventDateForCurrentContext(const EventOccurrence &occurrence) const;
    bool isOccurrenceVisibleInCurrentContext(const EventOccurrence &occurrence) const;
    void revealEventInCurrentContext(const EventOccurrence &occurrence);
    bool selectOccurrenceInCurrentView(const QString &occurrenceKey, int eventId = -1);
    void selectFallbackVisibleEvent();
    EventOccurrence masterEntryForEventId(int eventId) const;
    EventOccurrence firstOccurrenceForEvent(const Event &event) const;
    const Event *baseEventForOccurrence(const EventOccurrence *occurrence) const;
    void selectTableRow(QTableWidget *table, int row);

    void setTimeFilter(EventTimeFilter filter);
    void handleAgendaListSelectionChanged();
    void handleListSelectionChanged();
    void handleListCellClicked(int row, int column);
    void handleAddEvent();
    void handleEditEvent();
    void handleDeleteEvent();
    bool handleCompleteEvent(bool shouldBeCompleted);

    QVector<EventOccurrence> currentFilteredEntries() const;
    const EventOccurrence *selectedOccurrenceInTable(const QTableWidget *table) const;
    const EventOccurrence *selectedAgendaListOccurrence() const;
    const EventOccurrence *currentSelectedOccurrence() const;
    QString selectedOccurrenceKey(const QTableWidget *table) const;
    QString selectedAgendaListOccurrenceKey() const;
    void selectOccurrenceByKey(QTableWidget *table, const QString &occurrenceKey);
    bool selectAgendaListOccurrenceByKey(const QString &occurrenceKey);

    Ui::MainWindow *ui;
    EventManager m_eventManager;
    EventQuery m_query;
    QString m_storagePath;
    bool m_isApplyingAutoCompletion = false;
    QVector<EventOccurrence> m_currentAgendaListEntries;
    QVector<EventOccurrence> m_currentListEntries;
    QSplitter *m_listContentSplitter = nullptr;
    QWidget *m_listMonthPanel = nullptr;
    MonthCalendarWidget *m_monthCalendarWidget = nullptr;
    ListTableState m_listTableState;
};

#endif // MAINWINDOW_H






