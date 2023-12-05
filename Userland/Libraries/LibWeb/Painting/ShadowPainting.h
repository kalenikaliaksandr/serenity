/*
 * Copyright (c) 2021-2022, Sam Atkins <atkinssj@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGfx/Color.h>
#include <LibWeb/Forward.h>
#include <LibWeb/Painting/PaintContext.h>
#include <LibWeb/Painting/PaintOuterBoxShadowParams.h>
#include <LibWeb/Painting/ShadowData.h>

namespace Web::Painting {

void paint_inner_box_shadow(Gfx::Painter&, PaintOuterBoxShadowParams params);

Gfx::IntRect get_outer_box_shadow_bounding_rect(PaintOuterBoxShadowParams params);
void paint_outer_box_shadow(Gfx::Painter& painter, PaintOuterBoxShadowParams params);

void paint_box_shadow(
    PaintContext&,
    CSSPixelRect const& bordered_content_rect,
    CSSPixelRect const& borderless_content_rect,
    BordersData const& borders_data,
    BorderRadiiData const&,
    Vector<ShadowData> const&);
void paint_text_shadow(PaintContext&, Layout::LineBoxFragment const&, Vector<ShadowData> const&);

struct OuterBoxShadowMetrics {
    DevicePixelRect shadow_bitmap_rect;
    DevicePixelRect non_blurred_shadow_rect;
    DevicePixelRect inner_bounding_rect;
    DevicePixels blurred_edge_thickness;
    DevicePixels double_radius;
    DevicePixels blur_radius;

    DevicePixelRect top_left_corner_rect;
    DevicePixelRect top_right_corner_rect;
    DevicePixelRect bottom_right_corner_rect;
    DevicePixelRect bottom_left_corner_rect;

    DevicePixelPoint top_left_corner_blit_pos;
    DevicePixelPoint top_right_corner_blit_pos;
    DevicePixelPoint bottom_right_corner_blit_pos;
    DevicePixelPoint bottom_left_corner_blit_pos;

    DevicePixelSize top_left_corner_size;
    DevicePixelSize top_right_corner_size;
    DevicePixelSize bottom_right_corner_size;
    DevicePixelSize bottom_left_corner_size;

    DevicePixels left_start;
    DevicePixels top_start;
    DevicePixels right_start;
    DevicePixels bottom_start;

    DevicePixelRect left_edge_rect;
    DevicePixelRect right_edge_rect;
    DevicePixelRect top_edge_rect;
    DevicePixelRect bottom_edge_rect;

    CornerRadius top_left_shadow_corner;
    CornerRadius top_right_shadow_corner;
    CornerRadius bottom_right_shadow_corner;
    CornerRadius bottom_left_shadow_corner;
};

OuterBoxShadowMetrics get_outer_box_shadow_configuration(PaintOuterBoxShadowParams params);

}
