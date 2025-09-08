#pragma once

#include "ParameterEditorController.h" 
#include <QJsonObject>

class DroneDataLoader : public ParameterEditorController {
    Q_OBJECT
    Q_PROPERTY(QJsonObject droneMap READ droneMap NOTIFY droneMapChanged)

public:
    explicit DroneDataLoader(QObject* parent = nullptr);

    Q_INVOKABLE void loadFromFile(const QString& filePath);
    Q_INVOKABLE void confirmUploadParameters(const QString& filePath);

    QJsonObject droneMap() const { return m_droneMap; }

signals:
    void droneMapChanged();

private:
    QJsonObject m_droneMap;
};
