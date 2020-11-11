#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <ncurses.h>
#include <time.h>

#define field_width 14
#define field_height 21

#define offset_y 5 // Amount of space away from the console borders
#define offset_x 5 // Amount of space away from the console borders

#define key_right 77
#define key_left 75
#define key_down 80

#define piece_size_big 4
#define piece_size_small 3

u_int8_t playing_field[field_height][field_width];

u_int8_t tetromino3_shapes[5][piece_size_small][piece_size_small] = {
    {
        {1,0,0},
        {1,1,1},
        {0,0,0},
    },
    {
        {0,0,1},
        {1,1,1},
        {0,0,0},
    },
    {
        {0,1,0},
        {1,1,1},
        {0,0,0},
    },
    {
        {1,1,0},
        {0,1,1},
        {0,0,0},
    },
    {
        {0,7,7},
        {7,7,0},
        {0,0,0},
    },
};
u_int8_t tetromino4_shapes[2][piece_size_big][piece_size_big] = {
    {
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {1, 1, 1, 1},
        {0, 0, 0, 0},
    },
    {
        {0, 0, 0, 0},
        {0, 1, 1, 0},
        {0, 1, 1, 0},
        {0, 0, 0, 0},
    },
};

// Tetromino definitions
struct tetromino {
    u_int8_t shape4[4][4];
    u_int8_t shape3[3][3];
    signed char size;
    u_int8_t rotation;
    signed char ypos;
    signed char xpos;
};
typedef struct tetromino Tetromino;

// Tetromino constructor
Tetromino* new_Tetromino() {
    Tetromino* t = malloc(sizeof(Tetromino));
    u_int8_t shape_num = rand() % 7 + 1;
    t->size = (shape_num == 1 || shape_num == 2) ? piece_size_big : piece_size_small;
    t->rotation = 0;
    t->xpos = field_width / 2 - 2;

    // fill shape
    if (t->size == 3) {
        for (signed char i = 0; i < 3; i++) {
            for (signed char j = 0; j < 3; j++) {
                t->shape3[i][j] = tetromino3_shapes[shape_num - 3][i][j];
            }
        }
    }
    else {
        for (signed char i = 0; i < 4; i++) {
            for (signed char j = 0; j < 4; j++) {
                t->shape4[i][j] = tetromino4_shapes[shape_num - 1][i][j];
            }
        }
    }

    // determine ypos so that the piece spawn completely visible at the top
    switch (shape_num)
    {
    case 1:
        t->ypos = -2;
        break;
    case 2:
        t->ypos = -1;
        break;
    default:
        t->ypos = 0;
        break;
    }
    return t;
}

// Returns the block (0 or 1-7) at the specified location when taking rotation into account.
u_int8_t rotated_block(Tetromino* piece, signed char y, signed char x) {
    signed char sum;
    switch (piece->rotation % 4) {
        case 0:
            return piece->size == 4 ? piece->shape4[y][x] : piece->shape3[y][x];
        case 1:
            if (piece->size == 4) {
                sum = y + 12 - 4*x;
                return piece->shape4[sum / 4][sum % 4];
            }
            else {
                sum = y + 6 - 3*x;
                return piece->shape3[sum / 3][sum % 3];
            }
        case 2:
            if (piece->size == 4) {
                sum = 15 - (4*y + x);
                return piece->shape4[sum / 4][sum % 4];
            }
            else {
                sum = 8 - (3*y + x);
                return piece->shape3[sum / 3][sum % 3];
            }
        case 3:
            if (piece->size == 4) {
                sum = 15 - (y + 12 - 4*x);
                return piece->shape4[sum / 4][sum % 4];
            }
            else {
                sum = 8 - (y + 6 - 3*x);
                return piece->shape3[sum / 3][sum % 3];
            }
    }
}

// Draws the playing field and the piece given as parameter to the console.
void draw_console(Tetromino* piece) {
    char symbols[] = " ABCDEFG#"; // The values to be drawn. For example, value 3 get converted to symbols[3] --> C

    for (signed char i = 0; i < field_height; i++) {
        for (signed char j = 0; j < field_width; j++) {
            mvaddch(offset_y + i, offset_x + j, symbols[ playing_field[i][j] ]);
        }
    }

    u_int8_t to_draw;
    signed char size = piece->size;
    for (signed char i = 0; i < size; i++) {
        for (signed char j = 0; j < size; j++) {
            to_draw = rotated_block(piece, i, j);
            if (to_draw != 0) mvaddch(offset_y + piece->ypos + i, offset_x + piece->xpos + j, symbols[ to_draw ]);
        }
    }
}

