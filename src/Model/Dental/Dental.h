#pragma once
#include <QObject>
#include <QCoreApplication>
#include <string>
#include <string_view>
//Some important enums and string literals
namespace Dental
{
	enum Type { Molar, Premolar, Frontal };

	constexpr int teethCount = 32;

	enum Surface { Occlusal, Medial, Distal, Buccal, Lingual, Cervical, SurfaceCount };
	enum Quadrant { First, Second, Third, Fourth };
	enum BridgePos { Begin, Middle, End };
	enum MobilityDegree { I, II, III, MobilityCount };
	enum class StatusType { General, Restoration, Caries, NonCariesLesion, DefectiveRestoration, Mobility };

	enum Status {
		Healthy,
		Temporary,
		Restoration,
		Caries,
		DefectiveRestoration,
		NonCariesLesion,
		Pulpitis,
		Necrosis,
		Resorption,
		ApicalLesion,
		RootCanal,
		Post,
		Root,
		Fracture,
		Missing,
		Periodontitis,
		Mobility,
		Crown,
		Bridge,
		Splint,
		Implant,
		HasSupernumeral,
		Impacted,
		Denture,
		Calculus,
		StatusCount
	};

	//The names are translated when accessed (not at static initialization),
	//so the translator installed in main() applies to them
	struct TranslatedNames
	{
		const char* const* source;

		std::string operator[](int index) const
		{
			return QCoreApplication::translate("QObject", source[index]).toStdString();
		}
	};

	inline constexpr const char* statusNamesSource[StatusCount]
	{
		QT_TRANSLATE_NOOP("QObject", "Healthy Tooth"), 
		QT_TRANSLATE_NOOP("QObject", "Primary Tooth"), 
		QT_TRANSLATE_NOOP("QObject", "Restoration"),
		QT_TRANSLATE_NOOP("QObject", "Caries"),
		QT_TRANSLATE_NOOP("QObject", "Defective Restoration"),
		QT_TRANSLATE_NOOP("QObject", "Non-Caries Lesion"),
		QT_TRANSLATE_NOOP("QObject", "Pulpitis"),
		QT_TRANSLATE_NOOP("QObject", "Necrosis"),
		QT_TRANSLATE_NOOP("QObject", "Resorption"),
		QT_TRANSLATE_NOOP("QObject", "Apical Lesion"),
		QT_TRANSLATE_NOOP("QObject", "Root Canal Treatment"),
		QT_TRANSLATE_NOOP("QObject", "Radicular Post"),
		QT_TRANSLATE_NOOP("QObject", "Rood / Severely Destroyed Tooth"),
		QT_TRANSLATE_NOOP("QObject", "Fracture"),
		QT_TRANSLATE_NOOP("QObject", "Missing Tooth"),
		QT_TRANSLATE_NOOP("QObject", "Periodontitis"),
		QT_TRANSLATE_NOOP("QObject", "Mobility"),
		QT_TRANSLATE_NOOP("QObject", "Crown"),
		QT_TRANSLATE_NOOP("QObject", "Bridge"),
		QT_TRANSLATE_NOOP("QObject", "Splint / Adhesive Bridge"),
		QT_TRANSLATE_NOOP("QObject", "Implant"),
		QT_TRANSLATE_NOOP("QObject", "Supernumeral Tooth"),
		QT_TRANSLATE_NOOP("QObject", "Impacted Tooth"),
		QT_TRANSLATE_NOOP("QObject", "Denture"),
		QT_TRANSLATE_NOOP("QObject", "Calculus")
	};

	inline constexpr const char* mobilityNamesSource[MobilityCount]
	{
		QT_TRANSLATE_NOOP("QObject", "Mobility I"), 
		QT_TRANSLATE_NOOP("QObject", "Mobility II"),
		QT_TRANSLATE_NOOP("QObject", "Mobility III")
	};

	inline constexpr const char* surfaceNamesSource[SurfaceCount]
	{
		QT_TRANSLATE_NOOP("QObject", "Occlusal/Incisal"), 
		QT_TRANSLATE_NOOP("QObject", "Medial"), 
		QT_TRANSLATE_NOOP("QObject", "Distal"),
		QT_TRANSLATE_NOOP("QObject", "Vestibular"),
		QT_TRANSLATE_NOOP("QObject", "Lingual"),
		QT_TRANSLATE_NOOP("QObject", "Cervical")
	};

	inline const TranslatedNames statusNames{ statusNamesSource };
	inline const TranslatedNames mobilityNames{ mobilityNamesSource };
	inline const TranslatedNames surfaceNames{ surfaceNamesSource };

}