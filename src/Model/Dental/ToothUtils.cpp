#include "ToothUtils.h"
#include <string>
#include "Tooth.h"
#include "Model/User.h"

constexpr std::array<int, 32> s_tooth_FDI
{
    18, 17, 16, 15, 14, 13, 12, 11, 21, 22, 23, 24, 25, 26, 27, 28,
    38, 37, 36, 35, 34, 33, 32, 31, 41, 42, 43, 44, 45, 46, 47, 48
};

constexpr std::array<char, 32> s_temp_ADA
{
    ' ',' ',' ','A','B','C','D','E','F','G','H','I','J',' ',' ',' ',
    ' ',' ',' ','K','L','M','N','O','P','Q','R','S','T',' ',' ',' '
};

Dental::Type ToothUtils::getToothType(int index)
{
    constexpr static std::array<int, 32> toothType
    {
        0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 1, 1, 0, 0, 0,
        0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 1, 1, 0, 0, 0
    };

    return static_cast<Dental::Type>(toothType[index]);
}

int ToothUtils::getToothNumber(int index, bool temporary)
{
    if (index < 0 || index > 31) return -1;

    if (User::ADA_num) { return index + 1; }

    if (temporary) {
        return s_tooth_FDI[index] + 40;
    }
    return s_tooth_FDI[index];
}

std::string ToothUtils::getNomenclature(int tooth_idx, bool temporary)
{
    if (User::ADA_num && temporary) {
        return std::string( 1, s_temp_ADA[tooth_idx] );
    }

    auto num = getToothNumber(tooth_idx, temporary);

    return std::to_string(num);
}

std::string ToothUtils::getNomenclature(const ToothIndex& t)
{
    if (t.supernumeral) return " ";

    return getNomenclature(t.index, t.temp);
}

Dental::Quadrant ToothUtils::getQuadrant(int index)
{
    if (index < 8) return Dental::Quadrant::First;
    if (index < 16) return Dental::Quadrant::Second;
    if (index < 24) return Dental::Quadrant::Third;

    return Dental::Quadrant::Fourth;

}

std::array<std::string, 6> ToothUtils::getSurfaceNames(int index)
{

    auto getOcclusalName = [&] {

        //canine
        if (index == 5 || index == 10 || index == 21 || index == 26) return QObject::tr("Cuspidal").toStdString();

        //incisor
        if (getToothType(index) == Dental::Type::Frontal) return QObject::tr("Incisal").toStdString();

        //molar and premolar
        return QObject::tr("Occlusal").toStdString();
    };

    auto getBuccalName = [&] {
        if (getToothType(index) == Dental::Type::Frontal) return QObject::tr("Labial").toStdString();

        return QObject::tr("Buccal").toStdString();

    };

    auto getLingualName = [&] {

        if (index < 16) return QObject::tr("Palatal").toStdString();

        return QObject::tr("Lingual").toStdString();
    };


    return std::array<std::string, 6>{
        getOcclusalName(),
            QObject::tr("Medial").toStdString(),
            QObject::tr("Distal").toStdString(),
            getBuccalName(),
            getLingualName(),
            QObject::tr("Cervical").toStdString()
    };


}

std::string ToothUtils::getName(int idx, bool temp)
{
    static std::string toothPerm[32]
    {
        QObject::tr("Upper right third molar").toStdString(),
        QObject::tr("Upper right second molar").toStdString(),
        QObject::tr("Upper right first molar").toStdString(),
        QObject::tr("Upper right second premolar").toStdString(),
        QObject::tr("Upper right first premolar").toStdString(),
        QObject::tr("Upper right canine").toStdString(),
        QObject::tr("Upper right lateral incisor").toStdString(),
        QObject::tr("Upper right central incisor").toStdString(),
        QObject::tr("Upper left central incisor").toStdString(),
        QObject::tr("Upper left lateral incisor").toStdString(),
        QObject::tr("Upper left canine").toStdString(),
        QObject::tr("Upper left first premolar").toStdString(),
        QObject::tr("Upper left second premolar").toStdString(),
        QObject::tr("Upper left first molar").toStdString(),
        QObject::tr("Upper left second molar").toStdString(),
        QObject::tr("Upper left third molar").toStdString(),

        QObject::tr("Lower left third molar").toStdString(),
        QObject::tr("Lower left second molar").toStdString(),
        QObject::tr("Lower left first molar").toStdString(),
        QObject::tr("Lower left second premolar").toStdString(),
        QObject::tr("Lower left first premolar").toStdString(),
        QObject::tr("Lower left canine").toStdString(),
        QObject::tr("Lower left lateral incisor").toStdString(),
        QObject::tr("Lower left central incisor").toStdString(),
        QObject::tr("Lower right central incisor").toStdString(),
        QObject::tr("Lower right lateral incisor").toStdString(),
        QObject::tr("Lower right canine").toStdString(),
        QObject::tr("Lower right first premolar").toStdString(),
        QObject::tr("Lower right second premolar").toStdString(),
        QObject::tr("Lower right first molar").toStdString(),
        QObject::tr("Lower right second molar").toStdString(),
        QObject::tr("Lower right third molar").toStdString(),
    };

    static std::string toothTemp[32]
    {
        QObject::tr("").toStdString(),
        QObject::tr("").toStdString(),
        QObject::tr("").toStdString(),
        QObject::tr("Upper right second primary molar").toStdString(),
        QObject::tr("Upper right first primary molar").toStdString(),
        QObject::tr("Upper right primary canine").toStdString(),
        QObject::tr("Upper right second primary incisor").toStdString(),
        QObject::tr("Upper right first primary incisor").toStdString(),
        QObject::tr("Upper left first primary incisor").toStdString(),
        QObject::tr("Upper left second primary incisor").toStdString(),
        QObject::tr("Upper left primary canine").toStdString(),
        QObject::tr("Upper left first primary molar").toStdString(),
        QObject::tr("Upper left second primary molar").toStdString(),
        QObject::tr("").toStdString(),
        QObject::tr("").toStdString(),
        QObject::tr("").toStdString(),

        QObject::tr("").toStdString(),
        QObject::tr("").toStdString(),
        QObject::tr("").toStdString(),
        QObject::tr("Lower left second primary molar").toStdString(),
        QObject::tr("Lower left first primary molar").toStdString(),
        QObject::tr("Lower left primary canine").toStdString(),
        QObject::tr("Lower left second primary incisor").toStdString(),
        QObject::tr("Lower left first primary incisor").toStdString(),
        QObject::tr("Lower right first primary incisor").toStdString(),
        QObject::tr("Lower right second primary incisor").toStdString(),
        QObject::tr("Lower right primary canine").toStdString(),
        QObject::tr("Lower right first primary molar").toStdString(),
        QObject::tr("Lower right second primary molar").toStdString(),
        QObject::tr("").toStdString(),
        QObject::tr("").toStdString(),
        QObject::tr("").toStdString(),
    };

    if (!(idx < 0) && (idx < 32)) {
        return temp ?
            toothTemp[idx]
            :
            toothPerm[idx];
    }

    return {};


}