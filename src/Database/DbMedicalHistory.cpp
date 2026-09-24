#include "DbMedicalHistory.h"

#include "Database/Database.h"

static const char* columns =
	"rowid, patient_rowid, dentist_rowid, saved, history_date, general_health, "
	"physician, physician_contact, allergy_reaction, smoking, cigarettes_per_day, "
	"smoking_years, smoking_quit, lifestyle_other, oral_hygiene, dental_other, notes";

static MedicalHistory readHistory(Db& db)
{
	MedicalHistory h;

	h.rowid = db.asRowId(0);
	h.patientRowid = db.asRowId(1);
	h.dentistRowid = db.asRowId(2);
	h.saved = db.asString(3);
	h.date = Date(db.asString(4));
	h.generalHealth = db.asInt(5);
	h.physician = db.asString(6);
	h.physicianContact = db.asString(7);
	h.allergyReaction = db.asString(8);
	h.smoking = db.asInt(9);
	h.cigarettesPerDay = db.asInt(10);
	h.smokingYears = db.asInt(11);
	h.smokingQuit = db.asString(12);
	h.lifestyleOther = db.asString(13);
	h.oralHygiene = db.asString(14);
	h.dentalOther = db.asString(15);
	h.notes = db.asString(16);

	return h;
}

static void readDetails(MedicalHistory& h, Db& db)
{
	auto id = std::to_string(h.rowid);

	db.newStatement("SELECT item, answer, details FROM medical_history_item WHERE medical_history_rowid = " + id);

	while (db.hasRows()) {
		h.answers[db.asString(0)] = MedicalHistoryAnswer{ db.asInt(1), db.asString(2) };
	}

	db.newStatement(
		"SELECT name, dose, frequency, indication, notes FROM medical_history_medication "
		"WHERE medical_history_rowid = " + id + " ORDER BY rowid"
	);

	while (db.hasRows()) {
		h.medications.push_back(Medication{
			db.asString(0), db.asString(1), db.asString(2), db.asString(3), db.asString(4)
		});
	}

	db.newStatement(
		"SELECT surgery, date, reason, hospital, notes FROM medical_history_surgery "
		"WHERE medical_history_rowid = " + id + " ORDER BY rowid"
	);

	while (db.hasRows()) {
		h.surgeries.push_back(PastSurgery{
			db.asString(0), db.asString(1), db.asString(2), db.asString(3), db.asString(4)
		});
	}
}

std::optional<MedicalHistory> DbMedicalHistory::getCurrent(long long patientRowid)
{
	Db db(
		std::string("SELECT ") + columns + " FROM medical_history "
		"WHERE patient_rowid = " + std::to_string(patientRowid) + " ORDER BY rowid DESC LIMIT 1"
	);

	if (!db.hasRows()) return {};

	auto h = readHistory(db);

	readDetails(h, db);

	return h;
}

MedicalHistory DbMedicalHistory::get(long long rowid)
{
	Db db(
		std::string("SELECT ") + columns + " FROM medical_history WHERE rowid = " + std::to_string(rowid)
	);

	MedicalHistory h;

	if (db.hasRows()) {
		h = readHistory(db);
		readDetails(h, db);
	}

	return h;
}

std::vector<DbMedicalHistory::VersionInfo> DbMedicalHistory::getVersions(long long patientRowid)
{
	std::vector<VersionInfo> result;

	Db db(
		"SELECT rowid, saved, dentist_rowid FROM medical_history "
		"WHERE patient_rowid = " + std::to_string(patientRowid) + " ORDER BY rowid DESC"
	);

	while (db.hasRows()) {
		result.push_back(VersionInfo{ db.asRowId(0), db.asString(1), db.asRowId(2) });
	}

	return result;
}

long long DbMedicalHistory::insertVersion(const MedicalHistory& h)
{
	Db db;

	db.execute("BEGIN TRANSACTION");

	db.newStatement(
		"INSERT INTO medical_history "
		"(patient_rowid, dentist_rowid, saved, history_date, general_health, "
		"physician, physician_contact, allergy_reaction, smoking, cigarettes_per_day, "
		"smoking_years, smoking_quit, lifestyle_other, oral_hygiene, dental_other, notes) "
		"VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"
	);

	db.bind(1, h.patientRowid);

	if (h.dentistRowid) db.bind(2, h.dentistRowid); else db.bindNull(2);

	db.bind(3, h.saved);
	db.bind(4, h.date.to8601());
	db.bind(5, h.generalHealth);
	db.bind(6, h.physician);
	db.bind(7, h.physicianContact);
	db.bind(8, h.allergyReaction);
	db.bind(9, h.smoking);
	db.bind(10, h.cigarettesPerDay);
	db.bind(11, h.smokingYears);
	db.bind(12, h.smokingQuit);
	db.bind(13, h.lifestyleOther);
	db.bind(14, h.oralHygiene);
	db.bind(15, h.dentalOther);
	db.bind(16, h.notes);

	bool ok = db.execute();

	long long rowid = ok ? db.lastInsertedRowID() : 0;

	for (auto& [item, a] : h.answers)
	{
		if (!ok) break;

		if (a.value == MedicalHistoryAnswer::NotAnswered) continue;

		db.newStatement(
			"INSERT INTO medical_history_item (medical_history_rowid, item, answer, details) VALUES (?,?,?,?)"
		);

		db.bind(1, rowid);
		db.bind(2, item);
		db.bind(3, a.value);
		db.bind(4, a.details);

		ok = db.execute();
	}

	for (auto& m : h.medications)
	{
		if (!ok) break;

		db.newStatement(
			"INSERT INTO medical_history_medication "
			"(medical_history_rowid, name, dose, frequency, indication, notes) VALUES (?,?,?,?,?,?)"
		);

		db.bind(1, rowid);
		db.bind(2, m.name);
		db.bind(3, m.dose);
		db.bind(4, m.frequency);
		db.bind(5, m.indication);
		db.bind(6, m.notes);

		ok = db.execute();
	}

	for (auto& s : h.surgeries)
	{
		if (!ok) break;

		db.newStatement(
			"INSERT INTO medical_history_surgery "
			"(medical_history_rowid, surgery, date, reason, hospital, notes) VALUES (?,?,?,?,?,?)"
		);

		db.bind(1, rowid);
		db.bind(2, s.surgery);
		db.bind(3, s.date);
		db.bind(4, s.reason);
		db.bind(5, s.hospital);
		db.bind(6, s.notes);

		ok = db.execute();
	}

	db.execute(ok ? "COMMIT" : "ROLLBACK");

	return ok ? rowid : 0;
}
