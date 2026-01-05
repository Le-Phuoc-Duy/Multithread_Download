#pragma once
#include "../core/utils.h"
class MetadataStore
{
public:
    explicit MetadataStore(const std::string& path);

    bool load(DownloadMetadata& out);
    bool save(const DownloadMetadata& data);

    bool exists() const;
    bool validate(const DownloadMetadata& local,
        const std::string& remoteEtag,
        std::uint64_t remoteFileSize) const;

private:
    std::string metadataPath;
};

