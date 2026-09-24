#include "PatientFilesWidget.h"

#include <algorithm>

#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "Database/DbPatientFile.h"
#include "Model/MedicalHistory.h"
#include "Model/User.h"
#include "View/ModalDialogBuilder.h"

static constexpr QSize thumbnailSize{ 150, 110 };

static QString qs(const std::string& s) { return QString::fromStdString(s); }

//reads an image, scaled down to the bound if it is bigger; empty image if the format can not be read
static QImage loadImage(const QString& path, const QSize& bound = QSize())
{
	QImageReader reader(path);
	reader.setAutoTransform(true);

	if (!reader.canRead()) return QImage();

	auto size = reader.size();

	if (bound.isValid() && size.isValid() && (size.width() > bound.width() || size.height() > bound.height())) {
		reader.setScaledSize(size.scaled(bound, Qt::KeepAspectRatio));
	}

	return reader.read();
}

//true when the image format can be shown inside the application
static bool canPreview(const QString& path)
{
	return QImageReader(path).canRead();
}

static QString sizeText(long long bytes)
{
	return QLocale::system().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

PatientFilesWidget::PatientFilesWidget(long long patientRowid, QWidget* parent)
	: QWidget(parent), m_patientRowid(patientRowid)
{
	setAcceptDrops(true);

	auto mainLayout = new QVBoxLayout(this);

	//toolbar
	auto toolbar = new QHBoxLayout();

	auto addButton = new QPushButton(QIcon(":/icons/icon_add.png"), tr("Add files..."), this);
	addButton->setToolTip(tr("Add radiographs, CBCT files or documents (you can also drag and drop files here)"));
	m_openButton = new QPushButton(QIcon(":/icons/icon_open.png"), tr("Open"), this);
	m_openButton->setToolTip(tr("Open with the default program of the computer"));
	m_saveButton = new QPushButton(QIcon(":/icons/icon_save.png"), tr("Save a copy..."), this);
	m_editButton = new QPushButton(QIcon(":/icons/icon_edit.png"), tr("Rename / edit..."), this);
	m_deleteButton = new QPushButton(QIcon(":/icons/icon_remove.png"), tr("Delete"), this);

	for (auto b : { addButton, m_openButton, m_saveButton, m_editButton, m_deleteButton }) toolbar->addWidget(b);

	toolbar->addStretch();

	m_filter = new QComboBox(this);
	m_filter->addItem(tr("All types"), -1);
	for (int t = 0; t < PatientFile::TypeCount; t++) m_filter->addItem(PatientFile::typeName(t), t);
	toolbar->addWidget(m_filter);

	m_listButton = new QPushButton(tr("List"), this);
	m_galleryButton = new QPushButton(tr("Thumbnails"), this);
	m_listButton->setCheckable(true);
	m_galleryButton->setCheckable(true);
	m_listButton->setAutoExclusive(true);
	m_galleryButton->setAutoExclusive(true);
	m_listButton->setChecked(true);
	toolbar->addWidget(m_listButton);
	toolbar->addWidget(m_galleryButton);

	mainLayout->addLayout(toolbar);

	//views and preview
	auto splitter = new QSplitter(Qt::Horizontal, this);

	auto left = new QWidget(splitter);
	auto leftLayout = new QVBoxLayout(left);
	leftLayout->setContentsMargins(0, 0, 0, 0);

	m_views = new QStackedWidget(left);

	m_table = new QTableWidget(0, 5, m_views);
	m_table->setHorizontalHeaderLabels({ tr("Name"), tr("Type"), tr("Uploaded"), tr("Size"), tr("Description") });
	m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	m_table->horizontalHeader()->setStretchLastSection(true);
	m_table->setColumnWidth(0, 200);
	m_table->setColumnWidth(1, 115);
	m_table->setColumnWidth(2, 150);
	m_table->setColumnWidth(3, 70);
	m_table->verticalHeader()->setVisible(false);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->setIconSize(QSize(20, 20));
	m_views->addWidget(m_table);

	m_gallery = new QListWidget(m_views);
	m_gallery->setViewMode(QListView::IconMode);
	m_gallery->setIconSize(thumbnailSize);
	m_gallery->setGridSize(QSize(thumbnailSize.width() + 30, thumbnailSize.height() + 50));
	m_gallery->setResizeMode(QListView::Adjust);
	m_gallery->setMovement(QListView::Static);
	m_gallery->setWordWrap(true);
	m_gallery->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_views->addWidget(m_gallery);

	leftLayout->addWidget(m_views, 1);

	m_emptyLabel = new QLabel(tr("No files yet. Add radiographs or documents with \"Add files...\" or drag and drop them here."), left);
	m_emptyLabel->setAlignment(Qt::AlignCenter);
	m_emptyLabel->setWordWrap(true);
	m_emptyLabel->setStyleSheet("color: gray; padding: 12px;");
	leftLayout->addWidget(m_emptyLabel);

	auto right = new QWidget(splitter);
	auto rightLayout = new QVBoxLayout(right);
	rightLayout->setContentsMargins(0, 0, 0, 0);

	m_preview = new QLabel(right);
	m_preview->setAlignment(Qt::AlignCenter);
	m_preview->setMinimumSize(280, 220);
	m_preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	m_preview->setStyleSheet("background-color: white; border: 1px solid rgb(225, 225, 225);");
	m_preview->setWordWrap(true);
	m_preview->setToolTip(tr("Double click to view the image in full size"));
	rightLayout->addWidget(m_preview, 1);

	m_details = new QLabel(right);
	m_details->setWordWrap(true);
	m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
	m_details->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_details->setMinimumHeight(150);
	rightLayout->addWidget(m_details);

	splitter->setStretchFactor(0, 3);
	splitter->setStretchFactor(1, 2);
	splitter->setSizes({ 700, 400 });
	mainLayout->addWidget(splitter, 1);

	//connections
	connect(addButton, &QPushButton::clicked, this, &PatientFilesWidget::chooseFiles);
	connect(m_openButton, &QPushButton::clicked, this, &PatientFilesWidget::openCurrent);
	connect(m_saveButton, &QPushButton::clicked, this, &PatientFilesWidget::saveCopy);
	connect(m_editButton, &QPushButton::clicked, this, &PatientFilesWidget::editCurrent);
	connect(m_deleteButton, &QPushButton::clicked, this, &PatientFilesWidget::deleteSelected);

	connect(m_filter, &QComboBox::currentIndexChanged, this, [this] { fillViews(); });

	connect(m_listButton, &QPushButton::toggled, this, [this](bool checked) {
		if (!checked) return;
		int row = m_gallery->currentRow();
		m_views->setCurrentWidget(m_table);
		setCurrentRow(row);
	});

	connect(m_galleryButton, &QPushButton::toggled, this, [this](bool checked) {
		if (!checked) return;
		int row = m_table->currentRow();
		m_views->setCurrentWidget(m_gallery);
		setCurrentRow(row);
	});

	connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] { updatePreview(); updateButtons(); });
	connect(m_gallery, &QListWidget::itemSelectionChanged, this, [this] { updatePreview(); updateButtons(); });

	auto activate = [this] {
		if (auto f = current()) {
			if (canPreview(DbPatientFile::filePath(*f))) showImage(*f);
			else openCurrent();
		}
	};

	connect(m_table, &QTableWidget::cellDoubleClicked, this, activate);
	connect(m_gallery, &QListWidget::itemDoubleClicked, this, activate);

	auto deleteShortcut = new QShortcut(QKeySequence::Delete, this);
	deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
	connect(deleteShortcut, &QShortcut::activated, this, &PatientFilesWidget::deleteSelected);

	reload();
}

