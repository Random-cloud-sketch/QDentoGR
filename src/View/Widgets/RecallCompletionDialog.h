#pragma once

#include <QDialog>
#include <optional>

#include "Model/Recall.h"
#include "Model/CalendarStructs.h"

class QRadioButton;
class QLineEdit;
class OptionalDateEdit;

//A recall appointment is marked completed: the clinician decides what happens to the next recall date
class RecallCompletionDialog : public QDialog
{
	Q_OBJECT

	QRadioButton* m_manual;
	QRadioButton* m_calculated;
	QRadioButton* m_unchanged;
	OptionalDateEdit* m_manualDate;
	QLineEdit* m_note;

	QDate m_calculatedDate;

public:
	enum class Choice { Manual, Calculated, Unchanged };

	RecallCompletionDialog(const CalendarEvent& appointment, const std::optional<Recall>& recall, QWidget* parent = nullptr);

	Choice choice() const;
	//the new next recall date of the choice (Unchanged: the current one)
	QDate nextDate() const;
	std::string note() const;
};
