// Implements main window setup and initializes all layouts
#include "mainwindow.h"

#include "./ui_mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCheckBox>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QPainter>
#include <QMessageBox>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "MonthCalendarWidget.h"
#include "Storage.h"
#include "Utility.h"

namespace {

const int OCCURRENCE_KEY_ROLE = Qt::UserRole;
const int EVENT_ID_ROLE = Qt::UserRole + 1;
const int CATEGORY_ROLE = Qt::UserRole + 2;
const int ROW_KIND_ROLE = Qt::UserRole + 3;
const int FOLDER_ID_ROLE = Qt::UserRole + 4;
const int FOLDER_DEPTH_ROLE = Qt::UserRole + 5;
const int PARENT_FOLDER_ID_ROLE = Qt::UserRole + 6;

// Simple enum to differentiate folders from events in the list table
enum class ListTableRowKind {
    Occurrence = 0,
    Folder = 1,
};

const int MONTH_PANEL_MIN_HEIGHT = 0;           // Minimum height for list & month panel
const int TOP_CONTENT_MIN_HEIGHT = 0;           // Minimum height for list & month panel
const int DETAILS_MIN_HEIGHT = 190;             // Minimum height for list & month panel
const int DETAILS_PREFERRED_HEIGHT = 240;       // Minimum height for list & month panel
const int WINDOW_MIN_HEIGHT = 520;              // Minimum height for list & month panel
const QString TODAY_SUFFIX = QStringLiteral(" <Today>"); // String literal that goes onto the "Today" folder

class ListFolderTodayDelegate final : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;
        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
            const bool isFolder = index.data(ROW_KIND_ROLE).toInt() == static_cast<int>(ListTableRowKind::Folder);
            const QString displayText = index.data(Qt::DisplayRole).toString();
            if (!isFolder || index.column() != 0 || !displayText.endsWith(TODAY_SUFFIX)) {
                QStyledItemDelegate::paint(painter, option, index);
                return;
            }

            QStyleOptionViewItem adjustedOption(option);
            initStyleOption(&adjustedOption, index);

            const int suffixStart = displayText.lastIndexOf(TODAY_SUFFIX);
            if (suffixStart <= 0) {
                QStyledItemDelegate::paint(painter, option, index);
                return;
            }

            const QString baseText = displayText.left(suffixStart);
            adjustedOption.text.clear();

            const QWidget *widget = adjustedOption.widget;
            QStyle *style = widget != nullptr ? widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &adjustedOption, painter, widget);

            const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &adjustedOption, widget);
            if (!textRect.isValid()) {
                return;
            }

            painter->save();
            painter->setFont(adjustedOption.font);

            const QBrush foregroundBrush = qvariant_cast<QBrush>(index.data(Qt::ForegroundRole));
            const QColor baseColor = foregroundBrush.style() != Qt::NoBrush
                ? foregroundBrush.color()
                : adjustedOption.palette.color(QPalette::Text);

            QFontMetrics metrics(adjustedOption.font);
            const int suffixWidth = metrics.horizontalAdvance(TODAY_SUFFIX);
            const QString elidedBase = metrics.elidedText(baseText,
                                                          Qt::ElideRight,
                                                          qMax(0, textRect.width() - suffixWidth));
            const int baseWidth = metrics.horizontalAdvance(elidedBase);

            painter->setPen(baseColor);
            painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elidedBase);

            QRect suffixRect = textRect;
            suffixRect.setLeft(qMin(textRect.right(), textRect.left() + baseWidth));
            QFont suffixFont = adjustedOption.font;
            suffixFont.setBold(true);
            painter->setFont(suffixFont);
            painter->setPen(QColor(QStringLiteral("#FF4D4D")));
            painter->drawText(suffixRect, Qt::AlignLeft | Qt::AlignVCenter, TODAY_SUFFIX);

            painter->restore();
        }
};

