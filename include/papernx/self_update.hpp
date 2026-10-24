#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace papernx {

struct UpdateRelease {
    std::string version,title,notes,url,checksumUrl,sha256,stagedFile;
    std::uint64_t bytes{0};
    bool installable() const {return !url.empty() && (!sha256.empty() || !checksumUrl.empty());}
};
// Ready means DOWNLOADED+VERIFIED+STAGED; stagedFile identifies the selected manual-install candidate.
// Installation is manual, so no state here represents INSTALLED/BOOT_CONFIRMED.
enum class UpdateState {Idle,Checking,Current,Available,Downloading,Verifying,Ready,Cancelled,Error};
struct UpdateStatus {
    UpdateState state{UpdateState::Idle};
    UpdateRelease release;
    UpdateRelease staged;
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

// Network/file work stays on one owned worker. Updates are verified beside the
// running NRO for manual installation; this process never replaces its executable.
class SelfUpdater {
public:
    ~SelfUpdater();
    void configure(std::filesystem::path executable,std::string version,
                   std::string certificates={});
#ifdef PAPERNX_UPDATE_TESTING
    enum class FaultPoint {None,NoSpace,CandidateRename,CandidateRecord,PayloadFsync,CandidateRecordFsync,CancelAfterVerify,CancelAfterStage};
    void configureForTesting(std::filesystem::path executable,std::string version,
                             std::string certificates,UpdateSource source);
    static void failNextOperationForTesting(FaultPoint point);
#endif
    bool check();
    bool download();
    UpdateStatus status() const;
    void cancel();
    void stop();
    // These boot markers recover transactions created by older self-installing builds.
    enum class BootRecovery {Normal,Candidate,RolledBack,Error};
    BootRecovery prepareBoot(std::string &error);
    bool confirmBoot(std::string &error);
    bool rollbackBoot(std::string &error);
    void reportError(std::string error);
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
    bool validatePending(UpdateRelease &staged,std::string &error) const;
    void configureSource(std::filesystem::path executable,std::string version,
                         std::string certificates,UpdateSource source);
};

} // namespace papernx
