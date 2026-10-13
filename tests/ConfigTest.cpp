#include "../src/Config.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdio>

namespace {
int failures = 0;

void check(bool condition, const char *message)
{
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

QString uuid(char digit)
{
    return QString("00000000-0000-0000-0000-00000000000%1").arg(digit);
}

TabConfig tab(const QString &id, const QStringList &scenes)
{
    TabConfig result;
    result.id = id;
    result.name = id;
    result.scenes = scenes;
    return result;
}

CollectionConfig collection(const QString &id, const QList<TabConfig> &tabs)
{
    CollectionConfig result;
    result.collectionId = id;
    result.tabs = tabs;
    return result;
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    check(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "open fixture for writing");
    if (file.isOpen())
        file.write(contents);
}

void testRoundTripAndCollectionIsolation(const QString &directory)
{
    const QString path = directory + "/roundtrip.json";
    check(WallConfig().realtime, "new config defaults realtime on");
    WallConfig original;
    original.thumbSize = 224;
    original.realtime = true;

    CollectionConfig first = collection("Studio A", {tab(uuid('1'), {uuid('a')})});
    first.collapsedScenes.insert(uuid('a'));
    CollectionConfig second = collection("Studio B", {tab(uuid('2'), {uuid('b')})});
    second.collapsedScenes.insert(uuid('b'));
    original.collections = {first, second};

    check(saveWallConfig(original, path), "save versioned config");
    const WallConfig loaded = loadWallConfig(path);
    check(loaded.thumbSize == 224 && loaded.realtime, "round-trip global settings");
    check(loaded.collections.size() == 2, "round-trip separate collections");
    if (loaded.collections.size() == 2) {
        check(loaded.collections[0].collectionId == "Studio A", "first collection key preserved");
        check(loaded.collections[0].tabs[0].scenes == QStringList{uuid('a')},
              "first collection scene UUID preserved");
        check(loaded.collections[0].collapsedScenes.contains(uuid('a')),
              "first collection collapse state preserved");
        check(loaded.collections[1].collectionId == "Studio B", "second collection isolated");
        check(loaded.collections[1].tabs[0].scenes == QStringList{uuid('b')},
              "second collection scenes isolated");
        check(loaded.collections[1].collapsedScenes.contains(uuid('b')),
              "second collection collapse state isolated");
    }
}

void testRenameKeepsSceneUuid(const QString &directory)
{
    const QString path = directory + "/rename.json";
    WallConfig config;
    config.collections = {collection("Studio", {tab(uuid('3'), {uuid('c')})})};
    check(saveWallConfig(config, path), "save scene assignment before rename");

    WallConfig loaded = loadWallConfig(path);
    const QMap<QString, QString> liveSources{{uuid('c'), "Renamed scene"}};
    QSet<QString> liveUuids;
    for (auto it = liveSources.cbegin(); it != liveSources.cend(); ++it)
        liveUuids.insert(it.key());
    filterDeletedScenes(loaded.collections[0], liveUuids);
    check(loaded.collections[0].tabs[0].scenes == QStringList{uuid('c')},
          "scene assignment follows UUID after display-name change");
}

void testLegacyNameMigration(const QString &directory)
{
    const QString path = directory + "/legacy.json";
    QJsonArray scenes{QStringLiteral("Intro"), QStringLiteral("Ambiguous"), QStringLiteral("Missing")};
    QJsonObject legacyTab{{"id", uuid('4')}, {"name", "Main"}, {"color", "#123456"},
                          {"scenes", scenes}};
    QJsonObject legacy{{"version", 2}, {"thumbSize", 160}, {"realtime", false},
                       {"tabs", QJsonArray{legacyTab}}};
    writeFile(path, QJsonDocument(legacy).toJson());

    QMap<QString, QStringList> nameToUuids;
    nameToUuids.insert("Intro", {uuid('d')});
    nameToUuids.insert("Ambiguous", {uuid('e'), uuid('f')});
    WallConfig loaded = loadWallConfig(path, "Studio", nameToUuids);
    check(loaded.version == WallConfig::CURRENT_VERSION, "legacy config upgraded to current version");
    check(!loaded.realtime, "legacy user realtime setting preserved");
    check(loaded.collections.size() == 1, "legacy global tabs assigned to current collection");
    if (loaded.collections.size() == 1) {
        const TabConfig &migrated = loaded.collections[0].tabs[0];
        check(loaded.collections[0].collectionId == "Studio", "legacy collection key assigned");
        check(migrated.scenes == QStringList{uuid('d')}, "only exact unique legacy name migrated");
        check(migrated.legacySceneNames.isEmpty(), "legacy names consumed once");
    }
}

void testBareArrayMigration(const QString &directory)
{
    const QString path = directory + "/v1.json";
    writeFile(path, QJsonDocument(QJsonArray{QJsonObject{{"name", "Default"},
                                                        {"color", "#808080"}}})
                             .toJson());
    const WallConfig loaded = loadWallConfig(path, "Studio");
    check(loaded.collections.size() == 1, "bare global tab list loads into one collection");
    if (loaded.collections.size() == 1) {
        check(loaded.collections[0].tabs.size() == 1, "bare global tab list keeps tab");
        if (!loaded.collections[0].tabs.isEmpty())
            check(loaded.collections[0].tabs[0].isAll, "first bare global tab remains All");
    }
}

void testDeletedSceneFiltering()
{
    CollectionConfig active = collection(
            "Studio", {tab(uuid('5'), {uuid('1'), uuid('2')}), tab(uuid('6'), {uuid('2')})});
    active.collapsedScenes = {uuid('1'), uuid('2')};
    filterDeletedScenes(active, {uuid('2')});
    check(active.tabs[0].scenes == QStringList{uuid('2')}, "deleted scene removed from assignment");
    check(active.tabs[1].scenes == QStringList{uuid('2')}, "live scene retained in other tab");
    check(active.collapsedScenes == QSet<QString>{uuid('2')}, "deleted collapse state removed");
}

void testEmptySnapshotPreservesAssignments()
{
    // An empty OBS snapshot (startup / collection switch) must not erase
    // saved assignments: migration and filtering stay no-ops until a real
    // scene list arrives.
    CollectionConfig active = collection(
            "Studio", {tab(uuid('5'), {uuid('1'), uuid('2')}), tab(uuid('6'), {uuid('2')})});
    active.tabs[0].legacySceneNames = {"Intro"};
    active.collapsedScenes = {uuid('1'), uuid('2')};

    migrateLegacySceneNames(active, {});
    check(active.tabs[0].legacySceneNames == QStringList{"Intro"},
          "empty snapshot keeps legacy names for later migration");
    check(active.tabs[0].scenes == QStringList{uuid('1'), uuid('2')},
          "empty snapshot does not touch migrated scenes");

    filterDeletedScenes(active, {});
    check(active.tabs[0].scenes == QStringList{uuid('1'), uuid('2')},
          "empty snapshot keeps saved scene assignments");
    check(active.tabs[1].scenes == QStringList{uuid('2')},
          "empty snapshot keeps other tab assignments");
    check(active.collapsedScenes == QSet<QString>{uuid('1'), uuid('2')},
          "empty snapshot keeps collapse state");
}

void testSaveFailureIsReported(const QString &directory)
{
    // saveWallConfig must report failure (not look successful) so callers
    // can keep edits instead of losing them silently.
    WallConfig config;
    config.collections = {collection("Studio", {tab(uuid('9'), {uuid('a')})})};
    check(!saveWallConfig(config, directory + "/no-such-dir/config.json"),
          "unwritable path reports save failure");
}

void testUnsafeWritesDoNotReplaceConfig(const QString &directory)
{
    const QString path = directory + "/protected.json";
    WallConfig valid;
    valid.collections = {collection("Studio", {tab(uuid('7'), {})})};
    check(saveWallConfig(valid, path), "write initial valid config");
    const QByteArray original = readFile(path);

    WallConfig revised = valid;
    revised.realtime = false;
    check(saveWallConfig(revised, path), "replace valid config atomically");
    const QByteArray current = readFile(path);

    WallConfig invalidSize = revised;
    invalidSize.thumbSize = 0;
    check(!saveWallConfig(invalidSize, path), "reject invalid thumbnail size");
    check(readFile(path) == current, "invalid size leaves config untouched");

    WallConfig duplicate = revised;
    duplicate.collections[0].tabs.append(tab(uuid('7'), {}));
    check(!saveWallConfig(duplicate, path), "reject duplicate tab UUID");
    check(readFile(path) == current, "duplicate UUID leaves config untouched");

    WallConfig duplicateScene = revised;
    duplicateScene.collections[0].tabs[0].scenes = {uuid('8'), uuid('8')};
    check(!saveWallConfig(duplicateScene, path), "reject repeated scene UUID");
    check(readFile(path) == current, "repeated scene UUID leaves config untouched");

    check(readFile(path + ".bak") == original, "previous valid config copied to backup");

    const QString malformedPath = directory + "/malformed.json";
    const QByteArray malformed = "{";
    writeFile(malformedPath, malformed);
    check(!saveWallConfig(valid, malformedPath), "reject overwrite of malformed config");
    check(readFile(malformedPath) == malformed, "malformed source config remains untouched");

    const QString emptyPath = directory + "/empty.json";
    writeFile(emptyPath, {});
    check(!saveWallConfig(valid, emptyPath), "reject overwrite of empty config");
    check(readFile(emptyPath).isEmpty(), "empty source config remains untouched");

    QJsonObject invalidSizeRoot = QJsonDocument::fromJson(original).object();
    invalidSizeRoot["thumbSize"] = 401;
    const QString invalidSizePath = directory + "/invalid-size.json";
    const QByteArray invalidSizeSource = QJsonDocument(invalidSizeRoot).toJson();
    writeFile(invalidSizePath, invalidSizeSource);
    check(!saveWallConfig(valid, invalidSizePath), "reject existing config with invalid size");
    check(readFile(invalidSizePath) == invalidSizeSource,
          "invalid-size source config remains untouched");

    QJsonObject duplicateRoot = QJsonDocument::fromJson(original).object();
    QJsonArray duplicateCollections = duplicateRoot["collections"].toArray();
    QJsonObject duplicateCollection = duplicateCollections[0].toObject();
    QJsonArray duplicateTabs = duplicateCollection["tabs"].toArray();
    duplicateTabs.append(duplicateTabs[0]);
    duplicateCollection["tabs"] = duplicateTabs;
    duplicateCollections[0] = duplicateCollection;
    duplicateRoot["collections"] = duplicateCollections;
    const QString duplicatePath = directory + "/duplicate.json";
    const QByteArray duplicateSource = QJsonDocument(duplicateRoot).toJson();
    writeFile(duplicatePath, duplicateSource);
    check(!saveWallConfig(valid, duplicatePath), "reject existing duplicate-UUID config");
    check(readFile(duplicatePath) == duplicateSource,
          "duplicate-UUID source config remains untouched");
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) {
        std::fprintf(stderr, "FAIL: create test directory\n");
        return 1;
    }

    testRoundTripAndCollectionIsolation(directory.path());
    testRenameKeepsSceneUuid(directory.path());
    testLegacyNameMigration(directory.path());
    testBareArrayMigration(directory.path());
    testDeletedSceneFiltering();
    testEmptySnapshotPreservesAssignments();
    testSaveFailureIsReported(directory.path());
    testUnsafeWritesDoNotReplaceConfig(directory.path());

    if (failures)
        return 1;
    std::puts("All config tests passed.");
    return 0;
}
