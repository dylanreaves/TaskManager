// Implements dialog validation, field behavior and UI updates for adding/editing event details
#include "EventDialog.h"

#include "./ui_eventdialog.h"

#include <algorithm>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTime>

#include "Utility.h"

namespace {

QString eventTypeLabel(EventType type) {
    QString returnVal = QObject::tr("Event");
    switch (type) {
        case EventType::Task:
            returnVal = QObject::tr("Task");
            break;
        case EventType::Event:
            returnVal = QObject::tr("Event");
            break;
        case EventType::Reminder:
            returnVal = QObject::tr("Reminder");
            break;
        case EventType::ScheduleBlock:
            returnVal = QObject::tr("Schedule");
            break;
    }
    return returnVal;
}

QString recurrenceLabel(RecurrenceType kind) {
    QString returnVal = QObject::tr("Does not repeat");
    switch (kind) {
        case RecurrenceType::None:
            returnVal = QObject::tr("Does not repeat");
            break;
        case RecurrenceType::Daily:
            returnVal = QObject::tr("Daily");
            break;
        case RecurrenceType::Weekly:
            returnVal = QObject::tr("Weekly");
            break;
        case RecurrenceType::Monthly:
            returnVal = QObject::tr("Monthly");
            break;
        case RecurrenceType::Yearly:
            returnVal = QObject::tr("Yearly");
            break;
        case RecurrenceType::Custom:
            returnVal = QObject::tr("Custom");
            break;
    }
    return returnVal;
}

void configureInlineFieldRow(QHBoxLayout *rowLayout) {
    if (rowLayout == nullptr) {
        return;
    }
    rowLayout->setAlignment(Qt::AlignLeft);
    rowLayout->addStretch(1);
}

void configureInlineFieldWidget(QWidget *fieldWidget, QLabel *label, QWidget *inputWidget, int minimumInputWidth) {
    if (fieldWidget != nullptr) {
        fieldWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    }

    if (label != nullptr) {
        label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    }

    if (inputWidget != nullptr) {
        inputWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        inputWidget->setMinimumWidth(minimumInputWidth);
    }
}

} // namespace

