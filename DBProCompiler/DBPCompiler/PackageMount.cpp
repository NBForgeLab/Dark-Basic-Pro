#include "PackageMount.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {

using dbp::package::PackageErrorCode;
using dbp::package::PackageReader;
using dbp::package::PackageResult;

template <typename T>
PackageResult<T> MountError(std::string message) {
    return PackageResult<T>::Failure({
        PackageErrorCode::PublicationFailed,
        std::move(message),
        std::nullopt,
    });
}

class AuthenticatedPackageDataSource final : public IVFSDataSource {
public:
    AuthenticatedPackageDataSource(
        std::shared_ptr<PackageReader> reader,
        std::string path)
        : reader_(std::move(reader)),
          path_(std::move(path)) {}

    PackageResult<std::shared_ptr<IVFSReadStream>>
    Open() const override {
        std::lock_guard lock(mutex_);
        if (!cached_) {
            const auto loaded = reader_->ReadEntry(path_);
            if (!loaded) {
                return PackageResult<
                    std::shared_ptr<IVFSReadStream>>::Failure(
                        loaded.error());
            }
            cached_ = loaded.value();
        }
        const OwnedMemoryVFSDataSource memory(cached_);
        return memory.Open();
    }

private:
    std::shared_ptr<PackageReader> reader_;
    std::string path_;
    mutable std::mutex mutex_;
    mutable std::shared_ptr<const std::vector<std::uint8_t>> cached_;
};

template <typename Registrations>
void RollBack(const Registrations& registrations) {
    for (auto entry = registrations.rbegin();
         entry != registrations.rend();
         ++entry) {
        VFSRegistry::Unregister(entry->first, entry->second);
    }
}

} // namespace

PackageMount::PackageMount(
    std::vector<Registration> registrations) noexcept
    : registrations_(std::move(registrations)) {}

PackageMount::~PackageMount() {
    RollBack(registrations_);
}

PackageResult<std::unique_ptr<PackageMount>>
PackageMount::MountV2(
    std::shared_ptr<dbp::package::PackageReader> reader) {
    if (!reader) {
        return MountError<std::unique_ptr<PackageMount>>(
            "A null DBPAK reader cannot be mounted.");
    }

    std::vector<Registration> registrations;
    registrations.reserve(reader->manifest().records.size());
    for (const auto& record : reader->manifest().records) {
        auto source =
            std::make_shared<AuthenticatedPackageDataSource>(
                reader,
                record.path);
        if (!VFSRegistry::Register(record.path, source)) {
            RollBack(registrations);
            return MountError<std::unique_ptr<PackageMount>>(
                "A DBPAK path collides with an existing VFS mount.");
        }
        registrations.emplace_back(
            record.path,
            std::move(source));
    }
    return PackageResult<std::unique_ptr<PackageMount>>::Success(
        std::unique_ptr<PackageMount>(
            new PackageMount(std::move(registrations))));
}

PackageResult<std::unique_ptr<PackageMount>>
PackageMount::MountLegacy(
    std::shared_ptr<dbp::package::LegacyPckReader> reader) {
    if (!reader) {
        return MountError<std::unique_ptr<PackageMount>>(
            "A null legacy PCK reader cannot be mounted.");
    }

    std::vector<Registration> registrations;
    registrations.reserve(reader->entries().size());
    for (const auto& entry : reader->entries()) {
        auto source =
            std::make_shared<OwnedMemoryVFSDataSource>(entry.bytes);
        if (!VFSRegistry::Register(entry.path, source)) {
            RollBack(registrations);
            return MountError<std::unique_ptr<PackageMount>>(
                "A legacy PCK path collides with an existing VFS mount.");
        }
        registrations.emplace_back(
            entry.path,
            std::move(source));
    }
    return PackageResult<std::unique_ptr<PackageMount>>::Success(
        std::unique_ptr<PackageMount>(
            new PackageMount(std::move(registrations))));
}
