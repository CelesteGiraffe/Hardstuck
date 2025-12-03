#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

#include "storage/LocalDataStore.h"
#include "user/UserIdResolver.h"

int main()
{
    namespace fs = std::filesystem;
    const fs::path base = fs::temp_directory_path() / "hs_local_store_test";
    const std::string userId = "test-user";

    LocalDataStore store(base, userId);
    store.SetLimits(64, 2);

    std::string error;
    const std::vector<std::string> payloads = { "{\"timestamp\":\"t1\",\"playlist\":\"p\",\"mmr\":1,\"sessionType\":\"ranked\"}" };
    bool ok = store.AppendPayloadsWithVerification(payloads, error);
    assert(ok && error.empty());

    fs::path expected = base / userId / "local_history.jsonl";
    assert(fs::exists(expected));

    // Force rotation
    for (int i = 0; i < 10; ++i)
    {
        std::string p = std::string("{\"mmr\":") + std::to_string(i) + ",\"sessionType\":\"ranked\"}";
        store.AppendPayloadsWithVerification({p}, error);
    }
    fs::path rotated = base / userId / "local_history.jsonl.1";
    assert(fs::exists(rotated));

    // Resolver sanity checks
    const std::string resolvedPlatform = UserIdResolver::ResolveUserIdFromStrings("Epic-ABC_123", "");
    assert(resolvedPlatform.find("epic-abc-123") != std::string::npos);
    const std::string resolvedInstall = UserIdResolver::ResolveUserIdFromStrings("", "install-seed");
    assert(!resolvedInstall.empty());

    return 0;
}
