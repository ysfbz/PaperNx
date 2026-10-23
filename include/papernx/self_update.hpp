#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace papernx {

struct UpdateRelease {
    std::string version,title,notes,url,checksumUrl,sha256;
    std::uint64_t bytes{0};
    bool installable() const {return !url.empty() && (!sha256.empty() || !checksumUrl.empty());}
};
enum class UpdateState {Idle,Checking,Current,Available,Downloading,Ready,Error};
struct UpdateStatus {
    UpdateState state{UpdateState::Idle};
    UpdateRelease release;
    std::string error;
    int percent{0};
};
struct UpdateSource {
    std::string api{"https://api.github.com/repos/ysfbz/PaperNx/releases?per_page=30"};
    std::string assetPrefix{"https://github.com/ysfbz/PaperNx/releases/download/"};
};

// Includes numeric prerelease identifiers: reader.10 > reader.9; stable > prerelease.
bool updateVersionNewer(const std::string &candidate,const std::string &current);
bool selectUpdateRelease(const std::string &json,const std::string &current,
                         const UpdateSource &source,UpdateRelease &release,std::string &error);

// Network/file work stays on one owned worker. The running NRO is never written.
// applyPending is called only after the app, its fonts and RomFS are destroyed.
class SelfUpdater {
public:
    ~SelfUpdater();
    void configure(std::filesystem::path executable,std::string version,
                   std::string certificates={},UpdateSource source={});
    bool check();
    bool download();
    UpdateStatus status() const;
    void stop();
    bool applyPending(std::string &error);
    const std::filesystem::path &executable() const {return executable_;}
private:
    std::filesystem::path executable_;
    std::string current_,certificates_;
    UpdateSource source_;
    mutable std::mutex mutex_;
    UpdateStatus status_;
    std::atomic<bool> cancel_{false};
    std::thread worker_;
    void publish(UpdateState state,const std::string &error={});
    bool busy() const;
    bool validatePending(std::string &error) const;
};

} // namespace papernx
