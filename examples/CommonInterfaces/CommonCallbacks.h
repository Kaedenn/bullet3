#ifndef COMMON_CALLBACKS_H
#define COMMON_CALLBACKS_H

typedef void (*b3WheelCallback)(float deltax, float deltay);
typedef void (*b3ResizeCallback)(float width, float height);
typedef void (*b3MouseMoveCallback)(float x, float y);
typedef void (*b3MouseButtonCallback)(int button, int state, float x, float y);
typedef void (*b3KeyboardCallback)(int keycode, int state);
typedef void (*b3RenderCallback)();

enum
{
	B3G_ESCAPE = 27,
	B3G_SPACE = 32,
	B3G_F1 = 0xff00,
	B3G_F2,
	B3G_F3,
	B3G_F4,
	B3G_F5,
	B3G_F6,
	B3G_F7,
	B3G_F8,
	B3G_F9,
	B3G_F10,
	B3G_F11,
	B3G_F12,
	B3G_F13,
	B3G_F14,
	B3G_F15,
	B3G_LEFT_ARROW,
	B3G_RIGHT_ARROW,
	B3G_UP_ARROW,
	B3G_DOWN_ARROW,
	B3G_PAGE_UP,
	B3G_PAGE_DOWN,
	B3G_END,
	B3G_HOME,
	B3G_INSERT,
	B3G_DELETE,
	B3G_BACKSPACE,
	B3G_SHIFT,
	B3G_CONTROL,
	B3G_ALT,
	B3G_RETURN,
  /* XXX: Kaedenn: Added the following constants */
  B3G_LEFT = B3G_LEFT_ARROW,
  B3G_RIGHT = B3G_RIGHT_ARROW,
  B3G_UP = B3G_UP_ARROW,
  B3G_DOWN = B3G_DOWN_ARROW,
  B3G_KP_HOME = 0xff95,
  B3G_KP_LEFT = 0xff96,
  B3G_KP_UP = 0xff97,
  B3G_KP_RIGHT = 0xff98,
  B3G_KP_DOWN = 0xff99,
  B3G_KP_PGUP = 0xff9a,
  B3G_KP_PGDN = 0xff9b,
  B3G_KP_END = 0xff9c,
  B3G_KP_ENTER = 0xff9d,
  B3G_KP_INS = 0xff9e,
  B3G_KP_DEL = 0xff9f,
  B3G_KP_0 = B3G_KP_INS,
  B3G_KP_1 = B3G_KP_END,
  B3G_KP_2 = B3G_KP_DOWN,
  B3G_KP_3 = B3G_KP_PGDN,
  B3G_KP_4 = B3G_KP_LEFT,
  B3G_KP_5 = 0xff9d,
  B3G_KP_6 = B3G_KP_RIGHT,
  B3G_KP_7 = B3G_KP_HOME,
  B3G_KP_8 = B3G_KP_UP,
  B3G_KP_9 = B3G_KP_PGUP,
  B3G_NUMLOCK = 0xff7f
};

#endif
