#include "RecallView.h"

#include <QComboBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Database/DbPatient.h"
#include "View/Theme.h"

namespace {

	enum Column { Name, Id, LastRecall, NextRecall, Appointment, Notes, State, ColumnCount };

	QString filterName(RecallPresenter::Filter f, int leadDays = 0)
	{
		using F = RecallPresenter::Filter;

		switch (f)
		{
			case F::Active: return RecallView::tr("All active recalls");
			case F::DueToday: return RecallView::tr("Due today");
			case F::Next7: return RecallView::tr("Due in the next 7 days");
			case F::Next30: return RecallView::tr("Due in the next 30 days");
			case F::Next90: return RecallView::tr("Due in the next 90 days");
			case F::Overdue: return RecallView::tr("Overdue");
			case F::DueNotBooked: return leadDays > 0 ?
				RecallView::tr("Due or due within %1, not booked").arg(RecallText::withinDays(leadDays)) :
				RecallView::tr("Due but not booked");
			case F::Inactive: return RecallView::tr("Inactive recalls");
			case F::All: return RecallView::tr("All recalls (active and inactive)");
			default: return {};
		}
	}
}

RecallView::RecallView(QWidget* parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(20, 16, 20, 16);
	layout->setSpacing(10);

	auto title = new QLabel(tr("Periodontal recall"), this);
	auto titleFont = title->font();
	titleFont.setPointSizeF(titleFont.pointSizeF() * 1.6);
	titleFont.setBold(true);
	title->setFont(titleFont);
	title->setStyleSheet("color:" + Theme::colorToString(Theme::fontTurquoise));
	layout->addWidget(title);

	// ---------------------------------------------------------------- filters and actions
	auto bar = new QHBoxLayout();

	m_search = new QLineEdit(this);
	m_search->setPlaceholderText(tr("Search by patient name or identifier"));
	m_search->setClearButtonEnabled(true);
	m_search->setMinimumWidth(280);
	bar->addWidget(m_search);

	m_filter = new QComboBox(this);
	for (int f = 0; f < int(RecallPresenter::Filter::Count); f++) {
		m_filter->addItem(filterName(RecallPresenter::Filter(f)), f);
	}
	m_filter->setMinimumWidth(260);
	bar->addWidget(m_filter);

	bar->addStretch();

	auto addButton = new QPushButton(QIcon(":/icons/icon_add.png"), tr("Add patient"), this);
	m_editButton = new QPushButton(QIcon(":/icons/icon_edit.png"), tr("Edit recall"), this);
	m_bookButton = new QPushButton(QIcon(":/icons/icon_calendar.png"), tr("Book appointment"), this);
	m_openButton = new QPushButton(QIcon(":/icons/icon_user.png"), tr("Open patient"), this);

	bar->addWidget(addButton);
	bar->addWidget(m_editButton);
	bar->addWidget(m_bookButton);
	bar->addWidget(m_openButton);

	layout->addLayout(bar);

	// ---------------------------------------------------------------- list
	m_table = new QTableWidget(0, ColumnCount, this);
	m_table->setHorizontalHeaderLabels({
		tr("Patient"), tr("Identifier"), tr("Last recall"), tr("Next recall"), tr("Appointment"), tr("Notes"), tr("Status")
	});
	m_table->verticalHeader()->setVisible(false);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->setContextMenuPolicy(Qt::CustomContextMenu);
	m_table->setAlternatingRowColors(true);
	m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	m_table->horizontalHeader()->setStretchLastSection(false);
	m_table->horizontalHeader()->setSectionResizeMode(Notes, QHeaderView::Stretch);
	m_table->setColumnWidth(Name, 230);
	m_table->setColumnWidth(Id, 150);
	m_table->setColumnWidth(LastRecall, 150);
	m_table->setColumnWidth(NextRecall, 230);
	m_table->setColumnWidth(Appointment, 230);
	m_table->setColumnWidth(State, 90);
	m_table->setMinimumHeight(300);
	layout->addWidget(m_table, 1);

	m_countLabel = new QLabel(this);
	m_countLabel->setStyleSheet("color: gray;");
	layout->addWidget(m_countLabel);

	// ---------------------------------------------------------------- connections
	connect(m_search, &QLineEdit::textChanged, this, [this] { if (m_presenter) m_presenter->refresh(); });
	connect(m_filter, &QComboBox::currentIndexChanged, this, [this] { if (m_presenter) m_presenter->refresh(); });

	connect(addButton, &QPushButton::clicked, this, [this] { if (m_presenter) m_presenter->addPatientRequested(); });
	connect(m_editButton, &QPushButton::clicked, this, [this] { if (m_presenter && selectedPatient()) m_presenter->editRequested(selectedPatient()); });
	connect(m_bookButton, &QPushButton::clicked, this, [this] { if (m_presenter && selectedPatient()) m_presenter->bookRequested(selectedPatient()); });
	connect(m_openButton, &QPushButton::clicked, this, [this] { if (m_presenter && selectedPatient()) m_presenter->openPatientRequested(selectedPatient()); });

	connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] { if (m_presenter && selectedPatient()) m_presenter->editRequested(selectedPatient()); });
	connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] { updateButtons(); });
	connect(m_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) { contextMenuRequested(pos); });

	updateButtons();
}

