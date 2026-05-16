// Mainly handles the actions made by users from the GUI
#include "mainwindow.h"

#include "./ui_mainwindow.h"

#include <algorithm>

#include <QCheckBox>
#include <QDialog>
#include <QItemSelectionModel>
#include <QListWidget>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>

#include "EventDialog.h"
#include "MonthCalendarWidget.h"

namespace {

const int OCCURRENCE_KEY_ROLE = Qt::UserRole;
const int EVENT_ID_ROLE = Qt::UserRole + 1;
const int ROW_KIND_ROLE = Qt::UserRole + 3;
const int FOLDER_ID_ROLE = Qt::UserRole + 4;

// Simple enum to differentiate folders from events in the list table
enum class ListTableRowKind {
    Occurrence = 0,
    Folder = 1,
};

// Builds the next default start time for a newly added event.
QDateTime nextTopOfHour(const QDateTime &referenceDateTime) {
    QDateTime returnVal(referenceDateTime.date(), QTime(referenceDateTime.time().hour(), 0));

    if (returnVal <= referenceDateTime) {
        returnVal = returnVal.addSecs(3600);
    }
    return returnVal;
}

QDate weekStartForDate(const QDate &date) {
    return date.addDays(-(date.dayOfWeek() % 7));
}

const EventOccurrence *findOccurrenceByKey(const QVector<EventOccurrence> &occurrences, const QString &occurrenceKey) {
    const EventOccurrence *returnVal = nullptr;

    for (const EventOccurrence &occurrence : occurrences) {
        if (returnVal == nullptr && occurrence.occurrenceKey == occurrenceKey) {
            returnVal = &occurrence;
        }
    }
    return returnVal;
}

bool occurrencesMatch(const EventOccurrence &left, const EventOccurrence &right) {
    const bool sameOccurrenceKey = left.occurrenceKey == right.occurrenceKey;
    const bool sameEvent = left.eventId == right.eventId;
    const bool matchesSeries = left.seriesMaster || right.seriesMaster;
    bool returnVal = sameOccurrenceKey;

    if (!returnVal) {
        returnVal = sameEvent && matchesSeries;
    }
    return returnVal;
}

bool entriesContainOccurrence(const QVector<EventOccurrence> &entries, const EventOccurrence &occurrence) {
    bool returnVal = false;

    for (const EventOccurrence &candidate : entries) {
        if (!returnVal) {
            returnVal = occurrencesMatch(candidate, occurrence);
        }
    }
    return returnVal;
}

int findAgendaListRowByEventId(const QListWidget *listWidget, int eventId) {
    int returnVal = -1;

    if (listWidget != nullptr && eventId >= 0) {
        for (int row = 0; row < listWidget->count(); ++row) {
            const QListWidgetItem *item = listWidget->item(row);
            const bool matchesEvent = item != nullptr && item->data(EVENT_ID_ROLE).toInt() == eventId;
            if (returnVal < 0 && matchesEvent) {
                returnVal = row;
            }
        }
    }
    return returnVal;
}

int findTableRowByOccurrenceKey(const QTableWidget *table, const QString &occurrenceKey, bool skipFolders) {
    int returnVal = -1;

    if (table != nullptr && !occurrenceKey.isEmpty()) {
        for (int row = 0; row < table->rowCount(); ++row) {
            const QTableWidgetItem *item = table->item(row, 0);
            bool isFolder = false;
            bool matchesKey = false;

            if (item != nullptr) {
                isFolder = item->data(ROW_KIND_ROLE).toInt() == static_cast<int>(ListTableRowKind::Folder);
                matchesKey = item->data(OCCURRENCE_KEY_ROLE).toString() == occurrenceKey;
            }

            if (returnVal < 0 && item != nullptr && matchesKey && (!skipFolders || !isFolder)) {
                returnVal = row;
            }
        }
    }
    return returnVal;
}

int findTableRowByEventId(const QTableWidget *table, int eventId, bool skipFolders) {
    int returnVal = -1;

    if (table != nullptr && eventId >= 0) {
        for (int row = 0; row < table->rowCount(); ++row) {
            const QTableWidgetItem *item = table->item(row, 0);
            bool isFolder = false;
            bool matchesEvent = false;

            if (item != nullptr) {
                isFolder = item->data(ROW_KIND_ROLE).toInt() == static_cast<int>(ListTableRowKind::Folder);
                matchesEvent = item->data(EVENT_ID_ROLE).toInt() == eventId;
            }

            if (returnVal < 0 && item != nullptr && matchesEvent && (!skipFolders || !isFolder)) {
                returnVal = row;
            }
        }
    }
    return returnVal;
}

} // namespace

