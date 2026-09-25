#pragma once

#include <QWidget>
#include <QDate>

class DateEdit;
class QToolButton;

//A date which may also be "not set": a date edit with a calendar and a button which clears it
class OptionalDateEdit : public QWidget
{
	Q_OBJECT

	DateEdit* m_edit;
	QToolButton* m_clear;

	bool eventFilter(QObject* watched, QEvent* event) override;

public:
	OptionalDateEdit(QWidget* parent = nullptr);

	//an invalid date: not set
	void setDate(const QDate& date);
	QDate date() const;

signals:
	void dateChanged(const QDate& date);
};
