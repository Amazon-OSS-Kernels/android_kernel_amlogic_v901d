/*
 * Copyright (c) 2021 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * File is based off Android's ziparchive reader: system/core/libziparchive/zip_archive.h
 *
 * Used to extract img files from a zip file and flash it to a partition.
 *
 * It will flash every img file located in the zip folder.
 * I.E if image_files.zip has userdata.img, system.img, and boot.img, it will flash it to
 * userdata, system_a/system_b partition, and boot_a/boot_b partition.
 *
 * Read more about zip file formatting here: https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT
 * Wiki located here: https://www.amazon.com
 */

#ifndef _ZIP_ARCHIVE_H_
#define _ZIP_ARCHIVE_H_

#define EOCD_SIGNATURE 0x06054b50
#define LOCAL_FILE_HDR_SIGNATURE 0x04034b50
#define CDR_SIGNATURE 0x02014b50
#define GPBDD_FLAG_MASK 0x0008
#define MAX_COMMENT_LEN 65535

// The *optional* data descriptor start signature.
#define DATA_OPT_SIGNATURE 0x08074b50

typedef void* ZipArchiveHandle;

/* Zip compression methods we support */
enum {
    kCompressStored = 0,    // no compression
    kCompressDeflated = 8,  // standard deflate
};

typedef struct ZipString {
    const char* name;
    uint16_t name_length;
    // Offset of where the CDR for this zip file is located
    loff_t cd_offset;
} ZipString;

typedef struct CentralDirectory {
    loff_t start;
    size_t length;
} CentralDirectory;

/*
 * Represents information about a zip archive
 */
typedef struct ZipArchive {
    // open Zip archive
    const int close_file;
    // mapped central directory area
    loff_t directory_offset;
    CentralDirectory central_directory;
    // number of entries in the Zip archive
    uint16_t num_entries;
    // List of zip string
    ZipString* zip_string_list;
    const char* filename;
    loff_t file_size;
} ZipArchive;

/*
 * Represents information about a zip entry in a zip file.
 */
typedef struct ZipEntry {
    // Compression method: One of kCompressStored or
    // kCompressDeflated.
    uint16_t method;

    // Modification time. The zipfile format specifies
    // that the first two little endian bytes contain the time
    // and the last two little endian bytes contain the date.
    // See `GetModificationTime`.
    // TODO: should be overridden by extra time field, if present.
    uint32_t mod_time;

    // Suggested Unix mode for this entry, from the zip archive if created on
    // Unix, or a default otherwise.
    mode_t unix_mode;

    // 1 if this entry contains a data descriptor segment, 0
    // otherwise.
    uint8_t has_data_descriptor;

    // Crc32 value of this ZipEntry. This information might
    // either be stored in the local file header or in a special
    // Data descriptor footer at the end of the file entry.
    uint32_t crc32;

    // Compressed length of this ZipEntry. Might be present
    // either in the local file header or in the data descriptor
    // footer.
    uint32_t compressed_length;

    // Uncompressed length of this ZipEntry. Might be present
    // either in the local file header or in the data descriptor
    // footer.
    uint32_t uncompressed_length;

    // The offset to the start of data for this ZipEntry.
    loff_t offset;
 } ZipEntry;

// The "end of central directory" (EOCD) record. Each archive
// contains exactly once such record which appears at the end of
// the archive. It contains archive wide information like the
// number of entries in the archive and the offset to the central
// directory of the offset.
typedef struct __attribute__((packed)) {
    // End of central directory signature, should always be
    // |kSignature|.
    uint32_t eocd_signature;
    // The number of the current "disk", i.e, the "disk" that this
    // central directory is on.
    //
    // This implementation assumes that each archive spans a single
    // disk only. i.e, that disk_num == 1.
    uint16_t disk_num;
    // The disk where the central directory starts.
    //
    // This implementation assumes that each archive spans a single
    // disk only. i.e, that cd_start_disk == 1.
    uint16_t cd_start_disk;
    // The number of central directory records on this disk.
    //
    // This implementation assumes that each archive spans a single
    // disk only. i.e, that num_records_on_disk == num_records.
    uint16_t num_records_on_disk;
    // The total number of central directory records.
    uint16_t num_records;
    // The size of the central directory (in bytes).
    uint32_t cd_size;
    // The offset of the start of the central directory, relative
    // to the start of the file.
    uint32_t cd_start_offset;
    // Length of the central directory comment.
    uint16_t comment_length;
} EocdRecord;


