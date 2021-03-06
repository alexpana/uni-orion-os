#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

struct bitmap;
struct inode;

void inode_init (void);
#ifdef FILESYS_SUBDIRS
bool inode_create (block_sector_t, off_t, block_sector_t);
#else
bool inode_create (block_sector_t, off_t);
#endif
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

struct inode *inode_parent(const struct inode *inode);

#ifdef FILESYS_SYNC
  void inode_global_lock_init();
  void inode_global_lock();
  void inode_global_unlock();
  void inode_lock(struct inode* inode);
  void inode_unlock(struct inode* inode);
#endif

int inode_open_cnt(struct inode *inode);

#endif /* filesys/inode.h */
