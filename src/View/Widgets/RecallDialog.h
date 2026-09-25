#pragma once

#include <QDialog>
#include <optional>

#include "Model/Recall.h"

class QCheckBox;
class QComboBox;
class QSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class OptionalDateEdit;

//The periodontal recall of one patient: tracking on / off, next recall date (only the clinician sets it),
//interval, last recall, notes, the recall appointments and the history of all changes
class RecallDialog : public QDialog
{
	Q_OBJECT

	long long m_patient;
	std::optional<Recall> m_saved;
	bool m_bookRequested{ false };
	bool m_changed{ false };

	QCheckBox* m_active;
	OptionalDateEdit* m_nextDate;
	QComboBox* m_interval;
	QSpinBox* m_customInterval;
	QPushButton* m_calculateButton;
	OptionalDateEdit* m_lastDate;
	QLineEdit* m_notes;
	QLabel* m_warning;
	QTableWidget* m_appointments;
	QTableWidget* m_history;

	int intervalMonths() const;
	void setIntervalMonths(int months);
	void updateCalculateMenu();

	Recall current() const;
	bool saveChanges();

	void loadAppointments();
	void loadHistory();

public:
	//activateNew: a patient without a recall gets an active recall (e.g. added from the recall list)
	RecallDialog(long long patient_rowid, bool activateNew = false, QWidget* parent = nullptr);

	//the clinician wants to book a recall appointment in the calendar (the dialog was saved)
	bool bookRequested() const { return m_bookRequested; }
	//the recall was saved (or a note was added)
	bool changed() const { return m_changed; }
};