void PatientFilesWidget::reload(long long selectRowid)
{
	m_files = DbPatientFile::getFiles(m_patientRowid);

	fillViews();

	if (selectRowid) {
		for (int i = 0; i < (int)m_visible.size(); i++) {
			if (m_files[m_visible[i]].rowid == selectRowid) setCurrentRow(i);
		}
	}

	emit filesChanged(fileCount());
}

void PatientFilesWidget::fillViews()
{
	int type = m_filter->currentData().toInt();

	m_visible.clear();

	for (int i = 0; i < (int)m_files.size(); i++) {
		if (type == -1 || m_files[i].type == type) m_visible.push_back(i);
	}

	QSignalBlocker b1(m_table), b2(m_gallery);

	m_table->setRowCount(0);
	m_gallery->clear();

	QFileIconProvider icons;

	for (int row = 0; row < (int)m_visible.size(); row++)
	{
		auto& f = m_files[m_visible[row]];
		auto path = DbPatientFile::filePath(f);
		bool missing = !QFileInfo::exists(path);

		m_table->insertRow(row);

		auto nameItem = new QTableWidgetItem(icons.icon(QFileInfo(path)), qs(f.name) + (f.suffix().size() ? "." + f.suffix() : ""));
		if (missing) nameItem->setForeground(Qt::red);
		m_table->setItem(row, 0, nameItem);
		m_table->setItem(row, 1, new QTableWidgetItem(PatientFile::typeName(f.type)));
		m_table->setItem(row, 2, new QTableWidgetItem(MedicalHistory::savedToLocalFormat(f.uploaded)));
		m_table->setItem(row, 3, new QTableWidgetItem(sizeText(f.size)));
		m_table->setItem(row, 4, new QTableWidgetItem(qs(f.description)));

		auto item = new QListWidgetItem(QIcon(thumbnail(f)), qs(f.name), m_gallery);
		item->setToolTip(PatientFile::typeName(f.type) + "\n" + MedicalHistory::savedToLocalFormat(f.uploaded));
		if (missing) item->setForeground(Qt::red);
	}

	bool empty = m_files.empty();
	m_views->setVisible(!empty);
	m_emptyLabel->setVisible(empty);

	if (!m_visible.empty()) setCurrentRow(0);

	updatePreview();
	updateButtons();
}

