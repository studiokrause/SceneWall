#include "SceneWallConfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUuid>
#include <QtGlobal>

#include <obs-module.h>

#include "Locale.h"

namespace {
const char *kLegacyAllTabName = "All";
} // namespace

QString defaultHeaderColor()
{
	return QStringLiteral("#808080");
}

QString newSceneWallTabId()
{
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString sceneWallConfigFilePath()
{
	char *path = obs_module_config_path("tabs.json");
	if (path) {
		const QString result = QString::fromUtf8(path);
		bfree(path);
		return result;
	}

	const QString base = qEnvironmentVariable("APPDATA");
	if (base.isEmpty())
		return QString();
	return base + "/obs-studio/plugin_config/SceneWall/tabs.json";
}

SceneWallConfigData loadSceneWallConfig()
{
	SceneWallConfigData data;
	QJsonObject rawAssignments;

	QFile file(sceneWallConfigFilePath());
	if (file.exists() && file.open(QIODevice::ReadOnly)) {
		const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
		file.close();

		const QJsonArray tabArray = doc.isArray() ? doc.array() : doc.object().value("tabs").toArray();
		for (const QJsonValue &value : tabArray) {
			const QJsonObject tab = value.toObject();
			SceneWallTab def;
			def.id = tab.value("id").toString();
			def.name = tab.value("name").toString();
			def.color = tab.value("color").toString();
			def.isAll = tab.value("isAll").toBool(false);
			if (def.color.isEmpty())
				def.color = defaultHeaderColor();
			if (!def.name.isEmpty())
				data.tabs.append(def);
		}

		if (doc.isObject())
			rawAssignments = doc.object().value("assignments").toObject();
	}

	// Każda zakładka musi mieć stabilne id (starsze konfiguracje go nie mają).
	for (SceneWallTab &tab : data.tabs) {
		if (tab.id.isEmpty()) {
			tab.id = newSceneWallTabId();
			data.migrated = true;
		}
	}

	// Ustal zakładkę "All": flaga isAll, potem stara nazwa, na końcu dodaj nową.
	int allIndex = -1;
	for (int i = 0; i < data.tabs.size(); ++i) {
		if (data.tabs[i].isAll) {
			allIndex = i;
			break;
		}
	}
	if (allIndex < 0) {
		for (int i = 0; i < data.tabs.size(); ++i) {
			if (data.tabs[i].name == QLatin1String(kLegacyAllTabName)) {
				allIndex = i;
				break;
			}
		}
	}
	if (allIndex < 0) {
		SceneWallTab all;
		all.id = newSceneWallTabId();
		all.name = T("All");
		all.color = defaultHeaderColor();
		all.isAll = true;
		data.tabs.prepend(all);
		allIndex = 0;
		data.migrated = true;
	}
	for (int i = 0; i < data.tabs.size(); ++i)
		data.tabs[i].isAll = (i == allIndex);

	if (data.tabs[allIndex].name == QLatin1String(kLegacyAllTabName)) {
		data.tabs[allIndex].name = T("All");
		data.migrated = true;
	}

	// Przypisania: nowy format to id zakładki. Migruj stare wartości (nazwy zakładek).
	QSet<QString> ids;
	QHash<QString, QString> nameToId;
	for (const SceneWallTab &tab : data.tabs) {
		ids.insert(tab.id);
		nameToId.insert(tab.name, tab.id);
	}

	for (auto it = rawAssignments.begin(); it != rawAssignments.end(); ++it) {
		const QString sceneUuid = it.key();
		const QString value = it.value().toString();
		if (value.isEmpty())
			continue;

		if (ids.contains(value)) {
			data.assignments.insert(sceneUuid, value);
		} else if (nameToId.contains(value)) {
			data.assignments.insert(sceneUuid, nameToId.value(value));
			data.migrated = true;
		} else {
			// Przypisanie do nieistniejącej zakładki — pomijamy.
			data.migrated = true;
		}
	}

	return data;
}

void saveSceneWallConfig(const SceneWallConfigData &data)
{
	const QString path = sceneWallConfigFilePath();
	if (path.isEmpty())
		return;

	QDir().mkpath(QFileInfo(path).absolutePath());

	QJsonArray tabArray;
	for (const SceneWallTab &tab : data.tabs) {
		QJsonObject tabObj;
		tabObj["id"] = tab.id;
		tabObj["name"] = tab.name;
		tabObj["color"] = tab.color;
		tabObj["isAll"] = tab.isAll;
		tabArray.append(tabObj);
	}

	QJsonObject assignments;
	for (auto it = data.assignments.begin(); it != data.assignments.end(); ++it)
		assignments.insert(it.key(), it.value());

	QJsonObject root;
	root["tabs"] = tabArray;
	root["assignments"] = assignments;

	QFile file(path);
	if (file.open(QIODevice::WriteOnly)) {
		file.write(QJsonDocument(root).toJson());
		file.close();
	}
}

QColor colorForScene(const SceneWallConfigData &data, const QString &uuid)
{
	const QString tabId = data.assignments.value(uuid);
	if (!tabId.isEmpty()) {
		for (const SceneWallTab &tab : data.tabs) {
			if (!tab.isAll && tab.id == tabId) {
				const QColor color(tab.color);
				if (color.isValid())
					return color;
				break;
			}
		}
	}

	// Scena nieprzypisana: użyj koloru zakładki "All" z ustawień pluginu.
	for (const SceneWallTab &tab : data.tabs) {
		if (tab.isAll) {
			const QColor color(tab.color);
			if (color.isValid())
				return color;
			break;
		}
	}

	return QColor(defaultHeaderColor());
}
