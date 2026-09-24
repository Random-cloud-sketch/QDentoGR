#pragma once

#include <QWidget>
#include <QDate>
#include <QSet>
#include <vector>

class QLabel;
class IconButton;
class NavigatorMonthGrid;

//Compact multi-month calendar next to the appointments calendar.
//Shows the week that is currently shown, the selected date and the days with appointments.
class CalendarNavigator : public QWidget
{
	Q_OBJECT

public:
	//what the month grids show
	struct Marks
	{
		QDate weekFrom;
		QDate weekTo;
		QDate selected;
		QSet<QDate> busyDays;
	};

private:
	static constexpr int monthCount = 3;

	QDate m_firstMonth; //first day of the first shown month
	Marks m_marks;

	IconButton* prevButton;
	IconButton* nextButton;
	QLabel* rangeLabel;
	std::vector<QLabel*> monthLabels;
	std::vector<NavigatorMonthGrid*> months;

	void showMonths(QDate firstMonth);
	void refreshMarks();

	void paintEvent(QPaintEvent* event) override;

public:
	CalendarNavigator(QWidget* parent = nullptr);

	//the week shown in the appointments calendar and the selected date inside it
	void setShownWeek(QDate from, QDate to, QDate selected);

	//days which have appointments (marked with a dot)
	void setBusyDays(const QSet<QDate>& days);

	QDate firstDay() const;
	QDate lastDay() const;

signals:
	void dateClicked(QDate date);
	void monthsChanged();
};