EventDialog::EventDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EventDialog) {

    // Build the UI from the .ui file and set dialog size
    ui->setupUi(this);
    resize(720, 640);
    setupTypeSelector();

    // Use the default date/time as the starting point for new dialog fields.
    const QDateTime anchorDateTime = defaultDateTime();
    const QDate defaultUntilDate = anchorDateTime.date().addDays(7);

    ui->categoryComboBox->setEditable(true);
    ui->categoryComboBox->setInsertPolicy(QComboBox::NoInsert);

    ui->startDateTimeEdit->setCalendarPopup(true);
    ui->endDateTimeEdit->setCalendarPopup(true);
    ui->dueDateTimeEdit->setCalendarPopup(true);
    ui->remindAtDateTimeEdit->setCalendarPopup(true);
    ui->recurrenceUntilDateEdit->setCalendarPopup(true);

    configureDateTimeEditDisplay(ui->startDateTimeEdit, false);
    configureDateTimeEditDisplay(ui->endDateTimeEdit, false);
    configureDateTimeEditDisplay(ui->dueDateTimeEdit, false);
    configureDateTimeEditDisplay(ui->remindAtDateTimeEdit, false);
    ui->recurrenceUntilDateEdit->setDisplayFormat(QStringLiteral("MMM d, yyyy"));

    ui->startDateTimeEdit->setDateTime(anchorDateTime);
    ui->endDateTimeEdit->setDateTime(anchorDateTime.addSecs(3600));
    ui->dueDateTimeEdit->setDateTime(anchorDateTime);
    ui->remindAtDateTimeEdit->setDateTime(anchorDateTime);
    ui->recurrenceUntilDateEdit->setDate(defaultUntilDate);
    ui->recurrenceUntilDateEdit->setEnabled(false);

    // Add priorities to dropdown box using Priority enum
    const QList<Priority> priorities = {
        Priority::VeryLow,
        Priority::Low,
        Priority::Medium,
        Priority::High,
        Priority::VeryHigh,
    };
    for (Priority priority : priorities) {
        ui->priorityComboBox->addItem(priorityToString(priority), static_cast<int>(priority));
    }
    ui->priorityComboBox->setCurrentIndex(2);

    // Add recurrence types to dropdown box
    ui->recurrenceComboBox->addItem(recurrenceLabel(RecurrenceType::None),static_cast<int>(RecurrenceType::None));
    ui->recurrenceComboBox->addItem(recurrenceLabel(RecurrenceType::Daily),static_cast<int>(RecurrenceType::Daily));
    ui->recurrenceComboBox->addItem(recurrenceLabel(RecurrenceType::Weekly),static_cast<int>(RecurrenceType::Weekly));
    ui->recurrenceComboBox->addItem(recurrenceLabel(RecurrenceType::Monthly),static_cast<int>(RecurrenceType::Monthly));
    ui->recurrenceComboBox->addItem(recurrenceLabel(RecurrenceType::Yearly),static_cast<int>(RecurrenceType::Yearly));

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this] {
        validateAndAccept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Reapplying type-specific UI rules when the selected item type changes.
    connect(ui->typeComboBox,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) {
                if (!m_isUpdatingUi) {
                    applyTypeUi();
                }
            });
    connect(ui->recurrenceComboBox,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) {
                if (!m_isUpdatingUi) {
                    applyRecurrenceUi();
                }
            });
    connect(ui->recurrenceIntervalSpinBox,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int) {
                if (!m_isUpdatingUi) {
                    applyRecurrenceUi();
                }
            });
    connect(ui->allDayCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool) {
                if (!m_isUpdatingUi) {
                    applyAllDayUi();
                }
            });
    connect(ui->taskHasDueDateCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool) {
                if (!m_isUpdatingUi) {
                    applyTypeUi();
                }
            });
    connect(ui->recurrenceUntilCheckBox,
            &QCheckBox::toggled,
            this,
            [this](bool checked) {
                ui->recurrenceUntilDateEdit->setEnabled(checked);
            });
    connect(ui->startDateTimeEdit,
            &QDateTimeEdit::dateTimeChanged,
            this,
            [this](const QDateTime &startDateTime) {
                if (m_isUpdatingUi || !eventTypeUsesRange(selectedEventType())) {
                    return;
                }

                if (usesRecurringSchedulePattern()) {
                    normalizeSchedulePatternFields();
                } else if (ui->allDayCheckBox->isVisible() && ui->allDayCheckBox->isChecked()) {
                    if (ui->endDateTimeEdit->date() < startDateTime.date()) {
                        QSignalBlocker blocker(ui->endDateTimeEdit);
                        ui->endDateTimeEdit->setDate(startDateTime.date());
                    }
                } else if (ui->endDateTimeEdit->dateTime() < startDateTime) {
                    QSignalBlocker blocker(ui->endDateTimeEdit);
                    ui->endDateTimeEdit->setDateTime(startDateTime.addSecs(3600));
                }

                if (selectedRecurrenceType() == RecurrenceType::Weekly
                    && selectedRepeatDays().isEmpty()) {
                    setSelectedRepeatDays(
                        {static_cast<Qt::DayOfWeek>(startDateTime.date().dayOfWeek())});
                }

                if (ui->recurrenceUntilCheckBox->isChecked()
                    && ui->recurrenceUntilDateEdit->date() < startDateTime.date()) {
                    QSignalBlocker blocker(ui->recurrenceUntilDateEdit);
                    ui->recurrenceUntilDateEdit->setDate(startDateTime.date());
                }
            });

    ui->descriptionTextEdit->setTabChangesFocus(true);
    setSelectedEventType(EventType::Task);
    applyTypeUi();
}

EventDialog::~EventDialog() {
    delete ui;
}

void EventDialog::setMode(Mode mode) {
    m_mode = mode;
    updateDialogTitle();
}

void EventDialog::setAvailableCategories(const QStringList &categories) {
    const QString currentCategory = ui->categoryComboBox->currentText().trimmed();
    QSignalBlocker blocker(ui->categoryComboBox);

    ui->categoryComboBox->clear();
    ui->categoryComboBox->addItem(QString());
    ui->categoryComboBox->addItems(categories);

    if (!currentCategory.isEmpty() && ui->categoryComboBox->findText(currentCategory) < 0) {
        ui->categoryComboBox->addItem(currentCategory);
    }

    ui->categoryComboBox->setCurrentIndex(-1);
    ui->categoryComboBox->setEditText(currentCategory);
}

