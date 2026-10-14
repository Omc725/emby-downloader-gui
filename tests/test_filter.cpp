#include "episode_filter.hpp"
#include <iostream>
#include <cassert>

static Emby::MediaItem makeEp(int season, int episode, const std::string& name) {
    Emby::MediaItem item;
    item.id = "s" + std::to_string(season) + "e" + std::to_string(episode);
    item.name = name;
    item.seasonNumber = season;
    item.episodeNumber = episode;
    item.type = Emby::MediaType::Episode;
    return item;
}

int main() {
    std::cout << "[TEST] Starting EpisodeFilter verification...\n";

    // 3 Sezonluk bir dizi simüle edelim:
    // Sezon 1: 5 bölüm (1-5)
    // Sezon 2: 4 bölüm (1-4)
    // Sezon 3: 6 bölüm (1-6)
    std::vector<Emby::MediaItem> episodes;
    for (int e = 1; e <= 5; ++e) episodes.push_back(makeEp(1, e, "S1 Bölüm " + std::to_string(e)));
    for (int e = 1; e <= 4; ++e) episodes.push_back(makeEp(2, e, "S2 Bölüm " + std::to_string(e)));
    for (int e = 1; e <= 6; ++e) episodes.push_back(makeEp(3, e, "S3 Bölüm " + std::to_string(e)));

    assert(episodes.size() == 15);

    // Test 1: Tüm Sezonlar ve Tüm Bölümler
    {
        SeriesFilterOptions opt;
        opt.targetSeasonNumber = 0;
        opt.mode = EpisodeFilterMode::AllEpisodes;
        auto res = EpisodeFilter::filter(episodes, opt);
        assert(res.size() == 15);
        std::cout << "  - Test 1 (All Seasons & All Episodes): PASS (15 items)\n";
    }

    // Test 2: Yalnızca Sezon 2
    {
        SeriesFilterOptions opt;
        opt.targetSeasonNumber = 2;
        opt.mode = EpisodeFilterMode::AllEpisodes;
        auto res = EpisodeFilter::filter(episodes, opt);
        assert(res.size() == 4);
        for (const auto& ep : res) assert(ep.seasonNumber == 2);
        std::cout << "  - Test 2 (Specific Season 2): PASS (4 items)\n";
    }

    // Test 3: Her Sezonun İlk Bölümü (S*E01)
    {
        SeriesFilterOptions opt;
        opt.targetSeasonNumber = 0;
        opt.mode = EpisodeFilterMode::FirstEpisodeOfEachSeason;
        auto res = EpisodeFilter::filter(episodes, opt);
        assert(res.size() == 3);
        assert(res[0].seasonNumber == 1 && res[0].episodeNumber == 1);
        assert(res[1].seasonNumber == 2 && res[1].episodeNumber == 1);
        assert(res[2].seasonNumber == 3 && res[2].episodeNumber == 1);
        std::cout << "  - Test 3 (First Episode of Every Season): PASS (S01E01, S02E01, S03E01)\n";
    }

    // Test 4: Her Sezonun 3 ve 4. Bölümleri
    {
        SeriesFilterOptions opt;
        opt.targetSeasonNumber = 0;
        opt.mode = EpisodeFilterMode::SpecificEpisodesOfEachSeason;
        opt.specificEpisodeIndices = {3, 4};
        auto res = EpisodeFilter::filter(episodes, opt);
        // Her 3 sezonda da 3 ve 4. bölümler var -> toplam 6 bölüm
        assert(res.size() == 6);
        for (const auto& ep : res) {
            assert(ep.episodeNumber == 3 || ep.episodeNumber == 4);
        }
        std::cout << "  - Test 4 (Episodes 3 and 4 of Every Season): PASS (6 items)\n";
    }

    // Test 5: Her Sezonun Final Bölümü
    {
        SeriesFilterOptions opt;
        opt.targetSeasonNumber = 0;
        opt.mode = EpisodeFilterMode::FinalEpisodeOfEachSeason;
        auto res = EpisodeFilter::filter(episodes, opt);
        assert(res.size() == 3);
        assert(res[0].seasonNumber == 1 && res[0].episodeNumber == 5);
        assert(res[1].seasonNumber == 2 && res[1].episodeNumber == 4);
        assert(res[2].seasonNumber == 3 && res[2].episodeNumber == 6);
        std::cout << "  - Test 5 (Season Finale of Every Season): PASS (S01E05, S02E04, S03E06)\n";
    }

    // Test 6: Dize İndeks Ayrıştırıcı (3, 4 veya 1-3, 5)
    {
        auto indices1 = EpisodeFilter::parseIndicesString("3, 4");
        assert(indices1.size() == 2 && indices1[0] == 3 && indices1[1] == 4);

        auto indices2 = EpisodeFilter::parseIndicesString("1-3, 5");
        assert(indices2.size() == 4 && indices2[0] == 1 && indices2[1] == 2 && indices2[2] == 3 && indices2[3] == 5);

        std::cout << "  - Test 6 (Index String Parser '3, 4' and '1-3, 5'): PASS\n";
    }

    std::cout << "[TEST] All filter verification tests passed successfully.\n";
    return 0;
}
