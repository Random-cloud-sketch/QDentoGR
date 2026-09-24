#include "PatientFile.h"

#include <QFileInfo>
#include <QStringList>

static const QStringList imageSuffixes{ "jpg", "jpeg", "png", "tif", "tiff", "bmp", "gif", "webp" };
static const QStringList dicomSuffixes{ "dcm", "dicom" };
static const QStringList documentSuffixes{ "doc", "docx", "odt", "rtf", "txt", "xls", "xlsx", "ods", "csv" };

QString PatientFile::suffix() const
{
	return QFileInfo(QString::fromStdString(storedName)).suffix().toLower();
}

PatientFile::Type PatientFile::detectType(const QString& fileName)
{
	auto suffix = QFileInfo(fileName).suffix().toLower();

	if (imageSuffixes.contains(suffix)) return Radiograph;
	if (dicomSuffixes.contains(suffix)) return Cbct;
	if (suffix == "pdf") return Pdf;
	if (documentSuffixes.contains(suffix)) return Document;

	return Other;
}

QString PatientFile::typeName(int type)
{
	switch (type)
	{
	case Radiograph: return tr("Radiograph");
	case Cbct: return tr("CBCT / DICOM");
	case Photo: return tr("Clinical photo");
	case Pdf: return tr("PDF document");
	case Document: return tr("Document");
	default: return tr("Other");
	}
}

QString PatientFile::fileDialogFilter()
{
	auto patterns = [](const QStringList& suffixes) {
		QStringList result;
		for (auto& s : suffixes) result.append("*." + s);
		return result.join(" ");
	};

	return QStringList{
		tr("Radiographs, images, CBCT and documents") + " (" + patterns(imageSuffixes + dicomSuffixes + QStringList{ "pdf" } + documentSuffixes) + ")",
		tr("Radiographs and images") + " (" + patterns(imageSuffixes) + ")",
		tr("CBCT / DICOM") + " (" + patterns(dicomSuffixes) + ")",
		tr("PDF documents") + " (*.pdf)",
		tr("Documents") + " (" + patterns(documentSuffixes) + ")",
		tr("All files") + " (*)"
	}.join(";;");
}