void PatientFilesWidget::setCurrentRow(int row)
{
	if (row < 0 || row >= (int)m_visible.size()) return;

	if (m_views->currentWidget() == m_table) {
		m_table->clearSelection();
		m_table->setCurrentCell(row, 0);
		m_table->selectRow(row);
	}
	else {
		m_gallery->clearSelection();
		m_gallery->setCurrentRow(row);
		m_gallery->item(row)->setSelected(true);
	}
}

int PatientFilesWidget::currentRow() const
{
	return m_views->currentWidget() == m_table ? m_table->currentRow() : m_gallery->currentRow();
}

const PatientFile* PatientFilesWidget::current() const
{
	int row = currentRow();

	if (row < 0 || row >= (int)m_visible.size()) return nullptr;

	return &m_files[m_visible[row]];
}

std::vector<PatientFile> PatientFilesWidget::selected() const
{
	std::vector<PatientFile> result;

	std::vector<int> rows;

	if (m_views->currentWidget() == m_table) {
		for (auto& idx : m_table->selectionModel()->selectedRows()) rows.push_back(idx.row());
	}
	else {
		for (auto item : m_gallery->selectedItems()) rows.push_back(m_gallery->row(item));
	}

	for (int row : rows) {
		if (row >= 0 && row < (int)m_visible.size()) result.push_back(m_files[m_visible[row]]);
	}

	return result;
}

