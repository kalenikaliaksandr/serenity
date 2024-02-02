/*
 * Copyright (c) 2023, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/NonnullRefPtr.h>
#include <AK/RefCounted.h>
#include <AK/Vector.h>
#include <LibWeb/Forward.h>
#include <LibWeb/PixelUnits.h>

namespace Web::Painting {

class PropertyTreeNode : public RefCounted<PropertyTreeNode> {
public:
    enum class OverflowClip {
        Yes,
        No
    };
    enum class CSSClip {
        Yes,
        No
    };
    enum class CSSTransform {
        Yes,
        No
    };
    enum class ScrollOffset {
        Yes,
        No
    };

    static NonnullRefPtr<PropertyTreeNode> create(PropertyTreeNode* parent, Paintable const* paintable, OverflowClip overflow_clip, CSSClip css_clip, CSSTransform css_transform, ScrollOffset scroll_offset)
    {
        return adopt_ref(*new PropertyTreeNode(move(parent), paintable, overflow_clip, css_clip, css_transform, scroll_offset));
    }

    PropertyTreeNode(PropertyTreeNode* parent, Paintable const* paintable, OverflowClip overflow_clip, CSSClip css_clip, CSSTransform css_transform, ScrollOffset scroll_offset)
        : m_parent(parent)
        , m_paintable(paintable)
        , m_overflow_clip(overflow_clip)
        , m_css_clip(css_clip)
        , m_css_transform(css_transform)
        , m_scroll_offset(scroll_offset)
    {
        VERIFY(m_parent != this);
        if (m_parent)
            m_parent->m_children.append(*this);
    }

    PropertyTreeNode const* parent() const { return m_parent; }
    Paintable const* paintable() const { return m_paintable; }

    Optional<CSSPixelRect> clip_rect(Paintable const* paintable) const;

    Optional<int> m_scroll_frame_id;
    Optional<int> closest_scroll_frame_id(Paintable const* paintable) const;
    Optional<CSSPixelPoint> scroll_offset(Paintable const* paintable) const;

private:
    PropertyTreeNode* m_parent { nullptr };
    Paintable const* m_paintable { nullptr };
    Vector<NonnullRefPtr<PropertyTreeNode>> m_children;

    OverflowClip m_overflow_clip { OverflowClip::No };
    CSSClip m_css_clip { CSSClip::No };
    CSSTransform m_css_transform { CSSTransform::No };
    ScrollOffset m_scroll_offset { ScrollOffset::No };
};

}
