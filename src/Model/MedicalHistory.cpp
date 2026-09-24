#include "MedicalHistory.h"

#include <QDateTime>
#include <QLocale>

using Section = MedicalHistoryItems::Section;

const std::vector<MedicalHistoryItems::Definition>& MedicalHistoryItems::all()
{
	static const std::vector<Definition> items{

		{ "cardiovascular", QT_TRANSLATE_NOOP("MedicalHistory", "Cardiovascular disease"), Section::Condition },
		{ "hypertension", QT_TRANSLATE_NOOP("MedicalHistory", "Hypertension"), Section::Condition },
		{ "diabetes", QT_TRANSLATE_NOOP("MedicalHistory", "Diabetes mellitus"), Section::Condition },
		{ "respiratory", QT_TRANSLATE_NOOP("MedicalHistory", "Respiratory disease"), Section::Condition },
		{ "renal", QT_TRANSLATE_NOOP("MedicalHistory", "Kidney disease"), Section::Condition },
		{ "hepatic", QT_TRANSLATE_NOOP("MedicalHistory", "Liver disease"), Section::Condition },
		{ "gastrointestinal", QT_TRANSLATE_NOOP("MedicalHistory", "Gastrointestinal disease"), Section::Condition },
		{ "hematologic", QT_TRANSLATE_NOOP("MedicalHistory", "Blood disorders"), Section::Condition },
		{ "endocrine", QT_TRANSLATE_NOOP("MedicalHistory", "Endocrine disorders"), Section::Condition },
		{ "neurologic", QT_TRANSLATE_NOOP("MedicalHistory", "Neurological disorders"), Section::Condition },
		{ "psychiatric", QT_TRANSLATE_NOOP("MedicalHistory", "Psychiatric history"), Section::Condition },
		{ "immune", QT_TRANSLATE_NOOP("MedicalHistory", "Immune / autoimmune disorders"), Section::Condition },
		{ "osteoporosis", QT_TRANSLATE_NOOP("MedicalHistory", "Osteoporosis"), Section::Condition },
		{ "cancer", QT_TRANSLATE_NOOP("MedicalHistory", "Cancer / history of malignancy"), Section::Condition },
		{ "infectious", QT_TRANSLATE_NOOP("MedicalHistory", "Infectious diseases"), Section::Condition },
		{ "other_condition", QT_TRANSLATE_NOOP("MedicalHistory", "Other conditions"), Section::Condition },

		{ "allergy_drug", QT_TRANSLATE_NOOP("MedicalHistory", "Drug allergies"), Section::Allergy },
		{ "allergy_food", QT_TRANSLATE_NOOP("MedicalHistory", "Food allergies"), Section::Allergy },
		{ "allergy_other", QT_TRANSLATE_NOOP("MedicalHistory", "Other allergies (e.g. latex)"), Section::Allergy },

		{ "bleeding_abnormal", QT_TRANSLATE_NOOP("MedicalHistory", "History of abnormal bleeding"), Section::Risk },
		{ "bleeding_disorder", QT_TRANSLATE_NOOP("MedicalHistory", "Bleeding disorders"), Section::Risk },
		{ "anticoagulant", QT_TRANSLATE_NOOP("MedicalHistory", "Anticoagulant / antiplatelet medication"), Section::Risk },
		{ "surgical_complications", QT_TRANSLATE_NOOP("MedicalHistory", "Complications after previous surgery"), Section::Risk },
		{ "anesthesia_problems", QT_TRANSLATE_NOOP("MedicalHistory", "Problems with anaesthesia"), Section::Risk },
		{ "other_risk", QT_TRANSLATE_NOOP("MedicalHistory", "Other relevant medical risks"), Section::Risk },

		{ "alcohol", QT_TRANSLATE_NOOP("MedicalHistory", "Alcohol consumption"), Section::Lifestyle },
		{ "recreational_drugs", QT_TRANSLATE_NOOP("MedicalHistory", "Recreational drug use"), Section::Lifestyle },

		{ "family_cardiovascular", QT_TRANSLATE_NOOP("MedicalHistory", "Cardiovascular disease"), Section::Family },
		{ "family_diabetes", QT_TRANSLATE_NOOP("MedicalHistory", "Diabetes mellitus"), Section::Family },
		{ "family_cancer", QT_TRANSLATE_NOOP("MedicalHistory", "Cancer"), Section::Family },
		{ "family_periodontal", QT_TRANSLATE_NOOP("MedicalHistory", "Periodontal disease"), Section::Family },
		{ "family_other", QT_TRANSLATE_NOOP("MedicalHistory", "Other hereditary conditions"), Section::Family },

		{ "dental_perio_treatment", QT_TRANSLATE_NOOP("MedicalHistory", "Previous periodontal treatment"), Section::Dental },
		{ "dental_treatment", QT_TRANSLATE_NOOP("MedicalHistory", "Previous dental treatment"), Section::Dental },
		{ "dental_perio_surgery", QT_TRANSLATE_NOOP("MedicalHistory", "Previous periodontal surgery"), Section::Dental },
		{ "dental_surgery", QT_TRANSLATE_NOOP("MedicalHistory", "Previous oral / dental surgery"), Section::Dental },
		{ "dental_complications", QT_TRANSLATE_NOOP("MedicalHistory", "Complications after dental treatment"), Section::Dental },
		{ "dental_orthodontic", QT_TRANSLATE_NOOP("MedicalHistory", "Previous orthodontic treatment"), Section::Dental },
	};

	return items;
}

