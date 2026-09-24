#include "PatientTileInfo.h"

#include <QMenu>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QResizeEvent>

#include "Presenter/PatientInfoPresenter.h"
#include "View/Theme.h"

PatientTileInfo::PatientTileInfo(QWidget *parent)
	: RoundedFrame(parent)
{
	ui.setupUi(this);

	setFrameColor(Theme::border);

    //init context menu
    context_menu = new QMenu(this);

    QAction* action;

    action = (new QAction(tr("Edit"), context_menu));
    connect(action, &QAction::triggered, this, [=, this] { ui.patientTile->click(); });

    action->setIcon(QIcon(":/icons/icon_edit.png"));
    context_menu->addAction(action);

    action = (new QAction(tr("New Dental Visit"), context_menu));
    connect(action, &QAction::triggered, this, [=, this] { presenter->openDocument(TabType::DentalVisit); });
    action->setIcon(QIcon(":/icons/icon_sheet.png"));
    context_menu->addAction(action);

    action = (new QAction(tr("New Periodontal Measurment"), context_menu));
    connect(action, &QAction::triggered, this, [=, this] { presenter->openDocument(TabType::PerioStatus); });
    action->setIcon(QIcon(":/icons/icon_periosheet.png"));
    context_menu->addAction(action);

    action = (new QAction(tr("New Invoice"), context_menu));
    connect(action, &QAction::triggered, this, [=, this] { presenter->openDocument(TabType::Financial); });
    action->setIcon(QIcon(":/icons/icon_invoice.png"));
    context_menu->addAction(action);

    action = (new QAction(tr("Schedule and Appointment"), context_menu));
    connect(action, &QAction::triggered, this, [=, this] { presenter->openDocument(TabType::Calendar); });
    action->setIcon(QIcon(":/icons/icon_calendar.png"));
    context_menu->addAction(action);

    action = (new QAction(tr("Patient History"), context_menu));
    connect(action, &QAction::triggered, this, [=, this] { presenter->openDocument(TabType::PatientSummary); });
    action->setIcon(QIcon(":/icons/icon_history.png"));
    context_menu->addAction(action);

    action = (new QAction(tr("Medical history"), context_menu));
    connect(action, &QAction::triggered, this, [=, this] { if (presenter) presenter->medicalHistoryRequested(); });
    action->setIcon(QIcon(":/icons/icon_note.png"));
    context_menu->addAction(action);

    action = (new QAction(tr("Radiographs && documents"), context_menu));
    connect(action, &QAction::triggered, this, [=, this] { if (presenter) presenter->patientFilesRequested(); });
    action->setIcon(QIcon(":/icons/icon_open.png"));
    context_menu->addAction(action);

    //medical history strip below the patient tile (two lines)
    auto strip = new QWidget(this);
    strip->setFixedHeight(44);

    auto stripLayout = new QVBoxLayout(strip);
    stripLayout->setContentsMargins(12, 0, 12, 3);
    stripLayout->setSpacing(1);

    auto firstLine = new QHBoxLayout();
    firstLine->setSpacing(10);

    medicalHistoryButton = new QPushButton(QIcon(":/icons/icon_note.png"), tr("Medical history"), strip);
    medicalHistoryButton->setCursor(Qt::PointingHandCursor);
    medicalHistoryButton->setStyleSheet(
        "QPushButton{ color:" + Theme::colorToString(Theme::fontTurquoise) +
        " background-color: white; border: 1px solid" + Theme::colorToString(Theme::buttonFrame) +
        " border-radius: 9px; padding: 1px 10px; font-weight: bold; }"
        "QPushButton:hover{ background-color:" + Theme::colorToString(Theme::inactiveTabBG) + " }"
    );
    firstLine->addWidget(medicalHistoryButton);

    patientFilesButton = new QPushButton(QIcon(":/icons/icon_open.png"), tr("Radiographs && documents"), strip);
    patientFilesButton->setCursor(Qt::PointingHandCursor);
    patientFilesButton->setStyleSheet(medicalHistoryButton->styleSheet());
    firstLine->addWidget(patientFilesButton);

    for (auto& label : medicalHistoryLabels) {
        label = new QLabel(strip);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

    firstLine->addWidget(medicalHistoryLabels[0], 1);
    stripLayout->addLayout(firstLine);
    stripLayout->addWidget(medicalHistoryLabels[1]);

    ui.verticalLayout->addWidget(strip);

    connect(medicalHistoryButton, &QPushButton::clicked, this, [=, this] {
        if (presenter) presenter->medicalHistoryRequested();
    });

    connect(patientFilesButton, &QPushButton::clicked, this, [=, this] {
        if (presenter) presenter->patientFilesRequested();
    });

    context_menu->setStyleSheet(Theme::getPopupMenuStylesheet());

    //connect signalsh

    connect(ui.patientTile, &QPushButton::clicked, this, [=, this] {
		if (presenter) presenter->patientTileClicked();
	});

	connect (ui.patientTile->notesButton, &QPushButton::clicked, this, [=, this] {
		if (presenter) presenter->notesRequested();
	});

	connect (ui.patientTile->appointmentButton, &QPushButton::clicked, this, [=, this] {
		if (presenter) presenter->appointmentClicked();
	});

    connect (ui.patientTile->notificationButton, &QPushButton::clicked, this, [=, this]{
        if (presenter) presenter->notificationClicked();
    });

    connect(ui.patientTile, &TileButton::customContextMenuRequested, this, [&](QPoint point) {
        context_menu->popup(point);
    });

}

void PatientTileInfo::setPatient(const Patient& p, int age)
{
	ui.patientTile->setData(p, age);
}

void PatientTileInfo::setMedicalHistory(const std::optional<MedicalHistory>& history)
{
    //only the information entered by the dentist is shown - allergies and risks answered with "Yes"
    //(risk details are in the tooltip and in the medical history itself)
    auto alerts = history ? history->alerts(false) : QStringList();

    QString style = "color: " + Theme::colorToString(Theme::fontTurquoise);

    if (!history) {
        medicalHistoryItems = { tr("No medical history recorded") };
        style = "color: gray;";
    }
    else if (alerts.size()) {
        medicalHistoryItems = alerts;
        style = "color: rgb(200, 30, 30);";
    }
    else if (auto conditions = history->conditions(); conditions.size()) {
        medicalHistoryItems = { tr("Conditions: %1").arg(conditions.join(", ")) };
    }
    else {
        medicalHistoryItems = { tr("Updated %1").arg(MedicalHistory::savedToLocalFormat(history->saved)) };
    }

    QString tooltip;

    if (history) {

        tooltip = "<b>" + tr("Medical history") + "</b> (" + tr("Updated %1").arg(MedicalHistory::savedToLocalFormat(history->saved)) + ")<br>";

        QStringList escaped;
        for (auto& a : history->alerts()) escaped.append(a.toHtmlEscaped());

        if (escaped.size()) {
            tooltip += "<span style=\"color:rgb(200,30,30)\"><b>" + escaped.join("<br>") + "</b></span><br>";
        }

        tooltip += history->toHtml();
    }

    for (auto label : medicalHistoryLabels) {
        auto font = label->font();
        font.setBold(alerts.size());
        label->setFont(font);
        label->setStyleSheet(style);
        label->setToolTip(tooltip);
    }

    medicalHistoryButton->setToolTip(history ? tooltip : tr("Record the medical history of the patient"));

    elideMedicalHistoryText();
}

void PatientTileInfo::setPatientFileCount(int count)
{
    patientFilesButton->setText(count ?
        tr("Radiographs && documents (%1)").arg(count)
        :
        tr("Radiographs && documents")
    );

    patientFilesButton->setToolTip(count ?
        tr("%1 radiographs / documents").arg(count)
        :
        tr("No radiographs or documents yet")
    );

    elideMedicalHistoryText();
}

void PatientTileInfo::elideMedicalHistoryText()
{
    //fills the two lines with as many complete items as fit and shows the number of the remaining ones,
    //so that no warning is hidden without notice (all of them are in the tooltip)
    QFontMetrics metrics(medicalHistoryLabels[0]->font());

    const QString separator = "    ";

    auto more = [&](qsizetype count) {
        return count ? separator + tr("(+%1 more)").arg(count) : QString();
    };

    qsizetype next = 0;

    for (int line = 0; line < 2; line++)
    {
        const bool lastLine = line == 1;
        const int width = medicalHistoryLabels[line]->width();

        auto reserve = [&](qsizetype shownUntil) {
            return lastLine ? more(medicalHistoryItems.size() - shownUntil) : QString();
        };

        QString text;

        for (; next < medicalHistoryItems.size(); next++)
        {
            auto candidate = text.isEmpty() ? medicalHistoryItems[next] : text + separator + medicalHistoryItems[next];

            if (metrics.horizontalAdvance(candidate + reserve(next + 1)) > width) break;

            text = candidate;
        }

        //a single item longer than the whole line: moved to the next line, or shortened on the last one
        if (text.isEmpty() && next < medicalHistoryItems.size() && lastLine) {
            text = metrics.elidedText(medicalHistoryItems[next], Qt::ElideRight, width - metrics.horizontalAdvance(reserve(next + 1)));
            next++;
        }

        if (lastLine) text += more(medicalHistoryItems.size() - next);

        medicalHistoryLabels[line]->setText(text);
    }
}

void PatientTileInfo::resizeEvent(QResizeEvent* event)
{
    RoundedFrame::resizeEvent(event);

    elideMedicalHistoryText();
}

PatientTileInfo::~PatientTileInfo()
{}
