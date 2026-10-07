// Read every native SWORD commentary slot, including linked entries. The
// generic imp2vs writer rebuilds the isolated copy; SHA-256 verifies that
// all non-Baruc entries retain their original bytes.
#include <swmgr.h>
#include <swmodule.h>
#include <versekey.h>
#include <glib.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

int main(int argc, char **argv) {
    if (argc != 4 || (std::string(argv[1]) != "export" && std::string(argv[1]) != "snapshot")) {
        std::fprintf(stderr, "Usage: %s export|snapshot SWORD_ROOT IMP_FILE\n", argv[0]);
        return 2;
    }
    // A fixed, explicit tree: never augment it with ~/.sword.
    sword::SWMgr manager(argv[2], true, nullptr, false, false);
    auto *module = manager.getModule("TorresAmatNotas");
    if (!module) { std::fputs("TorresAmatNotas missing\n", stderr); return 1; }
    std::unique_ptr<sword::SWKey> owned(module->createKey());
    auto *key = dynamic_cast<sword::VerseKey *>(owned.get());
    if (!key) return 1;

    std::ofstream output(argv[3]);
    GChecksum *outside = g_checksum_new(G_CHECKSUM_SHA256);
    std::size_t outsideEntries = 0, barEntries = 0;
    key->setIntros(true);
    for (key->setPosition(sword::TOP); !key->popError(); key->increment()) {
        module->setKey(key);
        // Testament introductions have book=0; getOSISRef() in libsword
        // assumes a real book and cannot be called for those slots.
        const std::string ref = key->getBook() > 0 ? key->getOSISRef() :
            "intro:" + std::to_string(key->getTestament()) + ":" +
            std::to_string(key->getChapter()) + ":" + std::to_string(key->getVerse());
        const std::string text = module->getRawEntry();
        const bool bar = key->getBook() > 0 && key->getChapter() > 0 && key->getVerse() > 0 &&
            std::string(key->getOSISBookName()) == "Bar";
        if (std::string(argv[1]) == "export")
            output << "$$$" << key->getText() << "\n" << text << "\n";
        if (bar) {
            if (std::string(argv[1]) == "snapshot")
                output << "$$$" << ref << "\n" << text << "\n";
            ++barEntries;
        } else {
            const std::string record = ref + '\0' + text + '\0';
            g_checksum_update(outside, reinterpret_cast<const guchar *>(record.data()), record.size());
            ++outsideEntries;
        }
    }
    output.close();
    std::printf("notes_bar_entries=%zu notes_other_entries=%zu notes_other_sha256=%s\n",
                barEntries, outsideEntries, g_checksum_get_string(outside));
    g_checksum_free(outside);
    return output && barEntries == 213 ? 0 : 1;
}
