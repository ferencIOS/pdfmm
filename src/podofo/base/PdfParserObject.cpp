/***************************************************************************
 *   Copyright (C) 2005 by Dominik Seichter                                *
 *   domseichter@web.de                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                         *
 *   In addition, as a special exception, the copyright holders give       *
 *   permission to link the code of portions of this program with the      *
 *   OpenSSL library under certain conditions as described in each         *
 *   individual source file, and distribute linked combinations            *
 *   including the two.                                                    *
 *   You must obey the GNU General Public License in all respects          *
 *   for all of the code used other than OpenSSL.  If you modify           *
 *   file(s) with this exception, you may extend this exception to your    *
 *   version of the file(s), but you are not obligated to do so.  If you   *
 *   do not wish to do so, delete this exception statement from your       *
 *   version.  If you delete this exception statement from all source      *
 *   files in the program, then also delete it here.                       *
 ***************************************************************************/

#include "PdfParserObject.h"

#include <doc/PdfDocument.h>
#include "PdfArray.h"
#include "PdfDictionary.h"
#include "PdfEncrypt.h"
#include "PdfInputDevice.h"
#include "PdfInputStream.h"
#include "PdfParser.h"
#include "PdfStream.h"
#include "PdfVariant.h"
#include "PdfDefinesPrivate.h"

#include <iostream>
#include <sstream>

using namespace PoDoFo;
using namespace std;

static const int s_nLenEndObj    = 6; // strlen("endobj");
static const int s_nLenStream    = 6; // strlen("stream");
//static const int s_nLenEndStream = 9; // strlen("endstream");

PdfParserObject::PdfParserObject( PdfVecObjects* pCreator, const PdfRefCountedInputDevice & rDevice, 
                                  const PdfRefCountedBuffer & rBuffer, ssize_t lOffset )
    : PdfObject( PdfVariant::NullValue ), PdfTokenizer( rDevice, rBuffer ), m_pEncrypt( NULL )
{
    SetDocument(pCreator->GetParentDocument());
    InitPdfParserObject();
    m_lOffset = lOffset < 0 ? m_device.Device()->Tell() : lOffset;
}

PdfParserObject::PdfParserObject( const PdfRefCountedBuffer & rBuffer )
    : PdfObject( PdfVariant::NullValue ), PdfTokenizer( PdfRefCountedInputDevice(), rBuffer ), 
      m_pEncrypt( NULL )
{
    InitPdfParserObject();
    m_lOffset = -1;
}

void PdfParserObject::InitPdfParserObject()
{
    m_bIsTrailer = false;

    // Whether or not demand loading is disabled we still don't load
    // anything in the ctor. This just controls whether ::ParseFile(...)
    // forces an immediate demand load, or lets it genuinely happen
    // on demand.
    m_bLoadOnDemand = false;

    // We rely heavily on the demand loading infrastructure whether or not
    // we *actually* delay loading.
    EnableDelayedLoading();
    EnableDelayedLoadingStream();

    m_bStream = false;
    m_lStreamOffset = 0;
}

void PdfParserObject::ReadObjectNumber()
{
    PdfReference reference;
    try
    {
        int64_t obj = this->GetNextNumber();
        int64_t gen = this->GetNextNumber();

        reference = PdfReference(static_cast<uint32_t>(obj), static_cast<uint16_t>(gen));
        SetIndirectReference(reference);
    } catch( PdfError & e )
    {
        e.AddToCallstack( __FILE__, __LINE__, "Object and generation number cannot be read." );
        throw e;
    }
    
    if( !this->IsNextToken( "obj" ))
    {
        std::ostringstream oss;
        oss << "Error while reading object " << reference.ObjectNumber() << " "
            << reference.GenerationNumber() << ": Next token is not 'obj'." << std::endl;
        PODOFO_RAISE_ERROR_INFO( EPdfError::NoObject, oss.str().c_str() );
    }
}

