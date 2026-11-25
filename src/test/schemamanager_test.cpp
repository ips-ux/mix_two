#include "database/schemamanager.h"

#include <QSqlQuery>

#include "library/dao/settingsdao.h"
#include "test/mixxxdbtest.h"

class SchemaManagerTest : public MixxxDbTest {};

TEST_F(SchemaManagerTest, UpgradeFromPreviousToNextVersion) {
    // Verify that all schema migrations work as expected
    for (int currentVersion = 0;
            currentVersion < MixxxDb::kRequiredSchemaVersion;
            ++currentVersion) {
        const auto targetVersion = currentVersion + 1;
        SchemaManager schemaManager(dbConnection());

        {
            const auto result = schemaManager.upgradeToSchemaVersion(
                    targetVersion, MixxxDb::kDefaultSchemaFile);
            EXPECT_EQ(SchemaManager::Result::UpgradeSucceeded, result);
            EXPECT_EQ(targetVersion, schemaManager.readCurrentVersion());
            EXPECT_EQ(targetVersion, schemaManager.readLastUsedVersion());
        }

        {
            const auto result = schemaManager.upgradeToSchemaVersion(
                    targetVersion, MixxxDb::kDefaultSchemaFile);
            EXPECT_EQ(SchemaManager::Result::CurrentVersion, result);
            EXPECT_EQ(targetVersion, schemaManager.readCurrentVersion());
            EXPECT_EQ(targetVersion, schemaManager.readLastUsedVersion());
        }

        const auto minCompatibleVersion = schemaManager.readMinBackwardsCompatibleVersion();
        ASSERT_TRUE(minCompatibleVersion <= targetVersion);
        if (minCompatibleVersion == targetVersion) {
            continue;
        }

        // Verify that backwards compatibility works as advertised

        {
            const auto result = schemaManager.upgradeToSchemaVersion(
                    minCompatibleVersion, MixxxDb::kDefaultSchemaFile);
            EXPECT_EQ(SchemaManager::Result::NewerVersionBackwardsCompatible, result);
            EXPECT_EQ(targetVersion, schemaManager.readCurrentVersion());
            EXPECT_EQ(minCompatibleVersion, schemaManager.readLastUsedVersion());
        }

        if (minCompatibleVersion > 0) {
            const auto result = schemaManager.upgradeToSchemaVersion(
                    minCompatibleVersion - 1, MixxxDb::kDefaultSchemaFile);
            EXPECT_EQ(SchemaManager::Result::NewerVersionIncompatible, result);
            EXPECT_EQ(targetVersion, schemaManager.readCurrentVersion());
            EXPECT_EQ(minCompatibleVersion, schemaManager.readLastUsedVersion());
        }

        {
            const auto result = schemaManager.upgradeToSchemaVersion(
                    targetVersion, MixxxDb::kDefaultSchemaFile);
            EXPECT_EQ(SchemaManager::Result::UpgradeSucceeded, result);
            EXPECT_EQ(targetVersion, schemaManager.readCurrentVersion());
            EXPECT_EQ(targetVersion, schemaManager.readLastUsedVersion());
        }
    }
}

TEST_F(SchemaManagerTest, NonExistentSchema) {
    SchemaManager schemaManager(dbConnection());
    SchemaManager::Result result = schemaManager.upgradeToSchemaVersion(
            MixxxDb::kRequiredSchemaVersion, ":file_doesnt_exist.xml");
    EXPECT_EQ(SchemaManager::Result::SchemaError, result);
}

TEST_F(SchemaManagerTest, UpgradeFailed) {
    // Establish preconditions for test
    {
        // Upgrade to version 3 to get the modern library table.
        SchemaManager schemaManager(dbConnection());
        SchemaManager::Result result = schemaManager.upgradeToSchemaVersion(
                3, MixxxDb::kDefaultSchemaFile);
        ASSERT_EQ(SchemaManager::Result::UpgradeSucceeded, result);

        // Drop a table that is expected to exist.
        QSqlQuery query(dbConnection());
        ASSERT_TRUE(query.exec("DROP TABLE PlaylistTracks"));
    }

    SchemaManager schemaManager(dbConnection());
    SchemaManager::Result result = schemaManager.upgradeToSchemaVersion(
            MixxxDb::kRequiredSchemaVersion, MixxxDb::kDefaultSchemaFile);
    EXPECT_EQ(SchemaManager::Result::UpgradeFailed, result);
}

