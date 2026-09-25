#include "FinancialPresenter.h"

#include "Database/DbInvoice.h"
#include "Model/User.h"
#include "Presenter/RecipientPresenter.h"
#include "GlobalSettings.h"
#include "View/Widgets/TabView.h"
#include "View/Printer.h"

FinancialPresenter::FinancialPresenter(TabView* tabView, std::shared_ptr<Patient> patient, const std::vector<Procedure>& procedures) :
    TabInstance(tabView, TabType::Financial, patient),
    view(tabView->financialView()),
    m_invoice(*patient.get())
{
    m_invoice.date = Date::currentDate();

    m_invoice.taxEventDate = m_invoice.date;
    for (auto& p : procedures) {

        BusinessOperation newOp(p);
        
        bool toBeInserted{ true };

        for (auto& existingOp : m_invoice.businessOperations) {

            if (existingOp == newOp) //&& existingOp.unit_price == p.value)
            {
                existingOp.addQuantity(1);
                toBeInserted = false;
                break;
            }
        }

        if(toBeInserted) m_invoice.businessOperations.push_back(newOp);
    }

    if (!procedures.size()) m_invoice.taxEventDate = Date::currentDate();

    m_invoice.number = DbInvoice::getNewInvoiceNumber();

    //an invoice filled with the chosen procedures is not discarded without asking
    edited = !procedures.empty();
}

FinancialPresenter::FinancialPresenter(TabView* tabView, long long rowId) :
    TabInstance(tabView, TabType::Financial, nullptr),
    view(tabView->financialView()),
    m_invoice(DbInvoice::getInvoice(rowId))
{
}

FinancialPresenter::FinancialPresenter(TabView* tabView, const Recipient& r) :
    TabInstance(tabView, TabType::Financial, nullptr),
    view(tabView->financialView()),
    m_invoice(r)
{
    m_invoice.date = Date::currentDate();
    m_invoice.number = DbInvoice::getNewInvoiceNumber();

}

void FinancialPresenter::editOperation(int idx)
{
    if (idx < 0 || idx >= m_invoice.businessOperations.size()) return;;

    auto op = ModalDialogBuilder::editBusinessOperation(m_invoice.businessOperations[idx]);

    if (!op.has_value()) return;

    m_invoice.editOperation(op.value(), idx);
    view->setBusinessOperations(m_invoice.businessOperations, m_invoice.amount(), m_invoice.VAT);
    makeEdited();
}

void FinancialPresenter::addOperation()
{
    auto newOp = ModalDialogBuilder::addBusinessOperation();

    if (newOp.has_value())
        m_invoice.addOperation(newOp.value());

    view->setBusinessOperations(m_invoice.businessOperations, m_invoice.amount(), m_invoice.VAT);

    makeEdited();
}

void FinancialPresenter::removeOperation(int idx)
{
    if (idx < 0 || idx >= m_invoice.businessOperations.size()) return;

    m_invoice.removeOperation(idx);
    view->setBusinessOperations(m_invoice.businessOperations, m_invoice.amount(), m_invoice.VAT);
    makeEdited();
}

void FinancialPresenter::dateChanged(Date date)
{
    m_invoice.date = date;
    makeEdited();
}

void FinancialPresenter::taxEventDateChanged(Date date)
{
    m_invoice.taxEventDate = date;
    makeEdited();
}

void FinancialPresenter::paymentTypeChanged(PaymentType type)
{
    m_invoice.paymentType = type;
    makeEdited();
}

void FinancialPresenter::docTypeChanged(int index)
{
    m_invoice.type = static_cast<FinancialDocType>(index);

    //forcing refresh of the tab name:
    edited = false;
    makeEdited();

    if (m_invoice.type != FinancialDocType::Invoice) {

        auto mainDocDb = DbInvoice::getMainDocument(m_invoice.recipient.identifier, m_invoice.rowId);

        if (mainDocDb) {
            m_invoice.setMainDocumentData(mainDocDb->number, mainDocDb->date);
        }

    }

    view->setMainDocument(m_invoice.mainDocument());
}