// The default path for the storage file at Data/events.json
QString defaultStoragePath() {
    const QString buildRelativePath = QDir(QCoreApplication::applicationDirPath())
                                          .filePath(QStringLiteral("../../Data/events.json"));
    if (QFileInfo::exists(buildRelativePath)) {
        return QDir::cleanPath(buildRelativePath);
    }

    return QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/Data/events.json");
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_storagePath(defaultStoragePath()) {
    ui->setupUi(this);

    configureUi();
    bindSignals();
    loadEvents();
    refreshCategoryFilter();
    refreshVisibleData();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    enforceListMonthSplitterBounds();
    enforceContentVerticalSplitterBounds();
}

// Main interface rendering
void MainWindow::configureUi() {
    setWindowTitle(QStringLiteral("TaskManager"));
    ui->rootLayout->setContentsMargins(8, 8, 10, 10);
    ui->rootLayout->setSpacing(8);
    ui->listPageLayout->setContentsMargins(2, 2, 2, 2);
    ui->listPageLayout->setSpacing(4);

    ui->searchLineEdit->setClearButtonEnabled(true);

    ui->allEventsButton->setCheckable(true);
    ui->allEventsButton->setAutoExclusive(true);
    ui->todayButton->setCheckable(true);
    ui->todayButton->setAutoExclusive(true);
    ui->upcomingButton->setCheckable(true);
    ui->upcomingButton->setAutoExclusive(true);
    ui->thisWeekButton->setCheckable(true);
    ui->thisWeekButton->setAutoExclusive(true);
    ui->recurringButton->setCheckable(true);
    ui->recurringButton->setAutoExclusive(true);
    ui->completedButton->setCheckable(true);
    ui->completedButton->setAutoExclusive(true);
    ui->allEventsButton->setChecked(true);

    const auto configureListTable = [this](QTableWidget *table) {
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->setWordWrap(false);
        table->setTextElideMode(Qt::ElideRight);
        table->verticalHeader()->setVisible(false);
        table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        table->verticalHeader()->setDefaultSectionSize(28);
        table->verticalHeader()->setMinimumSectionSize(28);
        table->setShowGrid(true);
        table->setGridStyle(Qt::SolidLine);
        table->horizontalHeader()->setMinimumSectionSize(56);
        table->horizontalHeader()->setCascadingSectionResizes(true);
        table->setColumnCount(6);
        table->setHorizontalHeaderLabels(
            {tr("Time"),       // 0 = Time
             tr("Name"),       // 1 = Name
             tr("Type"),       // 2 = Event Type
             tr("Category"),   // 3 = Category
             tr("Priority"),   // 4 = Priority
             tr("Location")}); // 5 = Location
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Time
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);      // Name
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents); // Event Type
        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Category
        table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents); // Priority
        table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);          // Location
        table->horizontalHeader()->resizeSection(1, 150); // Name field wdith on startup.
        table->setStyleSheet(QStringLiteral(
            "QTableWidget { gridline-color: #4B5563; }"
            "QTableWidget::item { padding-top: 3px; padding-bottom: 3px; }"));
    };

    m_listContentSplitter = new QSplitter(Qt::Horizontal, ui->listPage);
    m_listContentSplitter->setChildrenCollapsible(false);
    m_listContentSplitter->setHandleWidth(6);
    m_listContentSplitter->setMinimumHeight(0);

    ui->listPageLayout->removeWidget(ui->eventTableWidget);
    ui->eventTableWidget->setParent(m_listContentSplitter);
    ui->eventTableWidget->setMinimumWidth(300);
    ui->eventTableWidget->setMinimumHeight(0);
    m_listContentSplitter->addWidget(ui->eventTableWidget);

    auto *calendarPanel = new QFrame(m_listContentSplitter);
    m_listMonthPanel = calendarPanel;
    calendarPanel->setObjectName(QStringLiteral("listCalendarPanel"));
    calendarPanel->setFrameShape(QFrame::StyledPanel);
    calendarPanel->setMinimumHeight(MONTH_PANEL_MIN_HEIGHT);
    calendarPanel->setStyleSheet(QStringLiteral(
        "QFrame#listCalendarPanel {"
        "background-color: #2B2B2B;"
        "border: 1px solid #3F3F46;"
        "border-radius: 12px;"
        "}"));
    auto *calendarPanelLayout = new QVBoxLayout(calendarPanel);
    calendarPanelLayout->setContentsMargins(0, 0, 0, 0);
    calendarPanelLayout->setSpacing(0);

    m_monthCalendarWidget = new MonthCalendarWidget(calendarPanel);
    const QSize recommendedMonthSize = m_monthCalendarWidget->minimumSizeHint();
    const int recommendedMonthHeight = recommendedMonthSize.height();
    m_monthCalendarWidget->setMinimumWidth(recommendedMonthSize.width());
    m_monthCalendarWidget->setMinimumHeight(recommendedMonthHeight);
    calendarPanel->setMinimumWidth(recommendedMonthSize.width());
    calendarPanel->setMinimumHeight(recommendedMonthHeight);
    calendarPanelLayout->addWidget(m_monthCalendarWidget);
    m_listContentSplitter->addWidget(calendarPanel);

    ui->listPage->setMinimumHeight(0);

    m_listContentSplitter->setStretchFactor(0, 1);
    m_listContentSplitter->setStretchFactor(1, 1);
    m_listContentSplitter->setSizes({760, 760});
    ui->listPageLayout->addWidget(m_listContentSplitter);

    ui->mainStackedWidget->setCurrentWidget(ui->listPage);

    configureListTable(ui->eventTableWidget);
    ui->eventTableWidget->setItemDelegate(new ListFolderTodayDelegate(ui->eventTableWidget));
    ui->tasksListWidget->setFocusPolicy(Qt::NoFocus);
    ui->tasksListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tasksListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->tasksListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tasksListWidget->setAlternatingRowColors(false);
    ui->tasksListWidget->setWordWrap(true);
    ui->tasksListWidget->setUniformItemSizes(false);
    ui->tasksListWidget->setSpacing(6);
    ui->tasksListWidget->setStyleSheet(QStringLiteral(
        "QListWidget { border: none; background: transparent; outline: 0; }"
        "QListWidget::item { background: transparent; border: none; padding: 0px; }"
        "QListWidget::item:selected { background: transparent; outline: 0; }"
        "QListWidget::item:focus { outline: 0; }"));
    ui->tasksTitleLabel->setText(tr("Today's Agenda:"));

    ui->mainHorizontalSplitter->setStretchFactor(0, 0);
    ui->mainHorizontalSplitter->setStretchFactor(1, 1);
    ui->mainHorizontalSplitter->setSizes({ui->sidebarFrame->minimumWidth(), 1000});

    ui->contentVerticalSplitter->setChildrenCollapsible(false);
    ui->contentVerticalSplitter->setHandleWidth(6);
    ui->contentVerticalSplitter->setStretchFactor(0, 1);
    ui->contentVerticalSplitter->setStretchFactor(1, 0);
    ui->mainStackedWidget->setMinimumHeight(recommendedMonthHeight);
    ui->detailsFrame->setMinimumHeight(DETAILS_MIN_HEIGHT);
    ui->contentVerticalSplitter->setSizes({recommendedMonthHeight + 80, DETAILS_PREFERRED_HEIGHT});

    ui->detailCategoryValueLabel->setAlignment(Qt::AlignCenter);
    ui->detailCategoryValueLabel->setMinimumHeight(22);
    ui->completeEventCheckBox->setMinimumHeight(24);
    ui->completeEventCheckBox->setStyleSheet(QStringLiteral("QCheckBox { padding: 2px 0; }"));
    ui->detailsContentLayout->setStretch(0, 11);
    ui->detailsContentLayout->setStretch(1, 9);
    ui->detailsGridLayout->setHorizontalSpacing(12);
    ui->detailsGridLayout->setVerticalSpacing(8);
    ui->detailsGridLayout->setColumnStretch(0, 0);
    ui->detailsGridLayout->setColumnStretch(1, 0);
    ui->detailsGridLayout->setRowMinimumHeight(5, 24);

    setMinimumHeight(qMax(WINDOW_MIN_HEIGHT,
                          recommendedMonthHeight + DETAILS_MIN_HEIGHT + ui->contentVerticalSplitter->handleWidth()));
    enforceListMonthSplitterBounds();
    enforceContentVerticalSplitterBounds();
    QTimer::singleShot(0, this, [this]() {
        if (m_listContentSplitter == nullptr || m_listMonthPanel == nullptr) {
            return;
        }

        const int totalWidth = m_listContentSplitter->width();
        if (totalWidth <= 0) {
            return;
        }

        const int handleWidth = m_listContentSplitter->handleWidth();
        const int usableWidth = qMax(0, totalWidth - handleWidth);
        const int leftMin = qMin(240, usableWidth);
        const int calendarMinWidth = qMax(0, m_listMonthPanel->minimumWidth());
        int leftMax = qMax(leftMin, usableWidth - calendarMinWidth);
        const int preferredLeft = qMax(leftMin, qMin(leftMax, usableWidth / 2));

        m_listContentSplitter->blockSignals(true);
        m_listContentSplitter->setSizes({preferredLeft, qMax(0, usableWidth - preferredLeft)});
        m_listContentSplitter->blockSignals(false);
        enforceListMonthSplitterBounds();
    });
    QTimer::singleShot(0, this, [this, recommendedMonthHeight]() {
        if (ui->contentVerticalSplitter == nullptr
            || ui->detailsFrame == nullptr
            || ui->mainStackedWidget == nullptr) {
            return;
        }

        const int totalHeight = ui->contentVerticalSplitter->height();
        if (totalHeight <= 0) {
            return;
        }

        const int handleHeight = ui->contentVerticalSplitter->handleWidth();
        const int usableHeight = qMax(0, totalHeight - handleHeight);
        const int topMin = qMax(TOP_CONTENT_MIN_HEIGHT,
                                qMax(ui->mainStackedWidget->minimumHeight(), recommendedMonthHeight));
        const int bottomMin = qMax(DETAILS_MIN_HEIGHT, ui->detailsFrame->minimumHeight());
        const int effectiveBottomMin = qMin(bottomMin, usableHeight);

        int targetBottom = qMax(DETAILS_PREFERRED_HEIGHT, effectiveBottomMin);
        targetBottom = qMin(targetBottom, usableHeight);
        int targetTop = qMax(0, usableHeight - targetBottom);

        if (targetTop < topMin) {
            targetTop = qMin(topMin, usableHeight);
            targetBottom = qMax(0, usableHeight - targetTop);
        }

        ui->contentVerticalSplitter->blockSignals(true);
        ui->contentVerticalSplitter->setSizes({targetTop, targetBottom});
        ui->contentVerticalSplitter->blockSignals(false);
        enforceContentVerticalSplitterBounds();
    });
    refreshDetailsPanel(nullptr);
    refreshActionButtonStates();
}