// Load an existing item into the dialog while keeping the UI rules in sync with its type.
void EventDialog::setEvent(const Event &event) {
    m_sourceEvent = event;
    m_isUpdatingUi = true;

    const QDateTime defaultTime = defaultDateTime();
    const QDateTime eventStart = event.getStartDateTime();
    const QDateTime eventEnd = event.getEndDateTime();
    const bool hasStartDateTime = eventStart.isValid();
    const bool hasEndDateTime = eventEnd.isValid();

    // Basic event details
    setSelectedEventType(event.getEventType());
    ui->nameLineEdit->setText(event.getName());
    ui->locationLineEdit->setText(event.getLocation());
    ui->descriptionTextEdit->setPlainText(event.getDescription());

    // Update date & time fields
    const QDateTime startDateTime = hasStartDateTime ? eventStart : defaultTime;
    const QDateTime endDateTime = hasEndDateTime ? eventEnd : startDateTime.addSecs(3600);
    const QDateTime dueDateTime = hasEndDateTime ? eventEnd : (hasStartDateTime ? eventStart : defaultTime);
    const QDateTime reminderDateTime = hasStartDateTime ? eventStart : defaultTime;

    ui->startDateTimeEdit->setDateTime(startDateTime);
    ui->endDateTimeEdit->setDateTime(endDateTime);
    ui->dueDateTimeEdit->setDateTime(dueDateTime);
    ui->remindAtDateTimeEdit->setDateTime(reminderDateTime);

    ui->allDayCheckBox->setChecked(event.isAllDay());
    ui->taskHasDueDateCheckBox->setChecked(hasStartDateTime || hasEndDateTime);

    // Priority Field
    const int priorityIndex = ui->priorityComboBox->findData(static_cast<int>(event.getPriority()));
    ui->priorityComboBox->setCurrentIndex(priorityIndex >= 0 ? priorityIndex : 2);

    // Category Field
    const QString category = event.getCategory().trimmed();
    if (!category.isEmpty() && ui->categoryComboBox->findText(category) < 0) {
        ui->categoryComboBox->addItem(category);
    }
    ui->categoryComboBox->setCurrentIndex(-1);
    ui->categoryComboBox->setEditText(category);

    // Recurrence Field
    const RecurrenceType recurrenceType = event.getRecurrenceType();
    const int reccurenceValue = static_cast<int>(recurrenceType);
    const int recurrenceIndex = ui->recurrenceComboBox->findData((reccurenceValue));
    const QDate recurrenceUntil = event.getRecurrenceUntil();
    const bool hasRecurrenceUntil = recurrenceUntil.isValid();

    ensureRecurrenceTypeVisible(event.getRecurrenceType());
    ui->recurrenceComboBox->setCurrentIndex(recurrenceIndex >= 0 ? recurrenceIndex : 0);
    ui->recurrenceIntervalSpinBox->setValue(std::max(1, event.getRecurrenceInterval()));
    ui->recurrenceUntilCheckBox->setChecked(hasRecurrenceUntil);
    ui->recurrenceUntilDateEdit->setDate(
        hasRecurrenceUntil ? recurrenceUntil : startDateTime.date().addMonths(1)
    );

    setSelectedRepeatDays(event.getRepeatDays());

    m_isUpdatingUi = false;
    applyTypeUi();
}

