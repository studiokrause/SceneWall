#pragma once
// Persistence model for SceneWall; the OBS-backed no-argument loader lives in SceneWall.cpp.

#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUuid>

struct TabConfig {
    QString id;
    QString name; // empty + isAll -> translated "All"
    QString colorHex = "#808080";
    bool isAll = false;
    QStringList scenes; // stable OBS scene-source UUIDs, in tab order
    QStringList legacySceneNames; // consumed once when migrating pre-v3 configs

    QColor color() const
    {
        QColor c(colorHex);
        return c.isValid() ? c : QColor(128, 128, 128);
    }
};

struct CollectionConfig {
    QString collectionId;
    QList<TabConfig> tabs;
    QSet<QString> collapsedScenes;

    int indexOfTab(const QString &tabId) const
    {
        for (int i = 0; i < tabs.size(); ++i)
            if (tabs[i].id == tabId)
                return i;
        return -1;
    }
};

struct WallConfig {
    static constexpr int CURRENT_VERSION = 3;

    int version = CURRENT_VERSION;
    int thumbSize = 160;
    bool realtime = true;
    QList<CollectionConfig> collections;

    int indexOfCollection(const QString &collectionId) const
    {
        for (int i = 0; i < collections.size(); ++i)
            if (collections[i].collectionId == collectionId)
                return i;
        return -1;
    }

    CollectionConfig &ensureCollection(const QString &collectionId)
    {
        int i = indexOfCollection(collectionId);
        if (i < 0) {
            CollectionConfig collection;
            collection.collectionId = collectionId;
            collections.append(collection);
            i = collections.size() - 1;
        }
        return collections[i];
    }
};

// Implemented with the OBS frontend API in SceneWall.cpp.
QString currentSceneCollectionKey();

inline QString getCfgPath()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                   "/obs-studio/plugin_config/SceneWall/";
    QDir().mkpath(path);
    return path + "tabs.json";
}

inline void migrateLegacySceneNames(CollectionConfig &collection,
                                    const QMap<QString, QStringList> &sceneNameToUuids)
{
    // An empty snapshot (OBS startup / collection switch) proves nothing:
    // keep legacy names until a real scene list arrives.
    if (sceneNameToUuids.isEmpty())
        return;
    for (TabConfig &tab : collection.tabs) {
        for (const QString &name : tab.legacySceneNames) {
            const QStringList matches = sceneNameToUuids.value(name);
            if (matches.size() == 1 && !tab.scenes.contains(matches.first()))
                tab.scenes.append(matches.first());
        }
        tab.legacySceneNames.clear();
    }
}

inline void filterDeletedScenes(CollectionConfig &collection, const QSet<QString> &liveSceneUuids)
{
    // An empty snapshot (OBS startup / collection switch) proves nothing:
    // keep saved assignments until a real scene list arrives.
    if (liveSceneUuids.isEmpty())
        return;
    for (TabConfig &tab : collection.tabs)
        for (int i = tab.scenes.size() - 1; i >= 0; --i)
            if (!liveSceneUuids.contains(tab.scenes[i]))
                tab.scenes.removeAt(i);

    for (auto it = collection.collapsedScenes.begin(); it != collection.collapsedScenes.end();) {
        if (!liveSceneUuids.contains(*it))
            it = collection.collapsedScenes.erase(it);
        else
            ++it;
    }
}

