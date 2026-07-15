#ifndef PREFS_MIGRATION_LOGIC_H
#define PREFS_MIGRATION_LOGIC_H

#include <stddef.h>
#include <stdint.h>

#include "lenses.h"

enum class PrefsLoadMode
{
  LOAD_SCHEMA = 0,
  MIGRATE_LEGACY = 1,
  LOAD_DEFAULTS = 2
};

// What the Health screen's "Prefs:" line should say. NEWER_SCHEMA_LOADED
// covers a firmware downgrade: the stored schema is newer than this build,
// values loaded best-effort, and reporting "Defaults" would mislead triage.
enum class PrefsHealthLabel
{
  SCHEMA_OK = 0,
  LEGACY_MIGRATED = 1,
  NEWER_SCHEMA_LOADED = 2,
  DEFAULTS = 3
};

PrefsHealthLabel selectPrefsHealthLabel(
    bool schema_valid,
    bool loaded_legacy,
    uint16_t loaded_version,
    uint16_t current_version);

size_t expectedLegacyLensBlobSize(size_t lens_count);
PrefsLoadMode selectPrefsLoadMode(
    uint16_t stored_schema,
    uint16_t current_schema,
    size_t legacy_blob_bytes,
    size_t expected_legacy_blob_bytes);
bool applyLegacyLensBlob(
    const uint8_t *legacy_blob,
    size_t legacy_blob_bytes,
    Lens *target_lenses,
    size_t target_lens_count);

#endif // PREFS_MIGRATION_LOGIC_H
