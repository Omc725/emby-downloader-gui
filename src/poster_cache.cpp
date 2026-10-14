#include "poster_cache.hpp"
#include <filesystem>
#include <curl/curl.h>
#include <thread>
#include <cstdlib>

namespace fs = std::filesystem;

struct IdlePayload {
    std::string itemId;
    std::string filePath;
    int targetWidth;
    int targetHeight;
    PosterCache::ImageLoadedCallback callback;
};

static gboolean onImageDownloadedIdle(gpointer data) {
    auto* payload = static_cast<IdlePayload*>(data);
    if (payload && payload->callback) {
        GError* err = nullptr;
        GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(
            payload->filePath.c_str(),
            payload->targetWidth,
            payload->targetHeight,
            TRUE,
            &err
        );
        if (pb) {
            payload->callback(pb);
            g_object_unref(pb);
        } else if (err) {
            g_error_free(err);
        }
    }
    delete payload;
    return G_SOURCE_REMOVE;
}

PosterCache& PosterCache::instance() {
    static PosterCache inst;
    return inst;
}

PosterCache::PosterCache() {
    const char* home = std::getenv("HOME");
    std::string base = home ? std::string(home) + "/.cache" : "/tmp";
    initialize(base + "/emby-downloader/posters");
}

void PosterCache::initialize(const std::string& cacheDir) {
    m_cacheDir = cacheDir;
    std::error_code ec;
    fs::create_directories(m_cacheDir, ec);
}

std::string PosterCache::getPosterFilePath(const std::string& itemId) const {
    return m_cacheDir + "/" + itemId + ".jpg";
}

GdkPixbuf* PosterCache::createPlaceholder(int width, int height) {
    GdkPixbuf* pb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    if (pb) {
        // RGB (40, 44, 52)
        gdk_pixbuf_fill(pb, 0x282c34ff);
    }
    return pb;
}

GdkPixbuf* PosterCache::getOrFetch(
    const std::string& itemId,
    const std::string& serverUrl,
    const std::string& token,
    const std::string& prefix,
    int targetWidth,
    int targetHeight,
    ImageLoadedCallback onLoaded
) {
    if (itemId.empty()) {
        return createPlaceholder(targetWidth, targetHeight);
    }

    std::string filePath = getPosterFilePath(itemId);
    std::error_code ec;
    if (fs::exists(filePath, ec)) {
        GError* err = nullptr;
        GdkPixbuf* pb = gdk_pixbuf_new_from_file_at_scale(
            filePath.c_str(),
            targetWidth,
            targetHeight,
            TRUE,
            &err
        );
        if (pb) {
            return pb;
        }
        if (err) g_error_free(err);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pendingDownloads[itemId]) {
            return createPlaceholder(targetWidth, targetHeight);
        }
        m_pendingDownloads[itemId] = true;
    }

    // Arka plan iş parçacığında indir
    std::string imgUrl = serverUrl + prefix + "/Items/" + itemId + "/Images/Primary?maxWidth=" + 
                         std::to_string(targetWidth * 2) + "&api_key=" + token;

    std::thread([this, itemId, imgUrl, filePath, targetWidth, targetHeight, onLoaded]() {
        CURL* curl = curl_easy_init();
        if (curl) {
            FILE* fp = std::fopen(filePath.c_str(), "wb");
            if (fp) {
                curl_easy_setopt(curl, CURLOPT_URL, imgUrl.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

                CURLcode res = curl_easy_perform(curl);
                std::fclose(fp);

                if (res == CURLE_OK) {
                    auto* payload = new IdlePayload{
                        itemId,
                        filePath,
                        targetWidth,
                        targetHeight,
                        onLoaded
                    };
                    g_idle_add(onImageDownloadedIdle, payload);
                } else {
                    std::error_code rec;
                    fs::remove(filePath, rec);
                }
            }
            curl_easy_cleanup(curl);
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pendingDownloads[itemId] = false;
        }
    }).detach();

    return createPlaceholder(targetWidth, targetHeight);
}
