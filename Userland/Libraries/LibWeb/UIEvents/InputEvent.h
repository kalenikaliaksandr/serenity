/*
 * Copyright (c) 2024, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/FlyString.h>
#include <LibWeb/UIEvents/UIEvent.h>

namespace Web::UIEvents {

struct InputEventInit : public UIEventInit {
    Optional<String> data;
    bool is_composing { false };
    FlyString input_type;
};

class InputEvent final : public UIEvent {
    WEB_PLATFORM_OBJECT(InputEvent, UIEvent);
    JS_DECLARE_ALLOCATOR(InputEvent);

public:
    static WebIDL::ExceptionOr<JS::NonnullGCPtr<InputEvent>> construct_impl(JS::Realm&, FlyString const& event_name, InputEventInit const& event_init);
    static WebIDL::ExceptionOr<JS::NonnullGCPtr<InputEvent>> create(JS::Realm&, FlyString const& event_name, InputEventInit const&);

    virtual ~InputEvent() override;

    Optional<String> data() const { return m_data; }
    bool is_composing() const { return m_is_composing; }
    FlyString input_type() const { return m_input_type; }

private:
    InputEvent(JS::Realm&, FlyString const& event_name, InputEventInit const&);

    virtual void initialize(JS::Realm&) override;

    Optional<String> m_data;
    bool m_is_composing;
    FlyString m_input_type;
};

}
