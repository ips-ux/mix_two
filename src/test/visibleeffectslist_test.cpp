// AI-generated (Claude Code). Start of AI-generated text.
#include <gtest/gtest.h>

#include <QDomDocument>
#include <QSet>
#include <QStringList>
#include <algorithm>

#include "effects/backends/effectmanifest.h"
#include "effects/backends/effectsbackend.h"
#include "effects/backends/effectsbackendmanager.h"
#include "effects/presets/effectxmlelements.h"
#include "effects/visibleeffectslist.h"
#include "mixxxtest.h"

namespace {

// The 14 positions of the BEAT FX SELECT switch of the DDJ-800, in the order
// printed on the controller. This list is written out here on purpose: it is
// an independent statement of that order, not a copy of the production list.
const QStringList kExpectedBeatFxOrder = {
        QStringLiteral("org.mixtwo.beatfx.lowcutecho"),
        QStringLiteral("org.mixtwo.beatfx.echo"),
        QStringLiteral("org.mixtwo.beatfx.mtdelay"),
        QStringLiteral("org.mixtwo.beatfx.spiral"),
        QStringLiteral("org.mixtwo.beatfx.reverb"),
        QStringLiteral("org.mixtwo.beatfx.trans"),
        QStringLiteral("org.mixtwo.beatfx.enigmajet"),
        QStringLiteral("org.mixtwo.beatfx.flanger"),
        QStringLiteral("org.mixtwo.beatfx.phaser"),
        QStringLiteral("org.mixtwo.beatfx.pitch"),
        QStringLiteral("org.mixtwo.beatfx.sliproll"),
        QStringLiteral("org.mixtwo.beatfx.roll"),
        QStringLiteral("org.mixtwo.beatfx.mobiussaw"),
        QStringLiteral("org.mixtwo.beatfx.mobiustri"),
};

class VisibleEffectsListTest : public MixxxTest {
  protected:
    VisibleEffectsListTest()
            : m_pBackendManager(new EffectsBackendManager()) {
    }

    QStringList idsOf(const QList<EffectManifestPointer>& list) const {
        QStringList ids;
        for (const EffectManifestPointer& pManifest : list) {
            ids.append(pManifest->id());
        }
        return ids;
    }

    QStringList firstIds(const VisibleEffectsList& visible, int count) const {
        return idsOf(visible.getList().mid(0, count));
    }

    // Every id must be in the list once, and the list must hold exactly the
    // effects of the built-in backend (taken from the backend, not from the
    // list under test).
    void expectAllBuiltInsOnce(const VisibleEffectsList& visible) const {
        const QStringList builtInIds =
                idsOf(m_pBackendManager->getManifestsForBackend(
                        EffectBackendType::BuiltIn));
        const QStringList ids = idsOf(visible.getList());
        EXPECT_EQ(builtInIds.size(), ids.size());
        EXPECT_EQ(ids.size(), QSet<QString>(ids.begin(), ids.end()).size());
        EXPECT_EQ(QSet<QString>(builtInIds.begin(), builtInIds.end()),
                QSet<QString>(ids.begin(), ids.end()));
    }

    // A document with a saved visible list holding the given effect ids.
    QDomDocument documentWithVisible(const QStringList& ids) const {
        QDomDocument doc;
        QDomElement root = doc.createElement(EffectXml::kRoot);
        doc.appendChild(root);
        QDomElement visibleElement = doc.createElement(EffectXml::kVisibleEffects);
        root.appendChild(visibleElement);
        for (const QString& id : ids) {
            QDomElement effectElement = doc.createElement(EffectXml::kEffect);
            visibleElement.appendChild(effectElement);
            QDomElement idElement = doc.createElement(EffectXml::kEffectId);
            idElement.appendChild(doc.createTextNode(id));
            effectElement.appendChild(idElement);
            QDomElement backendElement = doc.createElement(EffectXml::kEffectBackendType);
            backendElement.appendChild(doc.createTextNode(
                    EffectsBackend::backendTypeToString(EffectBackendType::BuiltIn)));
            effectElement.appendChild(backendElement);
        }
        return doc;
    }

    EffectsBackendManagerPointer m_pBackendManager;
};

TEST_F(VisibleEffectsListTest, BeatFxEffectsAreRegisteredBuiltIns) {
    for (const QString& id : kExpectedBeatFxOrder) {
        EXPECT_TRUE(m_pBackendManager->getManifest(id, EffectBackendType::BuiltIn))
                << "missing built-in effect " << id.toStdString();
    }
}

TEST_F(VisibleEffectsListTest, FirstStartPutsBeatFxFirstInPrintedOrder) {
    VisibleEffectsList visible;
    QDomDocument doc;
    visible.readEffectsXml(doc, m_pBackendManager);

    ASSERT_GE(visible.getList().size(), kExpectedBeatFxOrder.size());
    EXPECT_EQ(kExpectedBeatFxOrder.join(", ").toStdString(),
            firstIds(visible, kExpectedBeatFxOrder.size()).join(", ").toStdString());
    expectAllBuiltInsOnce(visible);
}

TEST_F(VisibleEffectsListTest, SavedOrderDoesNotMoveBeatFxPositions) {
    // Reference: the full list after a first start.
    VisibleEffectsList reference;
    reference.readEffectsXml(QDomDocument(), m_pBackendManager);
    const QStringList referenceIds = idsOf(reference.getList());

    // Saved visible list: the 14 effects reversed, with a non-Beat-FX effect
    // in front of them.
    QString otherId;
    for (const QString& id : referenceIds) {
        if (!kExpectedBeatFxOrder.contains(id)) {
            otherId = id;
            break;
        }
    }
    ASSERT_FALSE(otherId.isEmpty());
    QStringList saved = kExpectedBeatFxOrder;
    std::reverse(saved.begin(), saved.end());
    saved.prepend(otherId);

    VisibleEffectsList visible;
    visible.readEffectsXml(documentWithVisible(saved), m_pBackendManager);

    ASSERT_GE(visible.getList().size(), kExpectedBeatFxOrder.size());
    EXPECT_EQ(kExpectedBeatFxOrder.join(", ").toStdString(),
            firstIds(visible, kExpectedBeatFxOrder.size()).join(", ").toStdString());

    // No effect is lost or doubled.
    expectAllBuiltInsOnce(visible);
}

} // namespace
// End of AI-generated text.
