/*
 * Copyright (c) 2024, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/UIEvents/InputEvent.h>

namespace Web::UIEvents {

JS_DEFINE_ALLOCATOR(InputEvent);

WebIDL::ExceptionOr<JS::NonnullGCPtr<InputEvent>> InputEvent::construct_impl(JS::Realm& realm, FlyString const& event_name, InputEventInit const& event_init)
{
    return realm.heap().allocate<InputEvent>(realm, realm, event_name, event_init);
}

WebIDL::ExceptionOr<JS::NonnullGCPtr<InputEvent>> InputEvent::create(JS::Realm& realm, FlyString const& event_name, InputEventInit const& event_init)
{
    return realm.heap().allocate<InputEvent>(realm, realm, event_name, event_init);
}

InputEvent::InputEvent(JS::Realm& realm, FlyString const& event_name, InputEventInit const& event_init)
    : UIEvent(realm, event_name)
    , m_data(event_init.data)
    , m_is_composing(event_init.is_composing)
    , m_input_type(event_init.input_type)
{
}

InputEvent::~InputEvent() = default;

void InputEvent::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    set_prototype(&Bindings::ensure_web_prototype<Bindings::InputEventPrototype>(realm, "InputEvent"_fly_string));
}

}
