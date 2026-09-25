#pragma once

#include <QDialog>
#include "ui_CalendarEventDialog.h"
#include "Model/CalendarStructs.h"

class QCheckBox;

class CalendarEventDialog : public QDialog
{
	Q_OBJECT

	CalendarEvent m_result;

	//the patient chosen from the list (or of the edited appointment), while the name is not changed
	long long m_linkedRowid{ 0 };
	QString m_linkedName;

	//periodontal recall appointment (only for an appointment of a patient)
	QCheckBox* m_recallBox{ nullptr };

	bool isLinked(const QString& summary) const;
	void showPhoneError(const QString& error);

	void paintEvent(QPaintEvent* e) override;

public:
	CalendarEventDialog(const CalendarEvent& event, QWidget *parent = nullptr);
	CalendarEvent& result() { return m_result; }
	~CalendarEventDialog();

private:
	Ui::CalendarEventDialogClass ui;
};
