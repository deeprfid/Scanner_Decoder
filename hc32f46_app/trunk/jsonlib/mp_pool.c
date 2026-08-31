#include <string.h>
#include <stdio.h>
#include "mp_pool.h"
//#include "hc32f46_driver.h"

void init_mem_sta(void);
void *malloc_hexp(unsigned int size);
void *calloc_hexp(unsigned int num, unsigned int size);
void free_hexp(void *p);
void add_new_mem_sta(int size);
int get_left_heap_size(char *prefix);

/*
static int MaxMPoolLen;
#define MPBlockSize 8

static unsigned char *mp_pool;
static unsigned char *mp_blockflag;
typedef struct
{
	void *address;
	size_t size;
	unsigned short bheaderindex;
	unsigned short blkcnt;
	unsigned char isuse;
} MP_Maker;
static MP_Maker *mp_mallocmaker;

void mp_init(int msize)
{
	MaxMPoolLen = msize;
	mp_pool = malloc_hexp(MaxMPoolLen);
	mp_blockflag = malloc_hexp(MaxMPoolLen / MPBlockSize);
	mp_mallocmaker = malloc_hexp(sizeof(MP_Maker)*(MaxMPoolLen / MPBlockSize));
}

void mp_init_ex(void *p_pool, void *p_blockflag, void *p_mallocmaker, int msize)
{
	MaxMPoolLen = msize;
	mp_pool = p_pool;
	mp_blockflag = p_blockflag;
	mp_mallocmaker = p_mallocmaker;
}

void mp_resetpool()
{
	memset(mp_mallocmaker, 0, sizeof(MP_Maker)*(MaxMPoolLen / MPBlockSize));
	memset(mp_blockflag, 0, MaxMPoolLen / MPBlockSize);
}
static int mp_mkindexbyaddr(void *p)
{
	int i;
	for (i = 0; i < MaxMPoolLen / MPBlockSize; ++i)
	{
		if (mp_mallocmaker[i].isuse == 1)
		{
			if (mp_mallocmaker[i].address == p)
				return i;
		}
	}
	return -1;
}
static int mp_unusedmaker()
{
	int i;
	for (i = 0; i < MaxMPoolLen / MPBlockSize; ++i)
	{
		if (mp_mallocmaker[i].isuse == 0)
			return i;
	}
	return -1;
}
static void mp_clearflags(int mkindex)
{
	int i;
	int headindex = mp_mallocmaker[mkindex].bheaderindex;
	mp_mallocmaker[mkindex].isuse = 0;
	for (i = 0; i < mp_mallocmaker[mkindex].blkcnt; ++i)
		mp_blockflag[headindex+i] = 0;
}

static int mp_contblksindex(int cnt)
{
	int i,j;
	int isfind;
	for (i = 0; i < MaxMPoolLen / MPBlockSize; ++i)
	{
		isfind = 1;
		for (j = 0; j < cnt; ++j)
		{
			if (mp_blockflag[i+j] != 0)
			{
				isfind = 0;
				break;
			}
		}
		if (isfind == 1)
			return i;
	}
	return -1;
}
void mp_makerflags(int hindex, int blkcnt)
{
	int i;
	for (i = 0; i < blkcnt; ++i)
		mp_blockflag[i+hindex] = 1;
}
void *mp_malloc(size_t size, int zero)
{
	int blkcnt = size / MPBlockSize;
	int bheaderindex;
	int mkindex;
	if (!size)
		return NULL;
	mkindex = mp_unusedmaker();
	if (mkindex < 0)
	{
		//´íÎó
		return NULL;
	}
	blkcnt = (size % MPBlockSize == 0) ? blkcnt : (blkcnt + 1);
	bheaderindex = mp_contblksindex(blkcnt);
	if (bheaderindex < 0)
	{
		//´íÎó
		return NULL;
	}
	else
	{
		mp_mallocmaker[mkindex].address = mp_pool+bheaderindex*MPBlockSize;
		mp_mallocmaker[mkindex].isuse = 1;
		mp_mallocmaker[mkindex].blkcnt = blkcnt;
		mp_mallocmaker[mkindex].bheaderindex = bheaderindex;
		mp_mallocmaker[mkindex].size = size;
		mp_makerflags(bheaderindex, blkcnt);
		if (zero)
			memset(mp_mallocmaker[mkindex].address, 0, 
				blkcnt*MPBlockSize);
		
//		printf("malloc mkindex:%d, address:%p, blkcnt:%d, bheaderindex:%d, size:%d\n",
//			mkindex, mp_mallocmaker[mkindex].address, 
//			mp_mallocmaker[mkindex].blkcnt, 
//			mp_mallocmaker[mkindex].bheaderindex, 
//			mp_mallocmaker[mkindex].size);
		return mp_mallocmaker[mkindex].address;
	}
}

void mp_free(void *p)
{
	int mkindex;
	if (p == NULL)
		return;
	mkindex = mp_mkindexbyaddr(p);
	if (mkindex < 0)
	{
		//´íÎó
		return;
	}
	else
	{
		mp_clearflags(mkindex);
//		printf("free mkindex:%d\n", mkindex);
	}
}
*/
static int MaxMPoolLen;
static unsigned char *mp_pool;

void mp_init(int msize)
{
	MaxMPoolLen = msize;
	mp_pool = malloc_hexp(MaxMPoolLen);
}

void mp_init_ex(void *p_pool, int msize)
{
	MaxMPoolLen = msize;
	mp_pool = p_pool;
}

//int mp_malcnt = 0;
void mp_resetpool()
{
	memset(mp_pool, 0, MaxMPoolLen);
//	mp_malcnt = 0;
//	printf("---------------mp_resetpool\n");
}

void *mp_malloc(size_t size, int zero)
{
	unsigned char *start = mp_pool;

	if (!size)
		return NULL;
	
//	printf("mp_malloc size:%d, malcnt:%d\n", size, mp_malcnt);
	while(1)
	{
//		printf("start:%p\n", start);
		if (start[0] == 0x00)
		{
			if ((start+size+4) < (mp_pool+MaxMPoolLen))
			{
				unsigned short rlen = size+4;
				int remainder = rlen % 4;
				
				if (remainder != 0)
					rlen += 4 - remainder;			
				start[0] = 0x01;
				start[1] = (rlen >> 8) & 0xff;
				start[2] = rlen & 0xff;
				if (zero)
					memset(start+4, 0, rlen-4);
//				mp_malcnt++;
//				printf("addr:%p, rlen:%d\n", start+4, rlen);
				return (start+4);
			}
			else
			{
				return NULL;
			}
		}
//		printf("nbytes:%d\n", (start[1] << 8) | start[2]);
		start += (start[1] << 8) | start[2];
	}
}

void mp_free(void *p)
{
//	printf("-------------------------------free p:%p\n", p);
}
