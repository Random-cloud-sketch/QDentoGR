#pragma once

#include <string>
#include <QString>

//Converts text to capital letters, as used for the names and addresses of the patients.
//Works for Latin and Greek text. Greek words written in capitals have no tonos,
//so the accents are removed (ά -> Α, ί -> Ι) while the dialytika is kept (ϊ -> Ϊ, ΐ -> Ϊ).
//When a removed tonos separated two vowels, the dialytika is added (Μάιος -> ΜΑΪΟΣ).
namespace UpperCase
{
	QString convert(const QString& text);
	std::string convert(const std::string& text);

	//the cursor position in the converted text, for converting while the user types
	int convertedPosition(const QString& text, int position);
}
