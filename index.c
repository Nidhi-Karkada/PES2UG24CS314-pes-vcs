// index.c — Staging area implementation
//
// Text format of .pes/index (one entry per line, sorted by path):
//
//   <mode-octal> <64-char-hex-hash> <mtime-seconds> <size> <path>
//
// Example:
//   100644 a1b2c3d4e5f6...  1699900000 42 README.md
//   100644 f7e8d9c0b1a2...  1699900100 128 src/main.c
//
// This is intentionally a simple text format. No magic numbers, no
// binary parsing. The focus is on the staging area CONCEPT (tracking
// what will go into the next commit) and ATOMIC WRITES (temp+rename).
//
// PROVIDED functions: index_find, index_remove, index_status
// TODO functions:     index_load, index_save, index_add

#include "index.h"
#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// Helper for qsort to keep index sorted by path
static int compare_index_entries(const void *a, const void *b) {
    return strcmp(((const IndexEntry *)a)->path, ((const IndexEntry *)b)->path);
}

// ─── TODO Implementation ─────────────────────────────────────────────────────

int index_load(Index *index) {
    index->count = 0;
    FILE *f = fopen(INDEX_FILE, "r");
    if (!f) return 0; // Index doesn't exist yet, which is fine

    char hash_hex[HASH_HEX_SIZE + 1];
    while (index->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &index->entries[index->count];
        
        // Format: <mode> <hash> <mtime> <size> <path>
        int res = fscanf(f, "%o %64s %ld %zu ", 
                         &e->mode, hash_hex, &e->mtime_sec, &e->size);
        
        if (res == EOF) break;
        if (res != 4) {
            fclose(f);
            return -1;
        }

        // Read the path (rest of the line)
        if (fgets(e->path, sizeof(e->path), f) == NULL) break;
        e->path[strcspn(e->path, "\r\n")] = '\0'; // Strip newline

        hex_to_hash(hash_hex, &e->hash);
        index->count++;
    }

    fclose(f);
    return 0;
}

int index_save(const Index *index) {
    // 1. Sort entries by path
    Index sorted_idx = *index;
    qsort(sorted_idx.entries, sorted_idx.count, sizeof(IndexEntry), compare_index_entries);

    // 2. Open temporary file
    char temp_path[] = ".pes/index_tmp_XXXXXX";
    int fd = mkstemp(temp_path);
    if (fd < 0) return -1;

    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); return -1; }
/*
    // 3. Write entries
    for (int i = 0; i < sorted_idx.count; i++) {
        const IndexEntry *e = &sorted_idx.entries[i];
        char hash_hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&e->hash, hash_hex);

        fprintf(f, "%o %s %ld %zu %s\n", 
                e->mode, hash_hex, e->mtime_sec, e->size, e->path);
    }

    // 4. Atomic sync and rename
    fflush(f);
    fsync(fileno(f));
    fclose(f);

    if (rename(temp_path, INDEX_FILE) != 0) {
        unlink(temp_path);
        return -1;
    }

    return 0;
}

int index_add(Index *index, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    if (!S_ISREG(st.st_mode)) return -1; // Only stage regular files

    // 1. Read file content
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    void *data = malloc(st.st_size);
    if (fread(data, 1, st.st_size, f) != (size_t)st.st_size) {
        free(data); fclose(f); return -1;
    }
    fclose(f);

    // 2. Write as blob to object store
    ObjectID blob_id;
    if (object_write(OBJ_BLOB, data, st.st_size, &blob_id) != 0) {
        free(data); return -1;
    }
    free(data);

    // 3. Update index entry
    IndexEntry *e = index_find(index, path);
    if (!e) {
        if (index->count >= MAX_INDEX_ENTRIES) return -1;
        e = &index->entries[index->count++];
        strncpy(e->path, path, sizeof(e->path));
    }

    e->mode = (st.st_mode & S_IXUSR) ? 0100755 : 0100644;
    e->hash = blob_id;
    e->mtime_sec = st.st_mtime;
    e->size = st.st_size;

    return index_save(index);
}
*/
