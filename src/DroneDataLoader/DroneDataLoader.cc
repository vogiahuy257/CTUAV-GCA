#include "DroneDataLoader.h"
#include <QFile>
#include <QJsonDocument>
#include <QDebug>

DroneDataLoader::DroneDataLoader(QObject* parent)
    : ParameterEditorController(parent) {}

void DroneDataLoader::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file:" << filePath;
        return;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format";
        return;
    }

    m_droneMap = doc.object();
    emit droneMapChanged();
}

void DroneDataLoader::confirmUploadParameters(const QString& filePath) {
    if (buildDiffFromFile(filePath)) {   // gọi trực tiếp vì đã kế thừa
        sendDiff();
        qDebug() << "Parameters uploaded from" << filePath;
    } else {
        qWarning() << "Failed to build diff from file" << filePath;
    }
}
