#pragma once
#include <string>
#include <QCoreApplication>

constexpr const char* doctorPrefix = QT_TRANSLATE_NOOP("QObject", "dr. ");

struct Dentist
{
	long long rowID{ 0 };
	std::string fname;
	std::string lname;
	std::string pass;

	std::string getFullName(bool prefix = true) const
	{
		return prefix ?
			QCoreApplication::translate("QObject", doctorPrefix).toStdString() + fname + " " + lname
			:
			fname + " " + lname;
	}

};

struct Company
{
	std::string name;
	std::string identifier;
	std::string address;
	int vat{ 0 };
	std::string currency;
	std::string bank;
	std::string iban;
	std::string bic;
};

