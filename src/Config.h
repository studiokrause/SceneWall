#pragma once
// Persistence model for SceneWall. Header-only: no Q_OBJECT, no moc needed.

#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUuid>

struct TabConfig {
    QString id;
    QString name; // empty + isAll -> translated "All"
    QString colorHex = "#808080";
    bool isAll = false;
    QStringList scenes; // assignment AND order for this tab

    QColor color() const
    {
        QColor c(colorHex);
        return c.isValid() ? c : QColor(128, 128, 128);
    }
};

struct WallConfig {
    int version = 2;
    int thumbSize = 160;
    bool realtime = false;
    QList<TabConfig> tabs;

    int indexOfTab(const QString &tabId) const
    {
        for (int i = 0; i < tabs.size(); ++i)
            if (tabs[i].id == tabId)
                return i;
        return -1;
    }
};

inline QString getCfgPath()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                   "/obs-studio/plugin_config/SceneWall/";
    QDir().mkpath(path);
    return path + "tabs.json";
}

// Loads the config, migrating the old format (a bare JSON array of
// {"name","color"} objects) to the current versioned layout.
inline WallConfig loadWallConfig()
{
    WallConfig cfg;

    QFile file(getCfgPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return cfg;

    QByteArray raw = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(raw);

    if (doc.isArray()) {
        // v1: [{"name": "...", "color": "#..."}, ...]
        bool first = true;
        for (const auto &val : doc.array()) {
            QJsonObject o = val.toObject();
            TabConfig tab;
            tab.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            tab.name = o["name"].toString();
            tab.colorHex = o["color"].toString("#808080");
            tab.isAll = first;
            first = false;
            cfg.tabs.append(tab);
        }
        return cfg;
    }

    QJsonObject root = doc.object();
    cfg.version = root["version"].toInt(2);
    cfg.thumbSize = root["thumbSize"].toInt(160);
    cfg.realtime = root["realtime"].toBool(false);

    for (const auto &val : root["tabs"].toArray()) {
        QJsonObject o = val.toObject();
        TabConfig tab;
        tab.id = o["id"].toString();
        if (tab.id.isEmpty())
            tab.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        tab.name = o["name"].toString();
        tab.colorHex = o["color"].toString("#808080");
        tab.isAll = o["isAll"].toBool(false);
        for (const auto &s : o["scenes"].toArray())
            tab.scenes.append(s.toString());
        cfg.tabs.append(tab);
    }

    return cfg;
}

inline void saveWallConfig(const WallConfig &cfg)
{
    QJsonArray tabs;
    for (const TabConfig &t : cfg.tabs) {
        QJsonObject o;
        o["id"] = t.id;
        o["name"] = t.name;
        o["color"] = t.colorHex;
        o["isAll"] = t.isAll;
        QJsonArray scenes;
        for (const QString &s : t.scenes)
            scenes.append(s);
        o["scenes"] = scenes;
        tabs.append(o);
    }

    QJsonObject root;
    root["version"] = cfg.version;
    root["thumbSize"] = cfg.thumbSize;
    root["realtime"] = cfg.realtime;
    root["tabs"] = tabs;

    QFile file(getCfgPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.close();
    }
}
