#include <gtest/gtest.h>

#include "backends/KvBackend.h"
#include "backends/MemoryBackend.h"
#include "backends/FileBackend.h"
#ifdef HAVE_ROCKSDB
#include "backends/RocksDbBackend.h"
#endif
#ifdef HAVE_SQLITE
#include "backends/SqliteBackend.h"
#endif

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

class BackendConformanceTest : public ::testing::TestWithParam<std::string> {
protected:
    std::unique_ptr<KvBackend> backend;
    std::vector<std::filesystem::path> temp_dirs_;

    std::unique_ptr<KvBackend> makeBackend() {
        if (GetParam() == "MemoryBackend") {
            return std::make_unique<MemoryBackend>();
        }
#ifdef HAVE_ROCKSDB
        if (GetParam() == "RocksDbBackend") {
            auto dir = makeTempDir();
            return std::make_unique<RocksDbBackend>(dir);
        }
#endif
#ifdef HAVE_SQLITE
        if (GetParam() == "SqliteBackend") {
            auto dir = makeTempDir();
            return std::make_unique<SqliteBackend>(dir / "test.db");
        }
#endif
        auto dir = makeTempDir();
        return std::make_unique<FileBackend>(dir);
    }

    std::filesystem::path makeTempDir() {
        static std::atomic<int> counter{0};
        // Respect KV_TEST_TMPDIR env var (set in Nix sandbox where /tmp is read-only)
        const char* base_env = std::getenv("KV_TEST_TMPDIR");
        std::filesystem::path base = base_env ? std::filesystem::path(base_env)
                                              : std::filesystem::temp_directory_path();
        // Include PID to avoid collisions when ctest spawns separate processes per test
        auto dir = base / ("kv_" + std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1)));
        std::filesystem::create_directories(dir);
        temp_dirs_.push_back(dir);
        return dir;
    }

    void SetUp() override {
        backend = makeBackend();
    }

    void TearDown() override {
        backend.reset();
        for (const auto &d : temp_dirs_) {
            std::filesystem::remove_all(d);
        }
    }
};

// --- Basic operations ---

TEST_P(BackendConformanceTest, SetGetRoundtrip) {
    backend->set("key1", "value1");
    auto result = backend->get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value1");
}

TEST_P(BackendConformanceTest, GetMissingKeyReturnsEmpty) {
    auto result = backend->get("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST_P(BackendConformanceTest, OverwriteExistingKey) {
    backend->set("key1", "original");
    backend->set("key1", "updated");
    auto result = backend->get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "updated");
}

TEST_P(BackendConformanceTest, RemoveKey) {
    backend->set("key1", "value1");
    backend->remove("key1");
    auto result = backend->get("key1");
    EXPECT_FALSE(result.has_value());
}

TEST_P(BackendConformanceTest, GetAfterRemoveReturnsEmpty) {
    backend->set("key1", "value1");
    backend->set("key2", "value2");
    backend->remove("key1");

    EXPECT_FALSE(backend->get("key1").has_value());
    auto result = backend->get("key2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value2");
}

// --- List operations ---

TEST_P(BackendConformanceTest, ListWithPrefixReturnsOnlyMatchingKeys) {
    backend->set("user:1", "alice");
    backend->set("user:2", "bob");
    backend->set("item:1", "widget");

    auto result = backend->list("user:");
    std::sort(result.begin(), result.end());

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "user:1");
    EXPECT_EQ(result[1], "user:2");
}

TEST_P(BackendConformanceTest, ListOnEmptyBackendReturnsEmpty) {
    // Backend has no keys — list with any prefix should return empty
    auto result = backend->list("user:");
    EXPECT_TRUE(result.empty());
}

TEST_P(BackendConformanceTest, ListEmptyPrefixReturnsAllKeys) {
    // Empty prefix matches all keys
    backend->set("a", "1");
    backend->set("b", "2");
    auto result = backend->list("");
    EXPECT_EQ(result.size(), 2u);
}

// --- Clear operations ---

TEST_P(BackendConformanceTest, ClearRemovesAllKeysInNamespace) {
    backend->set("a", "1");
    backend->set("b", "2");
    backend->set("c", "3");

    backend->clear();

    EXPECT_FALSE(backend->get("a").has_value());
    EXPECT_FALSE(backend->get("b").has_value());
    EXPECT_FALSE(backend->get("c").has_value());
    EXPECT_TRUE(backend->list("").empty());
}

TEST_P(BackendConformanceTest, ClearDoesNotAffectOtherNamespaces) {
    auto other = makeBackend();

    backend->set("key1", "value1");
    other->set("key2", "value2");

    backend->clear();

    EXPECT_FALSE(backend->get("key1").has_value());
    auto result = other->get("key2");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value2");
}

