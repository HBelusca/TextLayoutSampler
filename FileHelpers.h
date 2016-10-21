//+---------------------------------------------------------------------------
//
//  Contents:   Helper for enumerating files.
//
//  History:    2013-10-04   dwayner    Created
//
//----------------------------------------------------------------------------
#pragma once

HRESULT ReadTextFile(const char16_t* filename, OUT std::u16string& text) throw(); // Read UTF-8 or ASCII
HRESULT WriteTextFile(const char16_t* filename, array_ref<char16_t const> text) throw();
HRESULT WriteTextFile(const char16_t* filename, const std::u16string& text) throw(); // Write as UTF-8
HRESULT WriteTextFile(const char16_t* filename, __in_ecount(textLength) const char16_t* text, uint32_t textLength) throw(); // Write as UTF-8
HRESULT ReadBinaryFile(const char16_t* filename, OUT std::vector<uint8_t>& fileBytes);
HRESULT WriteBinaryFile(const char16_t* filename, const std::vector<uint8_t>& fileData);
HRESULT WriteBinaryFile(const char16_t* filename, array_ref<uint8_t const> fileData);
HRESULT WriteBinaryFile(_In_z_ const char16_t* filename, _In_reads_bytes_(fileDataSize) const void* fileData, uint32_t fileDataSize);

std::u16string GetActualFileName(array_ref<const char16_t> fileName);
std::u16string GetFullFileName(array_ref<const char16_t> fileName);

// Expand the given path and mask into a list of nul-separated filenames.
HRESULT EnumerateMatchingFiles(
    __in_z_opt const char16_t* fileDirectory,
    __in_z_opt char16_t const* originalFileMask,
    IN OUT std::u16string& fileNames // Appended onto any existing names. It's safe for this to alias fileDirectory.
    );

const char16_t* FindFileNameStart(array_ref<const char16_t> fileName);

const char16_t* FindFileNameExtension(array_ref<const char16_t> fileName);

bool FileContainsWildcard(array_ref<const char16_t> fileName);
