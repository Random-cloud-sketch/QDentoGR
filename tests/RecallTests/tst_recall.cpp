#include <QtTest>

#include "Model/Recall.h"

//the rule of the recall badge, the startup notice and the filter "Due but not booked"
class RecallTests : public QObject
{
	Q_OBJECT

	const QDate today{ 2026, 9, 25 };

	static RecallListRow row(const QDate& next, bool active = true, const QDateTime& booked = {})
	{
		RecallListRow r;
		r.recall.active = active;
		r.recall.nextDate = next;
		r.bookedAppointment = booked;
		return r;
	}

	static int count(const std::vector<RecallListRow>& rows, const QDate& today, int lead)
	{
		int n = 0;
		for (auto& r : rows) n += r.needsAttention(today, lead);
		return n;
	}

private slots:

	void overdueWithoutAppointment()
	{
		QVERIFY(row(today.addDays(-10)).needsAttention(today, 0));
		//overdue recalls stay included whatever the lead time
		QVERIFY(row(today.addDays(-10)).needsAttention(today, 30));
	}

	void dueToday()
	{
		QVERIFY(row(today).needsAttention(today, 0));
	}

	void leadTimeBoundary()
	{
		QVERIFY(row(today.addDays(14)).needsAttention(today, 14));
		QVERIFY(!row(today.addDays(15)).needsAttention(today, 14));
		QVERIFY(!row(today.addDays(1)).needsAttention(today, 0));
	}

	void bookedAppointmentExcludes()
	{
		QVERIFY(!row(today.addDays(-10), true, QDateTime(today.addDays(3), QTime(10, 0))).needsAttention(today, 0));
		//an appointment of today (even if its time has passed) is booked
		QVERIFY(!row(today.addDays(-1), true, QDateTime(today, QTime(8, 0))).needsAttention(today, 0));
	}

	void inactiveOrWithoutDate()
	{
		QVERIFY(!row(today.addDays(-10), false).needsAttention(today, 0));
		QVERIFY(!row(QDate()).needsAttention(today, 365));
		QVERIFY(!row(QDate::fromString("not a date", Qt::ISODate)).needsAttention(today, 365));
	}

	void negativeLeadTimeIsZero()
	{
		QVERIFY(row(today).needsAttention(today, -5));
		QVERIFY(!row(today.addDays(1)).needsAttention(today, -5));
	}

	void countsPatients()
	{
		std::vector<RecallListRow> rows{
			row(today.addDays(-3)),									//overdue
			row(today),												//today
			row(today.addDays(10)),									//within 14 days
			row(today.addDays(20)),									//later
			row(today.addDays(-5), true, QDateTime(today.addDays(2), QTime(9, 0))),	//booked
			row(today.addDays(-5), false),							//inactive
			row(QDate())											//no date
		};

		QCOMPARE(count(rows, today, 0), 2);
		QCOMPARE(count(rows, today, 14), 3);
		QCOMPARE(count(rows, today, 30), 4);
	}

	void calculatedDateOnlyFromInterval()
	{
		Recall r;
		QVERIFY(!r.calculatedFrom(today).isValid());

		r.intervalMonths = 6;
		QCOMPARE(r.calculatedFrom(QDate(2026, 8, 31)), QDate(2027, 2, 28));
	}
};

QTEST_GUILESS_MAIN(RecallTests)
#include "tst_recall.moc"