// Connects buttons to functions
void MainWindow::bindSignals() {
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, [this] {
        refreshVisibleData();
    });
    connect(ui->priorityFilterComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        refreshVisibleData();
    });
    connect(ui->categoryFilterComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
        refreshVisibleData();
    });

    connect(ui->allEventsButton, &QPushButton::clicked, this, [this] {
        setTimeFilter(EventTimeFilter::AllEvents);
    });
    connect(ui->todayButton, &QPushButton::clicked, this, [this] {
        setTimeFilter(EventTimeFilter::Today);
    });
    connect(ui->upcomingButton, &QPushButton::clicked, this, [this] {
        setTimeFilter(EventTimeFilter::Upcoming);
    });
    connect(ui->thisWeekButton, &QPushButton::clicked, this, [this] {
        setTimeFilter(EventTimeFilter::ThisWeek);
    });
    connect(ui->recurringButton, &QPushButton::clicked, this, [this] {
        setTimeFilter(EventTimeFilter::Recurring);
    });
    connect(ui->completedButton, &QPushButton::clicked, this, [this] {
        setTimeFilter(EventTimeFilter::Completed);
    });

    connect(ui->tasksListWidget, &QListWidget::itemSelectionChanged,
            this, &MainWindow::handleAgendaListSelectionChanged);
    connect(ui->eventTableWidget, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::handleListSelectionChanged);
    connect(ui->eventTableWidget, &QTableWidget::cellClicked,
            this, &MainWindow::handleListCellClicked);
    connect(m_monthCalendarWidget, &MonthCalendarWidget::currentPageChanged, this, [this](int, int) {
        refreshMonthCalendar(QDateTime::currentDateTime());
    });
    connect(m_monthCalendarWidget, &MonthCalendarWidget::dayActivated, this, [this](const QDate &, const QDate &jumpDate) {
        if (jumpDate.isValid()) {
            revealActionableDateFolder(jumpDate);
        }
    });
    connect(m_monthCalendarWidget, &MonthCalendarWidget::occurrenceActivated, this, [this](const QString &occurrenceKey, int eventId) {
        if (!occurrenceKey.isEmpty()) {
            selectOccurrenceByKey(ui->eventTableWidget, occurrenceKey);
            if (selectedOccurrenceKey(ui->eventTableWidget) == occurrenceKey) {
                return;
            }
        }

        if (eventId < 0) {
            return;
        }

        for (int row = 0; row < ui->eventTableWidget->rowCount(); ++row) {
            QTableWidgetItem *item = ui->eventTableWidget->item(row, 0);
            if (item != nullptr
                && item->data(ROW_KIND_ROLE).toInt() != static_cast<int>(ListTableRowKind::Folder)
                && item->data(EVENT_ID_ROLE).toInt() == eventId) {
                selectTableRow(ui->eventTableWidget, row);
                return;
            }
        }
    });
    connect(m_listContentSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        enforceListMonthSplitterBounds();
    });
    connect(ui->contentVerticalSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
        enforceContentVerticalSplitterBounds();
    });
    connect(ui->addEventButton, &QPushButton::clicked, this, &MainWindow::handleAddEvent);
    connect(ui->editEventButton, &QPushButton::clicked, this, &MainWindow::handleEditEvent);
    connect(ui->deleteEventButton, &QPushButton::clicked, this, &MainWindow::handleDeleteEvent);
    connect(ui->completeEventCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        const EventOccurrence *selectedOccurrence = currentSelectedOccurrence();
        if (selectedOccurrence == nullptr || checked == selectedOccurrence->completed) {
            return;
        }

        const bool previousCompletedState = selectedOccurrence->completed;
        if (!handleCompleteEvent(checked)) {
            QSignalBlocker blocker(ui->completeEventCheckBox);
            ui->completeEventCheckBox->setChecked(previousCompletedState);
        }
    });
}