void RecallView::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.fillRect(rect(), Theme::background);
}

void RecallView::setPresenter(RecallPresenter* presenter)
{
	m_presenter = presenter;
}

RecallPresenter::Filter RecallView::filter() const
{
	return RecallPresenter::Filter(m_filter->currentData().toInt());
}

QString RecallView::searchText() const
{
	return m_search->text();
}

long long RecallView::selectedPatient() const
{
	int row = m_table->currentRow();

	if (row < 0 || row >= int(m_rowPatients.size()) || m_table->selectedItems().isEmpty()) return 0;

	return m_rowPatients[row];
}

void RecallView::updateButtons()
{
	bool selected = selectedPatient();

	m_editButton->setEnabled(selected);
	m_bookButton->setEnabled(selected);
	m_openButton->setEnabled(selected);
}

void RecallView::contextMenuRequested(const QPoint& pos)
{
	auto item = m_table->itemAt(pos);

	if (!item || !m_presenter) return;

	m_table->selectRow(item->row());

	auto patient = selectedPatient();

	if (!patient) return;

	QMenu menu(this);
	menu.setStyleSheet(Theme::getPopupMenuStylesheet());

	menu.addAction(QIcon(":/icons/icon_edit.png"), tr("Edit recall"), this, [this, patient] { m_presenter->editRequested(patient); });
	menu.addAction(QIcon(":/icons/icon_calendar.png"), tr("Book appointment"), this, [this, patient] { m_presenter->bookRequested(patient); });
	menu.addAction(QIcon(":/icons/icon_user.png"), tr("Open patient"), this, [this, patient] { m_presenter->openPatientRequested(patient); });

	menu.exec(m_table->viewport()->mapToGlobal(pos));
}

void RecallView::setFilter(RecallPresenter::Filter filter)
{
	QSignalBlocker blockSearch(m_search);
	QSignalBlocker blockFilter(m_filter);

	m_search->clear();
	m_filter->setCurrentIndex(m_filter->findData(int(filter)));
}

