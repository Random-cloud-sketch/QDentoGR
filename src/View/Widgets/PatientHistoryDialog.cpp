#include "PatientHistoryDialog.h"
#include "Presenter/PatientHistoryPresenter.h"
#include "View/GlobalFunctions.h"
#include "View/Widgets/PatientFilesWidget.h"
#include "View/uiComponents/ListTable.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <set>
#include <array>

PatientHistoryDialog::PatientHistoryDialog(PatientHistoryPresenter& p, QWidget *parent)
	: presenter(p), QDialog(parent)
{
	ui.setupUi(this);

	setWindowTitle(tr("Patient History"));
	setWindowIcon(QIcon(":/icons/icon_history.png"));
	setWindowFlag(Qt::WindowMaximizeButtonHint);

	ui.tabWidget->setCurrentIndex(0);

	ui.perioTab->hide();

	procedure_proxy.setSourceModel(&procedure_model);
	ui.procedureTable->setModel(&procedure_proxy);
	ui.procedureTable->setSortingEnabled(true);

	ui.docView->setModel(&doc_model);
	ui.docDetailsView->setModel(&doc_details_model);

	//init procedure table
	ui.procedureTable->setSelectionMode(QAbstractItemView::SelectionMode::NoSelection);

	ui.procedureTable->hideColumn(0);
	ui.procedureTable->hideColumn(7);

	ui.procedureTable->setColumnWidth(1, 100);
	ui.procedureTable->setColumnWidth(2, 70);
	ui.procedureTable->setColumnWidth(3, 200);
	ui.procedureTable->setColumnWidth(4, 65);
	ui.procedureTable->setColumnWidth(5, 200);
	ui.procedureTable->setColumnWidth(6, 100);
	ui.procedureTable->setColumnWidth(8, 200);
	
	setTableViewDefaults(ui.procedureTable);
	setTableViewDefaults(ui.docDetailsView);
	setTableViewDefaults(ui.docView);

	ui.docView->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
	

	connect(ui.docView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [&](const QItemSelection&, const QItemSelection&) {

		auto idx_rows = ui.docView->selectionModel()->selectedRows();

		std::set<int> rows;

		for (auto& idx : idx_rows) {
			rows.insert(idx.row());
		}

		if (rows.size() != 1) {
			doc_details_model.setTableData({});
			return;
		}

		auto currentRow = *rows.begin();

		doc_details_model.setTableData(details_data[currentRow]);

		for (int i = 0; i < details_data[currentRow].size(); i++) {

			ui.docDetailsView->setColumnHidden(i, details_data[currentRow][i].hidden);

			ui.docDetailsView->setColumnWidth(i, details_data[currentRow][i].width);
		}
	});

	connect(ui.snapshotViewer->getTeethScene(), &TeethViewScene::toothDoubleClicked, this,[&](int idx) {

			presenter.toothHistoryRequested(idx);
	});

	connect(ui.openDocButton, &QPushButton::clicked, this, [&] {

		auto idx_rows = ui.docView->selectionModel()->selectedRows();

		std::set<int> rows;

		for (auto& idx : idx_rows) {
			rows.insert(idx.row());
		}

		std::vector<int> docsToOpen;

		for (auto& row : rows) {
			docsToOpen.push_back(row);
		}

		presenter.openDocuments(docsToOpen);

	});

	connect(ui.docView, &QTableView::doubleClicked, this, [&] { ui.openDocButton->click(); });

}

void PatientHistoryDialog::setProcedures(const std::vector<Procedure> procedures)
{
	procedure_model.setProcedures(procedures);
}

void PatientHistoryDialog::setDocuments(const PlainTable& docList, const std::vector<PlainTable>& contents)
{
	doc_model.setTableData(docList);

	for (int i = 0; i < docList.size(); i++) {

		ui.docView->setColumnHidden(i, docList[i].hidden);

		ui.docView->setColumnWidth(i, docList[i].width);
	}

	details_data = contents;

}

void PatientHistoryDialog::setSnapshots(const std::vector<Snapshot>& snapshots)
{
	ui.snapshotViewer->setSnapshots(snapshots);
}

void PatientHistoryDialog::setPerioSnapshots(const std::vector<PerioSnapshot>& snapshots)
{

	if (snapshots.empty()) {
		ui.tabWidget->removeTab(ui.tabWidget->indexOf(ui.perioTab));
		return;
	}
	ui.perioTab->setSnapshots(snapshots);
}

void PatientHistoryDialog::setPatientNoteFlags(const std::array<std::string, 32>& notes)
{
	ui.snapshotViewer->getTeethScene()->setNotes(notes);
}

void PatientHistoryDialog::addPatientFilesTab(long long patientRowid)
{
	auto filesWidget = new PatientFilesWidget(patientRowid, ui.tabWidget);

	auto title = [](int count) {
		return count ? tr("Radiographs && documents (%1)").arg(count) : tr("Radiographs && documents");
	};

	int index = ui.tabWidget->addTab(filesWidget, QIcon(":/icons/icon_open.png"), title(filesWidget->fileCount()));

	connect(filesWidget, &PatientFilesWidget::filesChanged, this, [=, this](int count) {
		ui.tabWidget->setTabText(ui.tabWidget->indexOf(filesWidget), title(count));
	});

	Q_UNUSED(index);
}

void PatientHistoryDialog::setVisitHistory(const PlainTable& visits)
{
	visitTab = new QWidget(ui.tabWidget);

	auto layout = new QVBoxLayout(visitTab);

	auto table = new ListTable(visitTab);
	table->setModel(&visit_model);
	setTableViewDefaults(table);
	table->setSelectionMode(QAbstractItemView::SingleSelection);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);

	visit_model.setTableData(visits);

	for (int i = 0; i < visits.size(); i++) {
		table->setColumnHidden(i, visits[i].hidden);
		table->setColumnWidth(i, visits[i].width);
	}

	layout->addWidget(table);

	auto buttons = new QHBoxLayout();

	auto emptyLabel = new QLabel(tr("No saved visits yet"), visitTab);
	emptyLabel->setStyleSheet("color: gray;");
	emptyLabel->setVisible(!visits.rowCount());
	buttons->addWidget(emptyLabel);

	buttons->addStretch();

	auto openButton = new QPushButton(QIcon(":/icons/icon_sheet.png"), tr("Open visit"), visitTab);
	openButton->setEnabled(false);
	buttons->addWidget(openButton);

	layout->addLayout(buttons);

	auto selectedRow = [=] {
		auto rows = table->selectionModel()->selectedRows();
		return rows.size() == 1 ? rows[0].row() : -1;
	};

	connect(table->selectionModel(), &QItemSelectionModel::selectionChanged, this, [=] {
		openButton->setEnabled(selectedRow() != -1);
	});

	connect(openButton, &QPushButton::clicked, this, [=, this] {
		if (auto row = selectedRow(); row != -1) presenter.openVisit(row);
	});

	connect(table, &QTableView::doubleClicked, this, [=, this](const QModelIndex& index) {
		presenter.openVisit(index.row());
	});

	ui.tabWidget->insertTab(0, visitTab, QIcon(":/icons/icon_sheet.png"),
		visits.rowCount() ? tr("Visit history (%1)").arg(visits.rowCount()) : tr("Visit history")
	);

	//the dialog still opens on the procedures, unless the visit history was asked for
	ui.tabWidget->setCurrentWidget(ui.tab);
}

void PatientHistoryDialog::showVisitHistory()
{
	if (visitTab) ui.tabWidget->setCurrentWidget(visitTab);
}

PatientHistoryDialog::~PatientHistoryDialog()
{}