// AI-generated (Claude Code). Start of AI-generated text.
TEST_F(SchemaManagerTest, HotCueAutoLoadIsOnForExistingTracks) {
    // A library of the version before the flag exists, with two tracks.
    {
        SchemaManager schemaManager(dbConnection());
        ASSERT_EQ(SchemaManager::Result::UpgradeSucceeded,
                schemaManager.upgradeToSchemaVersion(
                        41, MixxxDb::kDefaultSchemaFile));
        QSqlQuery query(dbConnection());
        ASSERT_TRUE(query.exec("INSERT INTO library (artist, title) VALUES ('a', 'one')"));
        ASSERT_TRUE(query.exec("INSERT INTO library (artist, title) VALUES ('b', 'two')"));
        EXPECT_FALSE(query.exec("SELECT hotcue_autoload FROM library"));
    }

    SchemaManager schemaManager(dbConnection());
    ASSERT_EQ(SchemaManager::Result::UpgradeSucceeded,
            schemaManager.upgradeToSchemaVersion(
                    42, MixxxDb::kDefaultSchemaFile));

    // The tracks of the old library have the flag on.
    QSqlQuery query(dbConnection());
    ASSERT_TRUE(query.exec("SELECT hotcue_autoload FROM library ORDER BY id"));
    ASSERT_TRUE(query.next());
    EXPECT_EQ(1, query.value(0).toInt());
    ASSERT_TRUE(query.next());
    EXPECT_EQ(1, query.value(0).toInt());
    EXPECT_FALSE(query.next());

    // A row that does not set the flag gets it on as well.
    ASSERT_TRUE(query.exec("INSERT INTO library (artist, title) VALUES ('c', 'three')"));
    ASSERT_TRUE(query.exec("SELECT hotcue_autoload FROM library WHERE title='three'"));
    ASSERT_TRUE(query.next());
    EXPECT_EQ(1, query.value(0).toInt());
}
// End of AI-generated text.

// AI-generated (Claude Code). Start of AI-generated text.
TEST_F(SchemaManagerTest, BitDepthIsUnknownForExistingTracks) {
    // A library of the version before the bit depth exists, with two tracks.
    {
        SchemaManager schemaManager(dbConnection());
        ASSERT_EQ(SchemaManager::Result::UpgradeSucceeded,
                schemaManager.upgradeToSchemaVersion(
                        42, MixxxDb::kDefaultSchemaFile));
        QSqlQuery query(dbConnection());
        ASSERT_TRUE(query.exec("INSERT INTO library (artist, title) VALUES ('a', 'one')"));
        ASSERT_TRUE(query.exec("INSERT INTO library (artist, title) VALUES ('b', 'two')"));
        EXPECT_FALSE(query.exec("SELECT bit_depth FROM library"));
    }

    SchemaManager schemaManager(dbConnection());
    ASSERT_EQ(SchemaManager::Result::UpgradeSucceeded,
            schemaManager.upgradeToSchemaVersion(
                    43, MixxxDb::kDefaultSchemaFile));

    // The tracks of the old library keep their rows. Their bit depth is 0
    // (unknown), not NULL. The migration does not read any file.
    QSqlQuery query(dbConnection());
    ASSERT_TRUE(query.exec("SELECT title, bit_depth FROM library ORDER BY id"));
    ASSERT_TRUE(query.next());
    EXPECT_EQ(QStringLiteral("one"), query.value(0).toString());
    EXPECT_FALSE(query.value(1).isNull());
    EXPECT_EQ(0, query.value(1).toInt());
    ASSERT_TRUE(query.next());
    EXPECT_EQ(QStringLiteral("two"), query.value(0).toString());
    EXPECT_FALSE(query.value(1).isNull());
    EXPECT_EQ(0, query.value(1).toInt());
    EXPECT_FALSE(query.next());

    // The flag column of the version before is still there.
    ASSERT_TRUE(query.exec("SELECT hotcue_autoload FROM library"));
    ASSERT_TRUE(query.next());
    EXPECT_EQ(1, query.value(0).toInt());

    // A row that does not set the bit depth gets 0 as well.
    ASSERT_TRUE(query.exec("INSERT INTO library (artist, title) VALUES ('c', 'three')"));
    ASSERT_TRUE(query.exec("SELECT bit_depth FROM library WHERE title='three'"));
    ASSERT_TRUE(query.next());
    EXPECT_EQ(0, query.value(0).toInt());
}
// End of AI-generated text.
