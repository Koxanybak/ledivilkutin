#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <ncurses.h>
#include <time.h>

#define field_width 6
#define field_height 16

#define offset_y 5 // Amount of space away from the console borders
#define offset_x 5 // Amount of space away from the console borders

#define key_right 77
#define key_left 75
#define key_down 80

#define piece_size_big 4
#define piece_size_small 3

bool playing_field[field_height][field_width];

bool tetromino3_shapes[5][piece_size_small][piece_size_small] = {
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
bool tetromino4_shapes[2][piece_size_big][piece_size_big] = {
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
    bool shape4[4][4];
    bool shape3[3][3];
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

// Returns whether the given index is out of bounds
bool isOut(signed char i, bool width) {
    if (width) {
        return i < 0 || i >= field_width;
    } else {
        return i >= field_height;
    }
}

// Returns the block (0 or 1-7) at the specified location when taking rotation into account.
// THIS FUNCTION MUST BE CALLED BEFORE DRAWING THE BLOCK
bool rotated_block(Tetromino* piece, signed char y, signed char x) {
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
    char symbols[] = " O";

    for (signed char i = 0; i < field_height; i++) {
        for (signed char j = 0; j < field_width; j++) {
            mvaddch(offset_y + i, offset_x + j, symbols[ playing_field[i][j] ]);
        }
    }

    bool to_draw;
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
    bool shape_block; // The value of the shape array of the piece
    bool field_block; // The value of the playing_field array
    signed char size = piece->size;

    // When rotated and the piece collided only with the border,
    // we allow it to rotate but move it so that it's not inside the border if we can.
    if (rotated) {
        u_int8_t collided_with_border = 0; // 0: didn't collide with the border, 1: left wall, 2: floor, 3: right wall

        for (signed char i = ypos; i < ypos + size; i++) {
            for (signed char j = xpos; j < xpos + size; j++) {
                // Take rotation into account
                shape_block = rotated_block(piece, i - ypos, j - xpos);
                if (shape_block != 0) {
                    if (isOut(i, false) || isOut(j, true)) {
                        // collided with the border
                        if (i == field_height) collided_with_border = 2;
                        else if (j == field_width) collided_with_border = 3;
                        else if (j == -1) collided_with_border = 1;
                    }
                    else if (playing_field[i][j] != 0) {
                        // collided with another block
                        return false;
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
                if (!piece_fits(piece, true)) {
                    piece->xpos--;
                    return false;
                }
                else return true;
            case 2:
                piece->ypos--;
                if (!piece_fits(piece, true)) {
                    piece->ypos++;
                    return false;
                }
                else return true;
            case 3:
                piece->xpos--;
                if (!piece_fits(piece, true)) {
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

                if (shape_block != 0) {
                    // collided with another block
                    if (isOut(i, false) || isOut(j, true)) {
                        return false;
                    } else if (playing_field[i][j] != 0) {
                        return false;
                    }
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
    // Right
    if (key == 'd') {
        piece->xpos++;
        if (!piece_fits(piece, false)) {
            piece->xpos--;
            return false;
        }
        return true;
    }
    // Left
    else if (key == 'a') {
        piece->xpos--;
        if (!piece_fits(piece, false)) {
            piece->xpos++;
            return false;
        }
        return true;
    }
    // Rotate
    else if (key == 'w') {
        piece->rotation++;
        if (!piece_fits(piece, true)) {
            piece->rotation--;
            return false;
        }
        piece->rotation = piece->rotation % 4;
        return true;
    }
    // Down
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
            playing_field[i][j] = 0;
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

    // TÄ ON VAAN KONSOLILLA TESTAUSTA VARTEN
    // -------------------
    for (signed char i = 0; i < field_height; i++) {
        mvaddch(offset_y + i, offset_x - 1, '#');
        mvaddch(offset_y + i, offset_x + field_width, '#');
    }
    for (signed char i = -1; i <= field_width; i++) {
        mvaddch(offset_y + field_height, offset_x + i, '#');
    }
    // --------------------------

    // main game loop
    while (!game_over) {
        // tää usleep ei välttämättä toimi raudalla
        usleep(50*1000);
        loop_counter++;

        int key = getch();
        if (key == 'q') {
            game_over = true;
        }
        // Tästä funtiosta pitää muuttaa sitten toi "key" sellaseksi, että se vastaa
        // mitä raudalta tulee
        move_if_fits(crnt_piece, key);

        // tÄN COPY PASTEET OMAAN LOOPPIIS (en tiiä toimiiko toi yhellä rivillä oleva free() raudalla)
        // ehto on totta sillon kun palikka pakotetaan alas
        // tää if lause vaatii kans "game_over" muuttujan olemassaolon
        if (loop_counter == 20) {
            // force the piece down
            if (!move_if_fits(crnt_piece, 's')) {
                signed char size = crnt_piece->size;
                bool block_to_check; // block to check gameover with or the block to embed to the playing field

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
                    // Embed the piece to the field
                    for (signed char i = 0; i < size; i++) {
                        for (signed char j = 0; j < size; j++) {
                            block_to_check = rotated_block(crnt_piece, i, j);
                            if (block_to_check != 0) {
                                playing_field[crnt_piece->ypos+i][crnt_piece->xpos+j] = block_to_check;
                            }
                        }
                    }

                    // check for lines
                    for (signed char i = 0; i < field_height; i++) {
                        for (signed char j = 0; j < field_width; j++) {
                            if (playing_field[i][j] == 0) {
                                break;
                            }
                            // reached the end of the row with no empty space
                            if (j == field_width - 1) {
                                // destroy the row
                                for (signed char k = 0; k < field_width; k++) {
                                    playing_field[i][k] = 0;
                                }
                                // move rows above down
                                for (signed char k = i; k > 0; k--) {
                                    for (int l = 0; l < field_width; l++) {
                                        playing_field[k][l] = playing_field[k-1][l];
                                    }
                                }
                                for (signed char l = 0; l < field_width; l++) {
                                    playing_field[0][l] = 0;
                                }
                            }
                        }
                    }

                    // Huom. en oo muuten yhtään varma, pitääkö mun vapauttaa noiden taulukoiden muistit erikseen
                    // Jos pitää ja ei vapauteta, ni sit meiän 8kt täyttyy varmaa aika nopee.
                    free(crnt_piece);
                    crnt_piece = new_Tetromino();

                    // Check for game over again if the new piece spawn on top of another
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