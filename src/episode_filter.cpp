#include "episode_filter.hpp"
#include <map>
#include <set>
#include <algorithm>
#include <sstream>

std::vector<int> EpisodeFilter::parseIndicesString(const std::string& input) {
    std::set<int> uniqueIndices;
    std::stringstream ss(input);
    std::string token;

    while (std::getline(ss, token, ',')) {
        // Trim
        size_t first = token.find_first_not_of(" \t\r\n");
        size_t last = token.find_last_not_of(" \t\r\n");
        if (first == std::string::npos || last == std::string::npos) continue;
        token = token.substr(first, last - first + 1);

        // Check if range like "3-5"
        size_t dash = token.find('-');
        if (dash != std::string::npos) {
            try {
                int start = std::stoi(token.substr(0, dash));
                int end = std::stoi(token.substr(dash + 1));
                if (start <= end) {
                    for (int i = start; i <= end; ++i) {
                        uniqueIndices.insert(i);
                    }
                }
            } catch (...) {}
        } else {
            try {
                int val = std::stoi(token);
                uniqueIndices.insert(val);
            } catch (...) {}
        }
    }

    return std::vector<int>(uniqueIndices.begin(), uniqueIndices.end());
}

std::vector<Emby::MediaItem> EpisodeFilter::filter(
    const std::vector<Emby::MediaItem>& allEpisodes,
    const SeriesFilterOptions& options
) {
    // 1. Sezona göre grupla
    std::map<int, std::vector<Emby::MediaItem>> seasonGroups;
    for (const auto& ep : allEpisodes) {
        if (options.targetSeasonNumber > 0 && ep.seasonNumber != options.targetSeasonNumber) {
            continue;
        }
        seasonGroups[ep.seasonNumber].push_back(ep);
    }

    // Her sezon içindeki bölümleri bölüm numarasına göre sırala
    for (auto& [sNum, eps] : seasonGroups) {
        std::sort(eps.begin(), eps.end(), [](const Emby::MediaItem& a, const Emby::MediaItem& b) {
            return a.episodeNumber < b.episodeNumber;
        });
    }

    std::vector<Emby::MediaItem> matched;

    std::set<int> specificSet(options.specificEpisodeIndices.begin(), options.specificEpisodeIndices.end());

    for (const auto& [sNum, eps] : seasonGroups) {
        if (eps.empty()) continue;

        switch (options.mode) {
            case EpisodeFilterMode::AllEpisodes: {
                for (const auto& ep : eps) {
                    matched.push_back(ep);
                }
                break;
            }
            case EpisodeFilterMode::FirstEpisodeOfEachSeason: {
                matched.push_back(eps.front());
                break;
            }
            case EpisodeFilterMode::FinalEpisodeOfEachSeason: {
                matched.push_back(eps.back());
                break;
            }
            case EpisodeFilterMode::SpecificEpisodesOfEachSeason:
            case EpisodeFilterMode::CustomRange: {
                for (const auto& ep : eps) {
                    if (specificSet.find(ep.episodeNumber) != specificSet.end()) {
                        matched.push_back(ep);
                    }
                }
                break;
            }
        }
    }

    return matched;
}