void FinancialPresenter::mainDocumentChanged(long long num, Date date)
{
    //we assume, that since ui has enabled mainDocument,
    //the optional main doc has value:
    if (m_invoice.mainDocument()->number != num) {
        
        //it means that user has changed the number, so
        //we get the date from db if main document with that number already exist:

        auto db_date = DbInvoice::getMainDocDate(num, m_invoice.recipient.identifier);

        if (db_date)
            date = db_date.value();
    }

    m_invoice.setMainDocumentData(num, date);

    view->setMainDocument(m_invoice.mainDocument());

    makeEdited();
}

void FinancialPresenter::editRecipient()
{
    RecipientPresenter p(m_invoice.recipient);

    auto result = p.openDialog();

    if (!result) return;

    m_invoice.recipient = result.value();

    view->setInvoice(m_invoice);

    makeEdited();
}

void FinancialPresenter::editIssuer()
{
    ModalDialogBuilder::openSettingsDialog(ModalDialogBuilder::SettingsTab::Company);
    view->setInvoice(m_invoice);
}

void FinancialPresenter::invoiceNumberChanged(long long number)
{
    m_invoice.number = number;
    edited = false;
    makeEdited();
}

void FinancialPresenter::vatChanged(bool isVat)
{

    m_invoice.VAT = isVat ? m_invoice.issuer().vat : 0;

    view->setBusinessOperations(m_invoice.businessOperations, m_invoice.amount(), m_invoice.VAT);

    makeEdited();
}

long long FinancialPresenter::rowID() const
{
	return m_invoice.rowId;
}

bool FinancialPresenter::save()
{
    if (m_invoice.businessOperations.empty()) {
        ModalDialogBuilder::showError(
            QObject::tr("The financial document must contain at least one service").toStdString()
        );
        return false;
    }

    if(DbInvoice::invoiceAlreadyExists(m_invoice.number, m_invoice.rowId) &&
        !ModalDialogBuilder::askDialog(
            QObject::tr("Financial document with such number already exists. Are you sure you want to duplicate the numbering?").toStdString()
        )
    )
    {
        return false;
    }

    if (!isNew() && !edited) return true;

    if (isNew())
    {
        m_invoice.rowId = DbInvoice::insertInvoice(m_invoice);
    }
    else
    {
        DbInvoice::updateInvoice(m_invoice);

    }
    edited = false;

    refreshTabName();

	return true;
}

void FinancialPresenter::setDataToView()
{
    view->setPresenter(this);

    if (patient) {
        m_invoice.recipient = Recipient{ *patient.get() }; //refreshing the patient incase it's changed
    }

    view->setInvoice(m_invoice);
    view->setNumberSpinBox(m_invoice.number);
}

bool FinancialPresenter::isNew()
{
    return m_invoice.rowId == 0;
}

TabName FinancialPresenter::getTabName()
{
    int nameIdx = static_cast<int>(m_invoice.type);

    if (!m_invoice.rowId) {

        static const std::string newName[3]{ 
            QObject::tr("New Invoice").toStdString(), 
            QObject::tr("New Debit Note").toStdString(),  
            QObject::tr("New Credit Note").toStdString() };

        TabName tab;
        tab.header = newName[nameIdx];

        tab.header_icon = CommonIcon::INVOICE;

        tab.footer = m_invoice.recipient.name;

        return tab;
    }

    static const std::string docTypeName[3]{ 
        QObject::tr("Invoice").toStdString(), 
        QObject::tr("Debit note").toStdString(), 
        QObject::tr("Credit note").toStdString() };

    TabName tab;
    tab.header = docTypeName[nameIdx] + " " + m_invoice.getInvoiceNumber()
        ;
    tab.header_icon = CommonIcon::INVOICE;

    tab.footer = m_invoice.recipient.name;

    return tab;
}

void FinancialPresenter::print()
{
    if (!save()) return;

    Printer::printInvoice(m_invoice);
}