// Loads events stored in storage
void MainWindow::loadEvents() {
    QString errorMessage;
    const QVector<Event> events = Storage::loadFromFile(m_storagePath, &errorMessage);
    m_eventManager.setEvents(events);
    autoCompleteOverdueEventsIfNeeded(QDateTime::currentDateTime());

    if (!errorMessage.isEmpty()) {
        statusBar()->showMessage(errorMessage, 8000);
    } else {
        statusBar()->showMessage(
            tr("Loaded %1 events from %2").arg(m_eventManager.totalEvents()).arg(m_storagePath), 5000);
    }
}

bool MainWindow::persistEvents(const QVector<Event> &events, const QString &actionDescription) {
    QString errorMessage;
    Storage::saveToFile(events, m_storagePath, &errorMessage);

    if (errorMessage.isEmpty()) {
        return true;
    }

    QMessageBox::critical(
        this,
        tr("Save Failed"),
        tr("The app could not %1.\n\n%2").arg(actionDescription, errorMessage));
    return false;
}

// Function for auto completing events when their endDate passes
bool MainWindow::autoCompleteOverdueEventsIfNeeded(const QDateTime &now) {
    if (m_isApplyingAutoCompletion) {
        return false;
    }

    EventManager updatedManager = m_eventManager;
    if (!updatedManager.autoCompleteOverdueEvents(now)) {
        return false;
    }

    const QVector<Event> updatedEvents = updatedManager.getEvents();
    if (!persistEvents(updatedEvents, tr("save automatically completed events"))) {
        return false;
    }

    m_isApplyingAutoCompletion = true;
    m_eventManager.setEvents(updatedEvents);
    m_isApplyingAutoCompletion = false;
    statusBar()->showMessage(tr("Overdue events were moved into completed history."), 5000);
    return true;
}

