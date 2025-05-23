/*
 * libcsync -- a library to sync a directory with another
 *
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2012 ownCloud GmbH
 * SPDX-FileCopyrightText: 2008-2013 Andreas Schneider <asn@cryptomilk.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * @file csync.h
 *
 * @brief Application developer interface for csync.
 *
 * @defgroup csyncPublicAPI csync public API
 *
 * @{
 */

#ifndef _CSYNC_H
#define _CSYNC_H

#include <QByteArray>
#include <QVariant>
#include <QLoggingCategory>

#include <cstdint>
#include <sys/stat.h>
#include <sys/types.h>
#include <functional>
#include <memory>

#include "ocsynclib.h"
#include "config_csync.h"
#include "std/c_private.h"
#include "common/remotepermissions.h"

namespace OCC {
Q_DECLARE_LOGGING_CATEGORY(lcPermanentLog)

class SyncJournalFileRecord;

namespace EncryptionStatusEnums {

OCSYNC_EXPORT Q_NAMESPACE

enum class ItemEncryptionStatus : int {
    NotEncrypted = 0,
    Encrypted = 1,
    EncryptedMigratedV1_2 = 2,
    EncryptedMigratedV2_0 = 3,
};

Q_ENUM_NS(ItemEncryptionStatus)

enum class JournalDbEncryptionStatus : int {
    NotEncrypted = 0,
    Encrypted = 1,
    EncryptedMigratedV1_2Invalid = 2,
    EncryptedMigratedV1_2 = 3,
    EncryptedMigratedV2_0 = 4,
};

Q_ENUM_NS(JournalDbEncryptionStatus)

ItemEncryptionStatus fromDbEncryptionStatus(JournalDbEncryptionStatus encryptionStatus);

JournalDbEncryptionStatus toDbEncryptionStatus(ItemEncryptionStatus encryptionStatus);

ItemEncryptionStatus fromEndToEndEncryptionApiVersion(const double version);

}

}

#if defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && !defined(Q_CC_CLANG) && (__GNUC__ * 100 + __GNUC_MINOR__ < 408)
// openSuse 12.3 didn't like enum bitfields.
#define BITFIELD(size)
#elif defined(Q_CC_MSVC)
// MSVC stores enum and bool as signed, so we need to add a bit for the sign
#define BITFIELD(size) :(size+1)
#else
#define BITFIELD(size) :size
#endif

namespace CSyncEnums {
OCSYNC_EXPORT Q_NAMESPACE

enum csync_status_codes_e {
  CSYNC_STATUS_OK         = 0,

  CSYNC_STATUS_ERROR      = 1024, /* don't use this code,
                                     */
  CSYNC_STATUS_UNSUCCESSFUL,       /* Unspecific problem happend */
  CSYNC_STATUS_STATEDB_LOAD_ERROR, /* Statedb can not be loaded. */
  CSYNC_STATUS_UPDATE_ERROR,       /* general update or discovery error */
  CSYNC_STATUS_TIMEOUT,            /* UNUSED */
  CSYNC_STATUS_HTTP_ERROR,         /* UNUSED */
  CSYNC_STATUS_PERMISSION_DENIED,  /*  */
  CSYNC_STATUS_NOT_FOUND,
  CSYNC_STATUS_FILE_EXISTS,
  CSYNC_STATUS_OUT_OF_SPACE,
  CSYNC_STATUS_SERVICE_UNAVAILABLE,
  CSYNC_STATUS_STORAGE_UNAVAILABLE,
  CSYNC_STATUS_FILE_SIZE_ERROR,
  CSYNC_STATUS_OPENDIR_ERROR,
  CSYNC_STATUS_READDIR_ERROR,
  CSYNC_STATUS_OPEN_ERROR,
  CSYNC_STATUS_ABORTED,
    /* Codes for file individual status: */
    CSYNC_STATUS_INDIVIDUAL_IS_SYMLINK,
    CSYNC_STATUS_INDIVIDUAL_IGNORE_LIST,
    CSYNC_STATUS_INDIVIDUAL_IS_INVALID_CHARS,
    CSYNC_STATUS_INDIVIDUAL_TRAILING_SPACE,
    CSYNC_STATUS_INDIVIDUAL_EXCLUDE_LONG_FILENAME,
    CSYNC_STATUS_INDIVIDUAL_EXCLUDE_HIDDEN,
    CSYNC_STATUS_INVALID_CHARACTERS,
    CSYNC_STATUS_INDIVIDUAL_STAT_FAILED,
    CSYNC_STATUS_FORBIDDEN,
    CSYNC_STATUS_INDIVIDUAL_TOO_DEEP,
    CSYNC_STATUS_INDIVIDUAL_IS_CONFLICT_FILE,
    CSYNC_STATUS_INDIVIDUAL_CANNOT_ENCODE
};
Q_ENUM_NS(csync_status_codes_e)

/**
  * Instruction enum. In the file traversal structure, it describes
  * the csync state of a file.
  */
enum SyncInstructions {
    CSYNC_INSTRUCTION_NONE                       = 0,       /* Nothing to do (UPDATE|RECONCILE) */
    CSYNC_INSTRUCTION_EVAL                       = 1 << 0,  /* There was changed compared to the DB (UPDATE) */
    CSYNC_INSTRUCTION_REMOVE                     = 1 << 1,  /* The file need to be removed (RECONCILE) */
    CSYNC_INSTRUCTION_RENAME                     = 1 << 2,  /* The file need to be renamed (RECONCILE) */
    CSYNC_INSTRUCTION_EVAL_RENAME                = 1 << 11, /* The file is new, it is the destination of a rename (UPDATE) */
    CSYNC_INSTRUCTION_NEW                        = 1 << 3,  /* The file is new compared to the db (UPDATE) */
    CSYNC_INSTRUCTION_CONFLICT                   = 1 << 4,  /* The file need to be downloaded because it is a conflict (RECONCILE) */
    CSYNC_INSTRUCTION_IGNORE                     = 1 << 5,  /* The file is ignored (UPDATE|RECONCILE) */
    CSYNC_INSTRUCTION_SYNC                       = 1 << 6,  /* The file need to be pushed to the other remote (RECONCILE) */
    CSYNC_INSTRUCTION_STAT_ERROR                 = 1 << 7,
    CSYNC_INSTRUCTION_ERROR                      = 1 << 8,
    CSYNC_INSTRUCTION_TYPE_CHANGE                = 1 << 9,  /* Like NEW, but deletes the old entity first (RECONCILE)
                                                               Used when the type of something changes from directory to file
                                                               or back. */
    CSYNC_INSTRUCTION_UPDATE_METADATA            = 1 << 10, /* If the etag has been updated and need to be writen to the db,
                                                               but without any propagation (UPDATE|RECONCILE) */
    CSYNC_INSTRUCTION_CASE_CLASH_CONFLICT        = 1 << 12, /* The file need to be downloaded because it is a case clash conflict (RECONCILE) */
    CSYNC_INSTRUCTION_UPDATE_VFS_METADATA        = 1 << 13, /* vfs item metadata are out of sync and we need to tell operating system about it */
    CSYNC_INSTRUCTION_UPDATE_ENCRYPTION_METADATA = 1 << 14, /* encryption metadata needs update after certificate was migrated */
};

Q_ENUM_NS(SyncInstructions)

// This enum is used with BITFIELD(3) and BITFIELD(4) in several places.
// Also, this value is stored in the database, so beware of value changes.
enum ItemType {
    ItemTypeFile = 0,
    ItemTypeSoftLink = 1,
    ItemTypeDirectory = 2,
    ItemTypeSkip = 3,