// Read the current dialog fields back into an Event object without mutating app state yet.
Event EventDialog::eventData() const {
    Event event = m_sourceEvent;
    const EventType type = selectedEventType();
    const bool allDay = ui->allDayCheckBox->isVisible() && ui->allDayCheckBox->isChecked();

    // Set all basic fields
    event.setEventType(type);
    event.setName(ui->nameLineEdit->text().trimmed());
    event.setPriority(static_cast<Priority>(ui->priorityComboBox->currentData().toInt()));
    event.setCategory(ui->categoryComboBox->currentText().trimmed());
    event.setLocation(ui->locationLineEdit->text().trimmed());
    event.setDescription(ui->descriptionTextEdit->toPlainText().trimmed());
    event.setAllDay(allDay);

    // Set correct date/time fields depending on the EventType
    switch (type) {
        case EventType::Task: {
            const bool hasDueDate = ui->taskHasDueDateCheckBox->isChecked();

            // Tasks can exist without a due date.
            if (!hasDueDate) {
                event.setStartDateTime(QDateTime());
                event.setEndDateTime(QDateTime());
                event.setAllDay(false);
            } else {
                QDateTime dueDateTime = ui->dueDateTimeEdit->dateTime();
                if (allDay) {
                    const QDate dueDate = dueDateTime.date();
                    event.setStartDateTime(QDateTime(dueDate, QTime(0, 0, 0)));
                    event.setEndDateTime(QDateTime(dueDate, QTime(23, 59, 59)));
                } else {
                    event.setStartDateTime(dueDateTime);
                    event.setEndDateTime(dueDateTime);
                }
            }
            break;
        }
        case EventType::Event:
        case EventType::ScheduleBlock: {
            QDateTime startDateTime = ui->startDateTimeEdit->dateTime();
            QDateTime endDateTime = ui->endDateTimeEdit->dateTime();
            const bool recurringSchedulePattern = type == EventType::ScheduleBlock && usesRecurringSchedulePattern();

            // Recurring schedule blocks reuse the selected startDate and endDate
            if (recurringSchedulePattern) {
                endDateTime = QDateTime(startDateTime.date(), endDateTime.time());
            }

            // All-day events and schedule blocks cover the entire selected day/range.
            if (allDay) {
                const QDate endDate = recurringSchedulePattern ? startDateTime.date() : endDateTime.date();
                startDateTime = QDateTime(startDateTime.date(), QTime(0, 0, 0));
                endDateTime = QDateTime(endDate, QTime(23,59,59));
            }

            event.setStartDateTime(startDateTime);
            event.setEndDateTime(endDateTime);
            break;
        }
        case EventType::Reminder: {
            const QDateTime reminderDateTime = ui->remindAtDateTimeEdit->dateTime();

            // Reminders use a single date/time, so startDate and endDate are the same
            event.setAllDay(false);
            event.setStartDateTime(reminderDateTime);
            event.setEndDateTime(reminderDateTime);
            break;
        }
    }

    const bool recurrenceSupported = eventTypeSupportsRecurrence(type);
    const bool preserveHiddenRecurrence = !recurrenceSupported && type == m_sourceEvent.getEventType() && m_sourceEvent.hasRecurrence();

    // Save recurrence settings only for item types that currently support recurrence.
    if (recurrenceSupported) {
        const RecurrenceType recurrenceType = selectedRecurrenceType();
        const bool preserveCustomRecurrence = recurrenceType == RecurrenceType::Custom && m_sourceEvent.getEventType() == type && m_sourceEvent.getRecurrenceType() == RecurrenceType::Custom;

        if (preserveCustomRecurrence) {
            event.setRecurrenceType(RecurrenceType::Custom);
            event.setRecurrenceInterval(std::max(1, m_sourceEvent.getRecurrenceInterval()));
            event.setRecurrenceUntil(m_sourceEvent.getRecurrenceUntil());
            event.setRepeatDays(m_sourceEvent.getRepeatDays());
        } else {
            event.setRecurrenceType(recurrenceType);

            if (recurrenceType == RecurrenceType::None) {
                event.setRecurrenceInterval(1);
                event.setRecurrenceUntil(QDate());
                event.setRepeatDays({});
            } else {
                const bool hasRepeatEndDate = ui->recurrenceUntilCheckBox->isChecked();
                event.setRecurrenceInterval(std::max(1, ui->recurrenceIntervalSpinBox->value()));
                event.setRecurrenceUntil(hasRepeatEndDate ? ui->recurrenceUntilDateEdit->date() : QDate());
                event.setRepeatDays(recurrenceType == RecurrenceType::Weekly ? selectedRepeatDays() : QList<Qt::DayOfWeek>{});
            }
        }
    } else if (!preserveHiddenRecurrence) {
        // If recurrence is not supported and there is nothing to preserve, clear it.
        event.setRecurrenceType(RecurrenceType::None);
        event.setRecurrenceInterval(1);
        event.setRecurrenceUntil(QDate());
        event.setRepeatDays({});
    }

    return event;
}

