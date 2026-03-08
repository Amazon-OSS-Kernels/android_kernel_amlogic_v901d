/*
 * Copyright (c) 2021 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * File is based off Android's ziparchive reader: system/core/libziparchive/zip_archive.cc
 */

#include <amlogic/aml_mmc.h>
#include <asm-generic/unaligned.h>
#include <amzn_multiconfigs.h>
#include <fat.h>
#include <fs.h>
#include <linux/string.h>
#include <linux/time.h>
#include <u-boot/zlib.h>
#include <part.h>
#include <aboot.h>
#include <ide.h>
#include <common.h>
#include <malloc.h>
#include <config.h>
#include <fat_file_helper.h>
#include "zip_archive.h"

#define ZIP_MAX_LEN 0xffffffff
#define FILE_READ_ADDR 0x10800000
#define UPDATE_FILE_LOAD_ADDR 0x19000000 // Leave a buffer of 136MB to hold the extracted chunk
#define READ_BUFFER_MAX_SIZE 10000000 //10MB
#define MAX_UNCOMPRESSED_SIZE 0x10000000 //256MB
#define MAX_CHUNK_SIZE 0x8200000 //136.3MB

static int strcmp_l2(const char *s1, const char *s2)
{
    if (!s1 || !s2)
        return -1;
    return strncmp(s1, s2, strlen(s2));
}

// Copy "read_amount" from memory loaded update file at "start_offset" into "buffer"
void read_file_at_offset(loff_t start_offset, void* buffer, loff_t read_amount)
{
    memcpy(buffer, (void *)UPDATE_FILE_LOAD_ADDR + start_offset, read_amount);
}

int32_t map_eocd_to_archive(ZipArchive* archive, loff_t file_length,
                            loff_t read_amount, uint8_t* scan_buffer)
{
    const loff_t search_start = file_length - read_amount;

    read_file_at_offset(search_start, scan_buffer, read_amount);

    /*
    * Scan backward for the EOCD magic.  In an archive without a trailing
    * comment, we'll find it on the first try.  (We may want to consider
    * doing an initial minimal read; if we don't find it, retry with a
    * second read as above.)
    */
    int i = read_amount - sizeof(EocdRecord);
    for (; i >= 0; i--) {
        if (scan_buffer[i] == 0x50) {
            uint32_t* sig_addr = (const uint32_t *)(&scan_buffer[i]);
            if (get_unaligned(sig_addr)== EOCD_SIGNATURE) {
                printf("+++ Found EOCD at buf+%d\n", i);
                break;
            }
        }
    }
    if (i < 0) {
        printf("Zip: EOCD not found, %s is not zip\n", archive->filename);
        return -1;
    }

    const loff_t eocd_offset = search_start + i;
    EocdRecord *eocd = (EocdRecord *)(&(scan_buffer[i]));

    /*
    * Verify that there's no trailing space at the end of the central directory
    * and its comment.
    */
    const loff_t calculated_length = eocd_offset + sizeof(EocdRecord) + eocd->comment_length;
    if (calculated_length != file_length) {
        printf("Zip: %llu leftover bytes at the end of the central directory\n",
                     (file_length - calculated_length));
        return -1;
    }

    /*
    * Grab the CD offset and size, and the number of entries in the
    * archive and verify that they look reasonable.
    */
    if ((loff_t)(eocd->cd_start_offset) + eocd->cd_size > eocd_offset) {
        printf("Zip: bad offsets (dir %d, size %d, eocd %llu\n", eocd->cd_start_offset,
                                                                 eocd->cd_size,
                                                                 eocd_offset);
        return -1;
    }
    if (eocd->num_records == 0) {
        printf("Zip: empty archive?\n");
        return -1;
    }

    printf("+++ num_entries=%d, dir_size=%d, dir_offset=%d\n", eocd->num_records,
                                                               eocd->cd_size,
                                                               eocd->cd_start_offset);

    archive->num_entries = eocd->num_records;
    archive->directory_offset = eocd->cd_start_offset;
    archive->central_directory.start = eocd->cd_start_offset;
    archive->central_directory.length = eocd->cd_size;

    return 0;
}

/*
 * Find the zip Central Directory and link it to the ZipArchive
 *
 * On success, returns 0 after populating fields from the EOCD area:
 *   directory_offset
 *   directory_ptr
 *   num_entries
 * -1 on error
 */
int32_t map_central_directory(ZipArchive* archive)
{
    // Test file length. We use fat_size to make sure the file
    // is small enough to be a zip file (Its size must be less than
    // 0xffffffff bytes).
    loff_t file_length = archive->file_size;

    if (file_length > ZIP_MAX_LEN) {
        printf("Zip: zip file too long %llu\n", file_length);
        return -1;
    }
    if (file_length < (loff_t)sizeof(EocdRecord)) {
        printf("Zip: zip file too small to be zip %llu\n", file_length);
        return -1;
    }
    /*
     * Perform the traditional EOCD snipe hunt.
     *
     * We're searching for the End of Central Directory magic number,
     * which appears at the start of the EOCD block.  It's followed by
     * 18 bytes of EOCD stuff and up to 64KB of archive comment.  We
     * need to read the last part of the file into a buffer, dig through
     * it to find the magic number, parse some values out, and use those
     * to determine the extent of the CD.
     *
     * We start by pulling in the last part of the file.
     */
    // The maximum number of bytes to scan backwards for the EOCD start.
    const uint32_t kMaxEOCDSearch = MAX_COMMENT_LEN + sizeof(EocdRecord);
    loff_t read_amount = kMaxEOCDSearch;
    if (file_length < read_amount) {
        read_amount = file_length;
    }
    uint8_t scan_buffer[read_amount];

    int32_t result = map_eocd_to_archive(archive, file_length, read_amount, scan_buffer);
    return result;
}