void MainWindow::finalizeCrudUpdate(const QVector<Event> &updatedEvents, const QString &statusMessage, const QString &preferredOccurrenceKey,
                                    int preferredEventId,
                                    bool revealPreferredEvent) {
    m_eventManager.setEvents(updatedEvents);
    refreshCategoryFilter();

    if (revealPreferredEvent && preferredEventId >= 0) {
        EventOccurrence preferredOccurrence;

        if (!preferredOccurrenceKey.isEmpty()) {
            const bool useSeriesMaster = EventManager::isSeriesMasterKey(preferredOccurrenceKey);
            if (useSeriesMaster) {
                preferredOccurrence = masterEntryForEventId(preferredEventId);
            } else {
                preferredOccurrence = m_eventManager.occurrenceForEvent(preferredEventId, QDateTime());
            }
        } else {
            const Event *preferredEvent = m_eventManager.findEventById(preferredEventId);
            if (preferredEvent != nullptr) {
                preferredOccurrence = firstOccurrenceForEvent(*preferredEvent);
            }
        }

        if (preferredOccurrence.isValid()) {
            revealEventInCurrentContext(preferredOccurrence);
        }
    }

    refreshVisibleData();

    const bool hasPreferredSelection = !preferredOccurrenceKey.isEmpty() || preferredEventId >= 0;
    if (hasPreferredSelection) {
        QString occurrenceKey;
        if (!preferredOccurrenceKey.isEmpty()) {
            occurrenceKey = preferredOccurrenceKey;
        } else {
            occurrenceKey = masterEntryForEventId(preferredEventId).occurrenceKey;
        }

        if (!selectOccurrenceInCurrentView(occurrenceKey, preferredEventId)) {
            selectFallbackVisibleEvent();
        }
    } else {
        selectFallbackVisibleEvent();
    }

    statusBar()->showMessage(statusMessage, 5000);
}

void MainWindow::resetFacetControls() {
    {
        QSignalBlocker blocker(ui->searchLineEdit);
        ui->searchLineEdit->clear();
    }

    {
        QSignalBlocker blocker(ui->priorityFilterComboBox);
        ui->priorityFilterComboBox->setCurrentIndex(0);
    }

    {
        QSignalBlocker blocker(ui->categoryFilterComboBox);
        ui->categoryFilterComboBox->setCurrentIndex(0);
    }
}

// Resets the filters before revealing a hidden occurrence.
void MainWindow::clearRevealFilters() {
    resetFacetControls();
    m_query.timeFilter = EventTimeFilter::AllEvents;
    syncTimeFilterButtons(EventTimeFilter::AllEvents);
}

// Builds the default event values used by the Add Event dialog.
Event MainWindow::createDefaultAddEvent() const {
    Event event;
    QDate selectedDate = QDate::currentDate();

    if (m_monthCalendarWidget != nullptr) {
        selectedDate = m_monthCalendarWidget->selectedDate();
    }

    const QDateTime startDateTime = QDateTime::currentDateTime().addSecs(3600);
    event.setStartDateTime(startDateTime);
    event.setEndDateTime(startDateTime.addSecs(3600));
    event.setEventType(EventType::Task);
    event.setAllDay(false);
    event.setPriority(Priority::Medium);
    event.setRecurrenceType(RecurrenceType::None);
    event.setRecurrenceInterval(1);
    event.setRecurrenceUntil(QDate());
    event.setRepeatDays({});
    event.setNotified(false);
    event.setCompleted(false);
    event.setCompletedAt(QDateTime());
    return event;
}

