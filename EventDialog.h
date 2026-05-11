#ifndef EVENTDIALOG_H
#define EVENTDIALOG_H

#include <QDialog>
#include <QStringList>

#include "Event.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class EventDialog;
}
QT_END_NAMESPACE

class QDateTimeEdit;

class EventDialog : public QDialog {
    public:
        enum class Mode {
            Add,
            Edit,
        };

        explicit EventDialog(QWidget *parent = nullptr);
        ~EventDialog() override;

        void setMode(Mode mode);
        void setAvailableCategories(const QStringList &categories);
        void setEvent(const Event &event);
        Event eventData() const;

    private:
        void validateAndAccept();
        void ensureRecurrenceTypeVisible(RecurrenceType recurrenceType);
        void setupTypeSelector();
        void applyTypeUi();
        void applyRecurrenceUi();
        void applyAllDayUi();
        void applyScheduleFieldLabels();
        void normalizeSchedulePatternFields();
        void updateScheduleHelperText();
        void updateDialogTitle();
        void setSelectedEventType(EventType eventType);
        EventType selectedEventType() const;
        RecurrenceType selectedRecurrenceType() const;
        QList<Qt::DayOfWeek> selectedRepeatDays() const;
        void setSelectedRepeatDays(const QList<Qt::DayOfWeek> &repeatDays);
        void configureDateTimeEditDisplay(QDateTimeEdit *edit, bool dateOnly) const;
        bool usesRecurringSchedulePattern() const;
        bool eventTypeUsesRange(EventType eventType) const;
        bool eventTypeUsesSingleMoment(EventType eventType) const;
        bool eventTypeSupportsRecurrence(EventType eventType) const;
        QDateTime defaultDateTime() const;

        Ui::EventDialog *ui;
        Event m_sourceEvent;
        Mode m_mode = Mode::Add;
        bool m_isUpdatingUi = false;
};

#endif // EVENTDIALOG_H