// Debug print function
void print_zip_archive(ZipArchive* archive)
{
    int i;
    for(i = 0; i < archive->num_entries; i++) {
        if (archive->zip_string_list[i].name)
            printf("Filename: %s, filename_length: %d\n", archive->zip_string_list[i].name,
                                                          archive->zip_string_list[i].name_length);
    }
}

// Debug print function
void print_zip_entry(ZipEntry* entry)
{
    printf("Method: %d, mod_time: %d,"
            "has_data_descriptor: %d\ncrc32: %d,"
            "compressed_length: %d\n"
            "uncompressed_length: %d, offset: %llu\n", entry->method, entry->mod_time,
                                                       entry->has_data_descriptor, entry->crc32,
                                                       entry->compressed_length,
                                                       entry->uncompressed_length, entry->offset);
}

// Check if |length| bytes at |entry_name| constitute a valid entry name.
// Entry names must be valid UTF-8 and must not contain '0'.
int is_valid_entry_name(const uint8_t* entry_name, const size_t length)
{
    size_t i;
    for (i = 0; i < length; ++i) {
        const uint8_t byte = entry_name[i];
        if (byte == 0) {
            return 0;
        } else if ((byte & 0x80) == 0) {
            // Single byte sequence.
            continue;
        } else if ((byte & 0xc0) == 0x80 || (byte & 0xfe) == 0xfe) {
            // Invalid sequence.
            return 0;
        } else {
            uint8_t first;
            // 2-5 byte sequences.
            for (first = byte << 1; first & 0x80; first <<= 1) {
                ++i;

                // Missing continuation byte..
                if (i == length) {
                    return 0;
                }

                // Invalid continuation byte.
                const uint8_t continuation_byte = entry_name[i];
                if ((continuation_byte & 0xc0) != 0x80) {
                    return 0;
                }
            }
        }
    }

    return 1;
}

/*
 * Parses the Zip archive's Central Directory.  Allocates and populates the
 * zip_string_list.
 *
 * Returns 0 on success.
 */
int32_t parse_zip_archive(ZipArchive* archive)
{
    const size_t cd_length = archive->central_directory.length;
    const uint16_t num_entries = archive->num_entries;
    int list_index;
    uint16_t i;

    archive->zip_string_list = malloc(sizeof(ZipString) * num_entries);
    if (archive->zip_string_list == NULL) {
        printf("Zip: unable to allocate memory for list\n");
        return -1;
    }

    /*
    * Walk through the central directory, adding entries to the list
    * and verifying values.
    */
    loff_t start_offset = archive->central_directory.start;
    loff_t end_offset = start_offset + archive->central_directory.length;

    for (i = 0, list_index = 0; i < num_entries; i++, list_index++) {
        if (start_offset + sizeof(CentralDirectoryRecord) > end_offset) {
            printf("Zip: ran off the end at offset: %llu\n", start_offset);
            return -1;
        }

        CentralDirectoryRecord *cdr = malloc(sizeof(CentralDirectoryRecord));

        read_file_at_offset(start_offset, cdr, sizeof(CentralDirectoryRecord));

        if (cdr->record_signature != CDR_SIGNATURE) {
            printf("Zip: missed a central dir sig at %d\n", i);
            return -1;
        }

        const loff_t local_header_offset = cdr->local_file_header_offset;
        if (local_header_offset >= archive->directory_offset) {
            printf("Zip: bad LFH offset %llu at entry %d\n", local_header_offset, i);
            return -1;
        }

        const uint16_t file_name_length = cdr->file_name_length;
        const uint16_t extra_length = cdr->extra_field_length;
        const uint16_t comment_length = cdr->comment_length;
        char* file_name = calloc(0, (sizeof(char) * file_name_length));

        if (file_name == NULL) {
            printf("Zip: unable to allocate memory for file_name\n");
            return -1;
        }

        loff_t filename_offset = start_offset + sizeof(CentralDirectoryRecord);

        read_file_at_offset(filename_offset, file_name, file_name_length);

        if (filename_offset + file_name_length > end_offset) {
            printf("Zip: file name boundary exceeds the central range, "
                    "file_name_length: %d, cd_length: %zu",
                    file_name_length, archive->central_directory.length);
            return -1;
        }

        /* check that file name is valid UTF-8 and doesn't contain NUL (U+0000) characters */
        if (!is_valid_entry_name(file_name, file_name_length)) {
            printf("Zip: Invalid file name\n");
            return -1;
        }

        /* add the CDE filename to the list */
        ZipString entry_name;
        entry_name.name = file_name;
        entry_name.name_length = file_name_length;
        entry_name.cd_offset = start_offset;
        archive->zip_string_list[list_index] = entry_name;

        start_offset += sizeof(CentralDirectoryRecord) + file_name_length +
                        extra_length + comment_length;
        if ((start_offset - archive->central_directory.start) > cd_length) {
            printf("Zip: Bad CD advance %llu vs %zu at entry %d\n",
                                                    start_offset - archive->central_directory.start,
                                                    cd_length, i);
            return -1;
        }
        free(cdr);
    }

    print_zip_archive(archive);
    uint32_t lfh_start_bytes;

    read_file_at_offset(0, &lfh_start_bytes, sizeof(uint32_t));

    if (lfh_start_bytes != LOCAL_FILE_HDR_SIGNATURE) {
        printf("Zip: Entry at offset zero has invalid LFH signature: %d", lfh_start_bytes);
        return -1;
    }

    printf("+++ zip good scan %d entries\n", num_entries);

    return 0;
}

int32_t open_archive_internal(ZipArchive* archive)
{
    int32_t result = -1;
    if ((result = map_central_directory(archive)) < 0) {
        return result;
    }

    if ((result = parse_zip_archive(archive)) < 0) {
        return result;
    }

    return 0;
}