std::vector<MedicalHistoryItems::Definition> MedicalHistoryItems::ofSection(Section section)
{
	std::vector<Definition> result;

	for (auto& d : all()) {
		if (d.section == section) result.push_back(d);
	}

	return result;
}

QString MedicalHistoryItems::label(const Definition& d)
{
	return QCoreApplication::translate("MedicalHistory", d.label);
}

MedicalHistoryAnswer MedicalHistory::answer(const std::string& key) const
{
	auto it = answers.find(key);

	if (it == answers.end()) return {};

	return it->second;
}

static QString qs(const std::string& s) { return QString::fromStdString(s); }

static QString warningSign() { return QString(QChar(0x26A0)) + " "; }

QStringList MedicalHistory::alerts(bool riskDetails) const
{
	QStringList result;

	QStringList allergies;

	for (auto& d : MedicalHistoryItems::ofSection(Section::Allergy)) {

		auto a = answer(d.key);

		if (a.value != MedicalHistoryAnswer::Yes) continue;

		allergies.append(a.details.empty() ? MedicalHistoryItems::label(d) : qs(a.details));
	}

	if (allergies.size()) {
		result.append(warningSign() + tr("Allergies: %1").arg(allergies.join("; ")));
	}

	for (auto& d : MedicalHistoryItems::ofSection(Section::Risk)) {

		auto a = answer(d.key);

		if (a.value != MedicalHistoryAnswer::Yes) continue;

		QString text = warningSign() + MedicalHistoryItems::label(d);

		if (riskDetails && a.details.size()) text += ": " + qs(a.details);

		result.append(text);
	}

	return result;
}

QStringList MedicalHistory::conditions() const
{
	QStringList result;

	for (auto& d : MedicalHistoryItems::ofSection(Section::Condition)) {
		if (answer(d.key).value == MedicalHistoryAnswer::Yes) {
			result.append(MedicalHistoryItems::label(d));
		}
	}

	return result;
}

static QString answerText(const MedicalHistoryAnswer& a)
{
	switch (a.value)
	{
	case MedicalHistoryAnswer::No: return MedicalHistory::tr("No");
	case MedicalHistoryAnswer::Yes:
		return a.details.empty() ?
			MedicalHistory::tr("Yes")
			:
			MedicalHistory::tr("Yes") + " (" + qs(a.details) + ")";
	default: return "-";
	}
}

