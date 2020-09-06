#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <ncurses.h>
#include <time.h>

#define field_width 14
#define field_height 21

#define offset_y 5
#define offset_x 5

#define key_right 77
#define key_left 75
#define key_down 80

#define piece_size 4

int playing_field[field_height][field_width];

int tetromino_shapes[7][piece_size][piece_size] = {
    {
        {0, 0, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 0},
    },
    {
        {0, 2, 2, 0},
        {0, 2, 0, 0},
        {0, 2, 0, 0},
        {0, 0, 0, 0},
    },
    {
        {0, 3, 3, 0},
        {0, 0, 3, 0},
        {0, 0, 3, 0},
        {0, 0, 0, 0},
    },
    {
        {0, 0, 4, 0},
        {0, 4, 4, 0},
        {0, 0, 4, 0},
        {0, 0, 0, 0},
    },
    {
        {0, 0, 5, 0},
        {0, 5, 5, 0},
        {0, 5, 0, 0},
        {0, 0, 0, 0},
    },
    {
        {0, 6, 0, 0},
        {0, 6, 6, 0},
        {0, 0, 6, 0},
        {0, 0, 0, 0},
    },
    {
        {0, 0, 0, 0},
        {0, 7, 7, 0},
        {0, 7, 7, 0},
        {0, 0, 0, 0},
    },
};

// Tetromino definitions
struct tetromino {
    int shape[4][4];
    int rotation;
    int ypos;
    int xpos;
};
typedef struct tetromino Tetromino;
Tetromino* new_Tetromino() {
    Tetromino* t = malloc(sizeof(Tetromino));
    t->rotation = 0;
    t->ypos = 0;
    t->xpos = field_width / 2;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            t->shape[i][j] = tetromino_shapes[2][i][j];
        }
    }
}

// Returns the block (0 or 1-7) at the specified location when taking rotation into account.
int rotated_block(Tetromino* piece, int y, int x) {
    int sum;
    switch (piece->rotation % 4) {
        case 0:
            return piece->shape[y][x];
        case 1:
            sum = y + 12 - 4*x;
            return piece->shape[sum / 4][sum % 4];
        case 2:
            sum = 15 - (4*y + x);
            return piece->shape[sum / 4][sum % 4];
        case 3:
            sum = 15 - (y + 12 - 4*x);
            return piece->shape[sum / 4][sum % 4];
    }
}

void draw_console(int f[field_height][field_width], Tetromino* piece) {
    char symbols[] = " ABCDEFG#"; // The values to be drawn. For example, value 3 get converted to symbols[3] --> C

    for (int i = 0; i < field_height; i++) {
        for (int j = 0; j < field_width; j++) {
            mvaddch(offset_y + i, offset_x + j, symbols[ f[i][j] ]);
        }
    }

    int to_draw;
    for (int i = 0; i < piece_size; i++) {
        for (int j = 0; j < piece_size; j++) {
            to_draw = rotated_block(piece, i, j);
            if (to_draw != 0) mvaddch(offset_y + piece->ypos + i, offset_x + piece->xpos + j, symbols[ to_draw ]);
        }
    }
}

// Checks if the moved piece fits into the new location.
bool piece_fits(Tetromino* piece, bool rotated) {
    int ypos = piece->ypos;
    int xpos = piece->xpos;
    int shape_block; // The value of the shape array of the piece
    int field_block; // The value of the playing_field array

    // When rotated and the piece collided only with the border,
    // we allow it to rotate but move it so that it's not inside the border if we can.
    if (rotated) {
        int collided_with_border = -1; // -1: didn't collide with the border, 0: left wall, 1: floor, 2: right wall
        for (int i = ypos; i < ypos + piece_size; i++) {
            for (int j = xpos; j < xpos + piece_size; j++) {
                // Take rotation into account
                shape_block = rotated_block(piece, i - ypos, j - xpos);
                field_block = playing_field[i][j];

                if (field_block != 0 && shape_block != 0) {
                    if (field_block != 8) {
                        // collided with another block
                        return false;
                    }
                    else {
                        // collided with the border
                        if (i == field_height - 1) collided_with_border = 1;
                        else if (j == field_width - 1) collided_with_border = 2;
                        else if (j == 0) collided_with_border = 0;
                    }
                }
            }
        }

        // Try to uncollide the piece with the wall.
        // If it then collides with another block, the piece cannot be rotated this
        // way, so return false.
        switch (collided_with_border) {
            case -1:
                return true;
            case 0:
                piece->xpos++;
                if (!piece_fits(piece, false)) {
                    piece->xpos--;
                    return false;
                }
                else return true;
            case 1:
                piece->ypos--;
                if (!piece_fits(piece, false)) {
                    piece->ypos++;
                    return false;
                }
                else return true;
            case 2:
                piece->xpos--;
                if (!piece_fits(piece, false)) {
                    piece->xpos++;
                    return false;
                }
                else return true;
            default:
                return true;
        }
    }
    else {
        for (int i = ypos; i < ypos + piece_size; i++) {
            for (int j = xpos; j < xpos + piece_size; j++) {
                shape_block = rotated_block(piece, i - ypos, j - xpos);
                field_block = playing_field[i][j];

                if (field_block != 0 && shape_block != 0) {
                    // collided with another block
                    return false;
                }
            }
        }
    }
    return true;
}

// Moves the piece in the direction specified by the key parameter.
// If no key or no movement key was pressed, do nothing.
void move_if_fits(Tetromino* piece, int key) {
    // d
    if (key == 100) {
        piece->xpos++;
        if (!piece_fits(piece, false)) piece->xpos--;
    }
    // a
    else if (key == 97) {
        piece->xpos--;
        if (!piece_fits(piece, false)) piece->xpos++;
    }
    // w
    else if (key == 119) {
        piece->rotation++;
        if (!piece_fits(piece, true)) piece->rotation--;
        piece->rotation == piece->rotation % 4;
    }
    // s
    else if (key == 115) {
        piece->ypos++;
        if (!piece_fits(piece, false)) piece->ypos--;
    }
}


int main() {
    // initialize playing field
    for (int i = 0; i < field_height; i++) {
        for (int j = 0; j < field_width; j++) {
            if (i == field_height - 1 || j == 0 || j == field_width - 1) playing_field[i][j] = 8;
            else playing_field[i][j] = 0;
        }
    }

    // initialize game state values
    srand(time(0));
    bool game_over = false;
    Tetromino* crnt_piece = new_Tetromino();

    // initialize the console screen and the input source
    initscr();
    keypad(stdscr, true);
    raw();
    noecho();
    curs_set(0);
    nodelay(stdscr, true);

    // main game loop
    while (!game_over) {
        sleep(0.05);

        int c = getch();
        if (c == 'q') {
            game_over = true;
        }
        move_if_fits(crnt_piece, c);

        draw_console(playing_field, crnt_piece);
    }

    // close console screen
    endwin();

    return 0;
}