int32_t open_archive(ZipArchiveHandle* handle, char* filename, loff_t file_size)
{
    ZipArchive* archive = malloc(sizeof(ZipArchive));
    archive->filename = strdup(filename);
    archive->file_size = file_size;
    *handle = archive;

    return open_archive_internal(archive);
}

// Returns `mod_time` as a broken-down struct tm.
struct tm get_mod_time(ZipEntry* zip_entry)
{
  struct tm t;

  t.tm_hour = (zip_entry->mod_time >> 11) & 0x1f;
  t.tm_min = (zip_entry->mod_time >> 5) & 0x3f;
  t.tm_sec = (zip_entry->mod_time & 0x1f) << 1;

  t.tm_year = ((zip_entry->mod_time >> 25) & 0x7f) + 80;
  t.tm_mon = ((zip_entry->mod_time >> 21) & 0xf) - 1;
  t.tm_mday = (zip_entry->mod_time >> 16) & 0x1f;

  return t;
}

/*
 * Convert a ZipEntry to a list index, verifying that it's in a
 * valid range.
 */
int64_t entry_to_index(const ZipString* zip_string_list,
                       const uint32_t list_size,
                       const ZipString name)
{
    int i;

    for (i = 0; i < list_size; i++) {
        if (!strcmp(zip_string_list[i].name, name.name)) {
            return i;
        }
    }
    printf("Zip: Unable to find entry %s %d\n", name.name, name.name_length);
    return -1;
}

int32_t find_entry_internal(const ZipArchive* archive, const int ent, ZipEntry* data)
{
    const uint16_t name_length = archive->zip_string_list[ent].name_length;

    // Recover the start of the central directory entry
    int entry_cd_offset = archive->zip_string_list[ent].cd_offset;

    // This is the base of our offset region, we have to sanity check that
    // the name that's in the list is an offset to a location within
    // this region.
    loff_t base_offset = archive->central_directory.start;
    if (entry_cd_offset < base_offset ||
        entry_cd_offset > base_offset + archive->central_directory.length) {
        printf("Zip: Invalid cd_offset: %llu\n", entry_cd_offset);
        return -1;
    }

    CentralDirectoryRecord cdr;
    read_file_at_offset(entry_cd_offset, &cdr, sizeof(CentralDirectoryRecord));

    // The offset of the start of the central directory in the zipfile.
    // We keep this lying around so that we can sanity check all our lengths
    // and our per-file structures.
    const loff_t cd_offset = archive->directory_offset;

    // Fill out the compression method, modification time, crc32
    // and other interesting attributes from the central directory. These
    // will later be compared against values from the local file header.
    data->method = cdr.compression_method;
    data->mod_time = cdr.last_mod_date << 16 | cdr.last_mod_time;
    data->crc32 = cdr.crc32;
    data->compressed_length = cdr.compressed_size;
    data->uncompressed_length = cdr.uncompressed_size;

    // Figure out the local header offset from the central directory. The
    // actual file data will begin after the local header and the name /
    // extra comments.
    const loff_t local_header_offset = cdr.local_file_header_offset;
    if (local_header_offset + (loff_t)(sizeof(LocalFileHeader)) >= cd_offset) {
        printf("Zip: bad local hdr offset in zip");
        return -1;
    }

    LocalFileHeader lfh;
    read_file_at_offset(local_header_offset, &lfh, sizeof(LocalFileHeader));

    if (lfh.lfh_signature != LOCAL_FILE_HDR_SIGNATURE) {
        printf("Zip: didn't find signature at start of lfh, offset=%llu\n", local_header_offset);
        return -1;
    }

    // Paranoia: Match the values specified in the local file header
    // to those specified in the central directory.

    // Warn if central directory and local file header don't agree on the use
    // of a trailing Data Descriptor. The reference implementation is inconsistent
    // and appears to use the LFH value during extraction (unzip) but the CD value
    // while displayng information about archives (zipinfo). The spec remains
    // silent on this inconsistency as well.
    //
    // For now, always use the version from the LFH but make sure that the values
    // specified in the central directory match those in the data descriptor.
    //
    // NOTE: It's also worth noting that unzip *does* warn about inconsistencies in
    // bit 11 (EFS: The language encoding flag, marking that filename and comment are
    // encoded using UTF-8). This implementation does not check for the presence of
    // that flag and always enforces that entry names are valid UTF-8.
    if ((lfh.gpb_flags & GPBDD_FLAG_MASK) != (cdr.gpb_flags & GPBDD_FLAG_MASK)) {
        printf("Zip: gpb flag mismatch at bit 3. expected %d, was %d instead\n", cdr.gpb_flags,
                                                                                 lfh.gpb_flags);
    }

    // If there is no trailing data descriptor, verify that the central directory and local file
    // header agree on the crc, compressed, and uncompressed sizes of the entry.
    if ((lfh.gpb_flags & GPBDD_FLAG_MASK) == 0) {
        data->has_data_descriptor = 0;
        if (data->compressed_length != lfh.compressed_size ||
            data->uncompressed_length != lfh.uncompressed_size || data->crc32 != lfh.crc32) {
        printf("Zip: Inconsistent information, size/crc32 mismatch."
                "expected {%d %d %d}, was {%d %d %d}\n",
                                    data->compressed_length, data->uncompressed_length, data->crc32,
                                    lfh.compressed_size, lfh.uncompressed_size, lfh.crc32);
        return -1;
        }
    } else {
        data->has_data_descriptor = 1;
    }

    // 4.4.2.1: the upper byte of `version_made_by` gives the source OS. Unix is 3.
    if ((cdr.version_made_by >> 8) == 3) {
        data->unix_mode = (cdr.external_file_attributes >> 16) & 0xffff;
    } else {
        data->unix_mode = 0777;
    }

    // Check that the local file header name matches the declared
    // name in the central directory.
    if (lfh.file_name_length == name_length) {
        const loff_t name_offset = local_header_offset + sizeof(LocalFileHeader);
        if (name_offset + lfh.file_name_length > cd_offset) {
            printf("Zip: Invalid declared length");
            return -1;
        }

        char* name_buffer = calloc(0, sizeof(char) * name_length);
        if (name_buffer == NULL) {
            printf("Zip: failed to allocate memory for name_buffer\n");
            return -1;
        }

        read_file_at_offset(name_offset, name_buffer, name_length);

        if (memcmp(archive->zip_string_list[ent].name, name_buffer, name_length)) {
            printf("Zip: memcmp failed: %s and %s\n", archive->zip_string_list[ent].name,
                                                      name_buffer);
            return -1;
        }
        free(name_buffer);
    } else {
        printf("Zip: lfh name did not match central directory.\n");
        return -1;
    }

    const loff_t data_offset = local_header_offset + sizeof(LocalFileHeader) +
                                lfh.file_name_length + lfh.extra_field_length;
    if (data_offset > cd_offset) {
        printf("Zip: bad data offset %llu in zip", data_offset);
        return -1;
    }

    if ((loff_t)(data_offset + data->compressed_length) > cd_offset) {
        printf("Zip: bad compressed length in zip %llu + %d > %llu\n",
            data_offset, data->compressed_length,
            cd_offset);
        return -1;
    }

    if (data->method == kCompressStored &&
       (loff_t)(data_offset + data->uncompressed_length) > cd_offset) {
        printf("Zip: bad uncompressed length in zip (%llu + %d > %llu\n",
            data_offset, data->uncompressed_length,
            cd_offset);
        return -1;
    }

    data->offset = data_offset;
    print_zip_entry(data);

    return 0;
}

