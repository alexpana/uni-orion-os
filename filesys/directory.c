#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>

#include "userprog/process.h"

#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/path.h"

/**
 * In-memory representation of a directory. 
 */
struct dir 
{
    struct inode *inode;    /* Backing store. */
    off_t pos;              /* Current position. */
};

/**
 *  Disk representation of a directory entry.
 */
struct dir_entry 
{
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool is_directory;                  /* Type of the entry. */
    bool in_use;                        /* In use or free? */
};

static struct dir *root_dir;

void dir_init() {
    root_dir = dir_open(inode_open(ROOT_DIR_SECTOR));
}

/**
 * Creates a directory with space for ENTRY_CNT entries in the given SECTOR.
 * Returns true if successful, false on failure. 
 */
bool dir_create (block_sector_t sector, size_t entry_count)
{
#ifdef FILESYS_SUBDIRS
    return inode_create(sector, entry_count * sizeof(struct dir_entry), sector);
#else
    return inode_create(sector, entry_count * sizeof(struct dir_entry));
#endif
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *dir_open (struct inode *inode) 
{
    struct dir *dir = calloc (1, sizeof *dir);
    if (inode != NULL && dir != NULL)
    {
        dir->inode = inode;
        dir->pos = 0;
        return dir;
    }
    else
    {
        inode_close (inode);
        free (dir);
        return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *dir_open_root (void)
{
    struct inode *root_inode = inode_open(ROOT_DIR_SECTOR);
    return dir_open(root_inode);
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *dir_reopen (struct dir *dir) 
{
    return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void dir_close (struct dir *dir) 
{
    if (dir != NULL)
    {
        inode_close (dir->inode);
        free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *dir_get_inode (struct dir *dir) 
{
    return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool lookup (const struct dir *dir, const char *name, struct dir_entry *ep, off_t *ofsp) 
{
    struct dir_entry e;
    size_t ofs;

    ASSERT (dir != NULL);
    ASSERT (name != NULL);

    size_t entry_size = sizeof e;

    for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == entry_size; ofs += entry_size) 
    {
        if (e.in_use && !strcmp (name, e.name)) 
        {
            if (ep != NULL) *ep = e;
            if (ofsp != NULL) *ofsp = ofs;
            return true;
        }
    }

    return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool dir_lookup (const struct dir *dir, const char *name, struct inode **inode) 
{
    struct dir_entry e;

    ASSERT (dir != NULL);
    ASSERT (name != NULL);

    if (lookup (dir, name, &e, NULL))
    {
        *inode = inode_open (e.inode_sector);
    } else {
        *inode = NULL;
    }

    return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}

#ifdef FILESYS_SUBDIRS
struct inode *dir_open_from_path(char *path, bool *is_dir) {
    struct dir *current_dir;
    char entry_name_buffer[NAME_MAX + 1];
    int offset = 0;
    struct dir_entry dir_entry_buffer;

    // initialize the current directory
    if (path_is_relative(path)) {
        current_dir = process_working_directory(process_current());
    } else {
        current_dir = root_dir;
    }

    // walk the path
    while (*(path + offset) != '\0') {
        path_next_entry(path + offset, entry_name_buffer, NAME_MAX, &offset);

        // handle parent directory
        if (strcmp(entry_name_buffer, "..") == 0) {
            struct dir *old_dir = current_dir;
            current_dir = dir_parent(current_dir);
            free(old_dir);
        } else {
            // handle subdirectory
            bool found = lookup(current_dir, entry_name_buffer, &dir_entry_buffer, NULL);

            if (found) {
                if (dir_entry_buffer.is_directory) {
                    struct dir *old_dir = current_dir;
                    current_dir = dir_open(inode_open(dir_entry_buffer.inode_sector));
                    free(old_dir);
                } else {
                    path_next_entry(path + offset, entry_name_buffer, NAME_MAX, &offset);
                    if (entry_name_buffer[0] == '\0') {
                        *is_dir = false;
                        return inode_open(dir_entry_buffer.inode_sector);
                    }
                }
            } else {
                return NULL;
            }
        }
    }

    *is_dir = true;
    return current_dir->inode;
}

struct dir *dir_parent(const struct dir *dir) {
    if (dir == root_dir) {
        return root_dir;
    } else {
        return dir_open(inode_parent(dir->inode));
    }
}

struct dir *dir_parent_from_inode(const struct inode *inode) {
  return dir_open(inode_parent(inode));
}
#endif