// Function for when the category filter changes
void MainWindow::refreshCategoryFilter() {
    const QString currentCategory = ui->categoryFilterComboBox->currentText();
    QSignalBlocker blocker(ui->categoryFilterComboBox);

    ui->categoryFilterComboBox->clear();
    ui->categoryFilterComboBox->addItem(QStringLiteral("All Categories"));
    ui->categoryFilterComboBox->addItems(m_eventManager.categories());

    const int categoryIndex = ui->categoryFilterComboBox->findText(currentCategory);
    ui->categoryFilterComboBox->setCurrentIndex(categoryIndex >= 0 ? categoryIndex : 0);
}

void MainWindow::refreshMonthCalendar(const QDateTime &now) {
    if (m_monthCalendarWidget == nullptr) {
        return;
    }

    m_monthCalendarWidget->setMonthViewModel(
        m_eventManager.monthViewModel(m_monthCalendarWidget->visibleMonth(), m_query, now));
}

// Refresh every visible surface after filters, search, or data have changed.
void MainWindow::refreshVisibleData() {
    const QDateTime now = QDateTime::currentDateTime();
    autoCompleteOverdueEventsIfNeeded(now);

    m_query.searchText = ui->searchLineEdit->text();
    m_query.priorityEnabled = ui->priorityFilterComboBox->currentIndex() > 0;
    if (m_query.priorityEnabled) {
        m_query.priority = priorityFromString(ui->priorityFilterComboBox->currentText());
    }

    const QString category = ui->categoryFilterComboBox->currentText();
    m_query.category = category == QStringLiteral("All Categories") ? QString() : category;

    const QString previousAgendaOccurrenceKey = selectedAgendaListOccurrenceKey();
    const QString previousListOccurrenceKey = selectedOccurrenceKey(ui->eventTableWidget);

    m_currentAgendaListEntries = m_eventManager.agendaListEntries(now);
    m_currentListEntries = m_eventManager.listEntries(m_query, now);
    populateAgendaList(m_currentAgendaListEntries);
    populateListTable(m_currentListEntries);
    refreshMonthCalendar(now);

    {
        QSignalBlocker taskBlocker(ui->tasksListWidget);
        QSignalBlocker listBlocker(ui->eventTableWidget);

        ui->tasksListWidget->clearSelection();
        ui->eventTableWidget->clearSelection();

        if (!previousListOccurrenceKey.isEmpty()) {
            selectOccurrenceByKey(ui->eventTableWidget, previousListOccurrenceKey);
        } else if (!previousAgendaOccurrenceKey.isEmpty()) {
            selectAgendaListOccurrenceByKey(previousAgendaOccurrenceKey);
        }
    }

    refreshDetailsPanel(currentSelectedOccurrence());
    refreshTableAccentStates();
    refreshActionButtonStates();
}

