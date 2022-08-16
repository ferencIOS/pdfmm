/**
 * Copyright (C) 2010 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2021 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#include "PdfTest.h"

#include <utfcpp/utf8.h>

using namespace std;
using namespace mm;

static void readTestInputFileTo(string& str, const string_view& filepath);

static struct TestPaths
{
    TestPaths() :
        Input(PDF_TEST_RESOURCE_PATH), Output(PDF_TEST_OUTPUT_PATH) { }
    fs::path Input;
    fs::path Output;
} s_paths;


string TestUtils::GetTestOutputFilePath(const string_view& filename)
{
    return (s_paths.Output / filename).u8string();
}

string TestUtils::GetTestInputFilePath(const string_view& filename)
{
    return (s_paths.Input / filename).u8string();
}

const fs::path& TestUtils::GetTestInputPath()
{
    return s_paths.Input;
}

const fs::path& TestUtils::GetTestOutputPath()
{
    return s_paths.Output;
}

void TestUtils::ReadTestInputFileTo(string& str, const string_view& filename)
{
    readTestInputFileTo(str, GetTestInputFilePath(filename));
}

void TestUtils::AssertEqual(double expected, double actual, double threshold)
{
    if (std::abs(actual - expected) > threshold)
        FAIL("Expected different than actual");
}

void TestUtils::SaveFramePPM(charbuff& buffer, const void* data,
    PdfPixelFormat srcPixelFormat, unsigned width, unsigned height)
{
    BufferStreamDevice stream(buffer);
    SaveFramePPM(stream, data, srcPixelFormat, width, height);
}

void TestUtils::SaveFramePPM(OutputStream& stream, const void* data,
    PdfPixelFormat srcPixelFormat, unsigned width, unsigned height)
{
    // Write header
    stringstream headerStream;
    headerStream << "P7\nWIDTH " << width << "\nHEIGHT " << height << "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n";

    stream.Write(headerStream.str());

    unsigned lineSize = width * 4;
    charbuff scanline(lineSize);

    // Write pixel data
    switch (srcPixelFormat)
    {
        case PdfPixelFormat::RGBA:
        {
            stream.Write((const char*)data, width * height * 4);
            break;
        }
        case PdfPixelFormat::BGRA:
        {
            for (unsigned i1 = 0; i1 < height; i1++)
            {
                for (unsigned i2 = 0; i2 < width; i2++)
                {
                    scanline[i2 * 4 + 0] = ((const unsigned char*)data)[i1 * lineSize + i2 * 4 + 2];
                    scanline[i2 * 4 + 1] = ((const unsigned char*)data)[i1 * lineSize + i2 * 4 + 1];
                    scanline[i2 * 4 + 2] = ((const unsigned char*)data)[i1 * lineSize + i2 * 4 + 0];
                    scanline[i2 * 4 + 3] = ((const unsigned char*)data)[i1 * lineSize + i2 * 4 + 3];
                }

                stream.Write(scanline.data(), scanline.size());
            }
            break;
        }
        case PdfPixelFormat::Unknown:
        default:
            PDFMM_RAISE_ERROR(PdfErrorCode::InvalidEnumValue);
    }

    stream.Flush();
}

void readTestInputFileTo(string& str, const string_view& filepath)
{
#ifdef _WIN32
    auto filepath16 = utf8::utf8to16((string)filepath);
    ifstream stream((wchar_t*)filepath16.c_str(), ios_base::in | ios_base::binary);
#else
    ifstream stream((string)filepath, ios_base::in | ios_base::binary);
#endif

    stream.seekg(0, ios::end);
    if (stream.tellg() == -1)
        throw runtime_error("Error reading from stream");

    str.reserve((size_t)stream.tellg());
    stream.seekg(0, ios::beg);

    str.assign((istreambuf_iterator<char>(stream)), istreambuf_iterator<char>());
}
