#pragma once

#include <QWidget>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <vector>

#include "Model/PatientFile.h"

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTableWidget;

//Radiographs, CBCT/DICOM files and documents of a patient:
//upload (button or drag & drop), list/thumbnail view, preview, open, save a copy, edit and delete.
class PatientFilesWidget : public QWidget
{
	Q_OBJECT

	long long m_patientRowid;

	std::vector<PatientFile> m_files;
	std::vector<int> m_visible; //indexes in m_files after the type filter

	QHash<long long, QPixmap> m_thumbnails;
	QImage m_previewImage;

	QComboBox* m_filter;
	QPushButton* m_listButton;
	QPushButton* m_galleryButton;
	QPushButton* m_openButton;
	QPushButton* m_saveButton;
	QPushButton* m_editButton;
	QPushButton* m_deleteButton;

	QStackedWidget* m_views;
	QTableWidget* m_table;
	QListWidget* m_gallery;
	QLabel* m_emptyLabel;

	QLabel* m_preview;
	QLabel* m_details;

	void reload(long long selectRowid = 0);
	void fillViews();
	void setCurrentRow(int row);

	int currentRow() const;
	const PatientFile* current() const;
	std::vector<PatientFile> selected() const;

	void updatePreview();
	void showScaledPreview();
	void updateButtons();

	QPixmap thumbnail(const PatientFile& file);

	void addFiles(const QStringList& paths);
	void chooseFiles();
	void openCurrent();
	void saveCopy();
	void editCurrent();
	void deleteSelected();
	void showImage(const PatientFile& file);

protected:
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dropEvent(QDropEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;

public:
	PatientFilesWidget(long long patientRowid, QWidget* parent = nullptr);

	int fileCount() const { return static_cast<int>(m_files.size()); }

	//standalone window with the files of the patient
	static void openDialog(long long patientRowid, const QString& patientName, QWidget* parent = nullptr);

signals:
	void filesChanged(int count);
};
