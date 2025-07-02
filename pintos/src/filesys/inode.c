#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

//! BLOCK_SECTOR_SIZE = 512 bytes
//! struct inode_disk 안에서 length(4) + magic(4) + indirect_block_sector(4)를 제외한 나머지 500 bytes를 direct_block에 사용
//! block_sector_t = 4 bytes이므로 125개 direct block 포인터 저장 가능
#define INODE_DIRECT_CNT 125
//! block_sector_t 하나는 4 bytes이고,
//! 한 블록은 512 bytes이므로,
//! 512 / 4 = 128개의 섹터 번호 저장 가능 -> INDIRECT_BLOCK_CNT = 128
#define INDIRECT_BLOCK_CNT 128

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    // block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */  //? 4 bytes
    unsigned magic;                     /* Magic number. */  //? 4 bytes
    // uint32_t unused[125];               /* Not used. */
    //! direct 파일 데이터 블록 디스크 섹터 번호들 (unused 쓸바에 direct_block 배열로 하면 더 효율적)
    block_sector_t direct_block[INODE_DIRECT_CNT];  //? 4 * 125 = 500 bytes
    //! indirect 블록 테이블이 저장된 디스크 섹터 번호 (간접 테이블로 블록들 연결)
    block_sector_t indirect_block_sector;  //? 4 bytes
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  //! 기존 방식
  // if (pos < inode->data.length)
  //   return inode->data.start + pos / BLOCK_SECTOR_SIZE;

  size_t idx = pos / BLOCK_SECTOR_SIZE;
  //! direct block
  if (idx < INODE_DIRECT_CNT)
    return inode->data.direct_block[idx]; // direct block에서 바로 반환

  //! indirect block
  if (idx < INODE_DIRECT_CNT + INDIRECT_BLOCK_CNT) {
    // indirect 블록 안에 있는 포인터 배열 읽어서 블록 포인터 찾기
    block_sector_t indirect_block[INDIRECT_BLOCK_CNT];
    block_read(fs_device, inode->data.indirect_block_sector, indirect_block);

    // indirect block 배열에서 해당 인덱스의 실제 data block 번호 return
    return indirect_block[idx - INODE_DIRECT_CNT];  
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      //! 기존 방식: start 섹터부터 연속적으로 할당하는 방식
      // if (free_map_allocate (sectors, &disk_inode->start)) 
        // {
        //   block_write (fs_device, sector, disk_inode);
        //   if (sectors > 0) 
        //     {
        //       static char zeros[BLOCK_SECTOR_SIZE];
        //       size_t i;
        //              
        //       for (i = 0; i < sectors; i++) 
        //         block_write (fs_device, disk_inode->start + i, zeros);
        //     }
        //   success = true; 
        // } 

      //! direct_block 배열에 하나씩 할당
      static char zeros[BLOCK_SECTOR_SIZE];
      size_t i;
      for (i = 0; i < sectors; i++) {
        if (i >= INODE_DIRECT_CNT) {
          break;  // direct block 개수 초과 -> indirect block로
        }
        if (!free_map_allocate(1, &disk_inode->direct_block[i])) {
          return false;
        }
        block_write(fs_device, disk_inode->direct_block[i], zeros);
      }

      //! indirect_block
      if (sectors > INODE_DIRECT_CNT) {
        size_t indirect_cnt = sectors - INODE_DIRECT_CNT;

        // 인디렉트 블록 하나 할당
        if (!free_map_allocate(1, &disk_inode->indirect_block_sector)) {
          return false;
        }

        block_sector_t indirect_block[INDIRECT_BLOCK_CNT];
        memset(indirect_block, 0, sizeof(indirect_block));

        // 각 인디렉트 엔트리마다 블록 할당 + 0으로 초기화
        for (i = 0; i < indirect_cnt; i++) {
          if (!free_map_allocate(1, &indirect_block[i])) {
            return false;
          }
          block_write(fs_device, indirect_block[i], zeros);
        }

        // 인디렉트 블록 자체를 디스크에 기록
        block_write(fs_device, disk_inode->indirect_block_sector, indirect_block);
      }

      // 전체 섹터 정상 할당된 경우
      block_write(fs_device, sector, disk_inode);
    }
  return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          //! 기존에는 start 섹터부터 연속된 블록들을 해제
          // free_map_release (inode->data.start,
          // bytes_to_sectors (inode->data.length)); 
          //! indexed 구조에서는 direct_block 배열에 따라 개별 해제
          size_t sectors = bytes_to_sectors(inode->data.length);
          size_t i;
          for (i = 0; i < sectors; i++) {
            free_map_release(inode->data.direct_block[i], 1);
          }
          //! indirect block 해제
          if (sectors > INODE_DIRECT_CNT && inode->data.indirect_block_sector != 0) {
            size_t indirect_cnt = sectors - INODE_DIRECT_CNT;

            block_sector_t indirect_block[INDIRECT_BLOCK_CNT];
            block_read(fs_device, inode->data.indirect_block_sector, indirect_block);

            size_t i;
            for (i = 0; i < indirect_cnt; i++) {
              if (indirect_block[i] != 0) {
                free_map_release(indirect_block[i], 1);
              }
            }

            // indirect block 자체도 해제
            free_map_release(inode->data.indirect_block_sector, 1);
          }
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

