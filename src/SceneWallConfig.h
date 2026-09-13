#pragma once

#include <QColor>
#include <QHash>
#include <QList>
#include <QString>

struct SceneWallTab {
	QString id; // stabilny identyfikator (nie zmienia się przy zmianie nazwy)
	QString name;
	QString color;
	bool isAll = false;
};

struct SceneWallConfigData {
	QList<SceneWallTab> tabs;
	QHash<QString, QString> assignments; // uuid sceny -> id zakładki
	bool migrated = false;               // true, gdy wczytano stary format i trzeba zapisać
};

QString sceneWallConfigFilePath();
SceneWallConfigData loadSceneWallConfig();
void saveSceneWallConfig(const SceneWallConfigData &data);

QColor colorForScene(const SceneWallConfigData &data, const QString &uuid);
QString defaultHeaderColor();
QString newSceneWallTabId();