namespace sceneWallConfigDetail {
inline constexpr int MIN_THUMB_SIZE = 80;
inline constexpr int MAX_THUMB_SIZE = 400;

inline bool normalizeUuid(QString *value)
{
    const QUuid uuid(*value);
    if (uuid.isNull())
        return false;
    *value = uuid.toString(QUuid::WithoutBraces);
    return true;
}

inline bool valid(const WallConfig &config, bool allowLegacySceneNames = false)
{
    if (config.thumbSize < MIN_THUMB_SIZE || config.thumbSize > MAX_THUMB_SIZE)
        return false;

    QSet<QString> collectionIds;
    for (const CollectionConfig &collection : config.collections) {
        if (collectionIds.contains(collection.collectionId))
            return false;
        collectionIds.insert(collection.collectionId);

        QSet<QString> tabIds;
        for (const TabConfig &tab : collection.tabs) {
            const QString tabId = QUuid(tab.id).toString(QUuid::WithoutBraces);
            if (QUuid(tab.id).isNull() || tabIds.contains(tabId) ||
                (!allowLegacySceneNames && !tab.legacySceneNames.isEmpty()))
                return false;
            tabIds.insert(tabId);

            QSet<QString> scenes;
            for (const QString &sceneUuid : tab.scenes) {
                const QString normalized = QUuid(sceneUuid).toString(QUuid::WithoutBraces);
                if (QUuid(sceneUuid).isNull() || scenes.contains(normalized))
                    return false;
                scenes.insert(normalized);
            }
        }

        QSet<QString> collapsed;
        for (const QString &sceneUuid : collection.collapsedScenes)
            if (QUuid(sceneUuid).isNull() ||
                collapsed.contains(QUuid(sceneUuid).toString(QUuid::WithoutBraces)))
                return false;
            else
                collapsed.insert(QUuid(sceneUuid).toString(QUuid::WithoutBraces));
    }
    return true;
}

inline bool stringArray(const QJsonValue &value, QStringList *strings)
{
    if (!value.isArray())
        return false;
    for (const QJsonValue &item : value.toArray()) {
        if (!item.isString() || item.toString().isEmpty())
            return false;
        strings->append(item.toString());
    }
    return true;
}

inline bool parseTab(const QJsonValue &value, bool legacyNames, bool strict, TabConfig *tab)
{
    if (!value.isObject())
        return false;
    const QJsonObject object = value.toObject();
    if (strict && (!object.value("id").isString() || object.value("id").toString().isEmpty() ||
                   !object.value("name").isString() || !object.value("color").isString() ||
                   !object.value("isAll").isBool() || !object.value("scenes").isArray()))
        return false;
    tab->id = object.value("id").toString();
    if (tab->id.isEmpty())
        tab->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tab->name = object.value("name").toString();
    tab->colorHex = object.value("color").toString("#808080");
    tab->isAll = object.value("isAll").toBool(false);

    if (strict && !normalizeUuid(&tab->id))
        return false;

    const QJsonValue scenes = object.value("scenes");
    if (scenes.isUndefined())
        return true;
    QStringList values;
    if (!stringArray(scenes, &values))
        return false;
    if (strict)
        for (QString &sceneUuid : values)
            if (!normalizeUuid(&sceneUuid))
                return false;
    if (legacyNames)
        tab->legacySceneNames = values;
    else
        tab->scenes = values;
    return true;
}

inline bool parseConfig(const QByteArray &raw, const QString &legacyCollectionId,
                        const QMap<QString, QStringList> &sceneNameToUuids, WallConfig *result)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(raw, &error);
    if (error.error != QJsonParseError::NoError || document.isNull())
        return false;

    WallConfig config;
    if (document.isArray()) {
        CollectionConfig collection;
        collection.collectionId = legacyCollectionId;
        bool first = true;
        for (const QJsonValue &value : document.array()) {
            TabConfig tab;
            if (!parseTab(value, false, false, &tab))
                return false;
            tab.isAll = first;
            first = false;
            collection.tabs.append(tab);
        }
        config.collections.append(collection);
    } else if (document.isObject()) {
        const QJsonObject root = document.object();
        const int version = root.value("version").toInt(2);
        if (version == WallConfig::CURRENT_VERSION) {
            const QJsonValue collectionsValue = root.value("collections");
            if (!collectionsValue.isArray())
                return false;
            for (const QJsonValue &value : collectionsValue.toArray()) {
                if (!value.isObject())
                    return false;
                const QJsonObject source = value.toObject();
                if (!source.value("collectionId").isString() || !source.value("tabs").isArray() ||
                    !source.value("collapsedScenes").isArray())
                    return false;

                CollectionConfig collection;
                collection.collectionId = source.value("collectionId").toString();
                for (const QJsonValue &tabValue : source.value("tabs").toArray()) {
                    TabConfig tab;
                    if (!parseTab(tabValue, false, true, &tab))
                        return false;
                    collection.tabs.append(tab);
                }

                QStringList collapsed;
                if (!stringArray(source.value("collapsedScenes"), &collapsed))
                    return false;
                for (QString &sceneUuid : collapsed)
                    if (!normalizeUuid(&sceneUuid))
                        return false;
                collection.collapsedScenes = QSet<QString>(collapsed.begin(), collapsed.end());
                if (collection.collapsedScenes.size() != collapsed.size())
                    return false;
                config.collections.append(collection);
            }

            if (root.contains("thumbSize")) {
                if (!root.value("thumbSize").isDouble())
                    return false;
                config.thumbSize = root.value("thumbSize").toInt(-1);
            }
            if (root.contains("realtime")) {
                if (!root.value("realtime").isBool())
                    return false;
                config.realtime = root.value("realtime").toBool();
            }
        } else if (version == 2) {
            if (!root.value("tabs").isArray())
                return false;
            CollectionConfig collection;
            collection.collectionId = legacyCollectionId;
            for (const QJsonValue &value : root.value("tabs").toArray()) {
                TabConfig tab;
                if (!parseTab(value, true, false, &tab))
                    return false;
                collection.tabs.append(tab);
            }
            config.collections.append(collection);
            if (root.contains("thumbSize")) {
                if (!root.value("thumbSize").isDouble())
                    return false;
                config.thumbSize = root.value("thumbSize").toInt(-1);
            }
            if (root.contains("realtime")) {
                if (!root.value("realtime").isBool())
                    return false;
                config.realtime = root.value("realtime").toBool();
            }
        } else {
            return false;
        }
    } else {
        return false;
    }

