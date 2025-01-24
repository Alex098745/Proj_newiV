#pragma once 
#include <iostream>
#include <fstream>
#include <vector>
#include "nlohmann/json.hpp" 

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
#else
    #include <SDL.h>
    #include <SDL_image.h>
#endif

using namespace std;

class Board
{
public:
    Board() = default;
    Board(const unsigned int W, const unsigned int H) : W(W), H(H)
    {
    }

    // инициализация и отображение стартовой доски
    int start_draw()
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            print_exception("SDL_Init can't init SDL2 lib");
            return 1;
        }
        if (W == 0 || H == 0)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desctop display mode");
                return 1;
            }
            W = min(dm.w, dm.h);
            W -= W / 15;
            H = W;
        }
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
        if (win == nullptr)
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1;
        }
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == nullptr)
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1;
        }
        board = IMG_LoadTexture(ren, board_path.c_str());
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str());
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str());
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str());
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str());
        back = IMG_LoadTexture(ren, back_path.c_str());
        replay = IMG_LoadTexture(ren, replay_path.c_str());
        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1;
        }
        SDL_GetRendererOutputSize(ren, &W, &H);
        make_start_mtx();
        rerender();
        return 0;
    }

    // сброс доски для начала новой игры
    void redraw()
    {
        game_results = -1;
        history_mtx.clear();
        history_beat_series.clear();
        make_start_mtx();
        clear_active();
        clear_highlight();
    }

    // перемещение фигуры на новую позицию
    void move_piece(move_pos turn, const int beat_series = 0)
    {
        if (turn.xb != -1)
        {
            mtx[turn.xb][turn.yb] = 0;
        }
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);
    }

    // обработка перемещения фигуры, включая превращение в дамку
    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        if (mtx[i2][j2])
        {
            throw runtime_error("final position is not empty, can't move");
        }
        if (!mtx[i][j])
        {
            throw runtime_error("begin position is empty, can't move");
        }
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
            mtx[i][j] += 2;
        mtx[i2][j2] = mtx[i][j];
        drop_piece(i, j);
        add_history(beat_series);
    }

    // удаление фигуры с доски
    void drop_piece(const POS_T i, const POS_T j)
    {
        mtx[i][j] = 0;
        rerender();
    }

    // превращение обычной фигуры в дамку
    void turn_into_queen(const POS_T i, const POS_T j)
    {
        if (mtx[i][j] == 0 || mtx[i][j] > 2)
        {
            throw runtime_error("can't turn into queen in this position");
        }
        mtx[i][j] += 2;
        rerender();
    }

    // получение текущего состояния доски
    vector<vector<POS_T>> get_board() const
    {
        return mtx;
    }

    // выделение клеток на доске
    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        for (auto pos : cells)
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1;
        }
        rerender();
    }

    // очистка всех выделений
    void clear_highlight()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            is_highlighted_[i].assign(8, 0);
        }
        rerender();
    }

    // установка активной клетки
    void set_active(const POS_T x, const POS_T y)
    {
        active_x = x;
        active_y = y;
        rerender();
    }

    // очистка активной клетки
    void clear_active()
    {
        active_x = -1;
        active_y = -1;
        rerender();
    }

    // проверка, выделена ли клетка
    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y];
    }

    // откат доски на несколько ходов
    void rollback()
    {
        auto beat_series = max(1, *(history_beat_series.rbegin()));
        while (beat_series-- && history_mtx.size() > 1)
        {
            history_mtx.pop_back();
            history_beat_series.pop_back();
        }
        mtx = *(history_mtx.rbegin());
        clear_highlight();
        clear_active();
    }

    // отображение результата игры
    void show_final(const int res)
    {
        game_results = res;
        rerender();
    }

    // обновление размера окна
    void reset_window_size()
    {
        SDL_GetRendererOutputSize(ren, &W, &H);
        rerender();
    }

    // завершение работы и очистка ресурсов
    void quit()
    {
        SDL_DestroyTexture(board);
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // деструктор для безопасного завершения работы
    ~Board()
    {
        if (win)
            quit();
    }

private:
    // добавление текущего состояния доски в историю
    void add_history(const int beat_series = 0)
    {
        history_mtx.push_back(mtx);
        history_beat_series.push_back(beat_series);
    }
    // создание стартовой матрицы доски
    void make_start_mtx()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0;
                if (i < 3 && (i + j) % 2 == 1)
                    mtx[i][j] = 2;
                if (i > 4 && (i + j) % 2 == 1)
                    mtx[i][j] = 1;
            }
        }
        add_history();
    }

    // перерисовка текстур на экране
    void rerender()
    {
        // реализация отрисовки текстур
    }

    // запись ошибки в лог-файл
    void print_exception(const string& text) {
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Error: " << text << ". "<< SDL_GetError() << endl;
        fout.close();
    }

public:
    int W = 0;
    int H = 0;
    // история состояний доски
    vector<vector<vector<POS_T>>> history_mtx;

private:
    SDL_Window *win = nullptr;
    SDL_Renderer *ren = nullptr;
    // текстуры
    SDL_Texture *board = nullptr;
    SDL_Texture *w_piece = nullptr;
    SDL_Texture *b_piece = nullptr;
    SDL_Texture *w_queen = nullptr;
    SDL_Texture *b_queen = nullptr;
    SDL_Texture *back = nullptr;
    SDL_Texture *replay = nullptr;
    // пути к текстурам
    const string textures_path = project_path + "Textures/";
    const string board_path = textures_path + "board.png";
    const string piece_white_path = textures_path + "piece_white.png";
    const string piece_black_path = textures_path + "piece_black.png";
    const string queen_white_path = textures_path + "queen_white.png";
    const string queen_black_path = textures_path + "queen_black.png";
    const string white_path = textures_path + "white_wins.png";
    const string black_path = textures_path + "black_wins.png";
    const string draw_path = textures_path + "draw.png";
    const string back_path = textures_path + "back.png";
    const string replay_path = textures_path + "replay.png";
    // координаты выбранной клетки
    int active_x = -1, active_y = -1;
    // результат игры
    int game_results = -1;
    // матрица подсветки клеток
    vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));
    // матрица состояния доски
    vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));
    // история серии взятий
    vector<int> history_beat_series;
};