void PdfParserObject::ParseFile( PdfEncrypt* pEncrypt, bool bIsTrailer )
{
    if( !m_device.Device() )
    {
        PODOFO_RAISE_ERROR( EPdfError::InvalidHandle );
    }

    if (m_lOffset >= 0)
        m_device.Device()->Seek(m_lOffset);

    if( !bIsTrailer )
        ReadObjectNumber();

#ifndef VERBOSE_DEBUG_DISABLED
    std::cerr << "Parsing object number: " << m_reference.ObjectNumber()
              << " " << m_reference.GenerationNumber() << " obj"
              << " " << m_lOffset << " offset"
              << " (DL: " << ( m_bLoadOnDemand ? "on" : "off" ) << ")"
              << endl;
#endif // VERBOSE_DEBUG_DISABLED

    m_lOffset = m_device.Device()->Tell();
    m_pEncrypt = pEncrypt;
    m_bIsTrailer = bIsTrailer;

    if( !m_bLoadOnDemand )
    {
        // Force immediate loading of the object.  We need to do this through
        // the deferred loading machinery to avoid getting the object into an
        // inconsistent state.
        // We can't do a full DelayedStreamLoad() because the stream might use
        // an indirect /Length or /Length1 key that hasn't been read yet.
        DelayedLoad();

        // TODO: support immediate loading of the stream here too. For that, we need
        // to be able to trigger the reading of not-yet-parsed indirect objects
        // such as might appear in a /Length key with an indirect reference.
    }
}

void PdfParserObject::ForceStreamParse()
{
    // It's really just a call to DelayedLoad
    DelayedLoadStream();
}

// Only called via the demand loading mechanism
// Be very careful to avoid recursive demand loads via PdfVariant
// or PdfObject method calls here.
void PdfParserObject::ParseFileComplete( bool bIsTrailer )
{
    const char* pszToken;

    m_device.Device()->Seek( m_lOffset );
    if( m_pEncrypt )
        m_pEncrypt->SetCurrentReference( GetIndirectReference() );

    // Do not call GetNextVariant directly,
    // but GetNextToken, to handle empty objects like:
    // 13 0 obj
    // endobj

    EPdfTokenType eTokenType;
    bool gotToken = this->GetNextToken( pszToken, &eTokenType );
    
    if (!gotToken)
    {
        PODOFO_RAISE_ERROR_INFO( EPdfError::UnexpectedEOF, "Expected variant." );
    }

    // Check if we have an empty object or data
    if( strncmp( pszToken, "endobj", s_nLenEndObj ) != 0 )
    {
        this->GetNextVariant( pszToken, eTokenType, m_Variant, m_pEncrypt );
        this->SetDirty( false );

        if( !bIsTrailer )
        {
            bool gotToken = this->GetNextToken( pszToken );
            if (!gotToken)
            {
                PODOFO_RAISE_ERROR_INFO( EPdfError::UnexpectedEOF, "Expected 'endobj' or (if dict) 'stream', got EOF." );
            }
            if (strncmp(pszToken, "endobj", s_nLenEndObj) == 0)
            {
                // nothing to do, just validate that the PDF is correct
                // If it's a dictionary, it might have a stream, so check for that
            }
            else if (m_Variant.IsDictionary() && strncmp( pszToken, "stream", s_nLenStream ) == 0 )
            {
                m_bStream = true;
                m_lStreamOffset = m_device.Device()->Tell(); // NOTE: whitespace after "stream" handle in stream parser!
            }
            else
            {
                PODOFO_RAISE_ERROR_INFO( EPdfError::NoObject, pszToken );
            }
        }
    }
}