    config.version = WallConfig::CURRENT_VERSION;
    if (!valid(config, true))
        return false;

    if (!sceneNameToUuids.isEmpty())
        for (CollectionConfig &collection : config.collections)
            migrateLegacySceneNames(collection, sceneNameToUuids);

    *result = config;
    return true;
}

inline QJsonObject tabObject(const TabConfig &tab)
{
    QJsonArray scenes;
    for (const QString &sceneUuid : tab.scenes)
        scenes.append(sceneUuid);

    QJsonObject object;
    object["id"] = tab.id;
    object["name"] = tab.name;
    object["color"] = tab.colorHex;
    object["isAll"] = tab.isAll;
    object["scenes"] = scenes;
    return object;
}

inline QByteArray serialize(const WallConfig &config)
{
    QJsonArray collections;
    for (const CollectionConfig &collection : config.collections) {
        QJsonArray tabs;
        for (const TabConfig &tab : collection.tabs)
            tabs.append(tabObject(tab));

        QJsonArray collapsed;
        for (const QString &sceneUuid : collection.collapsedScenes)
            collapsed.append(sceneUuid);

        QJsonObject object;
        object["collectionId"] = collection.collectionId;
        object["tabs"] = tabs;
        object["collapsedScenes"] = collapsed;
        collections.append(object);
    }

    QJsonObject root;
    root["version"] = WallConfig::CURRENT_VERSION;
    root["thumbSize"] = config.thumbSize;
    root["realtime"] = config.realtime;
    root["collections"] = collections;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}
} // namespace sceneWallConfigDetail

inline WallConfig loadWallConfig(const QString &path, const QString &legacyCollectionId = {},
                                 const QMap<QString, QStringList> &sceneNameToUuids = {})
{
    WallConfig config;
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return config;

    WallConfig loaded;
    const QByteArray raw = file.readAll();
    if (file.error() == QFileDevice::NoError &&
        sceneWallConfigDetail::parseConfig(raw, legacyCollectionId, sceneNameToUuids, &loaded))
        return loaded;
    return config;
}

WallConfig loadWallConfig();

inline bool saveWallConfig(const WallConfig &config, const QString &path)
{
    if (!sceneWallConfigDetail::valid(config))
        return false;

    QFile previous(path);
    if (previous.exists()) {
        if (!previous.open(QIODevice::ReadOnly))
            return false;
        const QByteArray previousData = previous.readAll();
        const bool readOk = previous.error() == QFileDevice::NoError;
        previous.close();

        WallConfig checked;
        if (!readOk || !sceneWallConfigDetail::parseConfig(previousData, {}, {}, &checked))
            return false;

        QSaveFile backup(path + ".bak");
        if (!backup.open(QIODevice::WriteOnly) || backup.write(previousData) != previousData.size() ||
            !backup.commit())
            return false;
    }

    const QByteArray data = sceneWallConfigDetail::serialize(config);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size())
        return false;
    return file.commit();
}

inline bool saveWallConfig(const WallConfig &config)
{
    return saveWallConfig(config, getCfgPath());
}