int32_t find_entry(const ZipArchiveHandle handle, const ZipString entry_name, ZipEntry* data)
{
    const ZipArchive* archive = (ZipArchive *)handle;
    if (entry_name.name_length == 0) {
        printf("Zip: Invalid filename %s %d\n", entry_name.name, entry_name.name_length);
        return -1;
    }

    const int64_t ent = entry_to_index(archive->zip_string_list, archive->num_entries, entry_name);
    if (ent < 0) {
        printf("Zip: Could not find entry %s %d\n", entry_name.name, entry_name.name_length);
        return ent;
    }
    return find_entry_internal(archive, ent, data);
}

int initialize_zstream(z_stream* zstream)
{
    int zerr;

    /*
    * Initialize the zlib stream struct.
    */
    memset(zstream, 0, sizeof(zstream));
    zstream->zalloc = Z_NULL;
    zstream->zfree = Z_NULL;
    zstream->opaque = Z_NULL;
    zstream->next_in = NULL;
    zstream->avail_in = 0;
    zstream->data_type = Z_UNKNOWN;

    /*
    * Use the undocumented "negative window bits" feature to tell zlib
    * that there's no zlib header waiting for it.
    */
    zerr = inflateInit2(zstream, -MAX_WBITS);
    if (zerr != Z_OK) {
        if (zerr == Z_VERSION_ERROR) {
            printf("Installed zlib is not compatible with linked version (%s)", ZLIB_VERSION);
        } else {
            printf("Call to inflateInit2 failed (zerr=%d)", zerr);
        }
        return -1;
    }
    return 0;
}

// Write uncompressed files directly to the partition
int32_t write_entry_to_partition(ZipArchive* archive,
                                 ZipEntry* entry,
                                 ZipExtractor* zip_extractor)
{
    const uint32_t length = entry->uncompressed_length;

    if (length > MAX_UNCOMPRESSED_SIZE) {
        printf("Uncompressed file too large\n");
        return -1;
    }

    read_file_at_offset(entry->offset, FILE_READ_ADDR, length);

    flush_cache(FILE_READ_ADDR, length);
    mdelay(10);

    tvconfig_mmc_flash_write(zip_extractor->partition, (void *) FILE_READ_ADDR, length);
    memset(FILE_READ_ADDR, 0, length);

    return 0;
}

void* extract_from_zip(ZipExtractor* zip_extractor, z_stream *zstream, loff_t target_amount)
{
    const size_t kBufSize = READ_BUFFER_MAX_SIZE;
    uint8_t *write_buf = malloc(sizeof(uint8_t) * target_amount);
    uint32_t remaining_bytes = zip_extractor->bytes_to_process;
    int zerr;

    if (write_buf == NULL || target_amount > 50) {
        if (write_buf != NULL)
            free(write_buf);
        write_buf = FILE_READ_ADDR;
    }

    zstream->avail_out = target_amount;
    zstream->next_out = &write_buf[0];

    do {
        /* read as much as we can */
        if (zstream->avail_in == 0) {
            const size_t read_size = (remaining_bytes > kBufSize) ? kBufSize : remaining_bytes;
            loff_t offset = zip_extractor->amount_processed;
            read_file_at_offset(zip_extractor->zip_offset + offset,
                                zip_extractor->read_buffer,
                                read_size);
            remaining_bytes -= read_size;
            zstream->next_in = &zip_extractor->read_buffer[0];
            zstream->avail_in = read_size;
            zip_extractor->amount_processed += read_size;
            zip_extractor->bytes_to_process -= read_size;
        }

        /* uncompress the data */
        zerr = inflate(zstream, Z_NO_FLUSH);
        if (zerr != Z_OK && zerr != Z_STREAM_END) {
            printf("Zip:inflate zerr=%d %s"
                   "(nextIn=%p availableIn=%u nextOut=%p availableOut=%u)\n", zerr, zstream->msg,
                                                                zstream->next_in, zstream->avail_in,
                                                                zstream->next_out, zstream->avail_out);
            return NULL;
        }

        /* if we are full, return */
        if (zstream->avail_out == 0) {
            return (void *) &write_buf[0];
        }

        if ((zerr == Z_STREAM_END && zstream->avail_out != kBufSize)) {
            return (void *) &write_buf[0];
        }
    } while (zerr == Z_OK);
}

