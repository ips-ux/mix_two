// AI-generated (Claude Code). Start of AI-generated text.
#include "musicbrainz/lookupnotice.h"

#include <gtest/gtest.h>

#include <QRegularExpression>
#include <QSet>
#include <QSignalSpy>
#include <QUrlQuery>
#include <QUuid>

#include "musicbrainz/tagfetcher.h"
#include "musicbrainz/web/acoustidlookuptask.h"
#include "musicbrainz/web/coverartarchivelinkstask.h"
#include "musicbrainz/web/musicbrainzrecordingstask.h"
#include "test/mixxxtest.h"

namespace {

using namespace mixxx::musicbrainz;

class LookupNoticeTest : public MixxxTest {};

QSet<QString> queryKeys(const QUrlQuery& query) {
    QSet<QString> keys;
    const auto items = query.queryItems();
    for (const auto& item : items) {
        keys.insert(item.first);
    }
    return keys;
}

} // namespace

TEST_F(LookupNoticeTest, NoticeNeededUntilAccepted) {
    EXPECT_TRUE(needsLookupNotice(config()));
    acceptLookupNotice(config());
    EXPECT_FALSE(needsLookupNotice(config()));
}

TEST_F(LookupNoticeTest, NoConfigNeedsNotice) {
    EXPECT_TRUE(needsLookupNotice(UserSettingsPointer()));
}

TEST_F(LookupNoticeTest, AcceptStartsLookupAndAsksOnce) {
    int asked = 0;
    const auto accept = [&asked]() {
        ++asked;
        return true;
    };
    EXPECT_TRUE(confirmOnlineLookup(config(), accept));
    EXPECT_EQ(1, asked);
    EXPECT_TRUE(confirmOnlineLookup(config(), accept));
    EXPECT_EQ(1, asked);
}

TEST_F(LookupNoticeTest, CancelStopsLookupAndAsksAgain) {
    int asked = 0;
    const auto cancel = [&asked]() {
        ++asked;
        return false;
    };
    EXPECT_FALSE(confirmOnlineLookup(config(), cancel));
    EXPECT_TRUE(needsLookupNotice(config()));
    EXPECT_FALSE(confirmOnlineLookup(config(), cancel));
    EXPECT_EQ(2, asked);
}

TEST_F(LookupNoticeTest, NoAnswerFunctionCancels) {
    EXPECT_FALSE(confirmOnlineLookup(config(), std::function<bool()>()));
    EXPECT_TRUE(needsLookupNotice(config()));
}

// The notice lists what the request code sends. If the request code gets a
// new item, this test fails until the notice names it.
TEST_F(LookupNoticeTest, NoticeCoversEveryAcoustIdQueryItem) {
    const QUrlQuery query =
            mixxx::acoustIdLookupUrlQuery(QStringLiteral("AQADtEq"), 215);
    QSet<QString> listed;
    const QStringList named = lookupNoticeNamedQueryKeys();
    const QStringList options = lookupNoticeOptionQueryKeys();
    for (const QString& key : named) {
        listed.insert(key);
    }
    for (const QString& key : options) {
        listed.insert(key);
    }
    EXPECT_EQ(listed, queryKeys(query));
    EXPECT_EQ(QStringLiteral("AQADtEq"),
            query.queryItemValue(QStringLiteral("fingerprint")));
    EXPECT_EQ(QStringLiteral("215"),
            query.queryItemValue(QStringLiteral("duration")));
    EXPECT_FALSE(query.queryItemValue(QStringLiteral("client")).isEmpty());
}

