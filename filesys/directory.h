#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/block.h"

/* Maximum length of a file name component.
   This is the traditional UNIX maximum length.
   After directories are implemented, this maximum length may be
   retained, but much longer full path names must be allowed. */
#define NAME_MAX 16

enum dir_entry_type {
	T_FILE,
	T_DIRECTORY,
	T_INVALID
};

struct inode;

/* Initializes the directory module. */
void dir_init(void);

/* Opening and closing directories. */
bool dir_create (block_sector_t sector, size_t entry_cnt, block_sector_t parent);
struct dir *dir_open (struct inode *);
struct dir *dir_open_root (void);
struct dir *dir_reopen (struct dir *);
void dir_close (struct dir *);
struct inode *dir_get_inode(struct dir *);

/* Reading and writing. */
bool dir_lookup (const struct dir *, const char *name, struct inode **);
bool dir_add (struct dir *, const char *name, block_sector_t, bool is_dir);
bool dir_remove (struct dir *, const char *name);
bool dir_readdir (struct dir *, char name[NAME_MAX + 1]);

#ifdef FILESYS_SUBDIRS
struct inode *dir_open_from_path(const char *path, bool *is_dir);
struct dir *dir_parent(const struct dir *);
struct dir *dir_parent_from_inode(const struct inode *);
#endif

#endif /* filesys/directory.h */
