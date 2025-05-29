#ifndef __BUTTON_HPP__
#define __BUTTON_HPP__

#define DEBOUNCE_TIME 50
#define SHORT_PRESS_TRESHOLD 1000

enum button_state {
    NOP,
    PRESS,
    HOLD,
};

enum button_state handle_button_input(void);

#endif /* __BUTTON_HPP__ */