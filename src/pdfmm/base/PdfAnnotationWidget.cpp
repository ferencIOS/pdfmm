/*
 * SPDX-FileCopyrightText: (C) 2022 Francesco Pretto <ceztko@gmail.com>
 * SPDX-License-Identifier: LGPL-2.1
 */

#include <pdfmm/private/PdfDeclarationsPrivate.h>
#include "PdfAnnotationWidget.h"
#include "PdfPage.h"

using namespace std;
using namespace mm;

PdfAnnotationWidget::PdfAnnotationWidget(PdfPage& page, const PdfRect& rect)
    : PdfAnnotationActionBase(page, PdfAnnotationType::Widget, rect)
{
}

PdfAnnotationWidget::PdfAnnotationWidget(PdfObject& obj)
    : PdfAnnotationActionBase(obj, PdfAnnotationType::Widget)
{
}

void PdfAnnotationWidget::SetField(const shared_ptr<PdfField>& field)
{
    m_Field = field;
}

const PdfField& PdfAnnotationWidget::GetField() const
{
    const_cast<PdfAnnotationWidget&>(*this).initField();
    return *m_Field;
}

PdfField& PdfAnnotationWidget::GetField()
{
    initField();
    return *m_Field;
}

void PdfAnnotationWidget::initField()
{
    if (m_Field != nullptr)
        return;

    unique_ptr<PdfField> field;
    if (!PdfField::TryCreateFromObject(GetObject(), field))
        PDFMM_RAISE_ERROR_INFO(PdfErrorCode::InvalidHandle, "Invalid field");

    field->SetWidget(*this);
    m_Field = std::move(field);
}
