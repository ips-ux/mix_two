#include <gtest/gtest.h>

#include "controllers/midi/midiportwatcher.h"

namespace {

// Abbreviated /proc/asound/seq/clients with a DDJ-800 connected.
const QString kProcClients = QStringLiteral(
        "Client info\n"
        "  cur  clients : 3\n"
        "  peak clients : 4\n"
        "  max  clients : 192\n"
        "\n"
        "Client   0 : \"System\" [Kernel]\n"
        "  Port   0 : \"Timer\" (Rwe-)\n"
        "  Port   1 : \"Announce\" (R-e-)\n"
        "Client  24 : \"DDJ-800\" [Kernel Legacy]\n"
        "  Port   0 : \"DDJ-800 MIDI 1\" (RWeX)\n"
        "  Output pool :\n"
        "    Pool size          : 500\n"
        "    Cells in use       : 12\n");

TEST(MidiPortWatcherTest, ParseAlsaSeqClientsKeepsClientsAndPorts) {
    const QStringList lines = mixxx::midi::parseAlsaSeqClients(kProcClients);
    ASSERT_EQ(5, lines.size());
    EXPECT_EQ(QStringLiteral("Client   0 : \"System\" [Kernel]"), lines[0]);
    EXPECT_EQ(QStringLiteral("Client  24 : \"DDJ-800\" [Kernel Legacy]"), lines[3]);
    EXPECT_EQ(QStringLiteral("Port   0 : \"DDJ-800 MIDI 1\" (RWeX)"), lines[4]);
}

TEST(MidiPortWatcherTest, ParseAlsaSeqClientsIgnoresTrafficCounters) {
    QString busy = kProcClients;
    busy.replace(QStringLiteral("Cells in use       : 12"),
            QStringLiteral("Cells in use       : 87"));
    EXPECT_EQ(mixxx::midi::parseAlsaSeqClients(kProcClients),
            mixxx::midi::parseAlsaSeqClients(busy));
}

TEST(MidiPortWatcherTest, ParseAlsaSeqClientsSeesUnplug) {
    QString unplugged = kProcClients;
    unplugged.truncate(unplugged.indexOf(QStringLiteral("Client  24")));
    EXPECT_NE(mixxx::midi::parseAlsaSeqClients(kProcClients),
            mixxx::midi::parseAlsaSeqClients(unplugged));
}

TEST(MidiPortWatcherTest, CurrentPortNamesIsSortedWhenSupported) {
    const std::optional<QStringList> names = mixxx::midi::currentPortNames();
    if (!names) {
        GTEST_SKIP() << "MIDI port listing is not supported on this platform";
    }
    QStringList sorted = *names;
    sorted.sort();
    EXPECT_EQ(sorted, *names);
}

} // namespace
