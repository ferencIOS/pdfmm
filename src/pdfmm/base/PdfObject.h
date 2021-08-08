/**
 * Copyright (C) 2005 by Dominik Seichter <domseichter@web.de>
 * Copyright (C) 2020 by Francesco Pretto <ceztko@gmail.com>
 *
 * Licensed under GNU Library General Public License 2.0 or later.
 * Some rights reserved. See COPYING, AUTHORS.
 */

#ifndef PDF_OBJECT_H
#define PDF_OBJECT_H

#include "PdfName.h"
#include "PdfReference.h"
#include "PdfString.h"
#include "PdfVariant.h"
#include "PdfStream.h"

namespace mm {

class PdfEncrypt;
class PdfObject;
class PdfOutputDevice;
class PdfIndirectObjectList;
class PdfDictionary;
class PdfArray;
class PdfDocument;
class PdfDataContainer;

/**
 * This class represents a PDF indirect Object in memory
 *
 * It is possible to manipulate the stream which can be appended to the object
 * (if the object is of underlying type dictionary).  A PdfObject is uniquely
 * identified by an object number and a generation number which has to be
 * passed to the constructor.
 *
 * The object can be written to a file easily using the Write() function.
 *
 * \see Write()
 */
class PDFMM_API PdfObject
{
    friend class PdfIndirectObjectList;
    friend class PdfArray;
    friend class PdfDictionary;
    friend class PdfDocument;
    friend class PdfStream;
    friend class PdfDataContainer;
    friend class PdfObjectStreamParser;

public:

    /** Create a PDF object with object and generation number -1
     *  and the value of being an empty PdfDictionary.
     */
    PdfObject();

    virtual ~PdfObject();

    /** Create a PDF object with object and generation number -1
     *  and the value of the passed variant.
     *
     *  \param var the value of the object
     */
    PdfObject(const PdfVariant& var);

    /** Construct a PdfObject with object and generation number -1
     *  and a bool as value.
     *
     *  \param b the boolean value of this PdfObject
     */
    PdfObject(bool b);

    /** Construct a PdfObject with object and generation number -1
     *  and a int64_t as value.
     *
     *  \param l the int64_t value of this PdfObject
     */
    PdfObject(int64_t l);

    /** Construct a PdfObject with object and generation number -1
     *  and a double as value.
     *
     *  \param d the double value of this PdfObject
     */
    PdfObject(double d);

    /** Construct a PdfObject with object and generation number -1
     *  and a PdfString as value.
     *
     *  \param str the string value of this PdfObject
     */
    PdfObject(const PdfString& str);

    /** Construct a PdfObject with object and generation number -1
     *  and a PdfName as value.
     *
     *  \param name the value of this PdfObject
     */
    PdfObject(const PdfName& name);

    /** Construct a PdfObject with object and generation number -1
     *  and a PdfReference as value.
     *
     *  \param ref the value of the this PdfObject
     */
    PdfObject(const PdfReference& ref);

    /** Construct a PdfObject with object and generation number -1
     *  and a PdfArray as value.
     *
     *  \param arr the value of the this PdfObject
     */
    PdfObject(const PdfArray& arr);

    /** Construct a PdfObject with object and generation number -1
     *  and a PdfDictionary as value.
     *
     *  \param dict the value of the this PdfObject
     */
    PdfObject(const PdfDictionary& dict);

    /** Creates a copy of an existing PdfObject.
     *  All associated objects and streams will be copied along with the PdfObject.
     *  \param rhs PdfObject to clone
     */
    PdfObject(const PdfObject& rhs);

public:
    /** Clear all internal member variables and free the memory
     *  they have allocated.
     *  Sets the datatype to EPdfDataType::Null
     *
     *  This will reset the dirty flag of this object to be clean.
     *  \see IsDirty
     */
    void Clear();

    /** \returns the datatype of this object or EPdfDataType::Unknown
     *  if it does not have a value.
     */
    PdfDataType GetDataType() const;

