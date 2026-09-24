#pragma once

#include <QDialog>
#include <optional>
#include <vector>

#include "Model/MedicalHistory.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QRadioButton;
class QButtonGroup;
class QSpinBox;
class QTableWidget;
class QListWidget;
class QTextBrowser;
class QGridLayout;
class QVBoxLayout;
class QWidget;
class DateEdit;

//Medical history (anamnesis) of a patient. Every save stores a new version.
class MedicalHistoryDialog : public QDialog
{
	Q_OBJECT

	struct YesNoRow
	{
		std::string key;
		QRadioButton* no;
		QRadioButton* yes;
		QLineEdit* details;
	};

	long long m_patientRowid;
	std::optional<MedicalHistory> m_current;
	bool m_saved{ false };

	std::vector<YesNoRow> m_rows;

	QLabel* m_headerLabel;
	QLabel* m_alertLabel;

	DateEdit* m_dateEdit;
	QButtonGroup* m_healthGroup;
	QLineEdit* m_physicianEdit;
	QLineEdit* m_physicianContactEdit;

	QTableWidget* m_medicationTable;
	QPlainTextEdit* m_allergyReactionEdit;
	QTableWidget* m_surgeryTable;

	QButtonGroup* m_smokingGroup;
	QWidget* m_cigarettesRow;
	QWidget* m_smokingYearsRow;
	QWidget* m_smokingQuitRow;
	QSpinBox* m_cigarettesSpin;
	QSpinBox* m_smokingYearsSpin;
	QLineEdit* m_smokingQuitEdit;
	QPlainTextEdit* m_lifestyleOtherEdit;

	QPlainTextEdit* m_oralHygieneEdit;
	QPlainTextEdit* m_dentalOtherEdit;

	QPlainTextEdit* m_notesEdit;

	QListWidget* m_versionList;
	QTextBrowser* m_versionView;

	QWidget* createPage(QVBoxLayout*& layout);
	QGridLayout* addYesNoRows(QVBoxLayout* layout, MedicalHistoryItems::Section section, const QString& placeholder);
	QTableWidget* createTable(QVBoxLayout* layout, const QStringList& headers, const QString& addText, const QString& removeText);
	QPlainTextEdit* addTextField(QVBoxLayout* layout, const QString& label, int height);

	void setHistory(const MedicalHistory& h);
	MedicalHistory collect() const;
	bool hasUnsavedChanges() const;
	bool save();

	void loadVersions();
	void showVersion(int listIndex);
	void updateSmokingFields();

public:
	MedicalHistoryDialog(long long patientRowid, const QString& patientName, QWidget* parent = nullptr);
	bool saved() const { return m_saved; }

	void reject() override;
};
