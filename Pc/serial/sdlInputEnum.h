#ifndef ILL_SDL_INPUT_ENUM_H__
#define ILL_SDL_INPUT_ENUM_H__

#include <cstdint>
#include <glm/glm.hpp>

#include "Input/serial/inputEnum.h"

/**
Includes an enum of the keycodes
*/
#include <SDL_keycode.h>

namespace SdlPc {
    
/**
A button index on a mouse or joystick
*/
typedef uint8_t Button;

/**
The values that go into the InputBinding device type.

SDL isn't very flexible at detecting multiple mice and keyboards,
so these are the hardcoded enums for now.

Normally I'd try to detect multiple devices and let them be bound per player.
*/
enum class InputType {
    INVALID,
    PC_KEYBOARD_TYPE,       ///<typing on the keyboard, so send all characters, modifiers, arrow keys, etc...
    PC_KEYBOARD,            ///<keys on the keyboard
    PC_MOUSE_BUTTON,        ///<buttons on the mouse
    PC_MOUSE_WHEEL,         ///<wheels on the mouse
    PC_MOUSE                ///<movement of the mouse
};

//TODO: figure out a place to keep all of the special input types
typedef glm::ivec2 MousePosition;

}

#endif