// Only called during delayed loading. Must be careful to avoid
// triggering recursive delay loading due to use of accessors of
// PdfVariant or PdfObject.
void PdfParserObject::ParseStream()
{
    PODOFO_ASSERT( DelayedLoadDone() );

    int64_t lLen = -1;
    int c;

    if( !m_device.Device() || GetDocument() == nullptr )
    {
        PODOFO_RAISE_ERROR( EPdfError::InvalidHandle );
    }

    m_device.Device()->Seek( m_lStreamOffset );

    // From the PDF Reference manual
    // The keyword stream that follows
    // the stream dictionary should be followed by an end-of-line marker consisting of
    // either a carriage return and a line feed or just a line feed, and not by a carriage re-
    // turn alone.
    c = m_device.Device()->Look();
    if( PdfTokenizer::IsWhitespace( c ) )
    {
        c = m_device.Device()->GetChar();

        if( c == '\r' )
        {
            c = m_device.Device()->Look();
            if( c == '\n' )
            {
                c = m_device.Device()->GetChar();
            }
        }
    } 
    
    std::streamoff fLoc = m_device.Device()->Tell();	// we need to save this, since loading the Length key could disturb it!

    PdfObject* pObj = this->m_Variant.GetDictionary().GetKey( PdfName::KeyLength );  
    if( pObj && pObj->IsNumber() )
    {
        lLen = pObj->GetNumber();   
    }
    else if( pObj && pObj->IsReference() )
    {
        pObj = GetDocument()->GetObjects().GetObject( pObj->GetReference() );
        if( !pObj )
        {
            PODOFO_RAISE_ERROR_INFO( EPdfError::InvalidHandle, "/Length key referenced indirect object that could not be loaded" );
        }

        if( !pObj->IsNumber() )
        {
            PODOFO_RAISE_ERROR_INFO( EPdfError::InvalidStreamLength, "/Length key for stream referenced non-number" );
        }

        lLen = pObj->GetNumber();

        // Doo not remove the length object, as 2 or more object might use
        // the same object for key lengths.
    }
    else
    {
        PODOFO_RAISE_ERROR( EPdfError::InvalidStreamLength );
    }

    m_device.Device()->Seek( fLoc );	// reset it before reading!
    PdfDeviceInputStream reader( m_device.Device() );

	if( m_pEncrypt && !m_pEncrypt->IsMetadataEncrypted() ) {
		// If metadata is not encrypted the Filter is set to "Crypt"
		PdfObject* pFilterObj = this->m_Variant.GetDictionary().GetKey( PdfName::KeyFilter );
		if( pFilterObj && pFilterObj->IsArray() ) {
			PdfArray filters = pFilterObj->GetArray();
			for(PdfArray::iterator it = filters.begin(); it != filters.end(); it++) {
				if( (*it).IsName() )
					if( (*it).GetName() == "Crypt" )
						m_pEncrypt = 0;
			}
		}
	}
    if( m_pEncrypt )
    {
        m_pEncrypt->SetCurrentReference( GetIndirectReference() );
        PdfInputStream* pInput = m_pEncrypt->CreateEncryptionInputStream( &reader );
        getOrCreateStream().SetRawData( *pInput, static_cast<ssize_t>(lLen) );
        delete pInput;
    }
    else
        getOrCreateStream().SetRawData( reader, static_cast<ssize_t>(lLen) );

    this->SetDirty( false );
    /*
    SAFE_OP( GetNextStringFromFile( ) );
    if( strncmp( m_buffer.Buffer(), "endstream", s_nLenEndStream ) != 0 )
        return ERROR_PDF_MISSING_ENDSTREAM;
    */
}

void PdfParserObject::DelayedLoadImpl()
{
    ParseFileComplete( m_bIsTrailer );
}

void PdfParserObject::DelayedLoadStreamImpl()
{
    PODOFO_ASSERT(getStream() == nullptr);

    // Note: we can't use HasStream() here because it'll call DelayedLoad()
    if (HasStreamToParse())
    {
        try
        {
            ParseStream();
        }
        catch (PdfError & e)
        {
            std::ostringstream s;
            s << "Unable to parse the stream for object " << GetIndirectReference().ObjectNumber() << ' '
                << GetIndirectReference().GenerationNumber() << " obj .";
            e.AddToCallstack(__FILE__, __LINE__, s.str().c_str());
            throw;
        }
    }
}

void PdfParserObject::FreeObjectMemory( bool bForce )
{
    if( this->IsLoadOnDemand() && (bForce || !this->IsDirty()) )
    {
        Clear();
        FreeStream();
        EnableDelayedLoading();
        EnableDelayedLoadingStream();
    }
}