static QString healthText(int h)
{
	switch (h)
	{
	case MedicalHistory::Good: return MedicalHistory::tr("Good");
	case MedicalHistory::Fair: return MedicalHistory::tr("Fair");
	case MedicalHistory::Poor: return MedicalHistory::tr("Poor");
	default: return "-";
	}
}

static QString smokingText(int s)
{
	switch (s)
	{
	case MedicalHistory::NonSmoker: return MedicalHistory::tr("Non-smoker");
	case MedicalHistory::FormerSmoker: return MedicalHistory::tr("Former smoker");
	case MedicalHistory::Smoker: return MedicalHistory::tr("Smoker");
	default: return "-";
	}
}

static QString medicationText(const Medication& m)
{
	QStringList parts{ qs(m.name) };

	if (m.dose.size()) parts.append(MedicalHistory::tr("Dose") + ": " + qs(m.dose));
	if (m.frequency.size()) parts.append(MedicalHistory::tr("Frequency") + ": " + qs(m.frequency));
	if (m.indication.size()) parts.append(MedicalHistory::tr("Indication") + ": " + qs(m.indication));
	if (m.notes.size()) parts.append(qs(m.notes));

	return parts.join(", ");
}

static QString surgeryText(const PastSurgery& s)
{
	QStringList parts{ qs(s.surgery) };

	if (s.date.size()) parts.append(MedicalHistory::tr("Date") + ": " + qs(s.date));
	if (s.reason.size()) parts.append(MedicalHistory::tr("Reason") + ": " + qs(s.reason));
	if (s.hospital.size()) parts.append(MedicalHistory::tr("Hospital") + ": " + qs(s.hospital));
	if (s.notes.size()) parts.append(qs(s.notes));

	return parts.join(", ");
}

QString MedicalHistory::savedToLocalFormat(const std::string& saved)
{
	auto dt = QDateTime::fromString(qs(saved), Qt::ISODate);

	if (!dt.isValid()) return qs(saved);

	return QLocale::system().toString(dt, QLocale::ShortFormat);
}

QString MedicalHistory::toHtml() const
{
	QString html;

	auto heading = [&](const QString& title) {
		html += "<h4 style=\"margin-bottom:2px\">" + title.toHtmlEscaped() + "</h4>";
	};

	auto line = [&](const QString& label, const QString& value) {
		if (value.isEmpty() || value == "-") return;
		html += "<b>" + label.toHtmlEscaped() + ":</b> " + value.toHtmlEscaped() + "<br>";
	};

	auto items = [&](Section section) {
		for (auto& d : MedicalHistoryItems::ofSection(section)) {
			auto a = answer(d.key);
			if (a.value == MedicalHistoryAnswer::NotAnswered) continue;
			line(MedicalHistoryItems::label(d), answerText(a));
		}
	};

	heading(tr("General information"));
	line(tr("Date of medical history"), qs(date.toLocalFormat()));
	line(tr("General health status"), healthText(generalHealth));
	line(tr("Primary physician"), qs(physician));
	line(tr("Physician contact information"), qs(physicianContact));

	heading(tr("Medical conditions"));
	items(Section::Condition);

	heading(tr("Medications"));
	for (auto& m : medications) html += "&bull; " + medicationText(m).toHtmlEscaped() + "<br>";

	heading(tr("Allergies"));
	items(Section::Allergy);
	line(tr("Description of reaction"), qs(allergyReaction));

	heading(tr("Previous surgeries and hospitalizations"));
	for (auto& s : surgeries) html += "&bull; " + surgeryText(s).toHtmlEscaped() + "<br>";

	heading(tr("Bleeding and medical risk"));
	items(Section::Risk);

	heading(tr("Lifestyle and social history"));
	line(tr("Smoking"), smokingText(smoking));
	if (smoking == Smoker) line(tr("Cigarettes per day"), QString::number(cigarettesPerDay));
	if (smoking == Smoker || smoking == FormerSmoker) line(tr("Years of smoking"), QString::number(smokingYears));
	if (smoking == FormerSmoker) line(tr("Smoking cessation"), qs(smokingQuit));
	items(Section::Lifestyle);
	line(tr("Other lifestyle information"), qs(lifestyleOther));

	heading(tr("Family history"));
	items(Section::Family);

	heading(tr("Dental history"));
	items(Section::Dental);
	line(tr("Oral hygiene habits"), qs(oralHygiene));
	line(tr("Other dental history"), qs(dentalOther));

	heading(tr("Clinical notes"));
	if (notes.size()) html += qs(notes).toHtmlEscaped().replace("\n", "<br>");

	return html;
}

