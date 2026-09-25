#include "RecallPresenter.h"

#include <QMessageBox>
#include <QApplication>

#include "Database/DbPatient.h"
#include "Database/DbRecall.h"
#include "Model/TableRows.h"
#include "Presenter/TabPresenter.h"
#include "View/Widgets/TabView.h"
#include "View/Widgets/RecallView.h"
#include "View/Widgets/RecallDialog.h"
#include "View/Widgets/RecallCompletionDialog.h"
#include "Presenter/RecallNotifier.h"

RecallPresenter::RecallPresenter(TabView* tabView) :
	TabInstance(tabView, TabType::Recall, nullptr),
	view(tabView->recallView())
{
	view->setPresenter(this);

	m_notifierConnection = QObject::connect(&RecallNotifier::get(), &RecallNotifier::changed, [this] {
		if (isCurrent()) refresh();
	});
}

void RecallPresenter::setDataToView()
{
	view->setPresenter(this);

	//the recalls may have been changed elsewhere (patient, calendar)
	refresh();
}

TabName RecallPresenter::getTabName()
{
	return TabName{
		.header = QObject::tr("Periodontal recall").toStdString(),
		.footer = "",
		.header_icon = CommonIcon::RECALL
	};
}

bool RecallPresenter::matches(const RecallListRow& row, Filter filter, const QDate& today, int leadDays)
{
	auto& r = row.recall;
	auto& next = r.nextDate;

	if (filter == Filter::All) return true;
	if (filter == Filter::Inactive) return !r.active;
	if (!r.active) return false;

	switch (filter)
	{
		case Filter::Active: return true;
		case Filter::DueToday: return next.isValid() && next == today;
		case Filter::Next7: return next.isValid() && next >= today && next <= today.addDays(7);
		case Filter::Next30: return next.isValid() && next >= today && next <= today.addDays(30);
		case Filter::Next90: return next.isValid() && next >= today && next <= today.addDays(90);
		case Filter::Overdue: return next.isValid() && next < today;
		//due (overdue, today or within the lead time) and without a booked recall appointment
		case Filter::DueNotBooked: return row.needsAttention(today, leadDays);
		default: return false;
	}
}

QString RecallPresenter::searchable(const QString& text)
{
	auto decomposed = text.normalized(QString::NormalizationForm_D);

	QString result;

	for (auto c : decomposed) {
		if (c.category() != QChar::Mark_NonSpacing) result += c;
	}

	return result.toCaseFolded();
}

bool RecallPresenter::matchesSearch(const RecallListRow& row, const QString& search)
{
	auto s = searchable(search.trimmed());

	if (s.isEmpty()) return true;

	return searchable(QString::fromStdString(row.patientName)).contains(s) ||
		searchable(QString::fromStdString(row.patientId)).contains(s);
}

void RecallPresenter::refresh()
{
	auto today = QDate::currentDate();
	auto rows = DbRecall::list(today);
	int lead = RecallNotifier::get().leadDays();

	std::vector<int> counts(int(Filter::Count), 0);

	for (auto& row : rows) {
		for (int f = 0; f < int(Filter::Count); f++) {
			if (matches(row, Filter(f), today, lead)) counts[f]++;
		}
	}

	std::vector<RecallListRow> shown;

	for (auto& row : rows) {
		if (matches(row, view->filter(), today, lead) && matchesSearch(row, view->searchText())) shown.push_back(row);
	}

	//the next recall first (rows without a date at the end)
	std::stable_sort(shown.begin(), shown.end(), [](const RecallListRow& a, const RecallListRow& b) {
		if (a.recall.nextDate.isValid() != b.recall.nextDate.isValid()) return a.recall.nextDate.isValid();
		if (a.recall.nextDate != b.recall.nextDate) return a.recall.nextDate < b.recall.nextDate;
		return a.patientName < b.patientName;
	});

	view->setRows(shown, counts, lead);
}

void RecallPresenter::showFilter(Filter filter)
{
	view->setFilter(filter);

	refresh();
}

