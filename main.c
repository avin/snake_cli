#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>

#define WIDTH 25
#define HEIGHT 15
#define INITIAL_SNAKE_LENGTH (HEIGHT/2)
#define TAIL_MAX_LEN (WIDTH*HEIGHT+1)
#define MOVE_DELAY 100

#define FREE_SPACE_CELL "\033[90;47m  \033[0m"
#define APPLE_CELL "\033[31m▓▓\033[0m"
#define SNAKE_BODY_CELL "\033[32m██\033[0m"
#define SNAKE_HEAD_UP_CELL "\033[32;40m▀▀\033[0m"
#define SNAKE_HEAD_RIGHT_CELL "\033[32;40m █\033[0m"
#define SNAKE_HEAD_DOWN_CELL "\033[32;40m▄▄\033[0m"
#define SNAKE_HEAD_LEFT_CELL "\033[32;40m█ \033[0m"
#define SNAKE_CRASH_CELL "\033[32;40m░░\033[0m"

typedef struct {
    int x;
    int y;
} point_t;

typedef enum {
    DIR_UP = 1,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT
} Direction;

typedef struct {
    int field[HEIGHT][WIDTH];
    point_t tail[TAIL_MAX_LEN];
    int tail_len;
    int dir;
    int next_dir;
    int is_game_over;
    point_t apple_pos;
    int score;
} game_t;

/**
 * Настройка терминала в режим без ожидания нажатия Enter и без эхо-ввода
 */
void prepare_terminal() {
    struct termios term;
    if (tcgetattr(STDIN_FILENO, &term) == -1) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    term.c_lflag &= ~(ICANON | ECHO); // Отключаем канонический режим и эхо
    term.c_cc[VMIN] = 0; // Минимальное число символов для ввода
    term.c_cc[VTIME] = 0; // Таймаут чтения

    if (tcsetattr(STDIN_FILENO, TCSANOW, &term) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
}

/**
 * Восстановление стандартного режима терминала
 */
void reset_terminal() {
    struct termios term;
    if (tcgetattr(STDIN_FILENO, &term) == -1) {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    term.c_lflag |= (ICANON | ECHO); // Включаем обратно канонический режим и эхо

    if (tcsetattr(STDIN_FILENO, TCSANOW, &term) == -1) {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
}

void game_create_apple(game_t *game) {
    int size = WIDTH * HEIGHT;

    int free_size = size - game->tail_len;
    int p = rand() % free_size;
    int c = 0;

    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            if (game->field[y][x]) {
                continue;
            }
            if (c == p) {
                game->apple_pos.x = x;
                game->apple_pos.y = y;
                return;
            }
            c++;
        }
    }
}

/**
 * Инициализация стартовой позиции игры
 */
void game_init_game(game_t *game) {
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            game->field[y][x] = 0;
        }
    }
    game->is_game_over = 0;
    game->score = 0;
    game->dir = DIR_UP;
    game->next_dir = DIR_UP;
    game->tail_len = INITIAL_SNAKE_LENGTH;
    for (int i = 0; i < game->tail_len; ++i) {
        int x = WIDTH / 2;
        int y = HEIGHT / 2 + i;

        game->tail[i].x = x;
        game->tail[i].y = y;
        game->field[y][x] = 1;
    }

    game_create_apple(game);
}

void game_over(game_t *game) {
    game->is_game_over = 1;

    for (int i = 0; i < 4; ++i) {
        printf("\033[%d;%dH", game->tail[0].y + 2, game->tail[0].x * 2 + 2);
        printf("%s", SNAKE_CRASH_CELL);
        usleep(60000);
        fflush(stdout);
        printf("\033[%d;%dH", game->tail[0].y + 2, game->tail[0].x * 2 + 2);
        printf("%s", SNAKE_BODY_CELL);
        usleep(60000);
        fflush(stdout);
    }


    for (int i = 0; i < game->tail_len; ++i) {
        printf("\033[%d;%dH", game->tail[i].y + 2, game->tail[i].x * 2 + 2);
        printf("%s", SNAKE_CRASH_CELL);
        if (i > 0) {
            printf("\033[%d;%dH", game->tail[i - 1].y + 2, game->tail[i - 1].x * 2 + 2);
            printf(FREE_SPACE_CELL);
        }
        fflush(stdout);
        usleep(50000);
    }
    printf("\033[2J"); // Очищаем экран

    printf("\033[%d;%dH", HEIGHT / 2, WIDTH - 4);
    printf("GAME OVER!");
    printf("\033[%d;%dH", HEIGHT / 2 + 1, WIDTH - 6);
    printf("Your score: %d", game->score);
    printf("\033[%d;%dH", HEIGHT / 2 + 2, WIDTH - 12);
    printf("Press space to restart...");
    fflush(stdout);
}