// --- Concurrency ---

TEST_P(BackendConformanceTest, ConcurrentReads) {
    backend->set("shared_key", "shared_value");

    constexpr int num_threads = 8;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, &success_count]() {
            auto result = backend->get("shared_key");
            if (result.has_value() && result.value() == "shared_value") {
                success_count++;
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads);
}

TEST_P(BackendConformanceTest, ConcurrentWrites) {
    constexpr int num_threads = 8;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i]() {
            std::string key = "key_" + std::to_string(i);
            std::string value = "value_" + std::to_string(i);
            backend->set(key, value);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    for (int i = 0; i < num_threads; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string expected = "value_" + std::to_string(i);
        auto result = backend->get(key);
        ASSERT_TRUE(result.has_value()) << "Missing key: " << key;
        EXPECT_EQ(result.value(), expected);
    }
}

// --- Scan operations ---

TEST_P(BackendConformanceTest, ScanEmptyPatternReturnsAllKeys) {
    backend->set("user:1", "alice");
    backend->set("user:2", "bob");
    backend->set("item:1", "widget");

    auto result = backend->scan("");
    EXPECT_EQ(result.size(), 3u);
}

TEST_P(BackendConformanceTest, ScanFiltersBySubstring) {
    backend->set("user:1", "alice");
    backend->set("user:2", "bob");
    backend->set("item:1", "widget");

    auto result = backend->scan("user");
    std::sort(result.begin(), result.end());

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "user:1");
    EXPECT_EQ(result[1], "user:2");
}

TEST_P(BackendConformanceTest, ScanNoMatchReturnsEmpty) {
    backend->set("user:1", "alice");
    backend->set("item:1", "widget");

    auto result = backend->scan("zzz");
    EXPECT_TRUE(result.empty());
}

TEST_P(BackendConformanceTest, ScanMatchesSubstringAnywhere) {
    backend->set("my_user_key", "val1");
    backend->set("admin_user", "val2");
    backend->set("item:1", "val3");

    auto result = backend->scan("user");
    std::sort(result.begin(), result.end());

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "admin_user");
    EXPECT_EQ(result[1], "my_user_key");
}

// --- SearchValues operations ---

TEST_P(BackendConformanceTest, SearchValuesFindsSubstringCaseInsensitive) {
    backend->set("k1", "Hello World");
    backend->set("k2", "goodbye");
    backend->set("k3", "HELLO there");

    auto result = backend->searchValues("hello");
    std::sort(result.begin(), result.end());

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].first, "k1");
    EXPECT_EQ(result[0].second, "Hello World");
    EXPECT_EQ(result[1].first, "k3");
    EXPECT_EQ(result[1].second, "HELLO there");
}

TEST_P(BackendConformanceTest, SearchValuesNoMatchReturnsEmpty) {
    backend->set("k1", "Hello World");
    backend->set("k2", "goodbye");

    auto result = backend->searchValues("zzz");
    EXPECT_TRUE(result.empty());
}

TEST_P(BackendConformanceTest, SearchValuesMultipleMatches) {
    backend->set("event:1", "Team Meeting at 10am");
    backend->set("event:2", "Lunch meeting with Bob");
    backend->set("event:3", "Dentist appointment");

    auto result = backend->searchValues("meeting");
    ASSERT_EQ(result.size(), 2u);

    // Verify both matching entries are present
    std::sort(result.begin(), result.end());
    EXPECT_EQ(result[0].first, "event:1");
    EXPECT_EQ(result[0].second, "Team Meeting at 10am");
    EXPECT_EQ(result[1].first, "event:2");
    EXPECT_EQ(result[1].second, "Lunch meeting with Bob");
}

TEST_P(BackendConformanceTest, SearchValuesEmptySubstringReturnsAll) {
    backend->set("k1", "value1");
    backend->set("k2", "value2");

    auto result = backend->searchValues("");
    EXPECT_EQ(result.size(), 2u);
}

static auto backendValues() {
    std::vector<std::string> backends = {"MemoryBackend", "FileBackend"};
#ifdef HAVE_ROCKSDB
    backends.push_back("RocksDbBackend");
#endif
#ifdef HAVE_SQLITE
    backends.push_back("SqliteBackend");
#endif
    return ::testing::ValuesIn(backends);
}

INSTANTIATE_TEST_SUITE_P(
    Backends,
    BackendConformanceTest,
    backendValues(),
    [](const ::testing::TestParamInfo<std::string> &info) {
        return info.param;
    });
