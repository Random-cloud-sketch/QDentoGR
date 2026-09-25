#include "CalendarView.h"

#include <QDateTime>
#include <QHeaderView>
#include <QScrollBar>
#include <QPainter>
#include <QShortcut>
#include <QTimer>
#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>

#include "Presenter/CalendarPresenter.h"
#include "View/Theme.h"
#include "View/uiComponents/CalendarWidget.h"
#include "View/uiComponents/CalendarNavigator.h"
#include "View/uiComponents/IconButton.h"
#include <QLocale>

namespace {

    constexpr int hourLabelWidth = 60;

    //Hour ruler left of the appointments table, painted at the positions of the table rows
    class TimeAxisWidget : public QWidget
    {
        CalendarTable* table;

    public:
        //space above and below the table, so the first and the last label are not cut
        static constexpr int margin = 10;

        TimeAxisWidget(CalendarTable* table, QWidget* parent) : QWidget(parent), table(table)
        {
            setFixedWidth(hourLabelWidth);
        }

        void updateSize()
        {
            int rows = (table->lastHour() - table->firstHour()) * 60 / CalendarTable::minutesPerRow;

            setFixedHeight(rows * table->unitHeight() + 2 * margin);

            update();
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter painter(this);

            QFont hourFont = font();
            hourFont.setBold(true);

            QFont slotFont = font();
            slotFont.setPointSizeF(font().pointSizeF() * 0.85);

            int rowsPerHour = 60 / CalendarTable::minutesPerRow;
            int rows = (table->lastHour() - table->firstHour()) * rowsPerHour;

            //hours are always labelled, half hours only in the 30 minute grid
            int labelStep = table->slotMinutes() == 30 ? table->rowsPerSlot() : rowsPerHour;

            for (int row = 0; row <= rows; row += labelStep)
            {
                int minutes = table->firstHour() * 60 + row * CalendarTable::minutesPerRow;

                QString text = QString("%1:%2").arg(minutes / 60, 2, 10, QChar('0')).arg(minutes % 60, 2, 10, QChar('0'));

                bool fullHour = row % rowsPerHour == 0;

                painter.setFont(fullHour ? hourFont : slotFont);
                painter.setPen(fullHour ? QColor(Qt::darkCyan) : QColor(140, 140, 140));

                int y = margin + row * table->unitHeight();

                painter.drawText(QRect(0, y - margin, width(), 2 * margin), Qt::AlignCenter, text);
            }
        }
    };
}

