#include "CalendarPresenter.h"
#include "View/Widgets/CalendarView.h"
#include "Database/DbDentist.h"
#include "Database/DbPatient.h"
#include "Model/User.h"
#include "View/Widgets/TabView.h"
#include "Database/DbAppointment.h"
#include "View/Widgets/CalendarEventDialog.h"
#include "Presenter/TabPresenter.h"
#include "Presenter/PatientDialogPresenter.h"
#include "Model/TableRows.h"

CalendarPresenter::CalendarPresenter(TabView* tabView) :
    TabInstance(tabView, TabType::Calendar, nullptr),
    view(tabView->calendarView())
{
    view->setCalendarPresenter(this);

    shownWeek = getTodaysWeek();

    //opening the calendar shows the current time / the start of the working day
    view->requestScrollToWorkingTime();

    refreshView();

}

void CalendarPresenter::newAppointment(const CalendarEvent& event)
{
    clipboard_event = event;

    view->setEventList(events, clipboard_event);
}

void CalendarPresenter::setDataToView()
{
    view->setCalendarPresenter(this);
}

TabName CalendarPresenter::getTabName()
{
    return TabName{
        .header = QObject::tr("Appointments").toStdString(),
        .footer = "",
        .header_icon = CommonIcon::CALENDAR
        
    };
}

void CalendarPresenter::nextWeekRequested()
{
    shownWeek.first = shownWeek.first.addDays(7);
    shownWeek.second = shownWeek.second.addDays(7);

    refreshView();
}

void CalendarPresenter::prevWeekRequested()
{
    shownWeek.first = shownWeek.first.addDays(-7);
    shownWeek.second = shownWeek.second.addDays(-7);

    refreshView();
}

void CalendarPresenter::dateRequested(QDate date)
{
    if (date.year() == shownWeek.first.year() &&
        date.weekNumber() == shownWeek.first.weekNumber()) {

        return;
    }

    shownWeek.first = date.addDays(-date.dayOfWeek() + 1);
    shownWeek.second = date.addDays(-(date.dayOfWeek() - 7));

    refreshView();
}


void CalendarPresenter::currentWeekRequested()
{
    auto todaysWeek = getTodaysWeek();

    if (todaysWeek == shownWeek) return;

    shownWeek = todaysWeek;

    refreshView();
}

void CalendarPresenter::moveEvent(int index)
{
    clearUndo();

    setClipboard(events[index]);
}

void CalendarPresenter::newDocRequested(int index, TabType type)
{
    clearUndo();

    auto& event = events[index];

    if (type == TabType::Calendar) {

        CalendarEvent newEvent;

        newEvent.summary = event.summary;
        newEvent.patient_rowid = event.patient_rowid;

        setClipboard(newEvent);

        return;
    }

    RowInstance tab(type);

    tab.patientRowId = event.patient_rowid;

    if (!tab.patientRowId) {
        
        PatientDialogPresenter d(QObject::tr("New Patient").toStdString(), event.summary);

        auto result = d.open();

        if (!result) return;

        tab.patientRowId = result->rowid;

        //the appointment is linked to the patient, so next time the patient opens directly
        event.patient_rowid = result->rowid;
        DbAppointment::update(event);
    }

    TabPresenter::get().open(tab, true);

}

void CalendarPresenter::addEvent(const QTime& t, int daysFromMonday, int duration)
{
    clearUndo();

    QDateTime from(shownWeek.first.addDays(daysFromMonday), t);
    QDateTime to(from.addSecs(duration * 60));

    clipboard_event.start = from;
    clipboard_event.end = to;

    if (clipboard_event.rowid) { //existing event
        DbAppointment::update(clipboard_event);
        clipboard_event = CalendarEvent{};
        refreshView();
        return;
    }

    CalendarEventDialog d(clipboard_event);

    if (d.exec() != QDialog::Accepted) return;

    DbAppointment::insert(d.result(), User::dentist().rowID);

    refreshView();
}

void CalendarPresenter::editEvent(int index)
{
    clearUndo();

    auto& e = events[index];

    CalendarEventDialog d(e);

    if (d.exec() != QDialog::Accepted) return;

    DbAppointment::update(d.result());

    refreshView();
}

void CalendarPresenter::deleteEvent(int index)
{
    clearUndo();

    DbAppointment::remove(events[index].rowid);

    refreshView();
}

void CalendarPresenter::clearClipboard()
{
    clipboard_event = CalendarEvent{};

    view->setEventList(events, clipboard_event);
}

void CalendarPresenter::durationChange(int eventIdx, int duration)
{
    clearUndo();

    auto& event = events[eventIdx];

    event.end = event.start.addSecs(duration * 60);

    DbAppointment::update(event);

    refreshView();
}

void CalendarPresenter::cancelMove()
{
    clearClipboard();
}

CalendarPresenter::~CalendarPresenter()
{}

std::pair<QDate, QDate> CalendarPresenter::getTodaysWeek()
{
    auto currentDate = QDate::currentDate();
    int dayOfWeek = currentDate.dayOfWeek();
    return std::make_pair(currentDate.addDays(1 - dayOfWeek), currentDate.addDays(7 - dayOfWeek));
}

void CalendarPresenter::refreshView()
{
    view->updateWeekView(shownWeek.first, shownWeek.second, getCurrentDayColumn());

    events = DbAppointment::get(shownWeek.first, shownWeek.second, User::dentist().rowID);

    view->setEventList(events, clipboard_event);

    refreshBusyDays();
}

void CalendarPresenter::rescheduleEvent(int index, const QDateTime& start, const QDateTime& end, bool moved)
{
    if (index < 0 || index >= int(events.size())) return;

    auto& e = events[index];

    //nothing is changed when the new time is not valid
    if (!start.isValid() || !end.isValid() || end <= start) return;

    UndoEntry undo{ e.rowid, e.start, e.end };

    if (!DbAppointment::updateTime(e.rowid, start, end)) return;

    m_undo = undo;

    refreshView();

    view->showChangeNotice(start, end, moved);
}

void CalendarPresenter::undoLastChange()
{
    if (!m_undo.rowid) return;

    auto undo = m_undo;

    m_undo = UndoEntry{};

    //only the date and time of the appointment are restored
    DbAppointment::updateTime(undo.rowid, undo.start, undo.end);

    refreshView();

    view->showUndoneNotice();
}

void CalendarPresenter::clearUndo()
{
    if (!m_undo.rowid) return;

    m_undo = UndoEntry{};

    view->hideChangeNotice();
}

void CalendarPresenter::navigatorMonthsChanged()
{
    refreshBusyDays();
}

void CalendarPresenter::refreshBusyDays()
{
    QSet<QDate> days;

    for (auto& e : DbAppointment::get(view->navigatorFirstDay(), view->navigatorLastDay(), User::dentist().rowID)) {
        days.insert(e.start.date());
    }

    view->setBusyDays(days);
}

void CalendarPresenter::setClipboard(const CalendarEvent& e)
{
    clipboard_event = e;

    view->setEventList(events, clipboard_event);
}

int CalendarPresenter::getCurrentDayColumn()
{
    QDate currentDate = QDate::currentDate();

    if (currentDate >= shownWeek.first && currentDate <= shownWeek.second) {
        return currentDate.dayOfWeek() - 1;
    }

    return -1;
}