void RecallPresenter::addPatientRequested()
{
	auto rowid = view->choosePatient();

	if (!rowid) return;

	RecallActions::editRecall(rowid, true);

	refresh();
}

void RecallPresenter::editRequested(long long patient_rowid)
{
	//a saved change refreshes the list through the notifier
	if (!RecallActions::editRecall(patient_rowid) && isCurrent()) refresh();
}

void RecallPresenter::bookRequested(long long patient_rowid)
{
	RecallActions::bookAppointment(patient_rowid);
}

void RecallPresenter::openPatientRequested(long long patient_rowid)
{
	RecallActions::openPatient(patient_rowid);
}

RecallPresenter::~RecallPresenter()
{
	QObject::disconnect(m_notifierConnection);

	if (view->presenter() == this) view->setPresenter(nullptr);
}

// ---------------------------------------------------------------- actions

bool RecallActions::editRecall(long long patient_rowid, bool activateNew)
{
	if (patient_rowid <= 0) return false;

	RecallDialog d(patient_rowid, activateNew, QApplication::activeWindow());
	d.exec();

	if (d.changed()) RecallNotifier::get().refresh();

	if (d.bookRequested()) bookAppointment(patient_rowid);

	return d.changed();
}

void RecallActions::bookAppointment(long long patient_rowid)
{
	auto patient = DbPatient::get(patient_rowid);

	if (!patient.rowid) return;

	CalendarEvent event(patient);
	event.recall = true;

	//the calendar opens at the week of the next recall (the time is chosen by the clinician)
	auto recall = DbRecall::get(patient_rowid);

	QDate week = recall && recall->nextDate.isValid() && recall->nextDate >= QDate::currentDate() ?
		recall->nextDate : QDate();

	TabPresenter::get().openCalendar(event, week);
}

void RecallActions::openPatient(long long patient_rowid)
{
	if (patient_rowid <= 0) return;

	RowInstance row(TabType::PatientSummary);
	row.patientRowId = patient_rowid;

	TabPresenter::get().open(row, true);
}

static void saveError()
{
	QMessageBox::warning(QApplication::activeWindow(), QObject::tr("Periodontal recall"),
		QObject::tr("The change could not be saved."));
}

bool RecallActions::completeAppointment(const CalendarEvent& appointment)
{
	if (!appointment.recall || !appointment.patient_rowid) return false;

	auto recall = DbRecall::get(appointment.patient_rowid);

	RecallCompletionDialog d(appointment, recall, QApplication::activeWindow());

	if (d.exec() != QDialog::Accepted) return false;

	if (!DbRecall::setAppointmentStatus(appointment, "completed", d.note())) {
		saveError();
		return false;
	}

	//the last recall is the completed visit; the next date only as the clinician chose
	Recall r = recall ? *recall : Recall{};
	r.patient_rowid = appointment.patient_rowid;

	if (!recall) r.active = true;

	auto visitDate = appointment.start.date();

	if (!r.lastDate.isValid() || r.lastDate < visitDate) r.lastDate = visitDate;

	if (d.choice() != RecallCompletionDialog::Choice::Unchanged) r.nextDate = d.nextDate();

	auto reason = QObject::tr("Recall visit of %1 completed").arg(RecallText::date(visitDate)).toStdString();

	if (!DbRecall::save(r, recall, reason)) {
		saveError();
	}

	RecallNotifier::get().refresh();

	return true;
}

bool RecallActions::setAppointmentStatus(const CalendarEvent& appointment, const std::string& status)
{
	if (!appointment.recall || !appointment.patient_rowid) return false;

	if (!DbRecall::setAppointmentStatus(appointment, status)) {
		saveError();
		return false;
	}

	RecallNotifier::get().refresh();

	return true;
}

bool RecallActions::setRecallAppointment(const CalendarEvent& appointment, bool recall)
{
	if (!appointment.patient_rowid) return false;

	if (!DbRecall::setRecallAppointment(appointment.rowid, recall)) {
		saveError();
		return false;
	}

	RecallNotifier::get().refresh();

	return true;
}