TEST_F(LookupNoticeTest, NoticeNamesTheHostsAndFields) {
    const QString text = lookupNoticeText();
    const QUuid id = QUuid::fromString(
            QLatin1String("416a273e-51b8-4b1c-8873-4c9b4ed54a0f"));

    const QNetworkRequest mb = mixxx::musicBrainzRecordingRequest(id);
    EXPECT_TRUE(text.contains(mb.url().host()));
    // Only the "inc" option and the user agent go with a MusicBrainz request.
    EXPECT_EQ(QSet<QString>{QStringLiteral("inc")},
            queryKeys(QUrlQuery(mb.url())));
    ASSERT_EQ(1, mb.rawHeaderList().size());
    EXPECT_EQ(QByteArray("user-agent"), mb.rawHeaderList().first().toLower());
    EXPECT_TRUE(mb.url().path().contains(id.toString(QUuid::WithoutBraces)));

    const QNetworkRequest ca = mixxx::coverArtArchiveReleaseRequest(id);
    EXPECT_TRUE(text.contains(ca.url().host()));
    EXPECT_TRUE(ca.url().query().isEmpty());
    EXPECT_TRUE(ca.rawHeaderList().isEmpty());

    EXPECT_TRUE(text.contains(QStringLiteral("api.acoustid.org")));
    EXPECT_TRUE(text.contains(QStringLiteral("fingerprint")));
    EXPECT_TRUE(text.contains(QStringLiteral("duration")));
    EXPECT_TRUE(text.contains(QStringLiteral("client key")));
}

TEST_F(LookupNoticeTest, NoticeNamesTheRealUserAgent) {
    const QUuid id = QUuid::fromString(
            QLatin1String("416a273e-51b8-4b1c-8873-4c9b4ed54a0f"));
    const QNetworkRequest mb = mixxx::musicBrainzRecordingRequest(id);
    const QByteArray sent = mb.rawHeader("User-Agent");
    ASSERT_FALSE(sent.isEmpty());
    EXPECT_EQ(mixxx::musicBrainzUserAgent().toLatin1(), sent);
    // The notice holds the header value exactly as the request sends it.
    EXPECT_TRUE(lookupNoticeText().contains(QString::fromLatin1(sent)));
}

TEST_F(LookupNoticeTest, NoSentenceIsLongerThan25Words) {
    const QStringList sentences = lookupNoticeText().split(
            QRegularExpression(QStringLiteral("(?<=[.!?:])\\s+")),
            Qt::SkipEmptyParts);
    EXPECT_GT(sentences.size(), 5);
    for (const QString& sentence : sentences) {
        const int words = sentence.split(QRegularExpression(QStringLiteral("\\s+")),
                                          Qt::SkipEmptyParts)
                                  .size();
        EXPECT_LE(words, 25) << sentence.toStdString();
    }
}

TEST_F(LookupNoticeTest, WithdrawBringsTheNoticeBack) {
    acceptLookupNotice(config());
    ASSERT_FALSE(needsLookupNotice(config()));
    withdrawLookupNotice(config());
    EXPECT_TRUE(needsLookupNotice(config()));
}

// A caller of TagFetcher cannot skip the notice: without consent the fetcher
// sends no request and starts no fingerprint.
TEST_F(LookupNoticeTest, TagFetcherRefusesWithoutConsent) {
    TagFetcher fetcher(config());
    EXPECT_FALSE(fetcher.isLookupAllowed());
    QSignalSpy result(&fetcher, &TagFetcher::resultAvailable);
    QSignalSpy progress(&fetcher, &TagFetcher::fetchProgress);
    QSignalSpy noCover(&fetcher, &TagFetcher::coverArtLinkNotFound);
    fetcher.startFetch(TrackPointer());
    EXPECT_EQ(1, result.count());
    EXPECT_EQ(0, progress.count());
    fetcher.startFetchCoverArtLinks(QUuid::createUuid());
    EXPECT_EQ(1, noCover.count());
    fetcher.startFetchCoverArtImage(QUuid::createUuid(),
            QStringLiteral("https://example.invalid/a.jpg"));
}

TEST_F(LookupNoticeTest, TagFetcherWithoutConfigRefuses) {
    TagFetcher fetcher((UserSettingsPointer()));
    EXPECT_FALSE(fetcher.isLookupAllowed());
}

TEST_F(LookupNoticeTest, TagFetcherAllowedAfterAccept) {
    acceptLookupNotice(config());
    TagFetcher fetcher(config());
    EXPECT_TRUE(fetcher.isLookupAllowed());
}

// End of AI-generated text.