QDate MainWindow::eventDateForCurrentContext(const EventOccurrence &occurrence) const {
    QDateTime basisDateTime = occurrence.startDateTime;

    if (m_query.timeFilter == EventTimeFilter::Completed) {
        basisDateTime = occurrence.completedAt;
    }
    return basisDateTime.date();
}

// Checks whether an occurrence is visible in the currently active UI context.
bool MainWindow::isOccurrenceVisibleInCurrentContext(const EventOccurrence &occurrence) const {
    const bool inAgendaList = entriesContainOccurrence(m_currentAgendaListEntries, occurrence);
    const bool inListTable = entriesContainOccurrence(m_currentListEntries, occurrence);
    bool returnVal = false;

    if (inAgendaList || inListTable) {
        returnVal = true;
    }
    return returnVal;
}

// Applies the filters & search for events
void MainWindow::revealEventInCurrentContext(const EventOccurrence &occurrence) {
    const bool needsReveal = !isOccurrenceVisibleInCurrentContext(occurrence);

    if (needsReveal) {
        clearRevealFilters();

        const QDate calendarDate = eventDateForCurrentContext(occurrence);
        const bool canShowCalendarDate = calendarDate.isValid() && m_monthCalendarWidget != nullptr;

        if (canShowCalendarDate) {
            m_monthCalendarWidget->setSelectedDate(calendarDate);
            m_monthCalendarWidget->showSelectedDate();
        }
    }
}

// Tries to select an occurrence in the active view before falling back to the agenda list.
bool MainWindow::selectOccurrenceInCurrentView(const QString &occurrenceKey, int eventId) {
    bool returnVal = false;

    selectOccurrenceByKey(ui->eventTableWidget, occurrenceKey);
    if (!occurrenceKey.isEmpty()
        && selectedOccurrenceKey(ui->eventTableWidget) == occurrenceKey) {
        returnVal = true;
    }

    if (!returnVal && eventId >= 0) {
        const int rowToSelect = findTableRowByEventId(ui->eventTableWidget, eventId, true);
        if (rowToSelect >= 0) {
            selectTableRow(ui->eventTableWidget, rowToSelect);
            returnVal = true;
        }
    }

    if (!returnVal && !occurrenceKey.isEmpty()) {
        returnVal = selectAgendaListOccurrenceByKey(occurrenceKey);
    }

    if (!returnVal && eventId >= 0) {
        const int rowToSelect = findAgendaListRowByEventId(ui->tasksListWidget, eventId);
        if (rowToSelect >= 0) {
            ui->tasksListWidget->setCurrentRow(rowToSelect);
            returnVal = true;
        }
    }

    return returnVal;
}

// Selects the best visible fallback occurrence when nothing specific is selected.
void MainWindow::selectFallbackVisibleEvent() {
    bool selected = false;

    const int actionableRow = firstSelectableListRow(ui->eventTableWidget);
    if (actionableRow >= 0) {
        selectTableRow(ui->eventTableWidget, actionableRow);
        selected = true;
    }

    if (!selected && ui->tasksListWidget->count() > 0) {
        for (int row = 0; row < ui->tasksListWidget->count(); ++row) {
            QListWidgetItem *item = ui->tasksListWidget->item(row);
            const bool hasOccurrence = item != nullptr
                                       && !item->data(OCCURRENCE_KEY_ROLE).toString().isEmpty();
            if (!selected && hasOccurrence) {
                ui->tasksListWidget->setCurrentRow(row);
                selected = true;
            }
        }
    }

    if (!selected) {
        {
            QSignalBlocker taskBlocker(ui->tasksListWidget);
            QSignalBlocker listBlocker(ui->eventTableWidget);
            ui->tasksListWidget->clearSelection();
            ui->eventTableWidget->clearSelection();
        }

        refreshDetailsPanel(nullptr);
        refreshActionButtonStates();
    }
}