void RecallView::setRows(const std::vector<RecallListRow>& rows, const std::vector<int>& filterCounts, int leadDays)
{
	//the chosen patient stays chosen
	auto selected = selectedPatient();

	//numbers in the filter list
	{
		QSignalBlocker blocker(m_filter);

		for (int i = 0; i < m_filter->count() && i < int(filterCounts.size()); i++) {
			m_filter->setItemText(i, filterName(RecallPresenter::Filter(i), leadDays) + " (" + QString::number(filterCounts[i]) + ")");
		}
	}

	auto today = QDate::currentDate();

	const QColor red(200, 40, 40), orange(210, 120, 0), gray(140, 140, 140);

	m_table->setRowCount(0);
	m_rowPatients.clear();

	int selectRow = -1;

	for (auto& row : rows)
	{
		int r = m_table->rowCount();
		m_table->insertRow(r);
		m_rowPatients.push_back(row.recall.patient_rowid);

		if (row.recall.patient_rowid == selected) selectRow = r;

		auto set = [&](int column, const QString& text, const QColor& color = QColor(), bool bold = false) {
			auto item = new QTableWidgetItem(text);
			if (color.isValid()) item->setForeground(color);
			if (bold) { auto f = item->font(); f.setBold(true); item->setFont(f); }
			item->setToolTip(text);
			m_table->setItem(r, column, item);
			return item;
		};

		auto& recall = row.recall;
		QColor rowColor = recall.active ? QColor() : gray;

		set(Name, QString::fromStdString(row.patientName), rowColor, recall.active);
		set(Id, QString::fromStdString(row.patientId), gray);
		set(LastRecall, RecallText::date(recall.lastDate), rowColor);

		//next recall and how far it is
		QString next = tr("Not set");
		QColor nextColor = gray;
		bool nextBold = false;

		if (recall.nextDate.isValid())
		{
			int days = today.daysTo(recall.nextDate);

			next = RecallText::date(recall.nextDate) + "  ";

			if (days == 0) next += tr("(today)");
			else if (days > 0) next += tr("(in %1 days)").arg(days);
			else next += tr("(overdue %1 days)").arg(-days);

			nextColor = !recall.active ? gray : days < 0 ? red : days == 0 ? orange : QColor();
			nextBold = recall.active && days <= 0;
		}

		set(NextRecall, next, nextColor, nextBold);

		//recall appointment
		if (row.bookedAppointment.isValid()) {
			set(Appointment, tr("Booked: %1").arg(RecallText::dateTime(row.bookedAppointment)), rowColor);
		}
		else if (row.unmarkedAppointment.isValid()) {
			set(Appointment, tr("Not marked: %1").arg(RecallText::dateTime(row.unmarkedAppointment)), orange)
				->setToolTip(tr("The recall appointment has passed and is not marked completed or missed"));
		}
		else {
			set(Appointment, tr("Not booked"), recall.active ? QColor() : gray);
		}

		set(Notes, QString::fromStdString(recall.notes), rowColor);
		set(State, recall.active ? tr("Active") : tr("Inactive"), rowColor);
	}

	if (selectRow >= 0) m_table->selectRow(selectRow);
	else m_table->clearSelection();

	m_countLabel->setText(tr("Shown: %1").arg(rows.size()));

	updateButtons();
}

long long RecallView::choosePatient()
{
	QDialog d(this);
	d.setWindowTitle(tr("Add patient to the recall"));
	d.resize(460, 520);

	auto layout = new QVBoxLayout(&d);

	auto search = new QLineEdit(&d);
	search->setPlaceholderText(tr("Search by name or phone"));
	search->setClearButtonEnabled(true);
	layout->addWidget(search);

	auto list = new QListWidget(&d);
	layout->addWidget(list, 1);

	for (auto& p : DbPatient::getPatientList())
	{
		auto item = new QListWidgetItem(QString::fromStdString(p.summary), list);
		item->setData(Qt::UserRole, p.rowid);
	}

	list->sortItems();

	auto buttons = new QHBoxLayout();
	buttons->addStretch();
	auto ok = new QPushButton(tr("Choose"), &d);
	auto cancel = new QPushButton(tr("Cancel"), &d);
	ok->setDefault(true);
	buttons->addWidget(ok);
	buttons->addWidget(cancel);
	layout->addLayout(buttons);

	connect(search, &QLineEdit::textChanged, &d, [list](const QString& text) {

		auto s = RecallPresenter::searchable(text.trimmed());

		for (int i = 0; i < list->count(); i++) {
			list->item(i)->setHidden(s.size() && !RecallPresenter::searchable(list->item(i)->text()).contains(s));
		}
	});

	connect(list, &QListWidget::itemDoubleClicked, &d, &QDialog::accept);
	connect(ok, &QPushButton::clicked, &d, [&] { if (list->currentItem() && !list->currentItem()->isHidden()) d.accept(); });
	connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);

	search->setFocus();

	if (d.exec() != QDialog::Accepted || !list->currentItem()) return 0;

	return list->currentItem()->data(Qt::UserRole).toLongLong();
}
