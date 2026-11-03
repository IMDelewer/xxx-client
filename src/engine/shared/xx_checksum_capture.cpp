#include "xx_checksum_capture.h"

#include <base/str.h>

#include <charconv>
#include <limits>

bool SXxChecksumCapture::Add(EXxChecksumType Type, const CUuid &Challenge, int Start, int Length, const SHA256_DIGEST &Hash)
{
	if(Type != EXxChecksumType::DDNET || Start < 0 || Length < 0 || Start > std::numeric_limits<int>::max() - Length)
		return false;
	if(const SXxChecksumEntry *pEntry = Find(Type, Challenge, Start, Length))
		return pEntry->m_Hash == Hash;
	if(m_vEntries.size() >= XX_CHECKSUM_MAX_ENTRIES)
		return false;
	m_vEntries.push_back({Type, Challenge, Start, Length, Hash});
	return true;
}

const SXxChecksumEntry *SXxChecksumCapture::Find(EXxChecksumType Type, const CUuid &Challenge, int Start, int Length) const
{
	for(const auto &Entry : m_vEntries)
	{
		if(Entry.m_Type == Type && Entry.m_Challenge == Challenge && Entry.m_Start == Start && Entry.m_Length == Length)
			return &Entry;
	}
	return nullptr;
}

std::string SXxChecksumCapture::Serialize() const
{
	std::string Result = "v2|" + std::to_string(m_vEntries.size());
	for(const auto &Entry : m_vEntries)
	{
		char aUuid[UUID_MAXSTRSIZE];
		char aHash[SHA256_MAXSTRSIZE];
		FormatUuid(Entry.m_Challenge, aUuid, sizeof(aUuid));
		sha256_str(Entry.m_Hash, aHash, sizeof(aHash));
		Result += "|" + std::to_string(static_cast<int>(Entry.m_Type)) + "|" + aUuid + "|" + std::to_string(Entry.m_Start) + "|" + std::to_string(Entry.m_Length) + "|" + aHash;
	}
	return Result;
}

bool SXxChecksumCapture::Deserialize(std::string_view Data)
{
	if(Data.size() > 4096)
		return false;
	std::vector<std::string_view> vFields;
	for(size_t Start = 0;;)
	{
		const size_t End = Data.find('|', Start);
		vFields.push_back(Data.substr(Start, End == std::string_view::npos ? End : End - Start));
		if(End == std::string_view::npos)
			break;
		Start = End + 1;
	}
	auto ParseInt = [](std::string_view Field, int &Value) {
		if(Field.empty())
			return false;
		const auto Result = std::from_chars(Field.data(), Field.data() + Field.size(), Value);
		return Result.ec == std::errc() && Result.ptr == Field.data() + Field.size();
	};
	int Count;
	if(vFields.size() < 2 || vFields[0] != "v2" || !ParseInt(vFields[1], Count) || Count < 0 || Count > XX_CHECKSUM_MAX_ENTRIES || vFields.size() != 2 + 5 * static_cast<size_t>(Count))
		return false;
	SXxChecksumCapture Parsed;
	for(int i = 0; i < Count; ++i)
	{
		const size_t Offset = 2 + 5 * i;
		int Type, Start, Length;
		CUuid Challenge;
		SHA256_DIGEST Hash;
		if(!ParseInt(vFields[Offset], Type) || Type != static_cast<int>(EXxChecksumType::DDNET) ||
			!ParseInt(vFields[Offset + 2], Start) || !ParseInt(vFields[Offset + 3], Length) ||
			vFields[Offset + 1].size() != UUID_MAXSTRSIZE - 1 || ParseUuid(&Challenge, std::string(vFields[Offset + 1]).c_str()) != 0 ||
			vFields[Offset + 4].size() != SHA256_MAXSTRSIZE - 1 || sha256_from_str(&Hash, std::string(vFields[Offset + 4]).c_str()) != 0 ||
			Parsed.Find(static_cast<EXxChecksumType>(Type), Challenge, Start, Length) ||
			!Parsed.Add(static_cast<EXxChecksumType>(Type), Challenge, Start, Length, Hash))
			return false;
	}
	m_vEntries = std::move(Parsed.m_vEntries);
	return true;
}