CalendarView::CalendarView(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    m_axis = GlobalSettings::getCalendarAxis();

    calendarWidget = new CalendarWidget();
    calendarWidget->setWindowFlag(Qt::WindowType::Popup);

    setStyleSheet(Theme::getFancyStylesheet());
    ui.calendarButton->setGraphicsEffect(nullptr);
    ui.weekFrame->setFrameShape(QFrame::NoFrame);

    auto arrow = QPixmap(":/icons/icon_downArrow.png");

    ui.prevWeekButton->setIcon(QIcon(QPixmap(arrow.transformed(QTransform().rotate(90)))));
    ui.nextWeekButton->setIcon(QIcon(QPixmap(arrow.transformed(QTransform().rotate(270)))));

    ui.currentWeekButton->setHoverColor(Theme::mainBackgroundColor);
    ui.prevWeekButton->setHoverColor(Theme::mainBackgroundColor);
    ui.nextWeekButton->setHoverColor(Theme::mainBackgroundColor);

    auto font = ui.calendarButton->font();
    font.setPointSize(font.pointSize() * 2);
    font.setBold(true);
    ui.calendarButton->setFont(font);
    ui.calendarButton->setNormalColor(Theme::background);

    initTable();

    ui.scrollArea->setStyleSheet("#scrollAreaWidgetContents{background-color: white;}");// " +  Theme::colorToString(Theme::background) + ";}");
    ui.weekFrame->setStyleSheet("QFrame{background-color: " + Theme::colorToString(Theme::background) + ";}");
    ui.line->setStyleSheet("color: " + Theme::colorToString(Theme::border) + ";");

    //multi-month navigator on the right side
    navigator = new CalendarNavigator(this);
    ui.horizontalLayout_5->addWidget(navigator);

    connect(navigator, &CalendarNavigator::dateClicked, this, [&](QDate date) {

        m_selectedDate = date;
        m_dateChosen = true;

        if (presenter) presenter->dateRequested(date);

        //the same week is not refreshed by the presenter
        updateWeekView(m_weekFrom, m_weekTo, ui.calendarTable->todayColumn());

        //the first appointment of the day (or the start of the working day) is scrolled into view
        m_scrollTarget = QTime(m_axis.startHour, 0);

        QTime first;

        for (auto& e : m_events) {
            if (e.start.date() == date && (!first.isValid() || e.start.time() < first)) {
                first = e.start.time();
            }
        }

        if (first.isValid()) m_scrollTarget = first;

        requestScrollToWorkingTime();
    });

    connect(navigator, &CalendarNavigator::monthsChanged, this, [&] { if (presenter) presenter->navigatorMonthsChanged(); });

    connect(ui.nextWeekButton, &QPushButton::clicked, this, [=] { if(presenter) presenter->nextWeekRequested(); });
    connect(ui.prevWeekButton, &QPushButton::clicked, this, [=] { presenter->prevWeekRequested(); });
    connect(ui.currentWeekButton, &QPushButton::clicked, this, [&] { presenter->currentWeekRequested(); });
    connect(ui.calendarTable, &CalendarTable::eventEditRequested, this, [&](int index) { presenter->editEvent(index);});
    connect(ui.calendarTable, &CalendarTable::eventAddRequested, this, [&](const QTime& t, int daysFromMonday, int duration) { presenter->addEvent(t, daysFromMonday, duration); });
    connect(ui.calendarTable, &CalendarTable::deleteEventRequested, this, [&](int eventIdx) { presenter->deleteEvent(eventIdx); });
    connect(ui.calendarTable, &CalendarTable::googleEventAgainRequested, this, [&](int eventIdx) { presenter->createGoogleEventAgain(eventIdx); });
    connect(ui.calendarTable, &CalendarTable::recallActionRequested, this, [&](int eventIdx, int action) { presenter->recallAction(eventIdx, action); });
    connect(ui.calendarTable, &CalendarTable::moveEventRequested, this, [&](int index) { presenter->moveEvent(index); });
    connect(ui.calendarTable, &CalendarTable::operationCanceled, this, [&] { presenter->clearClipboard(); });
    connect(ui.calendarTable, &CalendarTable::eventDurationChange, this, [&](int eventIdx, int duration) { presenter->durationChange(eventIdx, duration); });
    connect(ui.calendarButton, &QPushButton::clicked, this, [&]{ showCalendarWidget(); });
    connect(ui.calendarTable, &CalendarTable::newDocRequested, this, [&](int index, TabType type) { presenter->newDocRequested(index, type); });
    connect(ui.calendarTable, &CalendarTable::eventTimeChangeRequested, this, [&](int index, const QDateTime& start, const QDateTime& end, bool moved) {
        if (presenter) presenter->rescheduleEvent(index, start, end, moved);
    });

    //notice after drag and drop, with undo
    notice = new QFrame(this);
    notice->setObjectName("changeNotice");
    notice->setStyleSheet(
        "#changeNotice { background-color: rgba(45, 55, 60, 235); border-radius: 18px; }"
        "#changeNotice QLabel { color: white; }"
        "#changeNotice QPushButton { color: " + Theme::colorToString(Theme::mainBackgroundColor) + " font-weight: bold; background: transparent; border: none; padding: 4px 8px; }"
        "#changeNotice QPushButton:hover { color: white; }"
    );

    auto noticeLayout = new QHBoxLayout(notice);
    noticeLayout->setContentsMargins(18, 6, 10, 6);
    noticeLayout->setSpacing(12);

    notice->setMinimumHeight(36); //the rounded ends need the full height

    noticeLabel = new QLabel(notice);
    undoButton = new QPushButton(tr("Undo"), notice);
    undoButton->setCursor(Qt::PointingHandCursor);

    noticeLayout->addWidget(noticeLabel);
    noticeLayout->addWidget(undoButton);

    notice->hide();

    noticeTimer = new QTimer(this);
    noticeTimer->setSingleShot(true);
    connect(noticeTimer, &QTimer::timeout, notice, &QWidget::hide);

    connect(undoButton, &QPushButton::clicked, this, [&] {
        notice->hide();
        if (presenter) presenter->undoLastChange();
    });
    connect(calendarWidget, &QCalendarWidget::clicked, this, [&](QDate date) { if (presenter)presenter->dateRequested(date); calendarWidget->close();  });


    auto nextWeekShortcut = new QShortcut(QKeySequence(Qt::Key_Right), this);
    connect(nextWeekShortcut, &QShortcut::activated, this, [=, this] {
        if (presenter) presenter->nextWeekRequested();
    });

    auto prevWeekShortcut = new QShortcut(QKeySequence(Qt::Key_Left), this);
    connect(prevWeekShortcut, &QShortcut::activated, this, [=, this] {
        if (presenter) presenter->prevWeekRequested();
    });

    auto scrollDownShortcut = new QShortcut(QKeySequence(Qt::Key_Down), this);
    connect(scrollDownShortcut, &QShortcut::activated, this, [=, this] {
        auto scrollBar = ui.scrollArea->verticalScrollBar();
        scrollBar->setValue(scrollBar->value() + scrollBar->singleStep());
    });

    auto scrollUpShortcut = new QShortcut(QKeySequence(Qt::Key_Up), this);
    connect(scrollUpShortcut, &QShortcut::activated, this, [=, this] {
        auto scrollBar = ui.scrollArea->verticalScrollBar();
        scrollBar->setValue(scrollBar->value() - scrollBar->singleStep());
    });
}

