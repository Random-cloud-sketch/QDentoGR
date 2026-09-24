#pragma once

#include <QWidget>
#include <QDate>
#include <QSet>
#include <vector>

class QLabel;
class QCalendarWidget;
class IconButton;
class QAbstractItemView;

//Compact multi-month calendar next to the appointments calendar.
//Shows the week that is currently shown, the selected date and the days with appointments.
class CalendarNavigator : public QWidget
{
	Q_OBJECT

	static constexpr int monthCount = 3;

	QDate m_firstMonth; //first day of the first shown month
	QDate m_weekFrom;
	QDate m_weekTo;
	QDate m_selected;
	QSet<QDate> m_busyDays;

	IconButton* prevButton;
	IconButton* nextButton;
	QLabel* rangeLabel;
	std::vector<QLabel*> monthLabels;
	std::vector<QCalendarWidget*> months;
	std::vector<QAbstractItemView*> monthViews;

	void showMonths(QDate firstMonth);
	void refreshFormats();

	void paintEvent(QPaintEvent* event) override;

public:
	CalendarNavigator(QWidget* parent = nullptr);

	//the week shown in the appointments calendar and the selected date inside it
	void setShownWeek(QDate from, QDate to, QDate selected);

	//days which have appointments (shown in bold)
	void setBusyDays(const QSet<QDate>& days);

	QDate firstDay() const;
	QDate lastDay() const;

signals:
	void dateClicked(QDate date);
	void monthsChanged();
};