static int partition_write_sparse_image(block_dev_desc_t *dev_desc, ZipExtractor *zip_extractor,
                                        sparse_header_t *sparse_header, z_stream* zstream)
{
    lbaint_t blk;
    lbaint_t blkcnt;
    lbaint_t blks;
    disk_partition_t *info = &zip_extractor->info;
    lbaint_t blk_other;
    disk_partition_t *info_other = NULL;
    uint64_t bytes_written = 0;
    unsigned int chunk;
    uint64_t chunk_data_sz;
    uint32_t *fill_buf = NULL;
    uint32_t *fill_val = NULL;
    chunk_header_t *chunk_header;
    uint32_t total_blocks = 0;
    int i;
    void *data;

    if (zip_extractor->partition_other != NULL)
        info_other = &zip_extractor->info_other;

    if ((info_other != NULL) && (info->blksz != info_other->blksz)) {
        printf("%s and %s block_sizes are not the same\n", zip_extractor->partition,
                                                            zip_extractor->partition_other);
        return -1;
    }

    if (sparse_header->file_hdr_sz > sizeof(sparse_header_t)) {
        /*
            * Skip the remaining bytes in a header that is longer than
            * we expected.
            */
        if (extract_from_zip(zip_extractor, zstream,
                            (sparse_header->file_hdr_sz - sizeof(sparse_header_t))) == NULL) {
            printf("Skipping extra sparse header bytes failed\n");
            return -1;
        }
    }

    printf("=== Sparse Image Header ===\n");
    printf("major_version: 0x%x\n", sparse_header->major_version);
    printf("minor_version: 0x%x\n", sparse_header->minor_version);
    printf("file_hdr_sz: %d\n", sparse_header->file_hdr_sz);
    printf("chunk_hdr_sz: %d\n", sparse_header->chunk_hdr_sz);
    printf("blk_sz: %d\n", sparse_header->blk_sz);
    printf("total_blks: %d\n", sparse_header->total_blks);
    printf("total_chunks: %d\n", sparse_header->total_chunks);

    /* verify sparse_header->blk_sz is an exact multiple of info->blksz */
    if (sparse_header->blk_sz != (sparse_header->blk_sz & ~(info->blksz - 1)) ||
        ((info_other != NULL) &&
        (sparse_header->blk_sz != (sparse_header->blk_sz & ~(info_other->blksz - 1))))) {
        printf("%s: Sparse image block size issue [%u]\n", __func__, sparse_header->blk_sz);
        return -1;
    }

    printf("Flashing Sparse Image\n");

    /* Start processing chunks */
    blk = info->start;
    if (info_other != NULL)
        blk_other = info_other->start;
    for (chunk = 0; chunk < sparse_header->total_chunks; chunk++) {
        /* Read and skip over chunk header */
        chunk_header = (chunk_header_t *) extract_from_zip(zip_extractor,
                                                            zstream,
                                                            sizeof(chunk_header_t));

        if (chunk_header == NULL) {
            printf("Extracting chunk header from zip failed\n");
            return -1;
        }
        if (chunk_header->chunk_type != CHUNK_TYPE_RAW) {
            printf("=== Chunk Header ===\n");
            printf("chunk_type: 0x%x\n", chunk_header->chunk_type);
            printf("chunk_data_sz: 0x%x\n", chunk_header->chunk_sz);
            printf("total_size: 0x%x\n", chunk_header->total_sz);
        }

        if (sparse_header->chunk_hdr_sz > sizeof(chunk_header_t)) {
            /*
                * Skip the remaining bytes in a header that is longer
                * than we expected.
                */
            if (extract_from_zip(zip_extractor,
                                    zstream,
                                    (sparse_header->file_hdr_sz - sizeof(sparse_header_t))) == NULL) {
                printf("Skipping extra chunk header bytes failed\n");
                return -1;
            }
        }

        chunk_data_sz = (uint64_t)sparse_header->blk_sz * (uint64_t)chunk_header->chunk_sz;
        // Verified info->blksz is the same as info_other->blksz earlier
        blkcnt = chunk_data_sz / info->blksz;

        switch (chunk_header->chunk_type) {
            case CHUNK_TYPE_RAW:
                if (chunk_header->total_sz != (sparse_header->chunk_hdr_sz + chunk_data_sz)) {
                    printf("Bogus chunk size for chunk type Raw\n");
                    return -1;
                }

                if (blk + blkcnt > info->start + info->size ||
                    ((info_other != NULL) &&
                    (blk_other + blkcnt > info_other->start + info_other->size))) {
                    printf("%s: Request would exceed partition size!\n", __func__);
                    return -1;
                }

                if (chunk_data_sz > MAX_CHUNK_SIZE) {
                    printf("Error: Chunk size too large\n");
                    return -1;
                }

                data = extract_from_zip(zip_extractor, zstream, chunk_data_sz);
                if (data == NULL) {
                    printf("Extracting chunk_data_sz: %llu failed\n", chunk_data_sz);
                    return -1;
                }
                flush_cache(FILE_READ_ADDR, chunk_data_sz);
                mdelay(10);

                blks = dev_desc->block_write(dev_desc->dev, blk, blkcnt, data);
                if (blks != blkcnt) {
                    printf("%s: Write failed " LBAFU "\n", __func__, blks);
                    return -1;
                }
                if (info_other != NULL) {
                    blks = dev_desc->block_write(dev_desc->dev, blk_other, blkcnt, data);
                    if (blks != blkcnt) {
                        printf("%s: Write failed " LBAFU "\n", __func__, blks);
                        return -1;
                    }
                    blk_other += blkcnt;
                }
                blk += blkcnt;
                bytes_written += blkcnt * info->blksz;
                total_blocks += chunk_header->chunk_sz;
                memset(FILE_READ_ADDR, 0, chunk_data_sz);
                break;

            case CHUNK_TYPE_FILL:
                if (chunk_header->total_sz != (sparse_header->chunk_hdr_sz + sizeof(uint32_t))) {
                    printf("Bogus chunk size for chunk type FILL\n");
                    return -1;
                }

                fill_buf =
                    (uint32_t *)memalign(ARCH_DMA_MINALIGN, ROUNDUP(info->blksz, ARCH_DMA_MINALIGN));
                if (fill_buf == NULL) {
                    printf("Malloc failed for: CHUNK_TYPE_FILL\n");
                    return -1;
                }

                fill_val = (uint32_t *)extract_from_zip(zip_extractor, zstream, sizeof(uint32_t));

                if (fill_val == NULL) {
                    printf("Failed to extract data from file\n");
                    return -1;
                }

                for (i = 0; i < (info->blksz / sizeof(uint32_t)); i++)
                    fill_buf[i] = *fill_val;

                if (blk + blkcnt > info->start + info->size ||
                    ((info_other != NULL) &&
                    (blk_other + blkcnt > info_other->start + info_other->size))) {
                    printf("%s: Request would exceed partition size!\n", __func__);
                    return -1;
                }

                for (i = 0; i < blkcnt; i++) {
                    blks = dev_desc->block_write(dev_desc->dev, blk, 1, fill_buf);
                    if (blks != 1) {
                        printf( "%s: Write failed, block # " LBAFU "\n", __func__, blkcnt);
                        free(fill_buf);
                        return -1;
                    }
                    if (info_other != NULL) {
                        blks = dev_desc->block_write(dev_desc->dev, blk_other, 1, fill_buf);
                        if (blks != 1) {
                            printf( "%s: Write failed, block # " LBAFU "\n", __func__, blkcnt);
                            free(fill_buf);
                            return -1;
                        }
                        blk_other++;
                    }
                    blk++;
                }
                bytes_written += blkcnt * info->blksz;
                total_blocks += chunk_data_sz / sparse_header->blk_sz;

                free(fill_buf);
                break;
            case CHUNK_TYPE_DONT_CARE:
                blk += blkcnt;
                if (info_other != NULL)
                    blk_other += blkcnt;
                total_blocks += chunk_header->chunk_sz;
            break;

            case CHUNK_TYPE_CRC32:
                if (chunk_header->total_sz != sparse_header->chunk_hdr_sz) {
                    printf("Bogus chunk size for chunk type Dont Care");
                    return -1;
                }
                total_blocks += chunk_header->chunk_sz;
                data = extract_from_zip(zip_extractor, zstream, chunk_data_sz);
                if (data == NULL) {
                    printf("Failed to extract data from file\n");
                    return -1;
                }
            break;

            default:
                printf("%s: Unknown chunk type: %x\n", __func__, chunk_header->chunk_type);
                return -1;
        }
        free(chunk_header);
    }

    printf("Wrote %d blocks, expected to write %d blocks\n",
            total_blocks, sparse_header->total_blks);
    printf("........ wrote %u bytes to '%s'\n", (int)bytes_written, zip_extractor->partition);
    if (zip_extractor->partition_other != NULL)
        printf("........ wrote %u bytes to '%s'\n", (int)bytes_written,
                                                    zip_extractor->partition_other);

    if (total_blocks != sparse_header->total_blks) {
        printf("sparse image write failure");
        return -1;
    }

    if (zstream->total_out != zip_extractor->uncompressed_length) {
        printf("Zip: size mismatch on inflated file (%lu vs %llu)", zstream->total_out,
                                                                zip_extractor->uncompressed_length);
        return -1;
    }

 	return 0;
}