QPixmap PatientFilesWidget::thumbnail(const PatientFile& f)
{
	if (m_thumbnails.contains(f.rowid)) return m_thumbnails[f.rowid];

	auto path = DbPatientFile::filePath(f);

	QPixmap result;

	auto image = loadImage(path, thumbnailSize);

	if (!image.isNull()) {
		result = QPixmap::fromImage(image);
	}
	else {
		//no preview for this format - system icon of the file type
		result = QFileIconProvider().icon(QFileInfo(path)).pixmap(64, 64);
	}

	m_thumbnails[f.rowid] = result;

	return result;
}

void PatientFilesWidget::updatePreview()
{
	auto f = current();

	m_previewImage = QImage();
	m_preview->setPixmap(QPixmap());
	m_preview->setText(QString());

	if (!f || selected().size() > 1)
	{
		m_details->setText(selected().size() > 1 ? tr("%1 files selected").arg(selected().size()) : QString());
		return;
	}

	auto path = DbPatientFile::filePath(*f);
	bool exists = QFileInfo::exists(path);

	if (exists) {
		m_previewImage = loadImage(path, QSize(1600, 1600));
	}

	if (!m_previewImage.isNull()) {
		showScaledPreview();
	}
	else if (exists) {
		m_preview->setText(tr("A preview is not available for this type of file.\nUse \"Open\" to view it with the program of the computer."));
	}
	else {
		m_preview->setText(tr("The file was not found in the storage folder."));
	}

	QString uploadedBy = f->dentistRowid ? qs(User::getNameFromRowid(f->dentistRowid)) : QString();

	auto row = [](const QString& label, const QString& value) {
		return value.isEmpty() ? QString() : "<b>" + label.toHtmlEscaped() + ":</b> " + value.toHtmlEscaped() + "<br>";
	};

	QString html = "<b style=\"font-size:11pt\">" + qs(f->name).toHtmlEscaped() + "</b><br>";
	html += row(tr("Type"), PatientFile::typeName(f->type));
	html += row(tr("Original file name"), qs(f->originalName));
	html += row(tr("Uploaded"), MedicalHistory::savedToLocalFormat(f->uploaded));
	html += row(tr("Uploaded by"), uploadedBy);
	html += row(tr("Size"), sizeText(f->size));
	html += row(tr("Description"), qs(f->description));

	if (!exists) {
		html += "<span style=\"color:rgb(200,30,30)\"><b>" + tr("The file was not found in the storage folder.").toHtmlEscaped() + "</b></span><br>";
	}

	m_details->setText(html);
}

void PatientFilesWidget::showScaledPreview()
{
	if (m_previewImage.isNull()) return;

	m_preview->setPixmap(QPixmap::fromImage(
		m_previewImage.scaled(m_preview->size() - QSize(8, 8), Qt::KeepAspectRatio, Qt::SmoothTransformation)
	));
}

void PatientFilesWidget::updateButtons()
{
	auto f = current();
	int count = static_cast<int>(selected().size());
	bool exists = f && QFileInfo::exists(DbPatientFile::filePath(*f));

	m_openButton->setEnabled(count == 1 && exists);
	m_saveButton->setEnabled(count == 1 && exists);
	m_editButton->setEnabled(count == 1);
	m_deleteButton->setEnabled(count >= 1);
}

void PatientFilesWidget::chooseFiles()
{
	auto paths = QFileDialog::getOpenFileNames(
		this,
		tr("Add radiographs and documents"),
		QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
		PatientFile::fileDialogFilter()
	);

	addFiles(paths);
}

void PatientFilesWidget::addFiles(const QStringList& paths)
{
	if (paths.isEmpty()) return;

	QStringList failed;
	long long lastRowid = 0;

	for (auto& path : paths)
	{
		if (QFileInfo(path).isDir()) {
			failed.append(QFileInfo(path).fileName() + " (" + tr("folders can not be added - add the files it contains") + ")");
			continue;
		}

		auto file = DbPatientFile::importFile(m_patientRowid, path);

		if (file) lastRowid = file->rowid;
		else failed.append(QFileInfo(path).fileName());
	}

	reload(lastRowid);

	if (failed.size()) {
		ModalDialogBuilder::showError(
			(tr("The following files could not be added:") + "\n" + failed.join("\n")).toStdString()
		);
	}
}

