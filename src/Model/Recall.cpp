#include "Recall.h"

#include <QCoreApplication>
#include <algorithm>

QDate Recall::calculatedFrom(const QDate& start) const
{
	if (!start.isValid() || intervalMonths <= 0) return {};

	return start.addMonths(intervalMonths);
}

bool RecallListRow::needsAttention(const QDate& today, int leadDays) const
{
	return recall.active &&
		recall.nextDate.isValid() &&
		recall.nextDate <= today.addDays(std::max(0, leadDays)) &&
		!bookedAppointment.isValid();
}

QString RecallText::date(const QDate& date)
{
	return date.isValid() ? date.toString("dd/MM/yyyy") : QString("—");
}

QString RecallText::dateTime(const QDateTime& dateTime)
{
	return dateTime.isValid() ? dateTime.toString("dd/MM/yyyy HH:mm") : QString("—");
}

QString RecallText::interval(int months)
{
	if (months <= 0) return QCoreApplication::translate("RecallText", "No interval");
	if (months == 1) return QCoreApplication::translate("RecallText", "1 month");

	return QCoreApplication::translate("RecallText", "%1 months").arg(months);
}

QString RecallText::withinDays(int days)
{
	return days == 1 ?
		QCoreApplication::translate("RecallText", "1 day (within)") :
		QCoreApplication::translate("RecallText", "%1 days (within)").arg(days);
}

QString RecallText::eventName(const std::string& event)
{
	if (event == "activated") return QCoreApplication::translate("RecallText", "Recall activated");
	if (event == "deactivated") return QCoreApplication::translate("RecallText", "Recall deactivated");
	if (event == "date_changed") return QCoreApplication::translate("RecallText", "Next recall date changed");
	if (event == "interval_changed") return QCoreApplication::translate("RecallText", "Recall interval changed");
	if (event == "last_date_changed") return QCoreApplication::translate("RecallText", "Last recall date changed");
	if (event == "notes_changed") return QCoreApplication::translate("RecallText", "Recall notes changed");
	if (event == "note") return QCoreApplication::translate("RecallText", "Clinician note");
	if (event == "booked") return QCoreApplication::translate("RecallText", "Recall appointment booked");
	if (event == "rescheduled") return QCoreApplication::translate("RecallText", "Recall appointment rescheduled");
	if (event == "cancelled") return QCoreApplication::translate("RecallText", "Recall appointment cancelled");
	if (event == "unmarked") return QCoreApplication::translate("RecallText", "Appointment no longer a recall appointment");
	if (event == "completed") return QCoreApplication::translate("RecallText", "Recall visit completed");
	if (event == "missed") return QCoreApplication::translate("RecallText", "Recall appointment missed");
	if (event == "status_reset") return QCoreApplication::translate("RecallText", "Recall appointment set back to scheduled");

	return QString::fromStdString(event);
}

//a stored value in the local format (dates / date-times), other values unchanged
static QString valueText(const std::string& value)
{
	auto text = QString::fromStdString(value);

	if (text.isEmpty()) return {};

	if (text.size() == 10) {
		auto d = QDate::fromString(text, Qt::ISODate);
		if (d.isValid()) return RecallText::date(d);
	}

	auto dt = QDateTime::fromString(text, Qt::ISODate);
	if (dt.isValid() && text.size() > 10) return RecallText::dateTime(dt);

	return text;
}

QString RecallText::details(const RecallHistoryEntry& e)
{
	QString value = valueText(e.value);
	QString previous = valueText(e.previousValue);

	QString text;

	if (e.event == "date_changed" || e.event == "last_date_changed")
	{
		text = (previous.isEmpty() ? QCoreApplication::translate("RecallText", "not set") : previous) + " → " + (value.isEmpty() ? QCoreApplication::translate("RecallText", "not set") : value);
	}
	else if (e.event == "interval_changed")
	{
		text = interval(QString::fromStdString(e.previousValue).toInt()) + " → " + interval(QString::fromStdString(e.value).toInt());
	}
	else if (e.event == "rescheduled")
	{
		text = previous + " → " + value;
	}
	else if (e.event == "notes_changed")
	{
		text = value.isEmpty() ? QCoreApplication::translate("RecallText", "(no notes)") : value;
	}
	else
	{
		text = value;
	}

	if (e.note.size()) {
		text += (text.isEmpty() ? "" : " - ") + QString::fromStdString(e.note);
	}

	return text;
}

QString RecallText::appointmentStatus(const std::string& recallStatus)
{
	if (recallStatus == "completed") return QCoreApplication::translate("RecallText", "Completed");
	if (recallStatus == "missed") return QCoreApplication::translate("RecallText", "Missed");

	return QCoreApplication::translate("RecallText", "Scheduled");
}
