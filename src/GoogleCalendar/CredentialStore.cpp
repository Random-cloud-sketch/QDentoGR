#include "CredentialStore.h"

#ifdef Q_OS_WIN

#include <windows.h>
#include <wincred.h>

bool CredentialStore::isAvailable()
{
	return true;
}

bool CredentialStore::write(const QString& target, const QString& secret)
{
	QByteArray blob = secret.toUtf8();
	std::wstring targetName = target.toStdWString();
	std::wstring userName = L"QDento";

	CREDENTIALW credential{};
	credential.Type = CRED_TYPE_GENERIC;
	credential.TargetName = targetName.data();
	credential.CredentialBlobSize = static_cast<DWORD>(blob.size());
	credential.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
	credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
	credential.UserName = userName.data();

	return CredWriteW(&credential, 0);
}

std::optional<QString> CredentialStore::read(const QString& target)
{
	PCREDENTIALW credential = nullptr;

	if (!CredReadW(target.toStdWString().c_str(), CRED_TYPE_GENERIC, 0, &credential)) return {};

	QString secret = QString::fromUtf8(reinterpret_cast<const char*>(credential->CredentialBlob), credential->CredentialBlobSize);

	CredFree(credential);

	return secret;
}

void CredentialStore::remove(const QString& target)
{
	CredDeleteW(target.toStdWString().c_str(), CRED_TYPE_GENERIC, 0);
}

#else

bool CredentialStore::isAvailable() { return false; }
bool CredentialStore::write(const QString&, const QString&) { return false; }
std::optional<QString> CredentialStore::read(const QString&) { return {}; }
void CredentialStore::remove(const QString&) {}

#endif
