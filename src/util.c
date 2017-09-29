#include "reef.h"

#include <sys/select.h>
#include <termios.h>

struct termios orig_termios;

static void reset_terminal_mode()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
    struct termios new_termios;

    tcgetattr(STDIN_FILENO, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    atexit(reset_terminal_mode);

    new_termios.c_lflag &= ~( ICANON | ECHO );
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
}

int kbdhit()
{
    struct timeval tv = {1L, 0L};
    fd_set rdfs;

    FD_ZERO(&rdfs);
    FD_SET (STDIN_FILENO, &rdfs);

    select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
    return (FD_ISSET(STDIN_FILENO, &rdfs));
}
