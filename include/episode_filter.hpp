#pragma once

#include <emby/models.hpp>
#include <vector>
#include <string>

enum class EpisodeFilterMode {
    AllEpisodes,                  // Tüm bölümler
    FirstEpisodeOfEachSeason,     // Her sezonun ilk bölümü
    FinalEpisodeOfEachSeason,     // Her sezonun final bölümü
    SpecificEpisodesOfEachSeason, // Her sezonun belirlenen bölümleri (örn. 3 ve 4. bölümler)
    CustomRange                   // Özel aralık (örn. 1-3, 5)
};

struct SeriesFilterOptions {
    int targetSeasonNumber = 0;   // 0 = Tüm sezonlar, >0 = Belirli bir sezon
    EpisodeFilterMode mode = EpisodeFilterMode::AllEpisodes;
    std::vector<int> specificEpisodeIndices; // Örn: {3, 4}
    bool downloadVideo = true;
    bool downloadAudio = false;
    std::string audioLanguage = ""; // Boş ise varsayılan/tümü
    bool downloadSubtitle = false;
    std::string subtitleLanguage = ""; // Boş ise varsayılan/tümü
};

class EpisodeFilter {
public:
    static std::vector<Emby::MediaItem> filter(
        const std::vector<Emby::MediaItem>& allEpisodes,
        const SeriesFilterOptions& options
    );

    static std::vector<int> parseIndicesString(const std::string& input);
};