// Validate the visible fields before allowing the dialog to commit a save.
void EventDialog::validateAndAccept() {
    const QString name = ui->nameLineEdit->text().trimmed();

    // Every event object must have a name
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Name"), tr("Please enter a name before saving."));
        ui->nameLineEdit->setFocus();
        return;
    }

    const EventType type = selectedEventType();
    const bool allDay = ui->allDayCheckBox->isVisible() && ui->allDayCheckBox->isChecked();

    // Events and Schedules must have a valid start/end range
    if (eventTypeUsesRange(type)) {
        const QDateTime startDateTime = ui->startDateTimeEdit->dateTime();
        const QDateTime endDateTime = ui->endDateTimeEdit->dateTime();
        const bool recurringSchedulePattern = (type == EventType::ScheduleBlock && usesRecurringSchedulePattern());
        const QDateTime normalizedEndDateTime = recurringSchedulePattern ? QDateTime(startDateTime.date(), endDateTime.time()) : endDateTime;
        const bool invalidRange = allDay ? normalizedEndDateTime.date() < startDateTime.date() : normalizedEndDateTime <= startDateTime;

        if (invalidRange) {
            QMessageBox::warning(
                this,
                tr("Invalid Time Range"),
                recurringSchedulePattern
                    ? tr("The schedule end time must be later than the start time.")
                    : tr("The end must be on or after the start for this item."));

            ui->endDateTimeEdit->setFocus();
            return;
        }
    } else if (type == EventType::Task && ui->taskHasDueDateCheckBox->isChecked()) {
        if (!ui->dueDateTimeEdit->dateTime().isValid()) {
            QMessageBox::warning(
                this,
                tr("Missing Due Date"),
                tr("Please choose a due date before saving this task."));
            ui->dueDateTimeEdit->setFocus();
            return;
        }
    } else if (type == EventType::Reminder && !ui->remindAtDateTimeEdit->dateTime().isValid()) {
        QMessageBox::warning(
            this,
            tr("Missing Reminder Time"),
            tr("Please choose when this reminder should happen."));
        ui->remindAtDateTimeEdit->setFocus();
        return;
    }

    if (eventTypeSupportsRecurrence(type)) {
        const RecurrenceType recurrenceType = selectedRecurrenceType();
        if (recurrenceType != RecurrenceType::None && recurrenceType != RecurrenceType::Custom) {
            if (ui->recurrenceIntervalSpinBox->value() < 1) {
                QMessageBox::warning(
                    this,
                    tr("Invalid Repeat Interval"),
                    tr("Repeat interval must be at least 1."));
                ui->recurrenceIntervalSpinBox->setFocus();
                return;
            }

            const QDate baseDate = ui->startDateTimeEdit->date();
            if (ui->recurrenceUntilCheckBox->isChecked()
                && ui->recurrenceUntilDateEdit->date() < baseDate) {
                QMessageBox::warning(
                    this,
                    tr("Invalid Repeat End Date"),
                    tr("The repeat end date cannot be earlier than the start date."));
                ui->recurrenceUntilDateEdit->setFocus();
                return;
            }

            if (recurrenceType == RecurrenceType::Weekly && selectedRepeatDays().isEmpty()) {
                QMessageBox::warning(
                    this,
                    tr("Choose Repeat Days"),
                    tr("Please select at least one day for a weekly repeating item."));
                ui->mondayRepeatCheckBox->setFocus();
                return;
            }

            if (type == EventType::ScheduleBlock
                && recurrenceType == RecurrenceType::Weekly
                && !selectedRepeatDays().contains(
                    static_cast<Qt::DayOfWeek>(ui->startDateTimeEdit->date().dayOfWeek()))) {
                QMessageBox::warning(
                    this,
                    tr("Choose a Matching First Day"),
                    tr("The first schedule date must fall on one of the selected repeat days."));
                ui->startDateTimeEdit->setFocus();
                return;
            }
        }
    }

    accept(); // Accept function derived from QDialog
}

void EventDialog::ensureRecurrenceTypeVisible(RecurrenceType recurrenceType) {
    const int recurrenceValue = static_cast<int>(recurrenceType);
    if (ui->recurrenceComboBox->findData(recurrenceValue) < 0) {
        ui->recurrenceComboBox->addItem(recurrenceLabel(recurrenceType), recurrenceValue);
    }
}

// Function for handling EventType Dropbox
void EventDialog::setupTypeSelector() {
    ui->typeComboBox->addItem(eventTypeLabel(EventType::Task), static_cast<int>(EventType::Task));
    ui->typeComboBox->addItem(eventTypeLabel(EventType::Event), static_cast<int>(EventType::Event));
    ui->typeComboBox->addItem(eventTypeLabel(EventType::Reminder), static_cast<int>(EventType::Reminder));
    ui->typeComboBox->addItem(eventTypeLabel(EventType::ScheduleBlock), static_cast<int>(EventType::ScheduleBlock));
    ui->detailsGroupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    ui->descriptionTextEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    configureInlineFieldRow(ui->scheduleRangeRowLayout);
    configureInlineFieldRow(ui->scheduleMomentRowLayout);
    configureInlineFieldRow(ui->recurrenceFieldLayout);

    configureInlineFieldWidget(ui->startFieldWidget, ui->startLabel, ui->startDateTimeEdit, 180);
    configureInlineFieldWidget(ui->endFieldWidget, ui->endLabel, ui->endDateTimeEdit, 180);
    configureInlineFieldWidget(ui->dueFieldWidget, ui->dueLabel, ui->dueDateTimeEdit, 180);
    configureInlineFieldWidget(ui->remindAtFieldWidget, ui->remindAtLabel, ui->remindAtDateTimeEdit, 180);
    configureInlineFieldWidget(ui->recurrenceFieldWidget, ui->recurrenceLabel, ui->recurrenceComboBox, 220);
}