void CalendarView::updateWeekView(QDate from, QDate to, int currentDayColumn)
{
    m_weekFrom = from;
    m_weekTo = to;

    //the selected date stays while it is in the shown week, otherwise today or the first day of the week
    if (!(m_selectedDate >= from && m_selectedDate <= to)) {
        m_selectedDate = currentDayColumn == -1 ? from : from.addDays(currentDayColumn);
        m_dateChosen = false;
    }

    calendarWidget->setSelectedDate(m_selectedDate);

    navigator->setShownWeek(from, to, m_selectedDate);

    static QString months[] =
    {
        tr("January"),
        tr("February"),
        tr("March"),
        tr("April"),
        tr("May"),
        tr("June"),
        tr("July"),
        tr("August"),
        tr("September"),
        tr("October"),
        tr("November"),
        tr("December")
    };

    int fromMonth = from.month();
    int toMonth = to.month();

    QString label;

    label += QString::number(from.day());

    if (fromMonth != toMonth) {
        label += " ";
        label += months[fromMonth - 1];
        label += " ";
    }

    label += "-";

    if (fromMonth != toMonth) {
        label += " ";
    }

    label += QString::number(to.day());
    label += " ";
    label += months[toMonth - 1];
    label += " ";
    label += QString::number(to.year());

    ui.calendarButton->setText(label);

    QLabel* dateLabels[] = {
        ui.labelMon,
        ui.labelTue,
        ui.labelWen,
        ui.labelThu,
        ui.labelFri,
        ui.labelSat,
        ui.labelSun
    };

    QString weekDay[] = {
        tr("Monday"),tr("Tuesday"),tr("Wednesday"),tr("Thursday"),tr("Friday"),tr("Saturday"),tr("Sunday")
    };

    QDate date = from;

    auto font = ui.labelMon->font();

    for (int i = 0; i < 7; i++) {

        auto& l = dateLabels[i];

        QString text = "<p style=\"text-align: center;line-height:150%\"; ><b>";

        text += weekDay[i];

        text += "</b><br>";

        //Greek interface shows Greek day/month names, otherwise the original format
        text += GlobalSettings::isGreekUi() ? QLocale(QLocale::Greek, QLocale::Greece).toString(date, "ddd d MMM yyyy") : date.toString();

        text += "</p>";

        l->setText(text);

        font.setBold(i == currentDayColumn);

        l->setFont(font);

        //the date clicked in the navigator is marked in the week
        l->setStyleSheet(m_dateChosen && date == m_selectedDate ?
            "QLabel{background-color: " + Theme::colorToString(Theme::inactiveTabBG) + "; border-radius: 8px;}"
            :
            ""
        );

        date = date.addDays(1);
    }

    ui.calendarTable->setTodayColumn(currentDayColumn);
}

