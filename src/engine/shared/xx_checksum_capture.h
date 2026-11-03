#ifndef ENGINE_SHARED_XX_CHECKSUM_CAPTURE_H
#define ENGINE_SHARED_XX_CHECKSUM_CAPTURE_H

#include <base/hash.h>
#include <base/types.h>

#include <engine/shared/uuid_manager.h>

#include <string>
#include <string_view>
#include <vector>

enum class EXxChecksumType
{
	DDNET = 0,
};

struct SXxChecksumEntry
{
	EXxChecksumType m_Type;
	CUuid m_Challenge;
	int m_Start;
	int m_Length;
	SHA256_DIGEST m_Hash;
};

static constexpr int XX_CHECKSUM_MAX_ENTRIES = 16;

struct SXxChecksumCapture
{
	std::vector<SXxChecksumEntry> m_vEntries;

	bool Add(EXxChecksumType Type, const CUuid &Challenge, int Start, int Length, const SHA256_DIGEST &Hash);
	const SXxChecksumEntry *Find(EXxChecksumType Type, const CUuid &Challenge, int Start, int Length) const;
	std::string Serialize() const;
	bool Deserialize(std::string_view Data);
	void Clear() { m_vEntries.clear(); }
};

#endif