void PatientFilesWidget::openCurrent()
{
	auto f = current();

	if (!f) return;

	auto path = DbPatientFile::filePath(*f);

	if (!QFileInfo::exists(path)) {
		ModalDialogBuilder::showError(tr("The file was not found in the storage folder.").toStdString());
		return;
	}

	if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
		ModalDialogBuilder::showError(tr("No program was found on this computer to open this type of file.").toStdString());
	}
}

void PatientFilesWidget::saveCopy()
{
	auto f = current();

	if (!f) return;

	auto fileName = qs(f->name);
	auto suffix = f->suffix();
	if (suffix.size() && !fileName.endsWith("." + suffix, Qt::CaseInsensitive)) fileName += "." + suffix;

	auto defaultPath = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(fileName);

	auto destination = QFileDialog::getSaveFileName(
		this,
		tr("Save a copy"),
		defaultPath,
		suffix.size() ? "*." + suffix : QString()
	);

	if (destination.isEmpty()) return;

	if (!DbPatientFile::exportFile(*f, destination)) {
		ModalDialogBuilder::showError(tr("The copy could not be saved.").toStdString());
	}
}

void PatientFilesWidget::editCurrent()
{
	auto f = current();

	if (!f) return;

	PatientFile file = *f;

	QDialog d(this);
	d.setWindowTitle(tr("Rename / edit file"));
	d.setMinimumWidth(460);

	auto form = new QFormLayout(&d);

	auto nameEdit = new QLineEdit(qs(file.name), &d);
	form->addRow(tr("Name:"), nameEdit);

	auto typeCombo = new QComboBox(&d);
	for (int t = 0; t < PatientFile::TypeCount; t++) typeCombo->addItem(PatientFile::typeName(t), t);
	typeCombo->setCurrentIndex(typeCombo->findData(file.type));
	form->addRow(tr("Type:"), typeCombo);

	auto descriptionEdit = new QPlainTextEdit(qs(file.description), &d);
	descriptionEdit->setPlaceholderText(tr("e.g. periapical 36, panoramic, CBCT of the maxilla"));
	descriptionEdit->setFixedHeight(90);
	form->addRow(tr("Description:"), descriptionEdit);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d);
	form->addRow(buttons);

	connect(buttons, &QDialogButtonBox::accepted, &d, [&] {
		if (nameEdit->text().trimmed().isEmpty()) {
			nameEdit->setFocus();
			return;
		}
		d.accept();
	});
	connect(buttons, &QDialogButtonBox::rejected, &d, &QDialog::reject);

	if (d.exec() != QDialog::Accepted) return;

	file.name = nameEdit->text().trimmed().toStdString();
	file.type = typeCombo->currentData().toInt();
	file.description = descriptionEdit->toPlainText().trimmed().toStdString();

	if (!DbPatientFile::update(file)) {
		ModalDialogBuilder::showError(tr("The changes could not be saved.").toStdString());
		return;
	}

	reload(file.rowid);
}

void PatientFilesWidget::deleteSelected()
{
	auto files = selected();

	if (files.empty()) return;

	auto question = files.size() == 1 ?
		tr("Delete \"%1\"? The file will be permanently removed from the patient's record.").arg(qs(files[0].name))
		:
		tr("Delete the %1 selected files? They will be permanently removed from the patient's record.").arg(files.size());

	if (!ModalDialogBuilder::askDialog(question.toStdString())) return;

	QStringList failed;

	for (auto& f : files) {
		if (DbPatientFile::remove(f)) m_thumbnails.remove(f.rowid);
		else failed.append(qs(f.name));
	}

	reload();

	if (failed.size()) {
		ModalDialogBuilder::showError((tr("The following files could not be deleted:") + "\n" + failed.join("\n")).toStdString());
	}
}