void CalendarView::initTable()
{
    ui.calendarTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui.calendarTable->verticalHeader()->hide();
    ui.calendarTable->horizontalHeader()->hide();

    //hour ruler instead of the fixed hour labels
    ui.hourLayout->removeItem(ui.tableTopSpacer);
    delete ui.tableTopSpacer;
    ui.tableTopSpacer = nullptr;

    auto axis = new TimeAxisWidget(ui.calendarTable, ui.scrollAreaWidgetContents);
    timeAxis = axis;
    ui.hourLayout->addWidget(axis);
    ui.hourLayout->addStretch();

    //the table starts below the margin of the ruler, so the rows and the labels are aligned
    ui.horizontalLayout_2->removeWidget(ui.calendarTable);

    auto tableColumn = new QVBoxLayout();
    tableColumn->setContentsMargins(0, TimeAxisWidget::margin, 0, TimeAxisWidget::margin);
    tableColumn->setSpacing(0);
    tableColumn->addWidget(ui.calendarTable);
    tableColumn->addStretch();

    ui.horizontalLayout_2->addLayout(tableColumn);

    //settings of the time axis above the ruler
    axisButton = new IconButton(ui.weekSpacerBegin);
    axisButton->setIcon(QIcon(":/icons/icon_settings.png"));
    axisButton->setFixedSize(30, 30);
    axisButton->setHoverColor(Theme::mainBackgroundColor);
    axisButton->setToolTip(tr("Working hours and time slots of the calendar"));

    ui.weekSpacerBegin->setText("");
    ui.weekSpacerBegin->setFixedWidth(hourLabelWidth);

    auto buttonLayout = new QHBoxLayout(ui.weekSpacerBegin);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addWidget(axisButton, 0, Qt::AlignCenter);

    connect(axisButton, &QPushButton::clicked, this, [&] { showAxisSettings(); });

    ui.weekSpacerEnd->changeSize(16, 10);

    auto [firstHour, lastHour] = visibleHours({});

    applyTimeAxis(firstHour, lastHour);
}

std::pair<int, int> CalendarView::visibleHours(const std::vector<CalendarEvent>& list) const
{
    int firstHour = m_axis.fullDay ? 0 : m_axis.startHour;
    int lastHour = m_axis.fullDay ? 24 : m_axis.endHour;

    for (auto& e : list)
    {
        firstHour = std::min(firstHour, e.start.time().hour());

        int endMinute = e.end.date() > e.start.date() ?
            24 * 60
            :
            e.end.time().hour() * 60 + e.end.time().minute();

        lastHour = std::max(lastHour, (endMinute + 59) / 60);
    }

    return { std::max(firstHour, 0), std::min(lastHour, 24) };
}

void CalendarView::applyTimeAxis(int firstHour, int lastHour)
{
    ui.calendarTable->setTimeAxis(firstHour, lastHour, m_axis.slotMinutes);

    static_cast<TimeAxisWidget*>(timeAxis)->updateSize();

    m_axisApplied = true;
}

void CalendarView::requestScrollToWorkingTime()
{
    m_scrollPending = true;

    if (isVisible()) {
        QTimer::singleShot(0, this, [this] { scrollToWorkingTime(); });
    }
}

void CalendarView::scrollToWorkingTime()
{
    if (!m_scrollPending) return;

    auto scrollBar = ui.scrollArea->verticalScrollBar();

    auto table = ui.calendarTable;

    //the scroll range follows the new height of the table only after the layout is updated
    int expectedMaximum = std::max(0, table->height() + 2 * TimeAxisWidget::margin - ui.scrollArea->viewport()->height());

    if (scrollBar->maximum() != expectedMaximum && m_scrollRetries < 20) {
        m_scrollRetries++;
        QTimer::singleShot(25, this, [this] { scrollToWorkingTime(); });
        return;
    }

    m_scrollPending = false;
    m_scrollRetries = 0;

    QTime now = QTime::currentTime();

    int y = 0;

    if (m_scrollTarget.isValid()) {
        y = std::max(0, table->timeToY(m_scrollTarget) - table->unitHeight() * 2);
        m_scrollTarget = QTime();
        scrollBar->setValue(y);
        return;
    }

    //today inside the working day: the current time, otherwise the start of the working day
    if (table->todayColumn() != -1 && now.hour() >= m_axis.startHour && now.hour() < m_axis.endHour && table->timeToY(now) >= 0) {
        y = table->timeToY(now) - table->unitHeight() * 4;
    }
    else if (table->timeToY(QTime(m_axis.startHour, 0)) >= 0) {
        y = table->timeToY(QTime(m_axis.startHour, 0));
    }

    scrollBar->setValue(std::max(0, y));
}