// Show only the controls that matter for the currently selected item type.
void EventDialog::applyTypeUi() {
    const EventType type = selectedEventType();
    const bool usesRange = eventTypeUsesRange(type);
    const bool hasDueDate = type == EventType::Task && ui->taskHasDueDateCheckBox->isChecked();
    const bool usesSingleMoment = eventTypeUsesSingleMoment(type);
    const bool showAllDay = usesRange || hasDueDate;
    const bool showRecurrence = eventTypeSupportsRecurrence(type);

    ui->taskHasDueDateCheckBox->setVisible(type == EventType::Task);
    ui->allDayCheckBox->setVisible(showAllDay);

    ui->scheduleRangeRowWidget->setVisible(usesRange);
    ui->scheduleMomentRowWidget->setVisible(hasDueDate || usesSingleMoment);
    ui->recurrenceFieldWidget->setVisible(showRecurrence);

    ui->startFieldWidget->setVisible(usesRange);
    ui->endFieldWidget->setVisible(usesRange);
    ui->dueFieldWidget->setVisible(hasDueDate);
    ui->remindAtFieldWidget->setVisible(usesSingleMoment);

    ui->startLabel->setVisible(usesRange);
    ui->startDateTimeEdit->setVisible(usesRange);
    ui->endLabel->setVisible(usesRange);
    ui->endDateTimeEdit->setVisible(usesRange);

    ui->dueLabel->setVisible(hasDueDate);
    ui->dueDateTimeEdit->setVisible(hasDueDate);

    ui->remindAtLabel->setVisible(usesSingleMoment);
    ui->remindAtDateTimeEdit->setVisible(usesSingleMoment);

    ui->recurrenceLabel->setVisible(showRecurrence);
    ui->recurrenceComboBox->setVisible(showRecurrence);

    switch (type) {
        case EventType::Task:
            ui->scheduleSectionTitleLabel->setText(tr("Due Date"));
            break;
        case EventType::Event:
            ui->scheduleSectionTitleLabel->setText(tr("Schedule"));
            break;
        case EventType::Reminder:
            ui->scheduleSectionTitleLabel->setText(tr("Reminder"));
            break;
        case EventType::ScheduleBlock:
            ui->scheduleSectionTitleLabel->setText(tr("Schedule"));
            break;
    }

    if (!showAllDay) {
        QSignalBlocker blocker(ui->allDayCheckBox);
        ui->allDayCheckBox->setChecked(false);
    }

    applyScheduleFieldLabels();
    normalizeSchedulePatternFields();
    applyAllDayUi();
    applyRecurrenceUi();
    updateScheduleHelperText();
    updateDialogTitle();
}

// Reveal recurrence-specific controls and keep their defaults valid for the chosen pattern.
void EventDialog::applyRecurrenceUi() {
    const EventType type = selectedEventType();
    const RecurrenceType recurrenceType = selectedRecurrenceType();
    const bool recurrenceSupported = eventTypeSupportsRecurrence(type);
    const bool hasRecurrence = recurrenceSupported && recurrenceType != RecurrenceType::None;
    const bool customRecurrence = recurrenceSupported && recurrenceType == RecurrenceType::Custom;
    const bool weeklyRecurrence = recurrenceType == RecurrenceType::Weekly;

    QString unitLabel;
    switch (recurrenceType) {
        case RecurrenceType::Daily:
            unitLabel = ui->recurrenceIntervalSpinBox->value() == 1 ? tr("day") : tr("days");
            break;
        case RecurrenceType::Weekly:
            unitLabel = ui->recurrenceIntervalSpinBox->value() == 1 ? tr("week") : tr("weeks");
            break;
        case RecurrenceType::Monthly:
            unitLabel = ui->recurrenceIntervalSpinBox->value() == 1 ? tr("month") : tr("months");
            break;
        case RecurrenceType::Yearly:
            unitLabel = ui->recurrenceIntervalSpinBox->value() == 1 ? tr("year") : tr("years");
            break;
        case RecurrenceType::Custom:
            unitLabel = tr("custom interval");
            break;
        case RecurrenceType::None:
            unitLabel = tr("days");
            break;
    }

    ui->recurrenceIntervalUnitLabel->setText(unitLabel);
    ui->recurrenceAdvancedContainer->setVisible(hasRecurrence);
    ui->recurrenceIntervalRowWidget->setVisible(hasRecurrence && !customRecurrence);
    ui->recurrenceUntilRowWidget->setVisible(hasRecurrence && !customRecurrence);
    ui->repeatDaysContainer->setVisible(hasRecurrence && weeklyRecurrence);
    ui->customRecurrenceNoticeLabel->setVisible(customRecurrence);
    ui->recurrenceUntilDateEdit->setEnabled(ui->recurrenceUntilCheckBox->isChecked() && hasRecurrence && !customRecurrence);

    if (hasRecurrence && weeklyRecurrence && selectedRepeatDays().isEmpty()) {
        setSelectedRepeatDays({static_cast<Qt::DayOfWeek>(ui->startDateTimeEdit->date().dayOfWeek())});
    }

    applyScheduleFieldLabels();
    normalizeSchedulePatternFields();
}

