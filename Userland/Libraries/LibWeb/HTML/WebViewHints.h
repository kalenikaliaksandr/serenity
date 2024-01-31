/*
 * Copyright (c) 2023, Andrew Kaster <akaster@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Types.h>
#include <LibIPC/Forward.h>

namespace Web::HTML {

struct WebViewHints {
    bool popup = false;
    Optional<i32> width = {};
    Optional<i32> height = {};
    Optional<i32> screen_x = {};
    Optional<i32> screen_y = {};
};

}

namespace IPC {

template<>
ErrorOr<void> encode(Encoder&, Web::HTML::WebViewHints const&);

template<>
ErrorOr<Web::HTML::WebViewHints> decode(Decoder&);

}