void game_change_dir(game_t *game, Direction new_dir) {
    // Предотвращаем движение в противоположную сторону
    if (abs((int) game->dir - (int) new_dir) != 2) {
        game->next_dir = new_dir;
    }
}

/**
 * Просчет перемещения змейки
 */
void game_update_snake(game_t *game) {
    if (game->is_game_over) {
        return;
    }
    game->dir = game->next_dir;
    point_t new_head = {
            game->tail[0].x,
            game->tail[0].y,
    };

    // Высчитываем новую позицию головы
    if (game->dir == DIR_UP) {
        new_head.y -= 1;
    } else if (game->dir == DIR_RIGHT) {
        new_head.x += 1;
    } else if (game->dir == DIR_DOWN) {
        new_head.y += 1;
    } else if (game->dir == DIR_LEFT) {
        new_head.x -= 1;
    }

    // Если вышли за пределы поля
    if (new_head.x < 0) {
        new_head.x = WIDTH - 1;
    }
    if (new_head.x > WIDTH - 1) {
        new_head.x = 0;
    }
    if (new_head.y < 0) {
        new_head.y = HEIGHT - 1;
    }
    if (new_head.y > HEIGHT - 1) {
        new_head.y = 0;
    }

    // Проверяем если врезались в себя
    for (int i = 1; i < game->tail_len; ++i) {
        if (game->tail[i].x == new_head.x && game->tail[i].y == new_head.y) {
            game_over(game);
            return;
        }
    }

    // Проверяем если съели яблоко
    int is_apple_eaten = 0;
    if (new_head.x == game->apple_pos.x && new_head.y == game->apple_pos.y) {
        game->tail_len++;
        is_apple_eaten = 1;
    }

    // Убираем последнюю клетку из массива занятости
    if (!is_apple_eaten) {
        point_t p = game->tail[game->tail_len - 1];
        game->field[p.y][p.x] = 0;
    }

    // Перемещаем позиции клеток змейки (кроме головы)
    for (int i = game->tail_len - 1; i > 0; --i) {
        game->tail[i].x = game->tail[i - 1].x;
        game->tail[i].y = game->tail[i - 1].y;
    }

    // Новая позиция головы
    game->tail[0].x = new_head.x;
    game->tail[0].y = new_head.y;

    game->field[new_head.y][new_head.x] = 1;

    // Если мы съели яблоко, то нужно нарисовать новое
    if (is_apple_eaten) {
        game->score += 1;
        game_create_apple(game);
    }
}

/**
 * Отрисовка поля
 */