// Switch date/time displays between all-day and time-specific modes.
void EventDialog::applyAllDayUi() {
    const bool allDay = ui->allDayCheckBox->isVisible() && ui->allDayCheckBox->isChecked();
    applyScheduleFieldLabels();
    normalizeSchedulePatternFields();
    configureDateTimeEditDisplay(ui->dueDateTimeEdit, allDay && ui->dueDateTimeEdit->isVisible());
    configureDateTimeEditDisplay(ui->remindAtDateTimeEdit, false);
    updateScheduleHelperText();
}

// Rename the schedule fields when a recurring schedule pattern acts more like a template than a one-off range.
void EventDialog::applyScheduleFieldLabels() {
    const bool allDay = ui->allDayCheckBox->isVisible() && ui->allDayCheckBox->isChecked();
    if (usesRecurringSchedulePattern()) {
        ui->startLabel->setText(tr("First on"));
        ui->endLabel->setText(allDay ? tr("Ends on") : tr("To"));
        ui->repeatDaysLabel->setText(tr("Occurs on"));
        ui->recurrenceUntilCheckBox->setText(tr("Until"));
        ui->startDateTimeEdit->setDisplayFormat(allDay ? QStringLiteral("MMM d, yyyy")
                                                       : QStringLiteral("MMM d, yyyy h:mm AP"));
        ui->endDateTimeEdit->setDisplayFormat(allDay ? QStringLiteral("MMM d, yyyy")
                                                     : QStringLiteral("h:mm AP"));
        return;
    }

    ui->startLabel->setText(tr("Start"));
    ui->endLabel->setText(tr("End"));
    ui->repeatDaysLabel->setText(tr("Repeat on"));
    ui->recurrenceUntilCheckBox->setText(tr("Ends on"));
    configureDateTimeEditDisplay(ui->startDateTimeEdit, allDay && ui->startDateTimeEdit->isVisible());
    configureDateTimeEditDisplay(ui->endDateTimeEdit, allDay && ui->endDateTimeEdit->isVisible());
}

// Keep recurring schedule end fields anchored to the same day as the first occurrence.
void EventDialog::normalizeSchedulePatternFields() {
    if (!usesRecurringSchedulePattern()) {
        return;
    }

    const QDateTime startDateTime = ui->startDateTimeEdit->dateTime();
    if (!startDateTime.isValid()) {
        return;
    }

    const bool allDay = ui->allDayCheckBox->isVisible() && ui->allDayCheckBox->isChecked();
    QDateTime normalizedEndDateTime;
    if (allDay) {
        normalizedEndDateTime = QDateTime(startDateTime.date(), QTime(23, 59, 59));
    } else {
        normalizedEndDateTime = QDateTime(startDateTime.date(), ui->endDateTimeEdit->time());
        if (!normalizedEndDateTime.isValid() || normalizedEndDateTime <= startDateTime) {
            normalizedEndDateTime = startDateTime.addSecs(3600);
            if (normalizedEndDateTime.date() != startDateTime.date()) {
                normalizedEndDateTime = QDateTime(startDateTime.date(), QTime(23, 59, 0));
            }
        }
    }

    if (ui->endDateTimeEdit->dateTime() != normalizedEndDateTime) {
        QSignalBlocker blocker(ui->endDateTimeEdit);
        ui->endDateTimeEdit->setDateTime(normalizedEndDateTime);
    }
}

// Explain the current schedule mode in plain language so the visible fields feel less ambiguous.
void EventDialog::updateScheduleHelperText() {
    const EventType type = selectedEventType();
    const bool allDay = ui->allDayCheckBox->isVisible() && ui->allDayCheckBox->isChecked();
    const bool hasDueDate = ui->taskHasDueDateCheckBox->isChecked();

    QString helperText;
    switch (type) {
        case EventType::Task:
            helperText = hasDueDate
                ? tr("Add a due date only when this task needs a deadline.")
                : tr("Tasks can stay undated until you are ready to schedule them.");
            break;
        case EventType::Event:
            helperText = allDay
                ? tr("Set a later end date to create a multi-day all-day event.")
                : tr("Set a later end date to create a multi-day or overnight event.");
            break;
        case EventType::Reminder:
            helperText = tr("Use one reminder time for a simple point-in-time prompt.");
            break;
        case EventType::ScheduleBlock:
            helperText = usesRecurringSchedulePattern()
                ? tr("Schedules repeat from the first occurrence on the selected days until the optional end date.")
                : tr("Use Repeat for class or work patterns such as weekly shifts.");
            break;
    }

    ui->scheduleHelperLabel->setText(helperText);
}