// Expands the matching actionable date folder before selecting its first visible occurrence.
void MainWindow::revealActionableDateFolder(const QDate &date) {
    const bool hasValidDate = date.isValid();
    int folderRow = -1;
    QString occurrenceKey;

    if (hasValidDate) {
        const QString folderId = QStringLiteral("actionable:date:%1").arg(date.toString(Qt::ISODate));
        m_listTableState.collapsedFolderIds.remove(folderId);
        applyListFolderVisibility();

        for (int row = 0; row < ui->eventTableWidget->rowCount(); ++row) {
            QTableWidgetItem *item = ui->eventTableWidget->item(row, 0);
            const bool matchesFolder = item != nullptr
                                       && item->data(FOLDER_ID_ROLE).toString() == folderId;
            if (folderRow < 0 && matchesFolder) {
                folderRow = row;
            }
        }

        if (folderRow >= 0) {
            for (int row = folderRow + 1; row < ui->eventTableWidget->rowCount(); ++row) {
                const bool isFolderRow = isListFolderRow(ui->eventTableWidget, row);
                const bool rowHidden = ui->eventTableWidget->isRowHidden(row);

                if (isFolderRow) {
                    break;
                }

                if (!rowHidden) {
                    QTableWidgetItem *item = ui->eventTableWidget->item(row, 0);
                    const QString candidateKey = item != nullptr
                        ? item->data(OCCURRENCE_KEY_ROLE).toString()
                        : QString();
                    if (occurrenceKey.isEmpty() && !candidateKey.isEmpty()) {
                        occurrenceKey = candidateKey;
                    }
                }
            }
        }

        if (!occurrenceKey.isEmpty()) {
            selectOccurrenceByKey(ui->eventTableWidget, occurrenceKey);
        } else if (folderRow >= 0) {
            QTableWidgetItem *item = ui->eventTableWidget->item(folderRow, 0);
            if (item != nullptr) {
                ui->eventTableWidget->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            }
        }
    }
}

void MainWindow::syncTimeFilterButtons(EventTimeFilter filter) {
    switch (filter) {
        case EventTimeFilter::AllEvents:
            ui->allEventsButton->setChecked(true);
            break;
        case EventTimeFilter::Today:
            ui->todayButton->setChecked(true);
            break;
        case EventTimeFilter::Upcoming:
            ui->upcomingButton->setChecked(true);
            break;
        case EventTimeFilter::ThisWeek:
            ui->thisWeekButton->setChecked(true);
            break;
        case EventTimeFilter::Recurring:
            ui->recurringButton->setChecked(true);
            break;
        case EventTimeFilter::Completed:
            ui->completedButton->setChecked(true);
            break;
    }
}

void MainWindow::handleAgendaListSelectionChanged() {
    const bool selectionEmpty = ui->tasksListWidget->selectedItems().isEmpty();

    if (selectionEmpty) {
        refreshTableAccentStates();
        refreshDetailsPanel(currentSelectedOccurrence());
        refreshActionButtonStates();
    } else {
        QSignalBlocker listBlocker(ui->eventTableWidget);
        const QString occurrenceKey = selectedAgendaListOccurrenceKey();
        ui->eventTableWidget->clearSelection();
        selectOccurrenceByKey(ui->eventTableWidget, occurrenceKey);

        refreshTableAccentStates();
        refreshDetailsPanel(selectedAgendaListOccurrence());
        refreshActionButtonStates();
    }
}

void MainWindow::handleListSelectionChanged() {
    const bool selectionEmpty = ui->eventTableWidget->selectedItems().isEmpty();

    if (selectionEmpty) {
        {
            QSignalBlocker taskBlocker(ui->tasksListWidget);
            ui->tasksListWidget->clearSelection();
        }

        refreshTableAccentStates();
        refreshDetailsPanel(currentSelectedOccurrence());
        refreshActionButtonStates();
    } else {
        QSignalBlocker taskBlocker(ui->tasksListWidget);
        ui->tasksListWidget->clearSelection();

        refreshTableAccentStates();
        refreshDetailsPanel(selectedOccurrenceInTable(ui->eventTableWidget));
        refreshActionButtonStates();
    }
}