    /** \returns a human readable string representation of GetDataType()
     *  The returned string must not be free'd.
     */
    const char* GetDataTypeString() const;

    /** \returns true if this variant is a bool
     */
    bool IsBool() const;

    /** \returns true if this variant is a number
     */
    bool IsNumber() const;

    /** \returns true if this variant is a real
     *
     *  This method strictly check for a floating point number and return false on integer
     */
    bool IsRealStrict() const;

    /** \returns true if this variant is an integer or a floating point number
     */
    bool IsNumberOrReal() const;

    /** \returns true if this variant is a string
     */
    bool IsString() const;

    /** \returns true if this variant is a name
     */
    bool IsName() const;

    /** \returns true if this variant is an array
     */
    bool IsArray() const;

    /** \returns true if this variant is a dictionary
     */
    bool IsDictionary() const;

    /** \returns true if this variant is raw data
     */
    bool IsRawData() const;

    /** \returns true if this variant is null
     */
    bool IsNull() const;

    /** \returns true if this variant is a reference
     */
    bool IsReference() const;

    /** Converts the current object into a string representation
     *  which can be written directly to a PDF file on disc.
     *  \param data the object string is returned in this object.
     *  \param writeMode additional options for writing to a string
     */
    void ToString(std::string& data, PdfWriteMode writeMode = PdfWriteMode::Clean) const;

    /** Get the value if this object is a bool.
     *  \returns the bool value.
     */
    bool GetBool() const;
    bool TryGetBool(bool& value) const;

    /** Get the value of the object as int64_t.
     *
     *  This method is lenient and narrows floating point numbers
     *  \return the value of the number
     */
    int64_t GetNumberLenient() const;
    bool TryGetNumberLenient(int64_t& value) const;

    /** Get the value of the object as int64_t
     *
     *  This method throws if the numer is a floating point number
     *  \return the value of the number
     */
    int64_t GetNumber() const;
    bool TryGetNumber(int64_t& value) const;

    /** Get the value of the object as a floating point
     *
     *  This method is lenient and returns also strictly integral numbers
     *  \return the value of the number
     */
    double GetReal() const;
    bool TryGetReal(double& value) const;

    /** Get the value of the object as floating point number
     *
     *  This method throws if the numer is integer
     *  \return the value of the number
     */
    double GetRealStrict() const;
    bool TryGetRealStrict(double& value) const;

    /** \returns the value of the object as string.
     */
    const PdfString& GetString() const;
    bool TryGetString(PdfString& str) const;

    /** \returns the value of the object as name
     */
    const PdfName& GetName() const;
    bool TryGetName(PdfName& str) const;

    /** Get the reference values of this object.
     *  \returns a PdfReference
     */
    PdfReference GetReference() const;
    bool TryGetReference(PdfReference& ref) const;

    /** Returns the value of the object as array
     *  \returns a array
     */
    const PdfArray& GetArray() const;
    PdfArray& GetArray();
    bool TryGetArray(const PdfArray*& arr) const;
    bool TryGetArray(PdfArray*& arr);

    /** Returns the dictionary value of this object
     *  \returns a PdfDictionary
     */
    const PdfDictionary& GetDictionary() const;
    PdfDictionary& GetDictionary();
    bool TryGetDictionary(const PdfDictionary*& dict) const;
    bool TryGetDictionary(PdfDictionary*& dict);

    /** Set the value of this object as bool
     *  \param b the value as bool.
     *
     *  This will set the dirty flag of this object.
     *  \see IsDirty
     */
    void SetBool(bool b);

    /** Set the value of this object as int64_t
     *  \param l the value as int64_t.
     *
     *  This will set the dirty flag of this object.
     *  \see IsDirty
     */
    void SetNumber(int64_t l);

    /** Set the value of this object as double
     *  \param d the value as double.
     *
     *  This will set the dirty flag of this object.
     *  \see IsDirty
     */
    void SetReal(double d);

    /** Set the name value of this object
    *  \param d the name value
    *
    *  This will set the dirty flag of this object.
    *  \see IsDirty
    */
    void SetName(const PdfName& name);

