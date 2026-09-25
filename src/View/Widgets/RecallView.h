#pragma once

#include <QWidget>
#include <vector>

#include "Model/Recall.h"
#include "Presenter/RecallPresenter.h"

class QComboBox;
class QLineEdit;
class QLabel;
class QTableWidget;
class QPushButton;

//The list of the periodontal recalls: filters, search and the actions for the selected patient
class RecallView : public QWidget
{
	Q_OBJECT

	RecallPresenter* m_presenter{ nullptr };

	QLineEdit* m_search;
	QComboBox* m_filter;
	QTableWidget* m_table;
	QLabel* m_countLabel;
	QPushButton* m_editButton;
	QPushButton* m_bookButton;
	QPushButton* m_openButton;

	std::vector<long long> m_rowPatients;	//patient rowid of each table row

	long long selectedPatient() const;
	void updateButtons();
	void contextMenuRequested(const QPoint& pos);

	void paintEvent(QPaintEvent* e) override;

public:
	RecallView(QWidget* parent = nullptr);

	void setPresenter(RecallPresenter* presenter);
	RecallPresenter* presenter() const { return m_presenter; }

	RecallPresenter::Filter filter() const;
	QString searchText() const;

	void setFilter(RecallPresenter::Filter filter);

	//the rows to show, the number of recalls of each filter and the lead time of the notification
	void setRows(const std::vector<RecallListRow>& rows, const std::vector<int>& filterCounts, int leadDays);

	//a patient of the patient list (0: none chosen)
	long long choosePatient();
};
