#include "PatientDialogPresenter.h"
#include "View/ModalDialogBuilder.h"
#include "View/Widgets/PatientFormDialog.h"
#include "Database/DbPatient.h"
#include "Model/User.h"
#include "Model/UpperCase.h"
#include <QString>
#include <QObject>
#include <QRegularExpression>

PatientDialogPresenter::PatientDialogPresenter(std::string dialogTitle, std::string patientData) :
	view(nullptr), dialogTitle(dialogTitle), patientId(Patient::newId())
{
	if (patientData.empty()) return;

	QString tempData = patientData.c_str();

	m_patient.emplace();

	m_patient->rowid = -1;

	QRegularExpression digitsOnly("[0-9-+]+");

	for (QString& word : tempData.split(" ")) {
	
		if (digitsOnly.match(word).hasMatch()) {
			m_patient->phone = word.toStdString();
		}
		else if (m_patient->firstName.empty()) {
			m_patient->firstName = UpperCase::convert(word).toStdString();
		}
		else if (m_patient->lastName.empty()) {
			m_patient->lastName = UpperCase::convert(word).toStdString();
		}
	}
}

PatientDialogPresenter::PatientDialogPresenter(const Patient& patient) :
	m_patient(patient),
	view(nullptr),
	rowid(patient.rowid),
	patientId(patient.id.size() ? patient.id : Patient::newId()),
	teeth_notes(patient.teethNotes),
	patientNotes(patient.patientNotes),
	dialogTitle(QT_TRANSLATE_NOOP("PatientFormDialog", "Edit Patient"))
{}

std::optional<Patient> PatientDialogPresenter::open()
{
	PatientFormDialog d(*this);
	d.exec();
    return m_patient;
}

void PatientDialogPresenter::setView(PatientFormDialog* view)
{
	this->view = view;

	view->setTitle(dialogTitle);

	view->setPatientId(patientId);

	if (!m_patient.has_value()) return;
	
	view->setPatient(*m_patient);
	m_patient.reset();

}

void PatientDialogPresenter::accept()
{

	if (!view->inputFieldsAreValid()) return;

	m_patient = getPatientFromView();
	
	if (rowid == 0) {
		m_patient->rowid = DbPatient::insert(m_patient.value());
		if (!m_patient->rowid) m_patient.reset();
	}
	else
	{
		if (!DbPatient::update(m_patient.value())) {
			m_patient.reset();
		}
	}
	
	view->close();
}

Patient PatientDialogPresenter::getPatientFromView()
{
	Patient patient = view->getPatient();

	patient.id = patientId;

	//if the patient has rowid, set the cached data
	if (rowid) { 
		patient.rowid = rowid;
		patient.teethNotes = teeth_notes;
		patient.patientNotes = patientNotes;
	}
	return patient;
}