    /** Set the string value of this object.
     * \param str the string value
     *
     * This will set the dirty flag of this object.
     * \see IsDirty
     */
    void SetString(const PdfString& str);

    void SetReference(const PdfReference& ref);

    void ForceCreateStream();

    /** Write the complete object to a file.
     *  \param device write the object to this device
     *  \param encrypt an encryption object which is used to encrypt this object
     *                  or nullptr to not encrypt this object
     *  \param writeMode additional options for writing the object
     *  \param keyStop if not KeyNull and a key == keyStop is found
     *                 writing will stop right before this key!
     */
    void Write(PdfOutputDevice& device, PdfWriteMode writeMode, PdfEncrypt* encrypt) const;

    /** Get the length of the object in bytes if it was written to disk now.
     *  \param writeMode additional options for writing the object
     *  \returns  the length of the object
     */
    size_t GetObjectLength(PdfWriteMode writeMode);

    /** Get a handle to a PDF stream object.
     *  If the PDF object does not have a stream,
     *  one will be created.
     *  \returns a PdfStream object
     */
    PdfStream& GetOrCreateStream();

    /** Get a handle to a const PDF stream object.
     * Throws if there's no stream
     */
    const PdfStream& GetStream() const;

    /** Get a handle to a const PDF stream object.
     * Throws if there's no stream
     */
    PdfStream& GetStream();

    bool TryGetStream(PdfStream*& stream);
    bool TryGetStream(const PdfStream*& stream) const;

    /** Check if this object has a PdfStream object
     *  appended.
     *
     *  \returns true if the object has a stream
     */
    bool HasStream() const;

    bool IsIndirect() const;

    /**
     * Sets this object to immutable,
     * so that no keys can be edited or changed.
     *
     * \param isImmutable if true set the object to be immutable
     *
     * This is used by PdfImmediateWriter and PdfStreamedDocument so
     * that no keys can be added to an object after setting stream data on it.
     *
     */
    void SetImmutable(bool isImmutable);

    const PdfVariant& GetVariant() const;

public:
    /** This operator is required for sorting a list of
     *  PdfObject instances. It compares the object number. If object numbers
     *  are equal, the generation number is compared.
     */
    bool operator<(const PdfObject& rhs) const;

    /** The equality operator with PdfObject checks for parent document and
     * indirect reference first
     */
    bool operator==(const PdfObject& rhs) const;

    /** The disequality operator with PdfObject checks for parent document and
     * indirect reference first
     */
    bool operator!=(const PdfObject& rhs) const;

    /** The equality operator with PdfVariant checks equality with variant object only
     */
    bool operator==(const PdfVariant& rhs) const;

    /** The disequality operator with PdfVariant checks disequality with variant object only
     */
    bool operator!=(const PdfVariant& rhs) const;

    /** Copy an existing PdfObject.
     *  All associated objects and streams will be copied along with the PdfObject.
     *  \param rhs PdfObject to clone
     *  \returns a reference to this object
     */
    const PdfObject& operator=(const PdfObject& rhs);

    operator const PdfVariant& () const;

public:
    /** The dirty flag is set if this variant
     *  has been modified after construction.
     *
     *  Usually the dirty flag is also set
     *  if you call any non-const member function
     *  (e.g. GetDictionary()) as PdfVariant cannot
     *  determine if you actually changed the dictionary
     *  or not.
     *
     *  \returns true if the value is dirty and has been
     *                modified since construction
     */
    inline bool IsDirty() const { return m_IsDirty; }

    /** Get the document of this object.
     *  \return the owner (if it wasn't changed anywhere, creator) of this object
     */
    inline PdfDocument* GetDocument() const { return m_Document; }

    /** Get an indirect reference to this object.
     *  \returns a PdfReference pointing to this object.
     */
    inline const PdfReference& GetIndirectReference() const { return m_IndirectReference; }

    inline const PdfDataContainer* GetParent() const { return m_Parent; }

