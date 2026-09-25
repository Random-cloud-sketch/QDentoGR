#pragma once

#include <QDialog>
#include <optional>
#include <vector>

#include "Model/MedicalHistory.h"

class QAbstractButton;
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

	//A control whose state can be a positive finding of its section (sidebar item / page).
	//Only the controls registered here are evaluated: negative answers, administrative fields
	//(date, physician) and descriptive notes are never findings.
	struct Finding
	{
		enum Kind {
			Answer,	//positive when the button is checked ("Yes", "Poor" health, "Smoker")
			Text,	//positive when the field contains non-whitespace text
			Table	//positive when a row contains text (every such row is highlighted)
		};

		Kind kind;
		int section;
		QAbstractButton* button{ nullptr };
		QWidget* text{ nullptr };			//QLineEdit or QPlainTextEdit
		QTableWidget* table{ nullptr };
		std::vector<QWidget*> highlight;	//widgets highlighted while the finding is positive
	};

	//data roles of the sidebar items
	static constexpr int OriginalTitleRole = Qt::UserRole + 1;
	static constexpr int PositiveFindingsRole = Qt::UserRole + 2;

	std::vector<Finding> m_findings;
	bool m_loading{ false }; //no evaluation while a saved history is being loaded

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

	QListWidget* m_navigation;

	QListWidget* m_versionList;
	QTextBrowser* m_versionView;

	QWidget* createPage(QVBoxLayout*& layout);
	QGridLayout* addYesNoRows(QVBoxLayout* layout, MedicalHistoryItems::Section section, const QString& placeholder);
	QTableWidget* createTable(QVBoxLayout* layout, const QStringList& headers, const QString& addText, const QString& removeText);
	QPlainTextEdit* addTextField(QVBoxLayout* layout, const QString& label, int height);

	//section = index of the page and of its sidebar item
	int currentSection() const;
	void addFinding(const Finding& finding);
	static bool isPositive(const Finding& finding);
	bool hasPositiveFindings(int section) const;
	//re-evaluates one section: highlights of its findings and its sidebar item
	void evaluateSection(int section);
	void evaluateAllSections();
	void updateSidebarStatus(int section, bool positive);
	void updateHighlights(const Finding& finding, bool positive);
	static void setHighlighted(QWidget* widget, bool on);

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
