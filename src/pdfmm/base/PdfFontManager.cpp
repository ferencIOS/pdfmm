/**
 * Copyright (C) 2007 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2020 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public License 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#include <pdfmm/private/PdfDefinesPrivate.h>
#include "PdfFontManager.h" 

#if defined(_WIN32) && !defined(PDFMM_HAVE_FONTCONFIG)
#include <pdfmm/common/WindowsLeanMean.h>
#endif // _WIN32

#include <algorithm>

#include <utfcpp/utf8.h>
#include <pdfmm/private/FreetypePrivate.h>

#include "PdfDictionary.h"
#include "PdfInputDevice.h"
#include "PdfOutputDevice.h"
#include "PdfFont.h"
#include "PdfFontMetricsFreetype.h"
#include "PdfFontStandard14.h"
#include "PdfFontType1.h"

using namespace std;
using namespace mm;

#if defined(_WIN32) && !defined(PDFMM_HAVE_FONTCONFIG)

static bool getFontData(chars& buffer, const LOGFONTW& inFont);
static bool getFontData(chars& buffer, HDC hdc, HFONT hf);
static void getFontDataTTC(chars& buffer, const chars& fileBuffer, const chars& ttcBuffer);

#endif // _WIN32

string extractBaseName(const string_view& fontName, bool& isBold, bool& isItalic);

PdfFontManager::PdfFontManager(PdfDocument& doc)
    : m_doc(&doc)
{
#if defined(PDFMM_HAVE_FONTCONFIG)
    m_fontConfig = PdfFontConfigWrapper::GetInstance();
#endif
    Init();
}

PdfFontManager::~PdfFontManager()
{
    this->EmptyCache();

    if (m_ftLibrary != nullptr)
    {
        FT_Done_FreeType(m_ftLibrary);
        m_ftLibrary = nullptr;
    }
}

void PdfFontManager::Init()
{
    // Initialize all the fonts stuff
    if (FT_Init_FreeType(&m_ftLibrary))
        PDFMM_RAISE_ERROR(PdfErrorCode::FreeType);
}

void PdfFontManager::EmptyCache()
{
    for (auto& pair : m_fontMap)
        delete pair.second;

    for (auto& pair : m_fontSubsetMap)
        delete pair.second;

    m_fontMap.clear();
    m_fontSubsetMap.clear();
}

PdfFont* PdfFontManager::GetFont(PdfObject& obj)
{
    const PdfReference& ref = obj.GetIndirectReference();

    // Search if the object is a cached normal font
    for (auto& pair : m_fontMap)
    {
        if (pair.second->GetObject().GetIndirectReference() == ref)
            return pair.second;
    }

    // Search if the object is a cached font subset
    for (auto& pair : m_fontSubsetMap)
    {
        if (pair.second->GetObject().GetIndirectReference() == ref)
            return pair.second;
    }

    // Create a new font
    unique_ptr<PdfFont> font;
    if (!PdfFont::TryCreateFromObject(obj, font))
        return nullptr;

    auto inserted = m_fontMap.insert({ Element(font->GetMetrics().GetFontNameSafe(),
        font->GetEncoding(),
        font->GetMetrics().IsBold(),
        font->GetMetrics().IsItalic(),
        font->GetMetrics().IsSymbol()), font.release() });
    return inserted.first->second;
}

PdfFont* PdfFontManager::GetFont(const string_view& fontName, const PdfFontCreationParams& params)
{
    if (params.Encoding.IsNull())
        PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InvalidHandle, "Invalid encoding");

    PdfFontCreationParams newParams = params;
    string baseFontName = extractBaseName(fontName, newParams.Bold, newParams.Italic);
    return getFont(baseFontName, newParams);
}

PdfFont* PdfFontManager::getFont(const string_view& fontName, const PdfFontCreationParams& params)
{
    auto found = m_fontMap.find(Element(
        fontName,
        params.Encoding,
        params.Bold,
        params.Italic,
        params.IsSymbolCharset));
    if (found != m_fontMap.end())
        return found->second;

    PdfStandard14FontType baseFont;
    if ((params.Flags & PdfFontCreationFlags::AutoSelectStandard14) == PdfFontCreationFlags::AutoSelectStandard14
        && PdfFontStandard14::IsStandard14Font(fontName, baseFont))
    {
        // TODO: Better handle params.Bold, params.Italic, params.IsSymbolCharset
        // For example, if we search for "Helvetica" and we set Bold=true, we want
        // to match "Helvetica-Bold"
        PdfFontInitParams initParams = { params.Bold, params.Italic, false, false };
        auto font = PdfFont::CreateStandard14(*m_doc, baseFont, params.Encoding, initParams).release();
        if (font != nullptr)
        {
            m_fontMap.insert({ Element(fontName,
                params.Encoding,
                font->GetMetrics().IsBold(),
                font->GetMetrics().IsItalic(),
                font->GetMetrics().IsSymbol()), font });
            return font;
        }
    }

    bool subsetting = (params.Flags & PdfFontCreationFlags::DoSubsetting) != PdfFontCreationFlags::None;
    string path = params.FilePath;
    if (path.empty())
        path = this->GetFontPath(fontName, params.Bold, params.Italic);

    if (path.empty())
    {
#if defined(_WIN32) && !defined(PDFMM_HAVE_FONTCONFIG)
        return GetWin32Font(m_fontMap, fontName, params.Encoding, params.Bold, params.Italic,
            params.IsSymbolCharset, params.Embed, subsetting);
#else
        return nullptr;
#endif
    }
    else
    {
        auto metrics = std::make_shared<PdfFontMetricsFreetype>(m_ftLibrary, path,
            params.IsSymbolCharset);
        return this->CreateFontObject(m_fontMap, fontName, metrics, params.Encoding,
            params.Bold, params.Italic, params.Embed, subsetting);
    }
}

PdfFont* PdfFontManager::GetFontSubset(const string_view& fontName, const PdfFontCreationParams& params)
{
    if (params.Encoding.IsNull())
        PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InvalidHandle, "Invalid encoding");

    if (params.Flags != PdfFontCreationFlags::None)
        PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InternalLogic, "Invalid font subset creation parameters");

    if (!params.Embed)
        PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InternalLogic, "Invalid font subset creation parameters");

    // WARNING: The characters are completely ignored right now!

    PdfFontCreationParams newParams = params;
    string baseFontName = extractBaseName(fontName, newParams.Bold, newParams.Italic);
    return getFontSubset(baseFontName, newParams);
}

PdfFont* PdfFontManager::getFontSubset(const std::string_view& fontName, const PdfFontCreationParams& params)
{
    auto found = m_fontSubsetMap.find(Element(
        fontName,
        params.Encoding,
        params.Bold,
        params.Italic,
        params.IsSymbolCharset));
    if (found == m_fontSubsetMap.end())
    {
        string path = params.FilePath;
        if (path.empty())
            path = this->GetFontPath(fontName, params.Bold, params.Italic);

        if (path.empty())
        {
#if defined(_WIN32) && !defined(PDFMM_HAVE_FONTCONFIG)
            return GetWin32Font(m_fontSubsetMap, fontName,
                params.Encoding, params.Bold, params.Italic,
                params.IsSymbolCharset, true, true);
#else
            PdfError::LogMessage(LogSeverity::Error, "No path was found for the specified fontname: {}", fontName);
            return nullptr;
#endif
        }

        auto metrics = (PdfFontMetricsConstPtr)PdfFontMetricsFreetype::CreateForSubsetting(m_ftLibrary, path,
            params.IsSymbolCharset);
        return CreateFontObject(m_fontSubsetMap, fontName, metrics, params.Encoding,
            params.Bold, params.Italic, true, true);
    }
    else
    {
        return found->second;
    }
}

PdfFont* PdfFontManager::GetFont(FT_Face face, const PdfEncoding& encoding,
    bool isSymbolCharset, bool embed)
{
    if (encoding.IsNull())
        PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InvalidHandle, "Invalid encoding");

    string name = FT_Get_Postscript_Name(face);
    if (name.empty())
    {
        PdfError::LogMessage(LogSeverity::Error, "Could not retrieve fontname for font!");
        return nullptr;
    }

    bool bold = (face->style_flags & FT_STYLE_FLAG_BOLD) != 0;
    bool italic = (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;

    auto found = m_fontMap.find(Element(
        name,
        encoding,
        bold,
        italic,
        isSymbolCharset));
    if (found == m_fontMap.end())
    {
        auto metrics = std::make_shared<PdfFontMetricsFreetype>(m_ftLibrary, face, isSymbolCharset);
        return this->CreateFontObject(m_fontMap, name, metrics, encoding,
            bold, italic, embed, false);
    }
    else
    {
        return found->second;
    }
}

void PdfFontManager::EmbedSubsetFonts()
{
    for (auto &pair : m_fontSubsetMap)
        pair.second->EmbedFontSubset();
}

#if defined(_WIN32) && !defined(PDFMM_HAVE_FONTCONFIG)

PdfFont* PdfFontManager::GetFont(const LOGFONTW& logFont,
    const PdfEncoding& encoding, bool embed)
{
    if (encoding.IsNull())
        PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InvalidHandle, "Invalid encoding");

    string fontname;
    utf8::utf16to8((char16_t*)logFont.lfFaceName, (char16_t*)logFont.lfFaceName + LF_FACESIZE, std::back_inserter(fontname));

    auto found = m_fontMap.find(Element(
        fontname,
        encoding,
        logFont.lfWeight >= FW_BOLD ? true : false,
        logFont.lfItalic == 0 ? false : true,
        logFont.lfCharSet == SYMBOL_CHARSET));

    if (found == m_fontMap.end())
        return GetWin32Font(m_fontMap, fontname, logFont, encoding, embed, false);
    else
        return found->second;
}

PdfFont* PdfFontManager::GetWin32Font(FontCacheMap& map, const string_view& fontName,
    const PdfEncoding& encoding, bool bold, bool italic,
    bool symbolCharset, bool embed, bool subsetting)
{
    u16string fontnamew;
    utf8::utf8to16(fontName.begin(), fontName.end(), std::back_inserter(fontnamew));

    // The length of this fontname must not exceed LF_FACESIZE,
    // including the terminating NULL
    if (fontnamew.length() >= LF_FACESIZE)
        return nullptr;

    LOGFONTW lf;
    lf.lfHeight = 0;
    lf.lfWidth = 0;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;
    lf.lfWeight = bold ? FW_BOLD : 0;
    lf.lfItalic = italic;
    lf.lfUnderline = 0;
    lf.lfStrikeOut = 0;
    // NOTE: ANSI_CHARSET should give a consistent result among
    // different locale configurations but sometimes dont' match fonts.
    // We prefer OEM_CHARSET over DEFAULT_CHARSET because it configures
    // the mapper in a way that will match more fonts
    lf.lfCharSet = symbolCharset ? SYMBOL_CHARSET : OEM_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

    memset(lf.lfFaceName, 0, LF_FACESIZE * sizeof(char16_t));
    memcpy(lf.lfFaceName, fontnamew.c_str(), fontnamew.length() * sizeof(char16_t));
    return GetWin32Font(map, fontName, lf, encoding, embed, subsetting);
}

PdfFont* PdfFontManager::GetWin32Font(FontCacheMap& map, const string_view& fontName,
    const LOGFONTW& logFont, const PdfEncoding& encoding, bool embed, bool subsetting)
{
    chars buffer;
    if (!getFontData(buffer, logFont))
        return nullptr;

    auto metrics = std::make_shared<PdfFontMetricsFreetype>(m_ftLibrary, buffer.data(), buffer.size(),
        logFont.lfCharSet == SYMBOL_CHARSET);
    return this->CreateFontObject(map, fontName, metrics, encoding,
        logFont.lfWeight >= FW_BOLD ? true : false,
        logFont.lfItalic ? true : false, embed, subsetting);
}

#endif // defined(_WIN32) && !defined(PDFMM_HAVE_FONTCONFIG)

string PdfFontManager::GetFontPath(const string_view& fontName, bool bold, bool italic)
{
#if defined(PDFMM_HAVE_FONTCONFIG)
    return m_fontConfig->GetFontConfigFontPath(fontName, bold, italic);
#else
    (void)fontName;
    (void)bold;
    (void)italic;
    return string();
#endif
}

PdfFont* PdfFontManager::CreateFontObject(FontCacheMap& map, const string_view& fontName,
    const PdfFontMetricsConstPtr& metrics, const PdfEncoding& encoding,
    bool bold, bool italic, bool embed, bool subsetting)
{
    PdfFont* font;

    try
    {
        PdfFontInitParams params = { bold, italic, embed, subsetting };
        font = PdfFont::Create(*m_doc, metrics, encoding, params).release();
        if (font != nullptr)
        {
            map.insert({ Element(fontName,
                encoding,
                metrics->IsBold(),
                metrics->IsItalic(),
                metrics->IsSymbol()), font });
        }
    }
    catch (PdfError& e)
    {
        PDFMM_PUSH_FRAME(e);
        e.PrintErrorMsg();
        PdfError::LogMessage(LogSeverity::Error, "Cannot initialize font: {}", fontName);
        return nullptr;
    }

    return font;
}

#ifdef PDFMM_HAVE_FONTCONFIG

void PdfFontManager::SetFontConfigWrapper(PdfFontConfigWrapper* fontConfig)
{
    if (m_fontConfig == fontConfig)
        return;

    if (fontConfig == nullptr)
        m_fontConfig = PdfFontConfigWrapper::GetInstance();
    else
        m_fontConfig = fontConfig;
}

#endif // PDFMM_HAVE_FONTCONFIG

PdfFontManager::Element::Element(const string_view& fontname, const PdfEncoding& encoding,
    bool bold, bool italic, bool isSymbolCharset) :
    FontName(fontname),
    EncodingId(encoding.GetId()),
    Bold(bold),
    Italic(italic),
    IsSymbolCharset(isSymbolCharset) { }

PdfFontManager::Element::Element(const Element& rhs) :
    FontName(rhs.FontName),
    EncodingId(rhs.EncodingId),
    Bold(rhs.Bold),
    Italic(rhs.Italic),
    IsSymbolCharset(rhs.IsSymbolCharset) { }

const PdfFontManager::Element& PdfFontManager::Element::operator=(const Element& rhs)
{
    FontName = rhs.FontName;
    EncodingId = rhs.EncodingId;
    Bold = rhs.Bold;
    Italic = rhs.Italic;
    IsSymbolCharset = rhs.IsSymbolCharset;
    return *this;
}

size_t PdfFontManager::HashElement::operator()(const Element& elem) const
{
    size_t hash = 0;
    utls::hash_combine(hash, elem.FontName, elem.EncodingId, elem.Bold, elem.Italic, elem.IsSymbolCharset);
    return hash;
};

bool PdfFontManager::EqualElement::operator()(const Element& lhs, const Element& rhs) const
{
    return lhs.EncodingId == rhs.EncodingId && lhs.Bold == rhs.Bold
        && lhs.Italic == rhs.Italic && lhs.FontName == rhs.FontName
        && lhs.IsSymbolCharset == rhs.IsSymbolCharset;
}

string extractBaseName(const string_view& fontName, bool& isBold, bool& isItalic)
{
    string name = (string)fontName;
    auto index = fontName.find(",BoldItalic");
    if (index != string_view::npos)
    {
        name.erase(index, char_traits<char>::length(",BoldItalic"));
        isBold = true;
        isItalic = true;
    }

    index = fontName.find(",Bold");
    if (index != string_view::npos)
    {
        name.erase(index, char_traits<char>::length(",Bold"));
        isBold = true;
    }

    index = fontName.find(",Italic");
    if (index != string_view::npos)
    {
        name.erase(index, char_traits<char>::length(",Italic"));
        isItalic = true;
    }

    return name;
}

#if defined(_WIN32) && !defined(PDFMM_HAVE_FONTCONFIG)

bool getFontData(chars& buffer, const LOGFONTW& inFont)
{
    bool success = false;

    HDC hdc = ::CreateCompatibleDC(nullptr);
    HFONT hf = CreateFontIndirectW(&inFont);
    if (hf != nullptr)
    {
        success = getFontData(buffer, hdc, hf);
        DeleteObject(hf);
    }
    ReleaseDC(0, hdc);
    return success;
}

bool getFontData(chars& buffer, HDC hdc, HFONT hf)
{
    HGDIOBJ oldFont = SelectObject(hdc, hf);
    bool sucess = false;

    // try get data from true type collection
    constexpr DWORD ttcf_const = 0x66637474;
    unsigned fileLen = GetFontData(hdc, 0, 0, nullptr, 0);
    unsigned ttcLen = GetFontData(hdc, ttcf_const, 0, nullptr, 0);

    if (fileLen != GDI_ERROR)
    {
        if (ttcLen == GDI_ERROR)
        {
            buffer.resize(fileLen);
            sucess = GetFontData(hdc, 0, 0, buffer.data(), (DWORD)fileLen) != GDI_ERROR;
        }
        else
        {
            chars fileBuffer(fileLen);
            if (GetFontData(hdc, ttcf_const, 0, fileBuffer.data(), fileLen) == GDI_ERROR)
            {
                sucess = false;
                goto Exit;
            }

            chars ttcBuffer(ttcLen);
            if (GetFontData(hdc, 0, 0, ttcBuffer.data(), ttcLen) == GDI_ERROR)
            {
                sucess = false;
                goto Exit;
            }

            getFontDataTTC(buffer, fileBuffer, ttcBuffer);
            sucess = true;
        }
    }

Exit:
    // clean up
    SelectObject(hdc, oldFont);
    return sucess;
}

// This function will recieve the device context for the
// TrueType Collection font, it will then extract necessary,
// tables and create the correct buffer.
void getFontDataTTC(chars& buffer, const chars& fileBuffer, const chars& ttcBuffer)
{
    uint16_t numTables = FROM_BIG_ENDIAN(*(uint16_t*)(ttcBuffer.data() + 4));
    unsigned outLen = 12 + 16 * numTables;
    const char* entry = ttcBuffer.data() + 12;

    //us: see "http://www.microsoft.com/typography/otspec/otff.htm"
    for (unsigned i = 0; i < numTables; i++)
    {
        uint32_t length = FROM_BIG_ENDIAN(*(uint32_t*)(entry + 12));
        length = (length + 3) & ~3;
        entry += 16;
        outLen += length;
    }

    buffer.resize(outLen);

    // copy font header and table index (offsets need to be still adjusted)
    memcpy(buffer.data(), ttcBuffer.data(), 12 + 16 * numTables);
    uint32_t dstDataOffset = 12 + 16 * numTables;

    // process tables
    const char* srcEntry = ttcBuffer.data() + 12;
    char* dstEntry = buffer.data() + 12;
    for (unsigned i = 0; i < numTables; i++)
    {
        // read source entry
        uint32_t offset = FROM_BIG_ENDIAN(*(uint32_t*)(srcEntry + 8));
        uint32_t length = FROM_BIG_ENDIAN(*(uint32_t*)(srcEntry + 12));
        length = (length + 3) & ~3;

        // adjust offset
        // U can use FromBigEndian() also to convert _to_ big endian
        *(uint32_t*)(dstEntry + 8) = FROM_BIG_ENDIAN(dstDataOffset);

        //copy data
        memcpy(buffer.data() + dstDataOffset, fileBuffer.data() + offset, length);
        dstDataOffset += length;

        // adjust table entry pointers for loop
        srcEntry += 16;
        dstEntry += 16;
    }
}

#endif // defined(_WIN32) && !defined(PDFMM_HAVE_FONTCONFIG)