void CalendarView::showAxisSettings()
{
    auto popup = new QFrame(this, Qt::Popup);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setObjectName("axisSettings");
    popup->setStyleSheet(
        "#axisSettings{background-color: white; border: 1px solid " + Theme::colorToString(Theme::border) + "; border-radius: 6px;}"
    );

    auto form = new QFormLayout(popup);
    form->setContentsMargins(14, 12, 14, 12);
    form->setVerticalSpacing(8);

    auto title = new QLabel(tr("Calendar hours"), popup);
    title->setStyleSheet("font-weight: bold; color: " + Theme::colorToString(Theme::fontTurquoise) + ";");
    form->addRow(title);

    auto hourText = [](int hour) { return QString("%1:00").arg(hour, 2, 10, QChar('0')); };

    auto startCombo = new QComboBox(popup);
    for (int h = 0; h <= 23; h++) startCombo->addItem(hourText(h), h);
    startCombo->setCurrentIndex(m_axis.startHour);

    auto endCombo = new QComboBox(popup);
    for (int h = 1; h <= 24; h++) endCombo->addItem(hourText(h), h);
    endCombo->setCurrentIndex(m_axis.endHour - 1);

    auto slotCombo = new QComboBox(popup);
    slotCombo->addItem(tr("15 minutes"), 15);
    slotCombo->addItem(tr("30 minutes"), 30);
    slotCombo->addItem(tr("60 minutes"), 60);
    slotCombo->setCurrentIndex(slotCombo->findData(m_axis.slotMinutes));

    auto fullDayCheck = new QCheckBox(tr("Show all 24 hours"), popup);
    fullDayCheck->setChecked(m_axis.fullDay);

    auto hint = new QLabel(tr("Appointments outside the working hours are always shown."), popup);
    hint->setWordWrap(true);
    hint->setStyleSheet("color: gray;");

    form->addRow(tr("Start of working day:"), startCombo);
    form->addRow(tr("End of working day:"), endCombo);
    form->addRow(tr("Time slot:"), slotCombo);
    form->addRow(fullDayCheck);
    form->addRow(hint);

    auto apply = [=, this](QComboBox* changed) {

        int start = startCombo->currentData().toInt();
        int end = endCombo->currentData().toInt();

        //the end of the day is always after the start
        if (end <= start) {
            if (changed == endCombo) {
                start = end - 1;
                QSignalBlocker b(startCombo);
                startCombo->setCurrentIndex(start);
            }
            else {
                end = start + 1;
                QSignalBlocker b(endCombo);
                endCombo->setCurrentIndex(end - 1);
            }
        }

        m_axis.startHour = start;
        m_axis.endHour = end;
        m_axis.slotMinutes = slotCombo->currentData().toInt();
        m_axis.fullDay = fullDayCheck->isChecked();

        GlobalSettings::setCalendarAxis(m_axis);

        m_axisApplied = false;

        setEventList(m_events, m_clipboard);

        requestScrollToWorkingTime();
    };

    connect(startCombo, &QComboBox::currentIndexChanged, popup, [=] { apply(startCombo); });
    connect(endCombo, &QComboBox::currentIndexChanged, popup, [=] { apply(endCombo); });
    connect(slotCombo, &QComboBox::currentIndexChanged, popup, [=] { apply(slotCombo); });
    connect(fullDayCheck, &QCheckBox::toggled, popup, [=] { apply(nullptr); });

    popup->adjustSize();
    popup->move(axisButton->mapToGlobal(QPoint(0, axisButton->height() + 4)));
    popup->show();
}

void CalendarView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), Theme::background);
}

void CalendarView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (m_scrollPending) {
        QTimer::singleShot(0, this, [this] { scrollToWorkingTime(); });
    }
}

void CalendarView::showCalendarWidget()
{
    auto pos = QCursor::pos();

    calendarWidget->move(pos);

    calendarWidget->show();
}

void CalendarView::setCalendarPresenter(CalendarPresenter* p)
{
    presenter = p;
}