static int partition_write_raw_image(block_dev_desc_t *dev_desc, ZipExtractor *zip_extractor)
{
	lbaint_t blkcnt;
	lbaint_t blks;
    disk_partition_t *info = &zip_extractor->info;
    disk_partition_t *info_other = NULL;

    if (zip_extractor->partition_other != NULL)
        info_other = &zip_extractor->info_other;

    // Want to restart the stream from beginning of the file since we read in sizeof(sparse_header)
    // to check img format
    z_stream zstream;
    initialize_zstream(&zstream);
    zip_extractor->amount_processed = 0;
    zip_extractor->bytes_to_process = zip_extractor->compressed_length;
    zip_extractor->read_buffer = &zip_extractor->read_buffer[0];

    void *data = extract_from_zip(zip_extractor, &zstream, zip_extractor->uncompressed_length);
    flush_cache(FILE_READ_ADDR, zip_extractor->uncompressed_length);
    mdelay(10);

    if (data == NULL) {
        printf("Extracting raw img from zip failed\n");
        return -1;
    }

    // update mbr: return 0 if successful, -1 if failed
    if (!strcmp_l2(zip_extractor->partition, "mbr")) {
        char cmd [128] = {0};
        sprintf(cmd, "store mbr %p", FILE_READ_ADDR);
        if (run_command(cmd, 1) != 0) {
            printf("store mbr failed\n");
            return -1;
        }
        memset(FILE_READ_ADDR, 0, zip_extractor->uncompressed_length);
        inflateEnd(&zstream);
        return 0;
    }

    if ((info_other != NULL) && (info->blksz != info_other->blksz)) {
        printf("%s and %s are not the same\n", zip_extractor->partition,
                                               zip_extractor->partition_other);
        return -1;
    }

	/* determine number of blocks to write */
	blkcnt = ((zip_extractor->uncompressed_length + (info->blksz - 1)) & ~(info->blksz - 1));
	blkcnt = blkcnt / info->blksz;

	if (blkcnt > info->size) {
        if (zip_extractor->partition_other != NULL)
		    printf("too large for partition: '%s' and '%s'\n", zip_extractor->partition,
                                                               zip_extractor->partition_other);
        else
		    printf("too large for partition: '%s'\n", zip_extractor->partition);
		return -1;
	}

	printf("Flashing Raw Image\n");

    blks = dev_desc->block_write(dev_desc->dev, info->start, blkcnt, data);

	if (blks != blkcnt) {
		printf("failed writing to device %d\n", dev_desc->dev);
		return -1;
	}

    if (info_other != NULL) {
        blks = dev_desc->block_write(dev_desc->dev, info_other->start, blkcnt, data);
        if (blks != blkcnt) {
            printf("failed writing to device %d\n", dev_desc->dev);
            return -1;
        }
    }
    memset(FILE_READ_ADDR, 0, zip_extractor->uncompressed_length);

	printf("........ wrote " LBAFU " bytes to '%s'\n", blkcnt * info->blksz,
                                                       zip_extractor->partition);
    if (zip_extractor->partition_other)
    	printf("........ wrote " LBAFU " bytes to '%s'\n", blkcnt * info->blksz,
                                                           zip_extractor->partition_other);

    inflateEnd(&zstream);

    return 0;
}

