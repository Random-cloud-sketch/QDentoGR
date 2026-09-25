#pragma once

#include <QString>
#include <optional>

//Secrets of the program (the Google refresh token) in the credential store of the operating system:
//Windows Credential Manager. Other systems are not supported (isAvailable() is false).
namespace CredentialStore
{
	bool isAvailable();
	bool write(const QString& target, const QString& secret);
	std::optional<QString> read(const QString& target);
	void remove(const QString& target);
}