void CalendarView::setEventList(const std::vector<CalendarEvent>& list, const CalendarEvent& clipboard_event)
{
    m_events = list;
    m_clipboard = clipboard_event;

    auto [firstHour, lastHour] = visibleHours(list);

    auto table = ui.calendarTable;

    bool axisChanged = !m_axisApplied ||
        firstHour != table->firstHour() ||
        lastHour != table->lastHour() ||
        m_axis.slotMinutes != table->slotMinutes();

    if (axisChanged) {
        applyTimeAxis(firstHour, lastHour);
    }

    table->setEvents(list, clipboard_event);

    timeAxis->update();
}

void CalendarView::showNotice(const QString& text, bool undo, int milliseconds)
{
    noticeLabel->setText(text);
    undoButton->setVisible(undo);

    //the whole text is always shown
    notice->ensurePolished();
    noticeLabel->ensurePolished();
    noticeLabel->setMinimumWidth(noticeLabel->fontMetrics().horizontalAdvance(text) + 6);

    //the bold font of the button comes from the style sheet, it has to be applied before measuring
    undoButton->ensurePolished();
    undoButton->setMinimumWidth(undoButton->fontMetrics().horizontalAdvance(undoButton->text()) + 20);

    //equal padding when there is no undo button
    notice->layout()->setContentsMargins(18, 6, undo ? 10 : 18, 6);

    //the size follows the new text and the shown / hidden undo button
    notice->layout()->invalidate();
    notice->layout()->activate();
    notice->resize(notice->layout()->sizeHint().expandedTo(notice->minimumSize()));
    placeNotice();
    notice->raise();
    notice->show();

    //long enough to use the undo
    noticeTimer->start(milliseconds ? milliseconds : undo ? 15000 : 3000);
}

void CalendarView::placeNotice()
{
    if (!notice) return;

    //bottom center of the appointments table
    auto area = ui.scrollArea->geometry();
    QPoint bottomCenter = ui.scrollArea->parentWidget()->mapTo(this, QPoint(area.center().x(), area.bottom()));

    notice->move(bottomCenter.x() - notice->width() / 2, bottomCenter.y() - notice->height() - 24);
}

void CalendarView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    if (notice && notice->isVisible()) placeNotice();
}

QString CalendarView::overlapText(int shortened, int removed) const
{
    QStringList parts;

    if (shortened == 1) parts << tr("1 overlapping appointment was shortened");
    if (shortened > 1) parts << tr("%1 overlapping appointments were shortened").arg(shortened);
    if (removed == 1) parts << tr("1 fully covered appointment was deleted");
    if (removed > 1) parts << tr("%1 fully covered appointments were deleted").arg(removed);

    return parts.join(", ");
}

void CalendarView::showOverlapNotice(int shortened, int removed)
{
    QString text = overlapText(shortened, removed);

    if (text.size()) showNotice(text, false, 6000);
}

void CalendarView::showChangeNotice(const QDateTime& start, const QDateTime& end, bool moved, int shortened, int removed)
{
    QLocale locale = GlobalSettings::isGreekUi() ? QLocale(QLocale::Greek, QLocale::Greece) : QLocale();

    QString endText = end.time() == QTime(0, 0) && end.date() > start.date() ? QString("24:00") : end.toString("HH:mm");

    QString text = moved ?
        tr("Appointment moved to %1 at %2").arg(locale.toString(start.date(), "ddd d/M")).arg(start.toString("HH:mm"))
        :
        tr("Appointment duration changed: %1 - %2").arg(start.toString("HH:mm")).arg(endText);

    QString overlaps = overlapText(shortened, removed);

    if (overlaps.size()) text += " - " + overlaps;

    showNotice(text, true);
}

void CalendarView::showSyncNotice(const QString& text)
{
    showNotice(text, false, 8000);
}

void CalendarView::showUndoneNotice()
{
    showNotice(tr("The change was undone"), false);
}

void CalendarView::hideChangeNotice()
{
    noticeTimer->stop();
    notice->hide();
}

QDate CalendarView::navigatorFirstDay() const
{
    return navigator->firstDay();
}

QDate CalendarView::navigatorLastDay() const
{
    return navigator->lastDay();
}

void CalendarView::setBusyDays(const QSet<QDate>& days)
{
    navigator->setBusyDays(days);
}

CalendarView::~CalendarView()
{
    delete calendarWidget;
}
