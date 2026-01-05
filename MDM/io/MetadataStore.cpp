#include "MetadataStore.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

MetadataStore::MetadataStore(const std::string& path) 
	: metadataPath(path){}

bool MetadataStore::exists() const{
	return fs::exists(metadataPath);
}

bool MetadataStore::load(DownloadMetadata& out) {
    std::ifstream in(metadataPath);
    if (!in.is_open())
        return false;

    std::size_t segmentCount = 0;

    in >> out.url;
    in >> out.etag;
    in >> out.fileSize;
    in >> out.completedBytes;
    in >> segmentCount;

    out.segments.clear();
    out.segments.reserve(segmentCount);

    for (std::size_t i = 0; i < segmentCount; ++i) {
        Segment seg{};
        int state = 0;

        in >> seg.index;
        in >> seg.offset;
        in >> seg.size;
        in >> state;

        seg.state = static_cast<SegmentState>(state);
        out.segments.push_back(seg);
    }

    return true;
}

bool MetadataStore::save(const DownloadMetadata& data) {
    const std::string tmpPath = metadataPath + ".tmp";

    {
        std::ofstream out(tmpPath, std::ios::trunc);
        if (!out.is_open())
            return false;

        out << data.url << '\n';
        out << data.etag << '\n';
        out << data.fileSize << '\n';
        out << data.completedBytes << '\n';
        out << data.segments.size() << '\n';

        for (const auto& seg : data.segments) {
            out << seg.index << ' '
                << seg.offset << ' '
                << seg.size << ' '
                << static_cast<int>(seg.state) << '\n';
        }
    }

    std::error_code ec;
    fs::rename(tmpPath, metadataPath, ec);

    if (ec) {
        fs::remove(tmpPath);
        return false;
    }

    return true;
}

bool MetadataStore::validate(const DownloadMetadata& local,
    const std::string& remoteEtag,
    std::uint64_t remoteFileSize) const {
    if (local.fileSize != remoteFileSize)
        return false;

    if (!local.etag.empty() && local.etag != remoteEtag)
        return false;

    return true;
}