// A structure representing the fixed length fields for a single
// record in the central directory of the archive. In addition to
// the fixed length fields listed here, each central directory
// record contains a variable length "file_name" and "extra_field"
// whose lengths are given by |file_name_length| and |extra_field_length|
// respectively.
typedef struct __attribute__((packed)) {
    // The start of record signature. Must be |kSignature|.
    uint32_t record_signature;
    // Source tool version. Top byte gives source OS.
    uint16_t version_made_by;
    // Tool version. Ignored by this implementation.
    uint16_t version_needed;
    // The "general purpose bit flags" for this entry. The only
    // flag value that we currently check for is the "data descriptor"
    // flag.
    uint16_t gpb_flags;
    // The compression method for this entry, one of |kCompressStored|
    // and |kCompressDeflated|.
    uint16_t compression_method;
    // The file modification time and date for this entry.
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    // The CRC-32 checksum for this entry.
    uint32_t crc32;
    // The compressed size (in bytes) of this entry.
    uint32_t compressed_size;
    // The uncompressed size (in bytes) of this entry.
    uint32_t uncompressed_size;
    // The length of the entry file name in bytes. The file name
    // will appear immediately after this record.
    uint16_t file_name_length;
    // The length of the extra field info (in bytes). This data
    // will appear immediately after the entry file name.
    uint16_t extra_field_length;
    // The length of the entry comment (in bytes). This data will
    // appear immediately after the extra field.
    uint16_t comment_length;
    // The start disk for this entry. Ignored by this implementation).
    uint16_t file_start_disk;
    // File attributes. Ignored by this implementation.
    uint16_t internal_file_attributes;
    // File attributes. For archives created on Unix, the top bits are the mode.
    uint32_t external_file_attributes;
    // The offset to the local file header for this entry, from the
    // beginning of this archive.
    uint32_t local_file_header_offset;
} CentralDirectoryRecord;

// The local file header for a given entry. This duplicates information
// present in the central directory of the archive. It is an error for
// the information here to be different from the central directory
// information for a given entry.
typedef struct __attribute__((packed)) {
    // The local file header signature, must be |kSignature|.
    uint32_t lfh_signature;
    // Tool version. Ignored by this implementation.
    uint16_t version_needed;
    // The "general purpose bit flags" for this entry. The only
    // flag value that we currently check for is the "data descriptor"
    // flag.
    uint16_t gpb_flags;
    // The compression method for this entry, one of |kCompressStored|
    // and |kCompressDeflated|.
    uint16_t compression_method;
    // The file modification time and date for this entry.
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    // The CRC-32 checksum for this entry.
    uint32_t crc32;
    // The compressed size (in bytes) of this entry.
    uint32_t compressed_size;
    // The uncompressed size (in bytes) of this entry.
    uint32_t uncompressed_size;
    // The length of the entry file name in bytes. The file name
    // will appear immediately after this record.
    uint16_t file_name_length;
    // The length of the extra field info (in bytes). This data
    // will appear immediately after the entry file name.
    uint16_t extra_field_length;
} LocalFileHeader;

typedef struct ZipExtractor {
    // Offset in the zip file
    loff_t zip_offset;
    // Length from current offset to end of entry
    loff_t bytes_to_process;
    // Amount of compressed data processed
    loff_t amount_processed;
    // Original length of the compressed file
    loff_t compressed_length;
    // Length of uncompressed file
    loff_t uncompressed_length;
    // Read buffer from zstream
    uint8_t *read_buffer;
    // Write buffer from zstream
    uint8_t *write_buffer;
    // Partition to write to
    char *partition;
    // Corresponding _b partition to write to if applicable
    char *partition_other;
    // Disk partition to write to
    disk_partition_t info;
    // Corresponding _b partition disk information
    disk_partition_t info_other;
} ZipExtractor;

/*
 * Open Zip Archive and load it into @handle
 * Returns 0 on success, -1 on error
 *
 * @handle: ZipArchiveHandle to load the information into
 * @filename: zip file to load in
 * @file_size: size of the zip file
 */
int32_t open_archive(ZipArchiveHandle* handle, char* filename, loff_t file_size);

/*
 * Find Zip Entry in Archive and load it into @data
 * Returns 0 on success, -1 on error
 *
 * @handle: ZipArchiveHandle to check for entry in
 * @entry_name: ZipString to search for in the archive
 * @data: ZipEntry to load the entry into if found
 */
int32_t find_entry(const ZipArchiveHandle handle, const ZipString entry_name, ZipEntry* data);


/*
 * Extract image from zip file and flash it to partition
 * zip_extractor.partiton should be set before calling this function otherwise error will occur
 *
 * Returns 0 on success, -1 on error
 *
 * @handle: ZipArchiveHandle to extract from
 * @entry: Entry to extract
 * @zip_extractor: zip_extractor handler for this extraction
 */
int32_t extract_image_to_partition(ZipArchiveHandle handle, ZipEntry* entry, ZipExtractor *zip_extractor);

/*
 * Extract version from zip file and store it in @buffer
 *
 * Returns 0 on success, -1 on error
 *
 * @handle: ZipArchiveHandle to extract from
 * @entry: Entry to extract
 * @buffer: Buffer to hold version
 */
int32_t extract_version(ZipArchiveHandle handle, ZipEntry* entry, char* buffer);

#endif
