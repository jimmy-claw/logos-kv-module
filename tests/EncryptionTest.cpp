#include <gtest/gtest.h>

#include "kv_plugin.h"

#include <filesystem>
#include <atomic>
#include <unistd.h>

class EncryptionTest : public ::testing::Test {
protected:
    std::unique_ptr<KvPlugin> plugin;
    std::filesystem::path temp_dir_;

    std::filesystem::path makeTempDir() {
        static std::atomic<int> counter{0};
        const char* base_env = std::getenv("KV_TEST_TMPDIR");
        std::filesystem::path base = base_env ? std::filesystem::path(base_env)
                                              : std::filesystem::temp_directory_path();
        auto dir = base / ("kv_enc_" + std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1)));
        std::filesystem::create_directories(dir);
        return dir;
    }

    void SetUp() override {
        temp_dir_ = makeTempDir();
        plugin = std::make_unique<KvPlugin>();
        plugin->setDataDir(QString::fromStdString(temp_dir_.string()));
    }

    void TearDown() override {
        plugin.reset();
        std::filesystem::remove_all(temp_dir_);
    }

    // 64 hex chars = 32 bytes
    static constexpr const char* KEY_HEX_1 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    static constexpr const char* KEY_HEX_2 = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
};

TEST_F(EncryptionTest, RoundtripSetGet) {
    plugin->setEncryptionKey("secure", KEY_HEX_1);
    plugin->set("secure", "greeting", "hello world");
    EXPECT_EQ(plugin->get("secure", "greeting"), "hello world");
}

TEST_F(EncryptionTest, StoredValueIsEncrypted) {
    plugin->setEncryptionKey("secure", KEY_HEX_1);
    plugin->set("secure", "secret", "plaintext-value");

    // Read the raw stored value without encryption key
    KvPlugin raw;
    raw.setDataDir(QString::fromStdString(temp_dir_.string()));
    QString stored = raw.get("secure", "secret");

    EXPECT_FALSE(stored.isEmpty());
    EXPECT_NE(stored, "plaintext-value");
}

TEST_F(EncryptionTest, NoKeyNamespaceWorksAsPlaintext) {
    plugin->set("plain", "key1", "value1");
    EXPECT_EQ(plugin->get("plain", "key1"), "value1");
}

TEST_F(EncryptionTest, WrongKeyReturnsEmpty) {
    plugin->setEncryptionKey("secure", KEY_HEX_1);
    plugin->set("secure", "key1", "secret-data");

    // Try to read with a different key
    KvPlugin wrong;
    wrong.setDataDir(QString::fromStdString(temp_dir_.string()));
    wrong.setEncryptionKey("secure", KEY_HEX_2);
    EXPECT_EQ(wrong.get("secure", "key1"), "");
}

TEST_F(EncryptionTest, MultiNamespaceDifferentKeys) {
    plugin->setEncryptionKey("ns1", KEY_HEX_1);
    plugin->setEncryptionKey("ns2", KEY_HEX_2);

    plugin->set("ns1", "key", "value-for-ns1");
    plugin->set("ns2", "key", "value-for-ns2");

    EXPECT_EQ(plugin->get("ns1", "key"), "value-for-ns1");
    EXPECT_EQ(plugin->get("ns2", "key"), "value-for-ns2");
}

TEST_F(EncryptionTest, EncryptedAndPlainNamespacesCoexist) {
    plugin->setEncryptionKey("encrypted", KEY_HEX_1);

    plugin->set("encrypted", "key", "secret");
    plugin->set("plain", "key", "public");

    EXPECT_EQ(plugin->get("encrypted", "key"), "secret");
    EXPECT_EQ(plugin->get("plain", "key"), "public");
}

TEST_F(EncryptionTest, EmptyValueRoundtrip) {
    plugin->setEncryptionKey("secure", KEY_HEX_1);
    plugin->set("secure", "empty", "");
    EXPECT_EQ(plugin->get("secure", "empty"), "");
}

TEST_F(EncryptionTest, UnicodeValueRoundtrip) {
    plugin->setEncryptionKey("secure", KEY_HEX_1);
    plugin->set("secure", "emoji", QString::fromUtf8("hello 🌍 world"));
    EXPECT_EQ(plugin->get("secure", "emoji"), QString::fromUtf8("hello 🌍 world"));
}
