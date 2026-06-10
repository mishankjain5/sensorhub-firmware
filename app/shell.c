#include "shell.h"

void shell_init(shell_t *sh, shell_out_fn out, shell_cmd_fn handler, void *user)
{
    sh->len = 0;
    sh->out = out;
    sh->handler = handler;
    sh->user = user;
}

void shell_prompt(shell_t *sh)
{
    sh->out("> ");
}

static void run_line(shell_t *sh)
{
    char *argv[SHELL_MAX_ARGS];
    int argc = 0;
    char *p = sh->line;

    sh->line[sh->len] = '\0';

    /* Tokenize in place: terminate each word, remember where it starts. */
    while (*p != '\0' && argc < SHELL_MAX_ARGS) {
        while (*p == ' ') {
            *p++ = '\0';
        }
        if (*p == '\0') {
            break;
        }
        argv[argc++] = p;
        while (*p != '\0' && *p != ' ') {
            p++;
        }
    }

    if (argc > 0) {
        sh->handler(argc, argv, sh->user);
    }
    sh->len = 0;
}

void shell_input(shell_t *sh, char c)
{
    if (c == '\r' || c == '\n') {
        sh->out("\n");
        run_line(sh);
        shell_prompt(sh);
    } else if (c == 0x08 || c == 0x7F) {          /* backspace / delete */
        if (sh->len > 0) {
            sh->len--;
            sh->out("\b \b");                     /* wipe it off the terminal */
        }
    } else if (c >= 0x20 && c < 0x7F) {           /* printable ASCII */
        if (sh->len < SHELL_LINE_MAX - 1) {
            sh->line[sh->len++] = c;
            char echo[2] = { c, '\0' };
            sh->out(echo);
        }
        /* Line full: extra characters are ignored rather than overflowing. */
    }
    /* Other control characters are ignored. */
}