void MainWindow::handleListCellClicked(int row, int /*column*/) {
    const bool folderRow = isListFolderRow(ui->eventTableWidget, row);

    if (folderRow) {
        toggleListFolder(ui->eventTableWidget, row);
        refreshTableAccentStates();
    }
}

// Function for Adding an event by opening the dialog
void MainWindow::handleAddEvent() {
    EventDialog dialog(this);
    dialog.setMode(EventDialog::Mode::Add);
    dialog.setAvailableCategories(m_eventManager.categories());
    dialog.setEvent(createDefaultAddEvent());

    const bool accepted = dialog.exec() == QDialog::Accepted;
    if (accepted) {
        Event newEvent = dialog.eventData();
        newEvent.setId(0);
        newEvent.setNotified(false);
        newEvent.setCompleted(false);
        newEvent.setCompletedAt(QDateTime());

        EventManager updatedManager = m_eventManager;
        updatedManager.addEvent(newEvent);

        const QVector<Event> updatedEvents = updatedManager.getEvents();
        const bool hasAddedEvent = !updatedEvents.isEmpty();
        if (hasAddedEvent) {
            const Event &addedEvent = updatedEvents.constLast();
            const bool saved = persistEvents(updatedEvents, tr("save the new event"));
            if (saved) {
                finalizeCrudUpdate(updatedEvents,
                                   tr("Added \"%1\".").arg(addedEvent.getName()),
                                   firstOccurrenceForEvent(addedEvent).occurrenceKey,
                                   addedEvent.getId(),
                                   true);
            }
        }
    }
}

// Function for Editing an event by opening the dialog
void MainWindow::handleEditEvent() {
    const EventOccurrence *selectedOccurrence = currentSelectedOccurrence();
    const Event *baseEvent = baseEventForOccurrence(selectedOccurrence);
    const bool canEdit = baseEvent != nullptr;

    if (canEdit) {
        EventDialog dialog(this);
        dialog.setMode(EventDialog::Mode::Edit);
        dialog.setAvailableCategories(m_eventManager.categories());
        dialog.setEvent(*baseEvent);

        const bool accepted = dialog.exec() == QDialog::Accepted;
        if (accepted) {
            Event updatedEvent = dialog.eventData();
            updatedEvent.setId(baseEvent->getId());
            updatedEvent.setNotified(baseEvent->isNotified());
            updatedEvent.setCompleted(baseEvent->isCompleted());
            updatedEvent.setCompletedAt(baseEvent->getCompletedAt());

            EventManager updatedManager = m_eventManager;
            const bool updated = updatedManager.editEvent(baseEvent->getId(), updatedEvent);
            if (!updated) {
                QMessageBox::warning(this,
                                     tr("Edit Failed"),
                                     tr("The selected event could not be found for editing."));
            } else {
                const QVector<Event> updatedEvents = updatedManager.getEvents();
                const bool saved = persistEvents(updatedEvents, tr("save the edited event"));
                if (saved) {
                    finalizeCrudUpdate(updatedEvents,
                                       tr("Updated \"%1\".").arg(updatedEvent.getName()),
                                       firstOccurrenceForEvent(updatedEvent).occurrenceKey,
                                       updatedEvent.getId(),
                                       true);
                }
            }
        }
    }
}

