set(SOURCES
    Canvas.cpp
    Painter.cpp
)

serenity_lib(LibAccelGfx accelgfx)
target_link_libraries(LibAccelGfx PRIVATE LibGfx GL EGL)
