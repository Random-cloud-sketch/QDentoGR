#pragma once

#include <QWidget>
#include "ui_CalendarView.h"
#include "Model/CalendarStructs.h"
#include "GlobalSettings.h"
#include <QDate>
#include <QSet>

class CalendarPresenter;
class CalendarWidget;
class CalendarNavigator;
class IconButton;
class QFrame;
class QLabel;
class QPushButton;
class QTimer;

class CalendarView : public QWidget
{
    Q_OBJECT

    CalendarPresenter* presenter{ nullptr };

    CalendarWidget* calendarWidget{ nullptr };

    CalendarNavigator* navigator{ nullptr };

    QWidget* timeAxis{ nullptr };

    IconButton* axisButton{ nullptr };

    //notice shown after an appointment is moved, with undo
    QFrame* notice{ nullptr };
    QLabel* noticeLabel{ nullptr };
    QPushButton* undoButton{ nullptr };
    QTimer* noticeTimer{ nullptr };

    void showNotice(const QString& text, bool undo);
    void placeNotice();
    void resizeEvent(QResizeEvent* event) override;

    GlobalSettings::CalendarAxis m_axis;

    //last shown data, used when the time axis settings change
    std::vector<CalendarEvent> m_events;
    CalendarEvent m_clipboard;

    QDate m_weekFrom;
    QDate m_weekTo;
    QDate m_selectedDate;
    bool m_dateChosen{ false }; //the selected date was clicked in the navigator

    bool m_axisApplied{ false };
    bool m_scrollPending{ true };
    QTime m_scrollTarget; //when valid, the next scroll goes to this time instead of the working time
    int m_scrollRetries{ 0 };

    void initTable();

    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

    void showCalendarWidget();
    void showAxisSettings();

    //working day, extended so that no appointment of the week is hidden
    std::pair<int, int> visibleHours(const std::vector<CalendarEvent>& list) const;
    void applyTimeAxis(int firstHour, int lastHour);
    void scrollToWorkingTime();

public:
    CalendarView(QWidget *parent = nullptr);
    void setCalendarPresenter(CalendarPresenter* p);
    void setEventList(const std::vector<CalendarEvent>& list, const CalendarEvent& clipboard);
    void updateWeekView(QDate from, QDate to, int currentDayColumn);

    //the next time the calendar is shown it scrolls to the current time / start of the working day
    void requestScrollToWorkingTime();

    //months shown in the navigator and the days with appointments in them
    QDate navigatorFirstDay() const;
    QDate navigatorLastDay() const;
    void setBusyDays(const QSet<QDate>& days);

    void showChangeNotice(const QDateTime& start, const QDateTime& end, bool moved);
    void showUndoneNotice();
    void hideChangeNotice();

    ~CalendarView();

private:
    Ui::CalendarView ui;
};