    /**
     * Retrieve if an object is immutable.
     *
     * This is used by PdfImmediateWriter and PdfStreamedDocument so
     * that no keys can be added to an object after setting stream data on it.
     *
     * \returns true if the object is immutable
     */
    inline bool IsImmutable() const { return m_IsImmutable; };


    /** Flag the object  incompletely loaded.  DelayedLoad() will be called
     *  when any method that requires more information than is currently
     *  available is loaded.
     *
     *  All constructors initialize a PdfVariant with delayed loading disabled .
     *  If you want delayed loading you must ask for it. If you do so, call
     *  this method early in your ctor and be sure to override DelayedLoadImpl().
     */
    inline void EnableDelayedLoading() { m_DelayedLoadDone = false; }

    /**
     * Returns true if delayed loading is disabled, or if it is enabled
     * and loading has completed. External callers should never need to
     * see this, it's an internal state flag only.
     */
    inline bool DelayedLoadDone() const { return m_DelayedLoadDone; }

protected:
    PdfObject(const PdfVariant& var, bool isDirty);

    /**
     * Dynamically load the contents of this object from a PDF file by calling
     * the virtual method DelayedLoadImpl() if the object is not already loaded.
     *
     * For objects complete created in memory and those that do not support
     * deferred loading this function does nothing, since deferred loading
     * will not be enabled.
     */
    void DelayedLoad() const;

    /** Load all data of the object if delayed loading is enabled.
     *
     * Never call this method directly; use DelayedLoad() instead.
     *
     * You should override this to control deferred loading in your subclass.
     * Note that this method should not load any associated streams, just the
     * base object.
     *
     * The default implementation throws. It should never be called, since
     * objects that do not support delayed loading should not enable it.
     *
     * While this method is not `const' it may be called from a const context,
     * so be careful what you mess with.
     */
    virtual void DelayedLoadImpl();

    virtual void DelayedLoadStreamImpl();

    /**
     *  Will throw an exception if called on an immutable object,
     *  so this should be called before actually changing a value!
     *
     */
    void AssertMutable() const;

    /** Sets the dirty flag of this PdfVariant
     *
     *  \see IsDirty
     */
    void SetDirty();

    void resetDirty();

    /** Set the owner of this object, i.e. the PdfIndirectObjectList to which
     *  this object belongs.
     *
     *  \param objects a vector of pdf objects
     */
    void SetDocument(PdfDocument& document);

    void SetVariantOwner();

    void FreeStream();

    PdfStream& getOrCreateStream();

    void forceCreateStream();

    PdfStream* getStream();

    void DelayedLoadStream() const;

    void delayedLoadStream() const;

    void EnableDelayedLoadingStream();

    inline void SetIndirectReference(const PdfReference& reference) { m_IndirectReference = reference; }

private:
    // Assign function that doesn't set dirty
    void Assign(const PdfObject& rhs);

    void assign(const PdfObject& rhs);

    void ResetDirty();

    void setDirty();

    // See PdfVariant.h for a detailed explanation of this member, which is
    // here to prevent accidental construction of a PdfObject of integer type
    // when passing a pointer. */
    template<typename T>
    PdfObject(T*) = delete;

    void copyFrom(const PdfObject& obj);

    // Shared initialization between all the ctors
    void InitPdfObject();

    inline void SetParent(PdfDataContainer* parent) { m_Parent = parent; }

protected:
    PdfVariant m_Variant;

private:
    PdfReference m_IndirectReference;
    PdfDocument* m_Document;
    PdfDataContainer* m_Parent;
    bool m_IsDirty; // Indicates if this object was modified after construction
    bool m_IsImmutable; // Indicates if this object may be modified

    mutable bool m_DelayedLoadDone;
    mutable bool m_DelayedLoadStreamDone;
    std::unique_ptr<PdfStream> m_Stream;
    // Tracks whether deferred loading is still pending (in which case it'll be
    // false). If true, deferred loading is not required or has been completed.
};

};

#endif // PDF_OBJECT_H
