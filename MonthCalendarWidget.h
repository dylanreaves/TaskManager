#ifndef MONTHCALENDARWIDGET_H
#define MONTHCALENDARWIDGET_H

#include <QDate>
#include <QPointer>
#include <QWidget>

#include "EventManager.h"

class QFrame;
class QLabel;
class QResizeEvent;
class QToolButton;
class QVBoxLayout;
class MonthCalendarGridWidget;

class MonthCalendarWidget : public QWidget {
    Q_OBJECT

    public:
        explicit MonthCalendarWidget(QWidget *parent = nullptr);
        QSize minimumSizeHint() const override;
        QSize sizeHint() const override;

        void setMonthViewModel(const MonthViewModel &model);

        QDate selectedDate() const;
        QDate visibleMonth() const;

        void setSelectedDate(const QDate &date);
        void showSelectedDate();

    signals:
        void selectedDateChanged(const QDate &date);
        void currentPageChanged(int year, int month);
        void dayActivated(const QDate &selectedDate, const QDate &jumpDate);
        void occurrenceActivated(const QString &occurrenceKey, int eventId);

    protected:
        void resizeEvent(QResizeEvent *event) override;

    private:
        void shiftMonth(int months);
        void setVisibleMonthInternal(const QDate &month, bool emitSignal);
        void updateHeaderLabel();
        void rebuildLegend();
        void closeOverflowPopover();
        void showOverflowPopover(const QDate &date, const QPoint &globalAnchor);
        void handleGridClick(const QPoint &localPos);
        void handleGridHover(const QPoint &localPos);
        void clearGridHover();

        MonthCalendarGridWidget *m_gridWidget = nullptr;
        QWidget *m_leftHeaderControls = nullptr;
        QToolButton *m_previousMonthButton = nullptr;
        QToolButton *m_nextMonthButton = nullptr;
        QToolButton *m_todayButton = nullptr;
        QLabel *m_titleLabel = nullptr;
        QWidget *m_legendContainer = nullptr;
        QVBoxLayout *m_legendLayout = nullptr;
        QPointer<QFrame> m_overflowPopup;
        MonthViewModel m_model;
        QDate m_visibleMonth;
        QDate m_selectedDate;
        int m_hoveredMoreCellIndex = -1;

        friend class MonthCalendarGridWidget;
};

#endif // MONTHCALENDARWIDGET_H



