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

TabConfig tab(const QString &id, const QStringList &scenes, int thumbSize = 160)
{
    TabConfig result;
    result.id = id;
    result.name = id;
    result.scenes = scenes;
    result.thumbSize = thumbSize;
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
    check(TabConfig().thumbSize == 160, "new tab defaults thumbnail size 160");
    WallConfig original;
    original.realtime = true;

    CollectionConfig first =
            collection("Studio A", {tab(uuid('1'), {uuid('a')}, 224)});
    first.collapsedScenes.insert(uuid('a'));
    CollectionConfig second =
            collection("Studio B", {tab(uuid('2'), {uuid('b')}, 120)});
    second.collapsedScenes.insert(uuid('b'));
    original.collections = {first, second};

    check(saveWallConfig(original, path), "save versioned config");
    const WallConfig loaded = loadWallConfig(path);
    check(loaded.realtime, "round-trip global settings");
    check(loaded.version == WallConfig::CURRENT_VERSION, "round-trip current version");
    check(loaded.collections.size() == 2, "round-trip separate collections");
    if (loaded.collections.size() == 2) {
        check(loaded.collections[0].collectionId == "Studio A", "first collection key preserved");
        check(loaded.collections[0].tabs[0].scenes == QStringList{uuid('a')},
              "first collection scene UUID preserved");
        check(loaded.collections[0].tabs[0].thumbSize == 224,
              "first tab keeps its own thumbnail size");
        check(loaded.collections[0].collapsedScenes.contains(uuid('a')),
              "first collection collapse state preserved");
        check(loaded.collections[1].collectionId == "Studio B", "second collection isolated");
        check(loaded.collections[1].tabs[0].scenes == QStringList{uuid('b')},
              "second collection scenes isolated");
        check(loaded.collections[1].tabs[0].thumbSize == 120,
              "second tab keeps its own thumbnail size");
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
    QJsonObject legacy{{"version", 2}, {"thumbSize", 200}, {"realtime", false},
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
        check(migrated.thumbSize == 200, "legacy global size becomes the tab size");
    }
}

void testV3GlobalSizeMigratesToTabs(const QString &directory)
{
    const QString path = directory + "/v3.json";
    QJsonObject v3tab{{"id", uuid('4')}, {"name", "Main"}, {"color", "#123456"},
                      {"isAll", false}, {"scenes", QJsonArray{}}};
    QJsonObject v3collection{{"collectionId", "Studio"}, {"tabs", QJsonArray{v3tab}},
                             {"collapsedScenes", QJsonArray{}}};
    QJsonObject v3{{"version", 3}, {"thumbSize", 250}, {"realtime", true},
                   {"collections", QJsonArray{v3collection}}};
    writeFile(path, QJsonDocument(v3).toJson());

    const WallConfig loaded = loadWallConfig(path, "Studio");
    check(loaded.version == WallConfig::CURRENT_VERSION, "v3 config upgraded to current version");
    check(loaded.collections.size() == 1, "v3 collections preserved");
    if (loaded.collections.size() == 1 && !loaded.collections[0].tabs.isEmpty())
        check(loaded.collections[0].tabs[0].thumbSize == 250,
              "v3 global size becomes each tab size");
}

void testInvalidPerTabSizeRejected(const QString &directory)
{
    // Out-of-range per-tab size fails validation, so the whole file is
    // rejected and the loader falls back to defaults.
    const QString path = directory + "/badtab.json";
    WallConfig config;
    config.collections = {collection("Studio", {tab(uuid('1'), {}, 401)})};
    check(!saveWallConfig(config, path), "reject per-tab size above maximum");
    config.collections[0].tabs[0].thumbSize = 79;
    check(!saveWallConfig(config, path), "reject per-tab size below minimum");

    QJsonObject badTab{{"id", uuid('1')}, {"name", "Main"}, {"color", "#808080"},
                       {"isAll", false}, {"thumbSize", 500}, {"scenes", QJsonArray{}}};
    QJsonObject badCollection{{"collectionId", "Studio"}, {"tabs", QJsonArray{badTab}},
                              {"collapsedScenes", QJsonArray{}}};
    QJsonObject badRoot{{"version", 4}, {"realtime", true},
                        {"collections", QJsonArray{badCollection}}};
    writeFile(path, QJsonDocument(badRoot).toJson());
    const WallConfig loaded = loadWallConfig(path, "Studio");
    check(loaded.collections.isEmpty(), "config with invalid per-tab size loads as default");
}

void testRemovedTabDropsItsSize(const QString &directory)
{
    // Sizes live on tabs: deleting a tab removes its size with it, and a
    // deleted scene leaves no per-scene trace behind.
    const QString path = directory + "/orphan.json";
    WallConfig config;
    config.collections = {collection(
            "Studio", {tab(uuid('1'), {uuid('a')}, 224), tab(uuid('2'), {uuid('b')}, 120)})};
    config.collections[0].collapsedScenes = {uuid('a'), uuid('b')};
    check(saveWallConfig(config, path), "save two sized tabs");

    WallConfig loaded = loadWallConfig(path);
    CollectionConfig &active = loaded.collections[0];
    active.tabs.removeAt(1); // user deletes the second tab
    filterDeletedScenes(active, {uuid('a')}); // user deletes scene b from OBS
    check(saveWallConfig(loaded, path), "save after tab and scene removal");

    const WallConfig reloaded = loadWallConfig(path);
    check(reloaded.collections.size() == 1, "collection survives removal");
    if (reloaded.collections.size() == 1) {
        check(reloaded.collections[0].tabs.size() == 1, "removed tab stays gone");
        if (!reloaded.collections[0].tabs.isEmpty()) {
            check(reloaded.collections[0].tabs[0].thumbSize == 224,
                  "remaining tab keeps its own size");
            check(reloaded.collections[0].tabs[0].scenes == QStringList{uuid('a')},
                  "deleted scene removed from assignment");
        }
        check(reloaded.collections[0].collapsedScenes == QSet<QString>{uuid('a')},
              "deleted scene collapse state removed");
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
    invalidSize.collections[0].tabs[0].thumbSize = 0;
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
    QJsonArray invalidSizeCollections = invalidSizeRoot["collections"].toArray();
    QJsonObject invalidSizeCollection = invalidSizeCollections[0].toObject();
    QJsonArray invalidSizeTabs = invalidSizeCollection["tabs"].toArray();
    QJsonObject invalidSizeTab = invalidSizeTabs[0].toObject();
    invalidSizeTab["thumbSize"] = 401;
    invalidSizeTabs[0] = invalidSizeTab;
    invalidSizeCollection["tabs"] = invalidSizeTabs;
    invalidSizeCollections[0] = invalidSizeCollection;
    invalidSizeRoot["collections"] = invalidSizeCollections;
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
    testV3GlobalSizeMigratesToTabs(directory.path());
    testInvalidPerTabSizeRejected(directory.path());
    testRemovedTabDropsItsSize(directory.path());
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
