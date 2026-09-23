#pragma once
#include "View/uiComponents/TileButton.h"

struct Recipient;
struct Issuer;

class RecipientTileButton : public TileButton
{
	Q_DECLARE_TR_FUNCTIONS(RecipientTileButton)

	QString name;
	QString id;
	QString address;
	QString phone;
	void paintInfo(QPainter* painter) override;

public:
	RecipientTileButton(QWidget* parent) : TileButton(parent) {};
	void setRecipient(const Recipient& r);

};

class IssuerTileButton : public TileButton
{
	Q_DECLARE_TR_FUNCTIONS(IssuerTileButton)

	QString name;
	QString id;
	QString address;
	QString phone;

	void paintInfo(QPainter* painter) override;

public:
	IssuerTileButton(QWidget* parent) : TileButton(parent) { m_reveresed = true; };
	void setIssuer(const Issuer& r);

};
