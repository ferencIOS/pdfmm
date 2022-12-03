/*
 * SPDX-FileCopyrightText: (C) 2022 Francesco Pretto <ceztko@gmail.com>
 * SPDX-License-Identifier: LGPL-2.1
 */

#include <pdfmm/private/PdfDeclarationsPrivate.h>
#include "PdfAnnotationCollection.h"
#include "PdfPage.h"

using namespace std;
using namespace mm;

PdfAnnotationCollection::PdfAnnotationCollection(PdfPage& page)
    : m_Page(&page), m_annotArray(nullptr)
{
}

PdfAnnotation& PdfAnnotationCollection::CreateAnnot(PdfAnnotationType annotType, const PdfRect& rect)
{
    return addAnnotation(PdfAnnotation::Create(*m_Page, annotType, rect));
}

PdfAnnotation& PdfAnnotationCollection::GetAnnotAt(unsigned index)
{
    return getAt(index);
}

const PdfAnnotation& PdfAnnotationCollection::GetAnnotAt(unsigned index) const
{
    return getAt(index);
}

void PdfAnnotationCollection::RemoveAnnotAt(unsigned index)
{
    initAnnotations();
    if (index >= m_Annots.size())
        PDFMM_RAISE_ERROR(PdfErrorCode::ValueOutOfRange);

    if (m_Annots[index] != nullptr)
    {
        // It may be null if the annotation is invalid
        m_annotMap->erase(m_annotMap->find(m_Annots[index]->GetObject().GetIndirectReference()));
    }

    m_annotArray->RemoveAt(index);
    m_Annots.erase(m_Annots.begin() + index);
    fixIndices(index);

    // NOTE: No need to remove the object from the document
    // indirect object list: it will be garbage collected
}

void PdfAnnotationCollection::RemoveAnnot(const PdfReference& ref)
{
    initAnnotations();
    auto found = m_annotMap->find(ref);
    if (found == m_annotMap->end())
        return;

    unsigned index = found->second;
    m_Annots.erase(m_Annots.begin() + index);
    m_annotArray->RemoveAt(index);
    m_annotMap->erase(found);
    fixIndices(index);

    // NOTE: No need to remove the object from the document
    // indirect object list: it will be garbage collected
}

unsigned PdfAnnotationCollection::GetCount() const
{
    const_cast<PdfAnnotationCollection&>(*this).initAnnotations();
    return (unsigned)m_Annots.size();
}

PdfAnnotationCollection::iterator PdfAnnotationCollection::begin()
{
    return m_Annots.begin();
}

PdfAnnotationCollection::iterator PdfAnnotationCollection::end()
{
    return m_Annots.end();
}

PdfAnnotationCollection::const_iterator PdfAnnotationCollection::begin() const
{
    return m_Annots.begin();
}

PdfAnnotationCollection::const_iterator PdfAnnotationCollection::end() const
{
    return m_Annots.end();
}

PdfAnnotation& PdfAnnotationCollection::createAnnotation(const type_info& typeInfo, const PdfRect& rect)
{
    return addAnnotation(PdfAnnotation::Create(*m_Page, typeInfo, rect));
}

PdfAnnotation& PdfAnnotationCollection::addAnnotation(unique_ptr<PdfAnnotation>&& annot)
{
    initAnnotations();
    if (m_annotArray == nullptr)
        m_annotArray = &m_Page->GetDictionary().AddKey("Annots", PdfArray()).GetArray();

    (*m_annotMap)[annot->GetObject().GetIndirectReference()] = m_annotArray->GetSize();
    m_annotArray->AddIndirectSafe(annot->GetObject());
    auto ret = annot.get();
    m_Annots.push_back(std::move(annot));
    return *ret;
}

PdfArray* PdfAnnotationCollection::getAnnotationsArray() const
{
    auto obj = const_cast<PdfAnnotationCollection&>(*this).m_Page->GetDictionary().FindKey("Annots");
    if (obj == nullptr)
        return nullptr;

    return &obj->GetArray();
}

PdfAnnotation& PdfAnnotationCollection::getAt(unsigned index) const
{
    const_cast<PdfAnnotationCollection&>(*this).initAnnotations();
    if (index >= m_Annots.size())
        PDFMM_RAISE_ERROR(PdfErrorCode::ValueOutOfRange);

    return *m_Annots[index];
}

void PdfAnnotationCollection::initAnnotations()
{
    if (m_annotMap != nullptr)
        return;

    m_annotMap.reset(new AnnotationMap());
    m_annotArray = getAnnotationsArray();
    if (m_annotArray == nullptr)
        return;

    m_Annots.reserve(m_annotArray->size());
    unique_ptr<PdfAnnotation> annot;
    unsigned i = 0;
    for (auto obj : m_annotArray->GetIndirectIterator())
    {
        (*m_annotMap)[obj->GetIndirectReference()] = i;
        // The annotation may be invalid. In that case we push a placeholder
        if (PdfAnnotation::TryCreateFromObject(*obj, annot))
        {
            annot->SetPage(*m_Page);
            m_Annots.push_back(std::move(annot));
        }
        else
        {
            m_Annots.push_back(nullptr);
        }

        i++;
    }
}

void PdfAnnotationCollection::fixIndices(unsigned index)
{
    for (auto& pair : *m_annotMap)
    {
        // Decrement indices where needed
        if (pair.second > index)
            pair.second--;
    }
}