void PatientFilesWidget::showImage(const PatientFile& f)
{
	auto image = loadImage(DbPatientFile::filePath(f));

	if (image.isNull()) return;

	QDialog d(this);
	d.setWindowTitle(qs(f.name));
	d.setWindowFlag(Qt::WindowMaximizeButtonHint);
	d.resize(1100, 800);

	auto layout = new QVBoxLayout(&d);

	auto toolbar = new QHBoxLayout();
	auto fitButton = new QPushButton(tr("Fit to window"), &d);
	auto actualButton = new QPushButton(tr("Actual size"), &d);
	auto zoomIn = new QPushButton("+", &d);
	auto zoomOut = new QPushButton(QString(QChar(0x2212)), &d);
	zoomIn->setToolTip(tr("Zoom in (Ctrl +)"));
	zoomOut->setToolTip(tr("Zoom out (Ctrl -)"));
	auto zoomLabel = new QLabel(&d);

	for (auto w : std::initializer_list<QWidget*>{ fitButton, actualButton, zoomOut, zoomIn, zoomLabel }) toolbar->addWidget(w);
	toolbar->addStretch();
	layout->addLayout(toolbar);

	auto scroll = new QScrollArea(&d);
	scroll->setAlignment(Qt::AlignCenter);
	scroll->setStyleSheet("background-color: rgb(40, 40, 40);");
	auto label = new QLabel(scroll);
	scroll->setWidget(label);
	layout->addWidget(scroll, 1);

	double factor = 1.0;

	auto apply = [&](double newFactor) {
		factor = std::clamp(newFactor, 0.05, 8.0);
		label->setPixmap(QPixmap::fromImage(image.scaled(image.size() * factor, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
		label->adjustSize();
		zoomLabel->setText(QString::number(qRound(factor * 100)) + " %");
	};

	auto fit = [&] {
		auto area = scroll->viewport()->size() - QSize(4, 4);
		apply(std::min(1.0, std::min(double(area.width()) / image.width(), double(area.height()) / image.height())));
	};

	connect(fitButton, &QPushButton::clicked, &d, fit);
	connect(actualButton, &QPushButton::clicked, &d, [&] { apply(1.0); });
	connect(zoomIn, &QPushButton::clicked, &d, [&] { apply(factor * 1.25); });
	connect(zoomOut, &QPushButton::clicked, &d, [&] { apply(factor / 1.25); });
	connect(new QShortcut(QKeySequence::ZoomIn, &d), &QShortcut::activated, &d, [&] { apply(factor * 1.25); });
	connect(new QShortcut(QKeySequence::ZoomOut, &d), &QShortcut::activated, &d, [&] { apply(factor / 1.25); });

	d.show();
	fit();
	d.exec();
}

void PatientFilesWidget::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void PatientFilesWidget::dropEvent(QDropEvent* event)
{
	QStringList paths;

	for (auto& url : event->mimeData()->urls()) {
		if (url.isLocalFile()) paths.append(url.toLocalFile());
	}

	addFiles(paths);

	event->acceptProposedAction();
}

void PatientFilesWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);

	showScaledPreview();
}

void PatientFilesWidget::openDialog(long long patientRowid, const QString& patientName, QWidget* parent)
{
	QDialog d(parent);
	d.setWindowTitle(tr("Radiographs & documents") + " - " + patientName);
	d.setWindowFlag(Qt::WindowMaximizeButtonHint);
	d.resize(1150, 720);

	auto layout = new QVBoxLayout(&d);
	layout->addWidget(new PatientFilesWidget(patientRowid, &d), 1);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, &d);
	connect(buttons, &QDialogButtonBox::rejected, &d, &QDialog::reject);
	layout->addWidget(buttons);

	d.exec();
}