// zip_extractor->partition_other is guaranteed to not be NULL here
int flash_bootloader(block_dev_desc_t *dev_desc, ZipExtractor* zip_extractor, z_stream* zstream)
{
	int map = 0, map_other = 0, ret = 0;
	char *ops[] = {"erase", "write"};

    map = AML_BL_USER;
    map_other = AML_BL_BOOT0;

    void *data = extract_from_zip(zip_extractor, zstream, zip_extractor->uncompressed_length);
    flush_cache(FILE_READ_ADDR, zip_extractor->uncompressed_length);
    mdelay(10);

    if (data == NULL) {
        printf("Extracting bootloader img from zip failed\n");
        return -1;
    }

    ret = amlmmc_write_bootloader(CONFIG_FASTBOOT_FLASH_MMC_DEV,
                                  map,
                                  zip_extractor->uncompressed_length,
                                  data);

    if (ret) {
        error("failed %s %s from device %d", ops[1], zip_extractor->partition, dev_desc->dev);
        return -1;
    }

    ret = amlmmc_write_bootloader(CONFIG_FASTBOOT_FLASH_MMC_DEV,
                                  map_other,
                                  zip_extractor->uncompressed_length,
                                  data);

    if (ret) {
        error("failed %s %s from device %d", ops[1], zip_extractor->partition_other, dev_desc->dev);
        return -1;
    }

    memset(FILE_READ_ADDR, 0, zip_extractor->uncompressed_length);

    printf("........ %s  %s\n", ops[1], zip_extractor->partition);
    printf("........ %s  %s\n", ops[1], zip_extractor->partition_other);

    return 0;
}

int extract_to_partition(ZipExtractor* zip_extractor, z_stream* zstream)
{
	block_dev_desc_t *dev_desc;

	dev_desc = get_dev("mmc", CONFIG_FASTBOOT_FLASH_MMC_DEV);
	if (!dev_desc || dev_desc->type == DEV_TYPE_UNKNOWN) {
		error("invalid mmc device\n");
		return -1;
	}

    // Flash bootloader and return 0 if successful
    if (!strcmp_l2(zip_extractor->partition, "bootloader")) {
        if (flash_bootloader(dev_desc, zip_extractor, zstream) < 0) {
            printf("Fail to flash bootloader\n");
            return -1;
        }
        return 0;
    }
#ifdef CONFIG_AML_PARTITION
    if (strcmp_l2(zip_extractor->partition, "mbr")) {
        char cmd [128] = {0};
        sprintf(cmd, "store erase partition %s", zip_extractor->partition);
        if (run_command(cmd, 1) != 0) {
            printf("store erase partition failed: %s\n", zip_extractor->partition);
            return -1;
        }
        if (get_partition_info_aml_by_name(dev_desc, zip_extractor->partition, &zip_extractor->info)) {
            error("cannot find partition: '%s'\n", zip_extractor->partition);
            return -1;
        }

        // If the entry has a corresponding _b partition
        if (zip_extractor->partition_other != NULL) {
            sprintf(cmd, "store erase partition %s", zip_extractor->partition_other);
            if (run_command(cmd, 1) != 0) {
                printf("store erase partition failed: %s\n", zip_extractor->partition_other);
                return -1;
            }
            if (get_partition_info_aml_by_name(dev_desc, zip_extractor->partition_other, &zip_extractor->info_other)) {
                error("cannot find partition: '%s'\n", zip_extractor->partition_other);
                return -1;
            }
        }
    }
#endif

    // Extract sizeof(sparse_header_t) to see if image is sparse or raw
    sparse_header_t *sparse_header = (sparse_header_t *) extract_from_zip(zip_extractor,
                                                                          zstream,
                                                                          sizeof(sparse_header_t));

    if (sparse_header == NULL) {
        printf("Failed to read sparse header\n");
        return -1;
    } else if (is_sparse_image(sparse_header)) {
        if (partition_write_sparse_image(dev_desc, zip_extractor, sparse_header, zstream) < 0) {
            printf("Failed to write partition\n");
            return -1;
        }
    } else {
        if (partition_write_raw_image(dev_desc, zip_extractor) < 0) {
            printf("Failed to write partition\n");
            return -1;
        }
    }
    printf("Wrote to partition successfully\n");
    return 0;
}