// Simple function that changes the title depending on if an event is being added/edited
void EventDialog::updateDialogTitle() {
    const QString title = m_mode == Mode::Add ? tr("Add Event") : tr("Edit Event");
    setWindowTitle(title);
    ui->dialogTitleLabel->setText(title);
}

// Sets the EventType
void EventDialog::setSelectedEventType(EventType eventType) {
    const int index = ui->typeComboBox->findData(static_cast<int>(eventType));
    if (index >= 0) {
        QSignalBlocker blocker(ui->typeComboBox);
        ui->typeComboBox->setCurrentIndex(index);
    }
}

EventType EventDialog::selectedEventType() const {
    return static_cast<EventType>(ui->typeComboBox->currentData().toInt());
}

RecurrenceType EventDialog::selectedRecurrenceType() const {
    return static_cast<RecurrenceType>(ui->recurrenceComboBox->currentData().toInt());
}

QList<Qt::DayOfWeek> EventDialog::selectedRepeatDays() const {
    QList<Qt::DayOfWeek> repeatDays;

    const QList<QPair<QCheckBox *, Qt::DayOfWeek>> buttons = {
        {ui->mondayRepeatCheckBox, Qt::Monday},
        {ui->tuesdayRepeatCheckBox, Qt::Tuesday},
        {ui->wednesdayRepeatCheckBox, Qt::Wednesday},
        {ui->thursdayRepeatCheckBox, Qt::Thursday},
        {ui->fridayRepeatCheckBox, Qt::Friday},
        {ui->saturdayRepeatCheckBox, Qt::Saturday},
        {ui->sundayRepeatCheckBox, Qt::Sunday},
    };

    for (const auto &[button, day] : buttons) {
        if (button->isChecked()) {
            repeatDays.append(day);
        }
    }

    return repeatDays;
}

// Function for setting what days an event repeats on
void EventDialog::setSelectedRepeatDays(const QList<Qt::DayOfWeek> &repeatDays) {
    const QList<QPair<QCheckBox *, Qt::DayOfWeek>> buttons = {
        {ui->mondayRepeatCheckBox, Qt::Monday},
        {ui->tuesdayRepeatCheckBox, Qt::Tuesday},
        {ui->wednesdayRepeatCheckBox, Qt::Wednesday},
        {ui->thursdayRepeatCheckBox, Qt::Thursday},
        {ui->fridayRepeatCheckBox, Qt::Friday},
        {ui->saturdayRepeatCheckBox, Qt::Saturday},
        {ui->sundayRepeatCheckBox, Qt::Sunday},
    };

    for (const auto &[button, day] : buttons) {
        QSignalBlocker blocker(button);
        button->setChecked(repeatDays.contains(day));
    }
}

void EventDialog::configureDateTimeEditDisplay(QDateTimeEdit *edit, bool dateOnly) const {
    edit->setDisplayFormat(dateOnly ? QStringLiteral("MMM d, yyyy")
                                    : QStringLiteral("MMM d, yyyy h:mm AP"));
}

bool EventDialog::usesRecurringSchedulePattern() const {
    const RecurrenceType recurrenceType = selectedRecurrenceType();
    return selectedEventType() == EventType::ScheduleBlock
        && recurrenceType != RecurrenceType::None
        && recurrenceType != RecurrenceType::Custom;
}

// Events & Schedules can use a time range i.e XX:xx - XX:xx
bool EventDialog::eventTypeUsesRange(EventType eventType) const {
    return eventType == EventType::Event || eventType == EventType::ScheduleBlock;
}
\
// Reminders only show one date
bool EventDialog::eventTypeUsesSingleMoment(EventType eventType) const {
    return eventType == EventType::Reminder;
}

// Events & Schedules can support reccurrence
bool EventDialog::eventTypeSupportsRecurrence(EventType eventType) const {
    return eventType == EventType::Event || eventType == EventType::ScheduleBlock;
}

// Sets the default dateTime as
QDateTime EventDialog::defaultDateTime() const {
    QDateTime roundedDateTime = QDateTime::currentDateTime();
    roundedDateTime.setSecsSinceEpoch(((roundedDateTime.toSecsSinceEpoch() + 1799) / 1800) * 1800);
    return roundedDateTime;
}



