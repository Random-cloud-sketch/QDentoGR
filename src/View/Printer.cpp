#include "Printer.h"

#include <QString>
#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

#include "GlobalFunctions.h"
#include "Model/Financial/Invoice.h"
#include "ModalDialogBuilder.h"

void Printer::printInvoice(const Invoice& i)
{

    const QString header = R"HTML(
        
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8" />
      <meta name="viewport" content="width=device-width, initial-scale=1.0" />
      <title>Invoice</title>
      <style>
        body {
          font-family: "Segoe UI", Tahoma, Geneva, Verdana, sans-serif;
          margin: 40px;
          color: #333;
        }
        .invoice-box {
          max-width: 800px;
          margin: auto;
          padding: 30px;
          border: 1px solid #eee;
          box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        }
        h1, h2, h3 {
          margin-bottom: 0;
        }
        .header,
        .footer {
          text-align: center;
        }
        .details {
          margin: 20px 0;
          display: flex;
          justify-content: space-between;
        }
        .details div {
          width: 45%;
        }
        table {
          width: 100%;
          border-collapse: collapse;
          margin-top: 20px;
        }
        th, td {
          border: 1px solid #ddd;
          padding: 10px;
          text-align: left;
        }
        th {
          background-color: #f4f4f4;
        }
        .bottom-section {
          display: flex;
          justify-content: space-between;
          margin-top: 20px;
        }
        .payment-details {
          width: 50%;
        }
        .payment-details p, .total p {
          margin: 5px 0;
        }
        .total {
          width: 45%;
          text-align: right;
        }
      </style>
    </head>
    <body>
      <div class="invoice-box">
        <div class="header">
          <h1>%1</h1>
          <p>%2</p>
          <p>%3</p>
        </div>

       <div class="details">
           <div>
            <h3>%4</h3>
            <p>
              %5<br />
              %6<br />
              %7<br />
              %8
            </p>
          </div>
          <div>
            <h3>%9</h3>
            <p>
              %10<br />
              %11<br />
              %12<br />
              %13
            </p>
          </div>
        </div>

        <table>
          <thead>
            <tr>
              <th>%14</th>
              <th>%15</th>
              <th>%16</th>
              <th>%17</th>
              <th>%18</th>
            </tr>
          </thead>

      <tbody>
    )HTML";

    QString title;

    switch (i.type)
    {
    case FinancialDocType::Invoice:
        title = QObject::tr("Invoice");
        break;
    case FinancialDocType::Debit:
        title = QObject::tr("Debit Note");
        break;
    case FinancialDocType::Credit:
        title = QObject::tr("Credit Note");
        break;
    }

    QString subtitle = title;
    subtitle += " #" + i.getInvoiceNumber();
    subtitle += " ";
    subtitle += i.date.toLocalFormat().c_str();

    QString mainDoc;

    auto mD = i.mainDocument();

    if (mD) {
        mainDoc += QObject::tr("Reference Invoice #");
        mainDoc += QString::number(mD->number);
        mainDoc += " ";
        mainDoc += mD->date.toLocalFormat().c_str();
    }


    auto issuer = i.issuer();

    QString result = QString(header).replace("<title>Invoice</title>", "<title>" + title + "</title>")
        .arg(title)
        .arg(subtitle)
        .arg(mainDoc)

        .arg(QObject::tr("Recipient:"))
        .arg(QObject::tr("Name: ") + i.recipient.name.c_str())
        .arg(QObject::tr("ID: ") + i.recipient.identifier.c_str())
        .arg(QObject::tr("Address: ") + i.recipient.address.c_str())
        .arg(QObject::tr("Phone number: ") + i.recipient.phone.c_str())

        .arg(QObject::tr("Issuer:"))
        .arg(QObject::tr("Name: ") + issuer.company_name.c_str())
        .arg(QObject::tr("ID: ") + issuer.identifier.c_str())
        .arg(QObject::tr("Address: ") + issuer.address.c_str())
        .arg(QObject::tr(" "))

        .arg(QObject::tr("Code:"))
        .arg(QObject::tr("Description:"))
        .arg(QObject::tr("Quantity:"))
        .arg(QObject::tr("Unit Price:"))
        .arg(QObject::tr("Total:"));

    const QString services =
        R"HTML(
        <tr>
          <td>%1</td>
          <td>%2</td>
          <td>%3</td>
          <td>%4</td>
          <td>%5</td>
        </tr>
        )HTML";

    for (auto& o : i.businessOperations)
    {
        result += services
            .arg(o.activity_code.c_str())
            .arg(o.activity_name.c_str())
            .arg(QString::number(o.quantity))
            .arg(priceToString(o.unit_price))
            .arg(priceToString(o.value_price()));
    }

    const QString footer =
        R"HTML(
              </tbody>
            </table>
               <div class="bottom-section">
                  <div class="payment-details">
                    <h3>%1</h3>
                    <p><strong>%2:</strong> %3</p>
                    <p><strong>%4:</strong> %5</p>
                    <p><strong>%6:</strong> %7</p>
                    <p><strong>%8:</strong> %9</p>
                    <p><strong>%10:</strong> %11</p>
                  </div>
                  <div class="total">
                    <p><strong>%12:</strong> %13</p>
                    <p><strong>%14:</strong> %15</p>
                    <p><strong>%16:</strong> %17</p>
                  </div>
                </div>
              </div>
            </body>
            </html>
        )HTML";

    QString paymentType;

    switch (i.paymentType)
    {
    case PaymentType::Cash:
        paymentType = QObject::tr("Cash");
        break;
    case PaymentType::Bank:
        paymentType = QObject::tr("Bank");
        break;
    case PaymentType::POS:
        paymentType = QObject::tr("POS");
        break;
    case PaymentType::Combined:
        paymentType = QObject::tr("Combined");
        break;
    }

    result += footer
        .arg(QObject::tr("Payment Details"))
        .arg(QObject::tr("Tax Event Date")).arg(i.taxEventDate.toLocalFormat().c_str())
        .arg(QObject::tr("Type of Payment")).arg(paymentType)
        .arg(QObject::tr("Bank")).arg(issuer.bank.c_str())
        .arg(QObject::tr("IBAN")).arg(issuer.iban.c_str())
        .arg(QObject::tr("BIC")).arg(issuer.bic.c_str())

        .arg(QObject::tr("Subtotal")).arg(priceToString(i.amount()))
        .arg(QObject::tr("VAT ") + QString::number(i.VAT) + "%").arg(priceToString(i.VATamount()))
        .arg(QObject::tr("Total")).arg(priceToString(i.amount() + i.VATamount()));

    ModalDialogBuilder::saveFile(result.toStdString(), title.toStdString() + " " + i.getInvoiceNumber() + ".html", "HTML (*html)");
}