// Checks if the moved piece fits into the new location.
bool piece_fits(Tetromino* piece, bool rotated) {
    signed char ypos = piece->ypos;
    signed char xpos = piece->xpos;
    u_int8_t shape_block; // The value of the shape array of the piece
    u_int8_t field_block; // The value of the playing_field array
    signed char size = piece->size;

    // When rotated and the piece collided only with the border,
    // we allow it to rotate but move it so that it's not inside the border if we can.
    if (rotated) {
        u_int8_t collided_with_border = 0; // 0: didn't collide with the border, 1: left wall, 2: floor, 3: right wall
        for (signed char i = ypos; i < ypos + size; i++) {
            for (signed char j = xpos; j < xpos + size; j++) {
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
                        if (i == field_height - 1) collided_with_border = 2;
                        else if (j == field_width - 1) collided_with_border = 3;
                        else if (j == 0) collided_with_border = 1;
                    }
                }
            }
        }

        // Try to uncollide the piece with the wall.
        // If it then collides with another block, the piece cannot be rotated this
        // way, so return false.
        switch (collided_with_border) {
            case 0:
                return true;
            case 1:
                piece->xpos++;
                if (!piece_fits(piece, false)) {
                    piece->xpos--;
                    return false;
                }
                else return true;
            case 2:
                piece->ypos--;
                if (!piece_fits(piece, false)) {
                    piece->ypos++;
                    return false;
                }
                else return true;
            case 3:
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
        for (signed char i = ypos; i < ypos + size; i++) {
            for (signed char j = xpos; j < xpos + size; j++) {
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
// If the piece doesn't fit, return false, otherwise true.
bool move_if_fits(Tetromino* piece, int key) {
    // d
    if (key == 'd') {
        piece->xpos++;
        if (!piece_fits(piece, false)) {
            piece->xpos--;
            return false;
        }
        return true;
    }
    // a
    else if (key == 'a') {
        piece->xpos--;
        if (!piece_fits(piece, false)) {
            piece->xpos++;
            return false;
        }
        return true;
    }
    // w
    else if (key == 'w') {
        piece->rotation++;
        if (!piece_fits(piece, true)) {
            piece->rotation--;
            return false;
        }
        piece->rotation = piece->rotation % 4;
        return true;
    }
    // s
    else if (key == 's') {
        piece->ypos++;
        if (!piece_fits(piece, false)) {
            piece->ypos--;
            return false;
        }
        return true;
    }
    else {
        return false;
    }
}


int main() {
    // initialize playing field
    for (signed char i = 0; i < field_height; i++) {
        for (signed char j = 0; j < field_width; j++) {
            if (i == field_height - 1 || j == 0 || j == field_width - 1) playing_field[i][j] = 8;
            else playing_field[i][j] = 0;
        }
    }

    // initialize game state values
    srand(time(0));
    bool game_over = false;
    Tetromino* crnt_piece = new_Tetromino();
    u_int8_t loop_counter = 0; // When the counter is at a certain value, force the piece down.

    // initialize the console screen and the input source
    initscr();
    keypad(stdscr, true);
    raw();
    noecho();
    curs_set(0);
    nodelay(stdscr, true);

    // main game loop
    while (!game_over) {
        // tää usleep ei välttämättä toimi raudalla
        usleep(50*1000);
        loop_counter++;

        int key = getch();
        if (key == 'q') {
            game_over = true;
        }
        move_if_fits(crnt_piece, key);

        if (loop_counter == 20) {
            // force the piece down
            if (!move_if_fits(crnt_piece, 's')) {
                signed char size = crnt_piece->size;
                u_int8_t block_to_check; // block to check gameover with or the block to embed to the playing field

                // check for game over
                for (signed char i = 0; i < size; i++) {
                    if (!game_over) {
                        for (signed char j = 0; j < size; j++) {
                            block_to_check = rotated_block(crnt_piece, i, j);
                            if (block_to_check != 0 && i + crnt_piece->ypos < 0) {
                                game_over = true;
                                break;
                            }
                        }
                    }
                    else break;
                }

                if (!game_over) {
                    // embed piece to the field
                    for (signed char i = 0; i < size; i++) {
                        for (signed char j = 0; j < size; j++) {
                            block_to_check = rotated_block(crnt_piece, i, j);
                            if (block_to_check != 0) {
                                playing_field[crnt_piece->ypos+i][crnt_piece->xpos+j] = block_to_check;
                            }
                        }
                    }

                    // check for lines
                    for (signed char i = 0; i < field_height - 1; i++) {
                        // ignore borders
                        for (signed char j = 1; j < field_width - 1; j++) {
                            if (playing_field[i][j] == 0) {
                                break;
                            }
                            // reached the end of the line with no empty space
                            if (j == field_width - 2) {
                                // destroy the row
                                for (signed char k = 1; k < field_width - 1; k++) {
                                    playing_field[i][k] = 0;
                                }
                                // move rows above down
                                for (signed char k = i; k > 0; k--) {
                                    for (int l = 1; l < field_width - 1; l++) {
                                        playing_field[k][l] = playing_field[k-1][l];
                                    }
                                }
                                for (signed char l = 1; l < field_width - 1; l++) {
                                    playing_field[0][l] = 0;
                                }
                            }
                        }
                    }

                    // Huom. en oo muuten yhtään varma, pitääkö mun vapauttaa noiden taulukoiden muistit erikseen
                    // Jos pitää ja ei vapauteta, ni sit meiän 8kt täyttyy varmaa aika nopee.
                    free(crnt_piece);
                    crnt_piece = new_Tetromino();

                    // check for game over again if the new piece spawn on top of another
                    size = crnt_piece->size;
                    signed char xpos = crnt_piece->xpos;
                    signed char ypos = crnt_piece->ypos;
                    for (signed char i = 0; i < size; i++) {
                        if (!game_over) {
                            for (signed char j = 0; j < size; j++) {
                                block_to_check = rotated_block(crnt_piece, i, j);
                                if (block_to_check != 0 && i + ypos >= 0 && playing_field[i + ypos][j + xpos] != 0) {
                                    game_over = true;
                                    break;
                                }
                            }
                        }
                        else break;
                    }
                }
            }
            loop_counter = 0;
        }

        // TÄN TILALLE SE FUNKTIO, JOKA PIIRTÄÄ TILAN LEDEILLE
        draw_console(crnt_piece);
    }

    // close console screen
    endwin();

    return 0;
}