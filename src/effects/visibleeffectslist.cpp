#include "effects/visibleeffectslist.h"

#include <QDomDocument>

#include "effects/backends/effectmanifest.h"
#include "effects/backends/effectsbackend.h"
#include "effects/backends/effectsbackendmanager.h"
#include "effects/presets/effectxmlelements.h"
#include "moc_visibleeffectslist.cpp"
#include "util/xml.h"

void VisibleEffectsList::setList(const QList<EffectManifestPointer>& newList) {
    m_list = newList;
    emit visibleEffectsListChanged();
}

int VisibleEffectsList::indexOf(EffectManifestPointer pManifest) const {
    return m_list.indexOf(pManifest);
}

const EffectManifestPointer VisibleEffectsList::at(int index) const {
    VERIFY_OR_DEBUG_ASSERT(index >= 0) {
        index = 0;
    }
    VERIFY_OR_DEBUG_ASSERT(index < m_list.size()) {
        index = m_list.size() - 1;
    }
    return m_list.at(index);
}

const EffectManifestPointer VisibleEffectsList::next(const EffectManifestPointer pManifest) const {
    int index = m_list.indexOf(pManifest);
    index++;
    if (index == m_list.size()) {
        index = 0;
    }
    return m_list.at(index);
}

const EffectManifestPointer VisibleEffectsList::previous(
        const EffectManifestPointer pManifest) const {
    int index = m_list.indexOf(pManifest);
    index--;
    if (index < 0) {
        index = m_list.size() - 1;
    }
    return m_list.at(index);
}

void VisibleEffectsList::readEffectsXml(
        const QDomDocument& doc,
        EffectsBackendManagerPointer pBackendManager) {
    QList<EffectManifestPointer> visibleEffects = readEffectsList(
            doc,
            pBackendManager,
            EffectXml::kVisibleEffects);
    const QList<EffectManifestPointer> hiddenEffects = readEffectsList(
            doc,
            pBackendManager,
            EffectXml::kHiddenEffects);

    // New effects will remain hidden since they are neither in the VisibleEffects
    // nor in the newly introduced HiddenEffects list.
    // Unhide all effects that are not in either list.
    const auto manifests = pBackendManager->getManifestsForBackend(EffectBackendType::BuiltIn);
    for (const EffectManifestPointer& pManifest : std::as_const(manifests)) {
        if (!visibleEffects.contains(pManifest) &&
                !hiddenEffects.contains(pManifest)) {
            // prepend so un-hidden effects are discoverable
            visibleEffects.prepend(pManifest);
        }
    }

    // Mix Two: the first 14 visible effects are the DDJ-800 BEAT FX selector
    // positions, in its printed order (LOW CUT ECHO, ECHO, MT DELAY, SPIRAL,
    // REVERB, TRANS, ENIGMA JET, FLANGER, PHASER, PITCH, SLIP ROLL, ROLL,
    // MOBIUS SAW, MOBIUS TRI). Where Mixxx has no such effect yet (bucket
    // 02) the position holds a stand-in, marked below; the rest follow.
    static const QStringList kDdj800Order = {
            QStringLiteral("org.mixtwo.beatfx.lowcutecho"),   // LOW CUT ECHO
            QStringLiteral("org.mixtwo.beatfx.echo"),         // ECHO
            QStringLiteral("org.mixtwo.beatfx.mtdelay"),      // MT DELAY
            QStringLiteral("org.mixtwo.beatfx.spiral"),       // SPIRAL
            QStringLiteral("org.mixtwo.beatfx.reverb"),       // REVERB
            QStringLiteral("org.mixtwo.beatfx.trans"),        // TRANS
            QStringLiteral("org.mixtwo.beatfx.enigmajet"),    // ENIGMA JET
            QStringLiteral("org.mixtwo.beatfx.flanger"),      // FLANGER
            QStringLiteral("org.mixtwo.beatfx.phaser"),       // PHASER
            QStringLiteral("org.mixtwo.beatfx.pitch"),        // PITCH
            QStringLiteral("org.mixtwo.beatfx.sliproll"),     // SLIP ROLL
            QStringLiteral("org.mixtwo.beatfx.roll"),         // ROLL
            QStringLiteral("org.mixtwo.beatfx.mobiussaw"),    // MOBIUS SAW
            QStringLiteral("org.mixtwo.beatfx.mobiustri"),    // MOBIUS TRI
    };
    QList<EffectManifestPointer> ordered;
    for (const QString& id : kDdj800Order) {
        for (const EffectManifestPointer& pManifest : std::as_const(visibleEffects)) {
            if (pManifest->id() == id && !ordered.contains(pManifest)) {
                ordered.append(pManifest);
                break;
            }
        }
    }
    for (const EffectManifestPointer& pManifest : std::as_const(visibleEffects)) {
        if (!ordered.contains(pManifest)) {
            ordered.append(pManifest);
        }
    }
    setList(ordered);
}

QList<EffectManifestPointer> VisibleEffectsList::readEffectsList(
        const QDomDocument& doc,
        EffectsBackendManagerPointer pBackendManager,
        const QString& xmlElementName) {
    QDomElement root = doc.documentElement();
    QDomElement visibleEffectsElement = XmlParse::selectElement(root, xmlElementName);
    QDomNodeList effectsElementsList = visibleEffectsElement.elementsByTagName(EffectXml::kEffect);
    QList<EffectManifestPointer> list;

    for (int i = 0; i < effectsElementsList.count(); ++i) {
        QDomNode effectNode = effectsElementsList.at(i);
        if (effectNode.isElement()) {
            QString id = XmlParse::selectNodeQString(effectNode, EffectXml::kEffectId);
            QString backendString = XmlParse::selectNodeQString(
                    effectNode, EffectXml::kEffectBackendType);
            EffectBackendType backendType = EffectsBackend::backendTypeFromString(backendString);
            EffectManifestPointer pManifest = pBackendManager->getManifest(id, backendType);
            if (pManifest) {
                list.append(pManifest);
            }
        }
    }
    return list;
}

void VisibleEffectsList::saveEffectsXml(QDomDocument* pDoc,
        EffectsBackendManagerPointer pBackendManager) {
    saveEffectsListXml(pDoc, m_list, EffectXml::kVisibleEffects);
    auto hiddenEffects = pBackendManager->getManifests();
    for (const auto& pManifest : std::as_const(m_list)) {
        hiddenEffects.removeAll(pManifest);
    }
    saveEffectsListXml(pDoc, hiddenEffects, EffectXml::kHiddenEffects);
}

void VisibleEffectsList::saveEffectsListXml(QDomDocument* pDoc,
        const QList<EffectManifestPointer>& list,
        const QString& xmlElementName) {
    QDomElement root = pDoc->documentElement();
    QDomElement visibleEffectsElement = pDoc->createElement(xmlElementName);
    root.appendChild(visibleEffectsElement);
    for (const auto& pManifest : std::as_const(list)) {
        VERIFY_OR_DEBUG_ASSERT(pManifest) {
            continue;
        }
        QDomElement effectElement = pDoc->createElement(EffectXml::kEffect);
        visibleEffectsElement.appendChild(effectElement);
        XmlParse::addElement(*pDoc, effectElement, EffectXml::kEffectId, pManifest->id());
        XmlParse::addElement(*pDoc,
                effectElement,
                EffectXml::kEffectBackendType,
                EffectsBackend::backendTypeToString(pManifest->backendType()));
    }
}