MainWindow::ListTableState *MainWindow::listStateForTable(QTableWidget *table) {
    if (table == ui->eventTableWidget) {
        return &m_listTableState;
    }

    return nullptr;
}

const MainWindow::ListTableState *MainWindow::listStateForTable(const QTableWidget *table) const {
    if (table == ui->eventTableWidget) {
        return &m_listTableState;
    }

    return nullptr;
}

void MainWindow::enforceListMonthSplitterBounds() {
    if (m_listContentSplitter == nullptr || m_listMonthPanel == nullptr) {
        return;
    }

    const int totalWidth = m_listContentSplitter->width();
    if (totalWidth <= 0) {
        return;
    }

    const int handleWidth = m_listContentSplitter->handleWidth();
    const int usableWidth = qMax(0, totalWidth - handleWidth);
    const int calendarMinWidth = qMax(0, m_listMonthPanel->minimumWidth());
    const int leftMin = qMin(240, usableWidth);
    const int leftMaxByShare = static_cast<int>(usableWidth * 0.50);
    const int hardLeftCap = qMax(240, usableWidth - calendarMinWidth);
    const int preferredLeftCap = qMax(240, qMin(780, leftMaxByShare));
    int leftMax = qMin(preferredLeftCap, hardLeftCap);
    leftMax = qMax(0, leftMax);
    if (leftMax < leftMin) {
        leftMax = leftMin;
    }

    QList<int> sizes = m_listContentSplitter->sizes();
    if (sizes.size() < 2) {
        return;
    }

    const int currentLeft = sizes.at(0);
    const int targetLeft = qBound(leftMin, currentLeft, leftMax);
    const int targetRight = qMax(0, usableWidth - targetLeft);
    if (targetLeft == currentLeft && sizes.at(1) == targetRight) {
        return;
    }

    m_listContentSplitter->blockSignals(true);
    m_listContentSplitter->setSizes({targetLeft, targetRight});
    m_listContentSplitter->blockSignals(false);
}