// Function for Deleting an event
void MainWindow::handleDeleteEvent() {
    const EventOccurrence *selectedOccurrence = currentSelectedOccurrence();
    const Event *baseEvent = baseEventForOccurrence(selectedOccurrence);
    const bool canDelete = baseEvent != nullptr;

    if (canDelete) {
        const QString deletedEventName = selectedOccurrence->name;
        const QMessageBox::StandardButton confirmation = QMessageBox::question(
            this,
            tr("Delete Event"),
            tr("Delete \"%1\"?\n\nThis will permanently remove it from the saved data.")
                .arg(deletedEventName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (confirmation == QMessageBox::Yes) {
            EventManager updatedManager = m_eventManager;
            const bool deleted = updatedManager.deleteEvent(baseEvent->getId());
            if (!deleted) {
                QMessageBox::warning(this,
                                     tr("Delete Failed"),
                                     tr("The selected event could not be found for deletion."));
            } else {
                const QVector<Event> updatedEvents = updatedManager.getEvents();
                const bool saved = persistEvents(updatedEvents, tr("delete the event"));
                if (saved) {
                    finalizeCrudUpdate(updatedEvents, tr("Deleted \"%1\".").arg(deletedEventName));
                }
            }
        }
    }
}

// Updates the selected item's completion state and saves the result.
bool MainWindow::handleCompleteEvent(bool shouldBeCompleted) {
    const EventOccurrence *selectedOccurrencePtr = currentSelectedOccurrence();
    bool returnVal = false;

    if (selectedOccurrencePtr != nullptr) {
        const EventOccurrence selectedOccurrence = *selectedOccurrencePtr;
        const int selectedEventId = selectedOccurrence.eventId;
        EventManager updatedManager = m_eventManager;
        bool updateSucceeded = false;
        QString dialogTitle;
        QString dialogMessage;
        QString saveAction;
        QString statusMessage;

        if (shouldBeCompleted) {
            updateSucceeded = updatedManager.markEventCompleted(selectedEventId,
                                                                QDateTime::currentDateTime());
            dialogTitle = tr("Completion Failed");
            dialogMessage = tr("The selected event could not be marked completed.");
            saveAction = tr("save the completed event");
            statusMessage = tr("Marked \"%1\" completed.").arg(selectedOccurrence.name);
        } else {
            updateSucceeded = updatedManager.reopenEvent(selectedEventId);
            dialogTitle = tr("Reopen Failed");
            dialogMessage = tr("The selected event could not be reopened.");
            saveAction = tr("save the reopened event");
            statusMessage = tr("Reopened \"%1\".").arg(selectedOccurrence.name);
        }

        if (!updateSucceeded) {
            QMessageBox::warning(this, dialogTitle, dialogMessage);
        } else {
            const QVector<Event> updatedEvents = updatedManager.getEvents();
            const bool saved = persistEvents(updatedEvents, saveAction);
            if (saved) {
                finalizeCrudUpdate(updatedEvents,
                                   statusMessage,
                                   selectedOccurrence.occurrenceKey,
                                   selectedEventId,
                                   false);
                returnVal = true;
            }
        }
    }

    return returnVal;
}

QVector<EventOccurrence> MainWindow::currentFilteredEntries() const {
    return m_currentListEntries;
}

EventOccurrence MainWindow::masterEntryForEventId(int eventId) const {
    return m_eventManager.masterEntryForEvent(eventId);
}

EventOccurrence MainWindow::firstOccurrenceForEvent(const Event &event) const {
    EventOccurrence returnVal;

    if (event.getId() <= 0) {
        returnVal = m_eventManager.masterEntryForEvent(event.getId());
    } else if (event.hasRecurrence() && !event.isCompleted()) {
        returnVal = m_eventManager.occurrenceForEvent(event.getId(), QDateTime());
    } else if (!event.getStartDateTime().isValid()) {
        returnVal = m_eventManager.masterEntryForEvent(event.getId());
    } else {
        returnVal = m_eventManager.occurrenceForEvent(event.getId(), event.getStartDateTime());
    }
    return returnVal;
}

const Event *MainWindow::baseEventForOccurrence(const EventOccurrence *occurrence) const {
    const Event *returnVal = nullptr;

    if (occurrence != nullptr) {
        returnVal = m_eventManager.findEventById(occurrence->eventId);
    }
    return returnVal;
}

void MainWindow::selectTableRow(QTableWidget *table, int row) {
    bool canSelect = table != nullptr && row >= 0 && row < table->rowCount();

    if (canSelect) {
        const QModelIndex index = table->model()->index(row, 0);
        canSelect = index.isValid();

        if (canSelect) {
            QItemSelectionModel *selectionModel = table->selectionModel();
            if (selectionModel != nullptr) {
                selectionModel->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            } else {
                table->setCurrentCell(row, 0);
                table->selectRow(row);
            }

            QTableWidgetItem *item = table->item(row, 0);
            if (item != nullptr) {
                table->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            }
        }
    }
}

const EventOccurrence *MainWindow::selectedOccurrenceInTable(const QTableWidget *table) const {
    const QString occurrenceKey = selectedOccurrenceKey(table);
    const EventOccurrence *returnVal = nullptr;

    if (!occurrenceKey.isEmpty() && table == ui->eventTableWidget) {
        returnVal = findOccurrenceByKey(m_currentListEntries, occurrenceKey);
    }
    return returnVal;
}

const EventOccurrence *MainWindow::selectedAgendaListOccurrence() const {
    return findOccurrenceByKey(m_currentAgendaListEntries, selectedAgendaListOccurrenceKey());
}

const EventOccurrence *MainWindow::currentSelectedOccurrence() const {
    const EventOccurrence *agendaOccurrence = selectedAgendaListOccurrence();
    const EventOccurrence *returnVal = agendaOccurrence;

    if (returnVal == nullptr) {
        returnVal = selectedOccurrenceInTable(ui->eventTableWidget);
    }
    return returnVal;
}

QString MainWindow::selectedOccurrenceKey(const QTableWidget *table) const {
    QString returnVal;

    if (table != nullptr && table->selectionModel() != nullptr) {
        const QModelIndexList selectedRows = table->selectionModel()->selectedRows();
        const bool hasSelection = !selectedRows.isEmpty();

        if (hasSelection) {
            const QTableWidgetItem *item = table->item(selectedRows.first().row(), 0);
            const bool isFolder = item != nullptr
                                  && item->data(ROW_KIND_ROLE).toInt()
                                         == static_cast<int>(ListTableRowKind::Folder);
            if (item != nullptr && !isFolder) {
                returnVal = item->data(OCCURRENCE_KEY_ROLE).toString();
            }
        }
    }
    return returnVal;
}

QString MainWindow::selectedAgendaListOccurrenceKey() const {
    QString returnVal;
    const QList<QListWidgetItem *> selectedItems = ui->tasksListWidget->selectedItems();

    if (!selectedItems.isEmpty()) {
        const QListWidgetItem *item = selectedItems.constFirst();
        returnVal = item->data(OCCURRENCE_KEY_ROLE).toString();
    }
    return returnVal;
}

void MainWindow::selectOccurrenceByKey(QTableWidget *table, const QString &occurrenceKey) {
    const bool canSelect = table != nullptr && !occurrenceKey.isEmpty();

    if (canSelect) {
        ListTableState *state = listStateForTable(table);
        if (state != nullptr) {
            const QStringList ancestorFolderIds = state->occurrenceAncestorFolderIds.value(occurrenceKey);
            for (const QString &folderId : ancestorFolderIds) {
                state->collapsedFolderIds.remove(folderId);
            }
            applyListFolderVisibility();
        }

        const int rowToSelect = findTableRowByOccurrenceKey(table, occurrenceKey, true);
        if (rowToSelect >= 0) {
            selectTableRow(table, rowToSelect);
        }
    }
}

bool MainWindow::selectAgendaListOccurrenceByKey(const QString &occurrenceKey) {
    bool returnVal = false;

    if (!occurrenceKey.isEmpty()) {
        for (int row = 0; row < ui->tasksListWidget->count(); ++row) {
            QListWidgetItem *item = ui->tasksListWidget->item(row);
            const bool matchesOccurrence = item != nullptr && item->data(OCCURRENCE_KEY_ROLE).toString() == occurrenceKey;
            if (!returnVal && matchesOccurrence) {
                ui->tasksListWidget->setCurrentRow(row);
                returnVal = true;
            }
        }
    }
    return returnVal;
}