QStringList MedicalHistory::differences(const MedicalHistory& before, const MedicalHistory& after)
{
	QStringList result;

	auto shorten = [](const QString& s) {
		QString oneLine = s.simplified();
		return oneLine.size() > 60 ? oneLine.left(57) + "..." : oneLine;
	};

	auto change = [&](const QString& label, const QString& oldValue, const QString& newValue) {
		if (oldValue == newValue) return;
		auto o = oldValue.isEmpty() ? QString("-") : shorten(oldValue);
		auto n = newValue.isEmpty() ? QString("-") : shorten(newValue);
		result.append(label + ": " + o + " " + QChar(0x2192) + " " + n);
	};

	change(tr("Date of medical history"), qs(before.date.toLocalFormat()), qs(after.date.toLocalFormat()));
	change(tr("General health status"), healthText(before.generalHealth), healthText(after.generalHealth));
	change(tr("Primary physician"), qs(before.physician), qs(after.physician));
	change(tr("Physician contact information"), qs(before.physicianContact), qs(after.physicianContact));

	for (auto& d : MedicalHistoryItems::all()) {
		change(MedicalHistoryItems::label(d), answerText(before.answer(d.key)), answerText(after.answer(d.key)));
	}

	change(tr("Description of reaction"), qs(before.allergyReaction), qs(after.allergyReaction));

	auto number = [](int n) { return n ? QString::number(n) : QString(); };

	change(tr("Smoking"), smokingText(before.smoking), smokingText(after.smoking));
	change(tr("Cigarettes per day"), number(before.cigarettesPerDay), number(after.cigarettesPerDay));
	change(tr("Years of smoking"), number(before.smokingYears), number(after.smokingYears));
	change(tr("Smoking cessation"), qs(before.smokingQuit), qs(after.smokingQuit));
	change(tr("Other lifestyle information"), qs(before.lifestyleOther), qs(after.lifestyleOther));
	change(tr("Oral hygiene habits"), qs(before.oralHygiene), qs(after.oralHygiene));
	change(tr("Other dental history"), qs(before.dentalOther), qs(after.dentalOther));
	change(tr("Clinical notes"), qs(before.notes), qs(after.notes));

	//lists: report removed and added entries
	auto listChanges = [&](const QString& label, const QStringList& oldList, const QStringList& newList) {
		for (auto& o : oldList) if (!newList.contains(o)) result.append(label + ": " + QChar(0x2212) + " " + o);
		for (auto& n : newList) if (!oldList.contains(n)) result.append(label + ": + " + n);
	};

	QStringList oldMeds, newMeds, oldSurgeries, newSurgeries;
	for (auto& m : before.medications) oldMeds.append(medicationText(m));
	for (auto& m : after.medications) newMeds.append(medicationText(m));
	for (auto& s : before.surgeries) oldSurgeries.append(surgeryText(s));
	for (auto& s : after.surgeries) newSurgeries.append(surgeryText(s));

	listChanges(tr("Medication"), oldMeds, newMeds);
	listChanges(tr("Surgery / hospitalization"), oldSurgeries, newSurgeries);

	return result;
}