void game_draw_field(game_t *game) {
    if (game->is_game_over) {
        return;
    }

    printf("\033[H"); // Перемещаем курсор в верхний левый угол

    printf("┌");
    for (int i = 0; i < WIDTH * 2; ++i) {
        printf("─");
    }
    printf("┐\n");

    for (int y = 0; y < HEIGHT; ++y) {
        printf("│");
        for (int x = 0; x < WIDTH; ++x) {
            int printed = 0;

            if (game->field[y][x]) {

                if (game->tail[0].y == y && game->tail[0].x == x) {
                    if (game->dir == DIR_UP) {
                        printf(SNAKE_HEAD_UP_CELL);
                    } else if (game->dir == DIR_RIGHT) {
                        printf(SNAKE_HEAD_RIGHT_CELL);
                    } else if (game->dir == DIR_DOWN) {
                        printf(SNAKE_HEAD_DOWN_CELL);
                    } else if (game->dir == DIR_LEFT) {
                        printf(SNAKE_HEAD_LEFT_CELL);
                    }
                } else {
                    printf(SNAKE_BODY_CELL);
                }

                printed = 1;
            }

            if (!printed) {
                if (game->apple_pos.y == y && game->apple_pos.x == x) {
                    printf(APPLE_CELL);
                    printed = 1;
                }
            }
            if (!printed) {
                printf(FREE_SPACE_CELL);
            }
        }
        printf("│\n");
    }

    printf("├");
    for (int i = 0; i < WIDTH * 2; ++i) {
        printf("─");
    }
    printf("┤\n");

    printf("│ Score: %d", game->score);

    printf("\033[%d;%dH", HEIGHT + 3, WIDTH * 2 + 2);
    printf("│\n");

    printf("└");
    for (int i = 0; i < WIDTH * 2; ++i) {
        printf("─");
    }
    printf("┘\n");
}

/**
 * Проверка на ввод с клавиатуры
 */
int is_key_pressed() {
    struct timeval tv;
    fd_set readfds;

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);

    return FD_ISSET(STDIN_FILENO, &readfds);
}

void quit() {
    reset_terminal();

    printf("\033[2J"); // Очищаем экран перед выходом
    printf("\033[H"); // Перемещаем курсор в верхний левый угол
    printf("\033[?25h"); // Показываем курсор

    exit(0);
}

void handle_key_event(game_t *game, char ch) {
    // Если нажата клавиша ESC (выход)
    if (ch == '\033') {
        // Проверим, есть ли последующие символы (для стрелок)
        char next[2];
        if (read(STDIN_FILENO, next, 2) == 2) {
            // Обработка стрелок
            if (next[0] == '[') {
                if (next[1] == 'A') { // Стрелка вверх
                    game_change_dir(game, DIR_UP);
                } else if (next[1] == 'B') { // Стрелка вниз
                    game_change_dir(game, DIR_DOWN);
                } else if (next[1] == 'C') { // Стрелка вправо
                    game_change_dir(game, DIR_RIGHT);
                } else if (next[1] == 'D') { // Стрелка влево
                    game_change_dir(game, DIR_LEFT);
                }
            }
        } else {
            // Если только ESC без дополнительных символов, выходим из игры
            quit();
        }
    } else if (ch == 'q') {
        // Клавиша 'q' для выхода
        quit();
    } else if (ch == 'w' || ch == 'k') {
        game_change_dir(game, DIR_UP);
    } else if (ch == 'd' || ch == 'l') {
        game_change_dir(game, DIR_RIGHT);
    } else if (ch == 's' || ch == 'j') {
        game_change_dir(game, DIR_DOWN);
    } else if (ch == 'a' || ch == 'h') {
        game_change_dir(game, DIR_LEFT);
    } else if (ch == ' ') {
        if (game->is_game_over) {
            game_init_game(game);
        }
    }
}

/**
 * Функция для вычисления разницы времени в миллисекундах
 */
int time_diff_ms(struct timeval *start, struct timeval *end) {
    return (int) ((end->tv_sec - start->tv_sec) * 1000 + (end->tv_usec - start->tv_usec) / 1000);
}

int main() {
    char ch;
    struct timeval last_frame_time, current_time;

    srand(time(NULL));

    prepare_terminal();

    printf("\033[2J");
    printf("\033[?25l");

    game_t global_game;
    game_t *game = &global_game;

    game_init_game(game);

    gettimeofday(&last_frame_time, NULL);

    while (1) {
        gettimeofday(&current_time, NULL);
        int elapsed_time = time_diff_ms(&last_frame_time, &current_time);

        if (elapsed_time >= MOVE_DELAY) {
            game_update_snake(game);
            game_draw_field(game);

            last_frame_time = current_time;
        }

        if (is_key_pressed() && read(STDIN_FILENO, &ch, 1) == 1) {
            handle_key_event(game, ch);
        }
    }

    return 0;
}
