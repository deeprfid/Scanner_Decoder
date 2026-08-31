#ifndef TAGINFO_Buffer_H
#define TAGINFO_Buffer_H
#include "ModuleReader.h"

class hapi_TagInfoBuffer
{
public:
	hapi_TagInfoBuffer();
	~hapi_TagInfoBuffer();
	int initBuffer(int maxtagcnt);
	void tagClear();
	int tagInsert(TAGINFO &tag, int timestamp=-1);
	int tagGetCnt();
//	TAGINFO *tagContains(TAGINFO &tag);
	int tagGetNext(TAGINFO &tag);
	void setRecHighestRssi(bool is_);
	void setUniByAnt(bool is_);
	void setUniByEmdData(bool is_);
	void setUniByTimeStamp(bool is_);
	void setUniByCrc(bool is_);
	void dump();

private:
//	#define MAXTAGBUFFERITEMCNT 1000
	typedef struct TagIndexItem_ST
	{
		int tagindex;
		TagIndexItem_ST *next;
	} TagIndexItem;

	typedef struct
	{
		TAGINFO tag;
		unsigned int crc32;
	} TAGINFOExt;

	int tagCmp(int cmpindex, TAGINFO &tag);
	void tagUpdate(int modifyindex, TAGINFO &tag);
	unsigned int m_ReadIndex;
	unsigned int m_WriteIndex;
	bool m_IsRecHighestRssi;
	bool m_IsUniByAnt;
	bool m_IsUniByEmdData;
	bool m_IsUniByTimeStamp;
	bool m_IsUniByCrc;

	TAGINFOExt *m_Buffer;
	TagIndexItem **m_pTagIndexBucketHeader;
	unsigned int crc32(TAGINFO &tag);

	TagIndexItem *m_TagIndexPool;
	int FindTagIndexItem(TAGINFO &tag, int crcindex, TagIndexItem*& pTII);

	int m_MaxTagCnt;
};

#endif


