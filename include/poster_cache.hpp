#pragma once

#include <gtk/gtk.h>
#include <string>
#include <functional>
#include <mutex>
#include <unordered_map>

class PosterCache {
public:
    using ImageLoadedCallback = std::function<void(GdkPixbuf* pixbuf)>;

    static PosterCache& instance();

    void initialize(const std::string& cacheDir = "");
    std::string getCacheDir() const { return m_cacheDir; }

    // Senkron olarak önbellekte varsa getirir, yoksa nullptr döner ve arka planda indirmeyi başlatır
    GdkPixbuf* getOrFetch(
        const std::string& itemId,
        const std::string& serverUrl,
        const std::string& token,
        const std::string& prefix,
        int targetWidth,
        int targetHeight,
        ImageLoadedCallback onLoaded
    );

    static GdkPixbuf* createPlaceholder(int width, int height);

private:
    PosterCache();
    ~PosterCache() = default;

    std::string getPosterFilePath(const std::string& itemId) const;

    std::string m_cacheDir;
    std::mutex m_mutex;
    std::unordered_map<std::string, bool> m_pendingDownloads;
};