int32_t inflate_entry_to_partition(ZipArchive* archive,
                                   ZipEntry* entry,
                                   ZipExtractor* zip_extractor)
{
    z_stream zstream;
    uint8_t read_buf [READ_BUFFER_MAX_SIZE];

    if (initialize_zstream(&zstream) < 0) {
        printf("Failed to initialize zstream\n");
        return -1;
    }

    zip_extractor->zip_offset = entry->offset;
    zip_extractor->amount_processed = 0;
    zip_extractor->bytes_to_process = entry->compressed_length;
    zip_extractor->compressed_length = entry->compressed_length;
    zip_extractor->uncompressed_length = entry->uncompressed_length;
    zip_extractor->read_buffer = read_buf;

    if (extract_to_partition(zip_extractor, &zstream) < 0) {
        printf("Extracting and writing to partition failed\n");
        return -1;
    }

    inflateEnd(&zstream);
    return 0;
}

int32_t extract_image_to_partition(ZipArchiveHandle handle,
                                   ZipEntry* entry,
                                   ZipExtractor *zip_extractor)
{
    ZipArchive* archive = (ZipArchive *)(handle);
    const uint16_t method = entry->method;

    // Sanity check that the caller's set the partition correctly
    if (zip_extractor->partition == NULL) {
        printf("Forgot to set extractor partition?\n");
        return -1;
    }

    // this should default to kUnknownCompressionMethod.
    int32_t return_value = -1;
    if (method == kCompressStored) {
        return_value = write_entry_to_partition(archive, entry, zip_extractor);
    } else if (method == kCompressDeflated) {
        return_value = inflate_entry_to_partition(archive, entry, zip_extractor);
    }

    return return_value;
}

int32_t write_version_to_memory(ZipArchive* archive, ZipEntry* entry, void* buffer)
{
    const uint32_t length = entry->uncompressed_length;

    if (length > MAX_UNCOMPRESSED_SIZE) {
        printf("Uncompressed file too large\n");
        return -1;
    }

    read_file_at_offset(entry->offset, FILE_READ_ADDR, length);

    flush_cache(FILE_READ_ADDR, length);
    mdelay(10);

    memset(buffer, 0, length);
    memcpy(buffer, FILE_READ_ADDR, length);
    memset(FILE_READ_ADDR, 0, length);
    return 0;
}

int32_t inflate_version_to_memory(ZipArchive* archive, ZipEntry* entry, void* buffer)
{
    const size_t kBufSize = 32768;
    uint8_t read_buf[32768];
    uint8_t write_buf[32768];
    z_stream zstream;
    int zerr;
    int bytes_written = 0;

    /*
     * Initialize the zlib stream struct.
     */
    memset(&zstream, 0, sizeof(zstream));
    zstream.zalloc = Z_NULL;
    zstream.zfree = Z_NULL;
    zstream.opaque = Z_NULL;
    zstream.next_in = NULL;
    zstream.avail_in = 0;
    zstream.next_out = &write_buf[0];
    zstream.avail_out = kBufSize;
    zstream.data_type = Z_UNKNOWN;

    /*
     * Use the undocumented "negative window bits" feature to tell zlib
     * that there's no zlib header waiting for it.
     */
    zerr = inflateInit2(&zstream, -MAX_WBITS);
    if (zerr != Z_OK) {
      if (zerr == Z_VERSION_ERROR) {
        printf("Installed zlib is not compatible with linked version (%s)", ZLIB_VERSION);
      } else {
        printf("Call to inflateInit2 failed (zerr=%d)", zerr);
      }
      return -1;
    }

    uint32_t remaining_bytes = entry->compressed_length;
    do {
        /* read as much as we can */
        if (zstream.avail_in == 0) {
        const size_t read_size = (remaining_bytes > kBufSize) ? kBufSize : remaining_bytes;
        const uint32_t offset = (entry->compressed_length - remaining_bytes);
        read_file_at_offset(entry->offset + offset, read_buf, read_size);
        remaining_bytes -= read_size;
        zstream.next_in = &read_buf[0];
        zstream.avail_in = read_size;
        }
        /* uncompress the data */
        zerr = inflate(&zstream, Z_NO_FLUSH);
        if (zerr != Z_OK && zerr != Z_STREAM_END) {
            printf("Zip: inflate zerr=%d (nIn=%p aIn=%u nOut=%p aOut=%u)", zerr, zstream.next_in,
                    zstream.avail_in, zstream.next_out, zstream.avail_out);
            return -1;
        }
        /* write when we're full or when we're done */
        if (zstream.avail_out == 0 || (zerr == Z_STREAM_END && zstream.avail_out != kBufSize)) {
            const size_t write_size = zstream.next_out - &write_buf[0];

            if (bytes_written + write_size > entry->uncompressed_length) {
                printf("Buffer overflow encountered while trying to write\n");
                return -1;
            }

            memcpy(buffer + bytes_written, &write_buf[0], write_size);
            bytes_written += write_size;

            zstream.next_out = &write_buf[0];
            zstream.avail_out = kBufSize;
        }
    } while (zerr == Z_OK);

    if (zstream.total_out != entry->uncompressed_length || remaining_bytes != 0) {
        printf("Zip: size mismatch on inflated file (%lu vs %d)", zstream.total_out,
                entry->uncompressed_length);
        return -1;
    }
    inflateEnd(&zstream);
    return 0;
}


int32_t extract_version(ZipArchiveHandle handle,
                        ZipEntry* entry,
                        char* buffer)
{
    ZipArchive* archive = (ZipArchive *)(handle);
    const uint16_t method = entry->method;

    // this should default to kUnknownCompressionMethod.
    int32_t return_value = -1;
    if (method == kCompressStored) {
        return_value = write_version_to_memory(archive, entry, buffer);
    } else if (method == kCompressDeflated) {
        return_value = inflate_version_to_memory(archive, entry, buffer);
    }

    return return_value;
}