    /** The file is a dehydrated placeholder, meaning data isn't available locally */
    ItemTypeVirtualFile = 4,

    /** A ItemTypeVirtualFile that wants to be hydrated.
     *
     * Actions may put this in the db as a request to a future sync, such as
     * implicit hydration (when the user wants to access file data) when using
     * suffix vfs. For pin-state driven hydrations changing the database is
     * not necessary.
     *
     * For some vfs plugins the placeholder files on disk may be marked for
     * (de-)hydration (like with a file attribute) and then the local discovery
     * will return this item type.
     *
     * The discovery will also use this item type to mark entries for hydration
     * if an item's pin state mandates it, such as when encountering a AlwaysLocal
     * file that is dehydrated.
     */
    ItemTypeVirtualFileDownload = 5,

    /** A ItemTypeFile that wants to be dehydrated.
     *
     * Similar to ItemTypeVirtualFileDownload, but there's currently no situation
     * where it's stored in the database since there is no action that triggers a
     * file dehydration without changing the pin state.
     */
    ItemTypeVirtualFileDehydration = 6,
};
Q_ENUM_NS(ItemType)
}

using namespace CSyncEnums;
using CSYNC_STATUS = CSyncEnums::csync_status_codes_e;
using csync_file_stat_t = struct csync_file_stat_s;

struct OCSYNC_EXPORT csync_file_stat_s {
  time_t modtime = 0;
  int64_t size = 0;
  uint64_t inode = 0;

  OCC::RemotePermissions remotePerm;
  ItemType type BITFIELD(4);
  bool child_modified BITFIELD(1);
  bool has_ignored_files BITFIELD(1); // Specify that a directory, or child directory contains ignored files.
  bool is_hidden BITFIELD(1); // Not saved in the DB, only used during discovery for local files.
  bool isE2eEncrypted BITFIELD(1);
  bool is_metadata_missing BITFIELD(1); // Indicates the file has missing metadata, f.ex. the file is not a placeholder in case of vfs.
  bool isPermissionsInvalid BITFIELD(1);

  QByteArray path;
  QByteArray rename_path;
  QByteArray etag;
  QByteArray file_id;
  QByteArray directDownloadUrl;
  QByteArray directDownloadCookies;
  QByteArray original_path; // only set if locale conversion fails

  // In the local tree, this can hold a checksum and its type if it is
  //   computed during discovery for some reason.
  // In the remote tree, this will have the server checksum, if available.
  // In both cases, the format is "SHA1:baff".
  QByteArray checksumHeader;
  QByteArray e2eMangledName;

  CSYNC_STATUS error_status = CSYNC_STATUS_OK;

  SyncInstructions instruction = CSYNC_INSTRUCTION_NONE; /* u32 */

  csync_file_stat_s()
    : type(ItemTypeSkip)
    , child_modified(false)
    , has_ignored_files(false)
    , is_hidden(false)
    , isE2eEncrypted(false)
    , is_metadata_missing(false)
    , isPermissionsInvalid(false)
  { }
};

/**
 * }@
 */
#endif /* _CSYNC_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
