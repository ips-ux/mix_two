#pragma once

#include <QDir>
#include <optional>

#include "preferences/usersettings.h"
#include "library/dao/dao.h"
#include "track/trackid.h"
#include "waveform/waveform.h"

class QSqlDatabase;

class AnalysisDao : public DAO {
  public:
    static const QString s_analysisTableName;

    enum AnalysisType {
        TYPE_UNKNOWN = 0,
        TYPE_WAVEFORM,
        TYPE_WAVESUMMARY
    };

    struct AnalysisInfo {
        AnalysisInfo()
                : analysisId(-1),
                  type(TYPE_UNKNOWN) {
        }
        int analysisId;
        TrackId trackId;
        AnalysisType type;
        QString description;
        QString version;
        QByteArray data;
        // AI-generated (Claude Code). Start of AI-generated text.
        // Mix Two: set by getAnalysisFilesForTrackByType() instead of data.
        /// The path of the compressed data file.
        QString dataPath;
        /// The checksum of the compressed data file in the database.
        int dataChecksum = 0;
        // End of AI-generated text.
    };

    explicit AnalysisDao(UserSettingsPointer pConfig);
    ~AnalysisDao() override = default;

    // The following functions can be used with a custom database
    // connection and independent of whether the DAO has been
    // initialized or not.
    bool deleteAnalysesByType(
            const QSqlDatabase& database,
            AnalysisType type) const;
    size_t getDiskUsageInBytes(
            const QSqlDatabase& database,
            AnalysisType type) const;

    QList<AnalysisInfo> getAnalysesForTrackByType(TrackId trackId, AnalysisType type);
    QList<AnalysisInfo> getAnalysesForTrack(TrackId trackId);

    // AI-generated (Claude Code). Start of AI-generated text.
    /// Mix Two: as getAnalysesForTrackByType(), but reads no file: `data` is
    /// empty, `dataPath` and `dataChecksum` are set. Read the data later with
    /// loadAnalysisData(), also in another thread.
    QList<AnalysisInfo> getAnalysisFilesForTrackByType(TrackId trackId, AnalysisType type);

    /// Mix Two: reads and uncompresses the data file of an analysis. Uses no
    /// database, so any thread can call it. A missing file or a file with
    /// another checksum gives no data.
    static std::optional<QByteArray> loadAnalysisData(const QString& dataPath, int dataChecksum);
    // End of AI-generated text.
    bool saveAnalysis(AnalysisInfo* analysis);
    bool deleteAnalysis(const int analysisId);
    void deleteAnalyses(const QList<TrackId>& trackIds);
    bool deleteAnalysesForTrack(TrackId trackId);

    void saveTrackAnalyses(
            TrackId trackId,
            ConstWaveformPointer pWaveform,
            ConstWaveformPointer pWaveSummary);

  private:
    QDir getAnalysisStoragePath() const;
    QByteArray loadDataFromFile(const QString& fileName) const;
    bool saveDataToFile(const QString& fileName, const QByteArray& data) const;
    bool deleteFile(const QString& filename) const;
    QList<AnalysisInfo> loadAnalysesFromQuery(TrackId trackId, QSqlQuery* query);
    // AI-generated (Claude Code). Start of AI-generated text.
    QList<AnalysisInfo> loadAnalysisFilesFromQuery(TrackId trackId, QSqlQuery* query);
    // End of AI-generated text.

    const UserSettingsPointer m_pConfig;
};
