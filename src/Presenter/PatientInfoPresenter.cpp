#include "PatientInfoPresenter.h"

#include "Database/DbPatient.h"
#include "Database/DbNotes.h"
#include "Database/DbNotification.h"
#include "Model/User.h"
#include "Presenter/PatientDialogPresenter.h"
#include "Presenter/TabPresenter.h"
#include "View/ModalDialogBuilder.h"
#include "Model/TableRows.h"
#include "View/Widgets/NotificationDialog.h"
#include "View/SubWidgets/PatientTileInfo.h"
#include "View/Widgets/MedicalHistoryDialog.h"
#include "Database/DbMedicalHistory.h"
#include "Database/DbPatientFile.h"
#include "View/Widgets/PatientFilesWidget.h"
#include "Database/DbDentalVisit.h"
#include "Presenter/PatientHistoryPresenter.h"
#include "Presenter/RecallPresenter.h"
#include "Database/DbRecall.h"

PatientInfoPresenter::PatientInfoPresenter(PatientTileInfo* view, std::shared_ptr<Patient> p) :
    patient(p), view(view), doc_date(Date::currentDate())
{}

void PatientInfoPresenter::setDate(const Date& date)
{
    doc_date = date;

    if (!m_isCurrent) return;

    view->setPatient(*patient, patient->getAge(doc_date));
}


void PatientInfoPresenter::patientTileClicked()
{
    if (patient == nullptr) return;

    PatientDialogPresenter p{ *patient };

    auto patient = p.open();

    if (!patient.has_value()) return;

    *this->patient = patient.value();

    TabPresenter::get().refreshPatientTabNames(patient->rowid);

    view->setPatient(*patient, patient->getAge(doc_date));

    refreshMedicalHistory();
    refreshPatientFiles();

    if (m_parent) {
        m_parent->patientDataChanged();
    }
}

void PatientInfoPresenter::appointmentClicked()
{
    TabPresenter::get().openCalendar(CalendarEvent(*patient));
}

void PatientInfoPresenter::notesRequested()
{
    auto result = ModalDialogBuilder::showMultilineDialog(
        patient->patientNotes,
        QObject::tr("Patient notes").toStdString(),
        true
    );

    if (result) {
        DbNotes::saveNote(result.value(), patient->rowid, -1);
        patient->patientNotes = result.value();
        view->setPatient(*patient, patient->getAge(doc_date));
    }
}


void PatientInfoPresenter::setCurrent(bool isCurrent)
{
    m_isCurrent = isCurrent;

    if (!isCurrent) return;

    view->setPresenter(this);

    view->setPatient(*patient, patient->getAge(doc_date));

    refreshMedicalHistory();
    refreshPatientFiles();
    refreshVisitCount();
    refreshRecall();
}

void PatientInfoPresenter::recallRequested()
{
    if (patient == nullptr || patient->rowid <= 0) return;

    RecallActions::editRecall(patient->rowid);

    refreshRecall();
}

void PatientInfoPresenter::refreshRecall()
{
    view->setRecall(patient && patient->rowid > 0 ? DbRecall::get(patient->rowid) : std::nullopt);
}

void PatientInfoPresenter::notificationClicked()
{
    NotificationDialog d;
    d.exec();

    auto result = d.getResult();

    if(!result) return;

    result->patientRowid = patient->rowid;

    DbNotification::insert(result.value());
}

void PatientInfoPresenter::medicalHistoryRequested()
{
    if (patient == nullptr || patient->rowid <= 0) return;

    MedicalHistoryDialog d(patient->rowid, QString::fromStdString(patient->firstLastName()));
    d.exec();

    refreshMedicalHistory();
}

void PatientInfoPresenter::refreshMedicalHistory()
{
    if (patient == nullptr || patient->rowid <= 0) {
        view->setMedicalHistory({});
        return;
    }

    view->setMedicalHistory(DbMedicalHistory::getCurrent(patient->rowid));
}

void PatientInfoPresenter::patientFilesRequested()
{
    if (patient == nullptr || patient->rowid <= 0) return;

    PatientFilesWidget::openDialog(patient->rowid, QString::fromStdString(patient->firstLastName()));

    refreshPatientFiles();
}

void PatientInfoPresenter::refreshPatientFiles()
{
    view->setPatientFileCount(patient && patient->rowid > 0 ? DbPatientFile::count(patient->rowid) : 0);
}

void PatientInfoPresenter::visitHistoryRequested()
{
    if (patient == nullptr || patient->rowid <= 0) return;

    //the patient object is kept alive while the dialog is open (a visit may be opened from it)
    auto keepPatient = patient;

    PatientHistoryPresenter p(*keepPatient);

    p.openDialog(true);
}

void PatientInfoPresenter::refreshVisitCount()
{
    view->setVisitCount(patient && patient->rowid > 0 ? DbDentalVisit::count(patient->rowid) : 0);
}

void PatientInfoPresenter::openDocument(TabType type)
{
    RowInstance doc(type);
    doc.rowID = 0,
    doc.patientRowId = patient->rowid,

    TabPresenter::get().open(doc, true);
}