bool inode_grow(struct inode *inode, off_t new_length) {
  // 기존파일과 신규 파일의 크기를 섹터 단위로 계산
  size_t old_sectors = bytes_to_sectors(inode->data.length);
  size_t new_sectors = bytes_to_sectors(new_length);

  // 0으로 채워진 메모리 버퍼 하나 생성
  static char zeros[BLOCK_SECTOR_SIZE];

  //! direct block grow
  size_t i;
  for (i = old_sectors; i < new_sectors; i++) {
    if (i >= INODE_DIRECT_CNT) {
      break;  // direct block 개수 초과 -> indirect block로
    }
    // 새로운 블록 할당 실패 -> grow 실패
    if (!free_map_allocate(1, &inode->data.direct_block[i])) {
      return false;
    }
    // 새로 할당된 블록을 0으로 초기화
    block_write(fs_device, inode->data.direct_block[i], zeros);
  }

  //! indirect block grow
  if (new_sectors > INODE_DIRECT_CNT) {
    size_t indirect_needed = new_sectors - INODE_DIRECT_CNT;
    block_sector_t indirect_block[INDIRECT_BLOCK_CNT];
    memset(indirect_block, 0, sizeof(indirect_block));

    // 기존 indirect block이 없다면 새로 할당
    if (inode->data.indirect_block_sector == 0) {
      if (!free_map_allocate(1, &inode->data.indirect_block_sector))
        return false;
    }
    else {
      block_read(fs_device, inode->data.indirect_block_sector, indirect_block);
    }
    
    size_t i = 0;
    // 기존에 indirect block까지 이미 일부 할당되어 있었으면 
    if (old_sectors > INODE_DIRECT_CNT){
      i = old_sectors - INODE_DIRECT_CNT;
    }
    for (; i < indirect_needed; i++) {
      if (!free_map_allocate(1, &indirect_block[i])){
        return false;
      }
      block_write(fs_device, indirect_block[i], zeros);
    }

    block_write(fs_device, inode->data.indirect_block_sector, indirect_block);
  }

  inode->data.length = new_length;  // 길이 업데이트
  
  block_write(fs_device, inode->sector, &inode->data);  // 디스크에 inode 정보 업데이트
  return true;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  //! grow 해야한다면
  off_t end = offset + size;  // 쓰려는 마지막 바이트 위치
  if (end > inode->data.length) {  //* 현재 파일 크기 (inode->data.length) 보다 더 뒤까지 쓰려고 하면
    if (!inode_grow(inode, end)) return 0;  // grow 실패 -> 쓰기 실패
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
