/**
 * Copyright (C) 2005 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2020 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public License 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#ifndef PDF_WRITER_H
#define PDF_WRITER_H

#include "PdfDefines.h"
#include "PdfInputDevice.h"
#include "PdfOutputDevice.h"
#include "PdfIndirectObjectList.h"
#include "PdfObject.h"

#include "PdfEncrypt.h"

namespace mm {

class PdfDictionary;
class PdfName;
class PdfPage;
class PdfPagesTree;
class PdfParser;
class PdfIndirectObjectList;
class PdfXRef;

/** The PdfWriter class writes a list of PdfObjects as PDF file.
 *  The XRef section (which is the required table of contents for any
 *  PDF file) is created automatically.
 *
 *  It does not know about pages but only about PdfObjects.
 *
 *  Most users will want to use PdfDocument.
 */
class PDFMM_API PdfWriter
{
private:
    PdfWriter(PdfIndirectObjectList* objects, const PdfObject& trailer, PdfVersion version);

public:
    /** Create a new pdf file, from an vector of PdfObjects
     *  and a trailer object.
     *  \param objects the vector of objects
     *  \param trailer a valid trailer object
     */
    PdfWriter(PdfIndirectObjectList& objects, const PdfObject& trailer);

    virtual ~PdfWriter();

    /** Internal implementation of the Write() call with the common code
     *  \param device write to this output device
     *  \param bRewriteXRefTable whether will rewrite whole XRef table (used only if GetIncrementalUpdate() returns true)
     */
    void Write(PdfOutputDevice& device);

    /** Create a XRef stream which is in some case
     *  more compact but requires at least PDF 1.5
     *  Default is false.
     *  \param useXRefStream if true a XRef stream object will be created
     */
    void SetUseXRefStream(bool useXRefStream);

    /** Set the written document to be encrypted using a PdfEncrypt object
     *
     *  \param encrypt an encryption object which is used to encrypt the written PDF file
     */
    void SetEncrypted(const PdfEncrypt& encrypt);


    /** Add required keys to a trailer object
     *  \param trailer add keys to this object
     *  \param size number of objects in the PDF file
     *  \param onlySizeKey write only the size key
     */
    void FillTrailerObject(PdfObject& trailer, size_t size, bool onlySizeKey) const;

public:
    /** Get the file format version of the pdf
     *  \returns the file format version as string
     */
    const char* GetPdfVersionString() const;

    inline void SetSaveOptions(PdfSaveOptions saveOptions) { m_saveOptions = saveOptions; }

    /** Set the write mode to use when writing the PDF.
     *  \param writeMode write mode
     */
    inline void SetWriteMode(PdfWriteMode writeMode) { m_WriteMode = writeMode; }

    /** Get the write mode used for writing the PDF
     *  \returns the write mode
     */
    inline PdfWriteMode GetWriteMode() const { return m_WriteMode; }

    /** Set the PDF Version of the document. Has to be called before Write() to
     *  have an effect.
     *  \param version  version of the pdf document
     */
    inline void SetPdfVersion(PdfVersion version) { m_Version = version; }

    /** Get the PDF version of the document
     *  \returns EPdfVersion version of the pdf document
     */
    inline PdfVersion GetPdfVersion() const { return m_Version; }

    /**
     *  \returns whether an XRef stream is used or not
     */
    inline bool GetUseXRefStream() const { return m_UseXRefStream; }

    /** Sets an offset to the previous XRef table. Set it to lower than
     *  or equal to 0, to not write a reference to the previous XRef table.
     *  The default is 0.
     *  \param lPrevXRefOffset the previous XRef table offset
     */
    inline void SetPrevXRefOffset(int64_t prevXRefOffset) { m_PrevXRefOffset = prevXRefOffset; }

    /**
     *  \returns offset to the previous XRef table, as previously set
     *     by SetPrevXRefOffset.
     *
     * \see SetPrevXRefOffset
     */
    inline int64_t GetPrevXRefOffset() const { return m_PrevXRefOffset; }

    /** Set whether writing an incremental update.
     *  Default is false.
     *  \param bIncrementalUpdate if true an incremental update will be written
     */
    void SetIncrementalUpdate(bool rewriteXRefTable);

    /**
     *  \returns whether writing an incremental update
     */
    inline bool GetIncrementalUpdate() const { return m_IncrementalUpdate; }

    /**
     * \returns true if this PdfWriter creates an encrypted PDF file
     */
    inline bool GetEncrypted() const { return m_Encrypt != nullptr; }

protected:
    /**
     * Create a PdfWriter from a PdfIndirectObjectList
     */
    PdfWriter(PdfIndirectObjectList& objects);

    /** Writes the pdf header to the current file.
     *  \param device write to this output device
     */
    void WritePdfHeader(PdfOutputDevice& device);

    /** Write pdf objects to file
     *  \param device write to this output device
     *  \param objects write all objects in this vector to the file
     *  \param pXref add all written objects to this XRefTable
     *  \param bRewriteXRefTable whether will rewrite whole XRef table (used only if GetIncrementalUpdate() returns true)
     */
    void WritePdfObjects(PdfOutputDevice& device, const PdfIndirectObjectList& objects, PdfXRef& xref);

    /** Creates a file identifier which is required in several
     *  PDF workflows.
     *  All values from the files document information dictionary are
     *  used to create a unique MD5 key which is added to the trailer dictionary.
     *
     *  \param identifier write the identifier to this string
     *  \param trailer trailer object
     *  \param pOriginalIdentifier write the original identifier (when using incremental update) to this string
     */
    void CreateFileIdentifier(PdfString& identifier, const PdfObject& trailer, PdfString* pOriginalIdentifier = nullptr) const;


    const PdfObject& GetTrailer() { return m_Trailer; }
    PdfIndirectObjectList& GetObjects() { return *m_Objects; }
    PdfEncrypt* GetEncrypt() { return m_Encrypt.get(); }
    PdfObject* GetEncryptObj() { return m_EncryptObj; }
    const PdfString& GetIdentifier() { return m_identifier; }
    void SetIdentifier(const PdfString& identifier) { m_identifier = identifier; }
    void SetEncryptObj(PdfObject* obj);
private:
    PdfIndirectObjectList* m_Objects;
    PdfObject m_Trailer;
    PdfVersion m_Version;

    bool m_UseXRefStream;

    std::unique_ptr<PdfEncrypt> m_Encrypt;    // If not nullptr encrypt all strings and streams and
                                               // create an encryption dictionary in the trailer
    PdfObject* m_EncryptObj;                  // Used to temporarily store the encryption dictionary

    PdfSaveOptions m_saveOptions;
    PdfWriteMode m_WriteMode;

    PdfString m_identifier;
    PdfString m_originalIdentifier; // used for incremental update
    int64_t m_PrevXRefOffset;
    bool m_IncrementalUpdate;
    bool m_rewriteXRefTable; // Only used if incremental update

    /**
     * This value is required when writing
     * a linearized PDF file.
     * It represents the offset of the whitespace
     * character before the first line in the XRef
     * section.
     */
    size_t m_FirstInXRef;
    size_t m_LinearizedOffset;
    size_t m_LinearizedLastOffset;
    size_t m_TrailerOffset;
};

};

#endif // PDF_WRITER_H