void MainWindow::enforceContentVerticalSplitterBounds() {
    if (ui->contentVerticalSplitter == nullptr || ui->detailsFrame == nullptr || ui->mainStackedWidget == nullptr) {
        return;
    }

    const int totalHeight = ui->contentVerticalSplitter->height();
    if (totalHeight <= 0) {
        return;
    }

    const int handleHeight = ui->contentVerticalSplitter->handleWidth();
    const int usableHeight = qMax(0, totalHeight - handleHeight);
    const int topMin = qMax(TOP_CONTENT_MIN_HEIGHT, ui->mainStackedWidget->minimumHeight());
    const int bottomMin = qMax(DETAILS_MIN_HEIGHT, ui->detailsFrame->minimumHeight());

    QList<int> sizes = ui->contentVerticalSplitter->sizes();
    if (sizes.size() < 2) {
        return;
    }

    const int effectiveBottomMin = qMin(bottomMin, usableHeight);
    int targetBottom = qMax(effectiveBottomMin, sizes.at(1));
    targetBottom = qMin(targetBottom, usableHeight);
    int targetTop = qMax(0, usableHeight - targetBottom);

    if (targetTop < topMin) {
        targetTop = qMin(topMin, usableHeight);
        targetBottom = qMax(0, usableHeight - targetTop);
    }

    if (targetBottom < effectiveBottomMin && usableHeight >= effectiveBottomMin) {
        targetBottom = effectiveBottomMin;
        targetTop = qMax(0, usableHeight - targetBottom);
    }

    if (targetTop == sizes.at(0) && targetBottom == sizes.at(1)) {
        return;
    }

    ui->contentVerticalSplitter->blockSignals(true);
    ui->contentVerticalSplitter->setSizes({targetTop, targetBottom});
    ui->contentVerticalSplitter->blockSignals(false);
}

void MainWindow::refreshActionButtonStates() {
    const EventOccurrence *selectedOccurrence = currentSelectedOccurrence();
    const bool hasSelection = selectedOccurrence != nullptr;
    ui->editEventButton->setEnabled(hasSelection);
    ui->deleteEventButton->setEnabled(hasSelection);
    ui->completeEventCheckBox->setEnabled(hasSelection);
}

void MainWindow::setTimeFilter(EventTimeFilter filter) {
    m_query.timeFilter = filter;
    syncTimeFilterButtons(filter);
    refreshVisibleData();
}




