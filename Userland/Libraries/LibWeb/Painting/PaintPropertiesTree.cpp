/*
 * Copyright (c) 2023, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/TypeCasts.h>
#include <LibWeb/Painting/PaintPropertiesTree.h>
#include <LibWeb/Painting/PaintableBox.h>

namespace Web::Painting {

Optional<CSSPixelRect> PropertyTreeNode::clip_rect(Paintable const* paintable) const
{
    Optional<CSSPixelRect> clip_rect;
    CSSPixelPoint offset;
    for (auto const* node = this; node; node = node->parent()) {
        //        auto const& paintable_box = verify_cast<PaintableBox>(*node->paintable());

        if (node->m_scroll_offset == ScrollOffset::Yes) {
            //            dbgln("before cast of scroll offset");
            auto const& paintable_box = verify_cast<PaintableBox>(*node->paintable());
            if (clip_rect.has_value()) {
                clip_rect->translate_by(-paintable_box.scroll_offset());
            }
        }

        if (node->m_overflow_clip == OverflowClip::Yes) {
            //            dbgln("before cast of overflow clip");
            if (node->paintable()->is_paintable_box()) {
                auto const& paintable_box = verify_cast<PaintableBox>(*node->paintable());
                if (node->paintable() != paintable) {
                    auto overflow_clip_rect = paintable_box.absolute_padding_box_rect();
                    overflow_clip_rect.translate_by(-offset);
                    if (clip_rect.has_value()) {
                        clip_rect->intersect(overflow_clip_rect);
                    } else {
                        clip_rect = overflow_clip_rect;
                    }

                    auto border_radii_data = paintable_box.normalized_border_radii_data(
                        PaintableBox::ShrinkRadiiForBorders::Yes);
                    if (border_radii_data.has_any_radius()) {
                        //                    const_cast<PaintableBox&>(paintable_box).set_corner_clip_radii(border_radii_data);
                        //                    dbgln(">paintable box = ({}) has corner clip radii", paintable_box.layout_node().debug_description());
                    }
                }
            }
        }

        if (node->m_css_clip == CSSClip::Yes) {
            //            dbgln("before cast of css clip");
            auto const& paintable_box = verify_cast<PaintableBox>(*node->paintable());
            if (auto css_clip_rect = paintable_box.get_clip_rect(); css_clip_rect.has_value()) {
                css_clip_rect->translate_by(-offset);
                if (clip_rect.has_value())
                    clip_rect->intersect(*css_clip_rect);
                else {
                    clip_rect = *css_clip_rect;
                }
            }
        }

        if (node->m_css_transform == CSSTransform::Yes) {
            //            dbgln("before cast of css transform");
            auto const& paintable_box = verify_cast<PaintableBox>(*node->paintable());
            auto affine_transform = Gfx::extract_2d_affine_transform(paintable_box.transform());
            if (!affine_transform.is_identity()) {
                auto translation = affine_transform.translation().to_type<CSSPixels>();
                offset.translate_by(translation);
            }
        }
    }

    return clip_rect;
}

Optional<int> PropertyTreeNode::closest_scroll_frame_id(Paintable const* paintable) const
{
    for (auto const* node = this; node; node = node->parent()) {
        if (node->paintable() != paintable) {
            if (node->m_scroll_frame_id.has_value())
                return node->m_scroll_frame_id;
        }
    }
    return {};
}

Optional<CSSPixelPoint> PropertyTreeNode::scroll_offset(Paintable const* paintable) const
{
    CSSPixelPoint offset;
    for (auto const* node = this; node; node = node->parent()) {
        if (node->paintable()->layout_node().is_viewport())
            continue;
        if (node->paintable() != paintable) {
            if (node->m_scroll_offset == ScrollOffset::Yes) {
                auto const& paintable_box = verify_cast<PaintableBox>(*node->paintable());
                offset.translate_by(paintable_box.scroll_offset());
            }
        }
    }
    //    dbgln(">>> scroll offset = ({}, {})", offset.x(), offset.y());
    return -offset;
}

}
