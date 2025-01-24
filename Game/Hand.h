#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// класс Hand: отвечает за обработку пользовательского ввода 
class Hand
{
  public:
    // конструктор: инициализирует объект Hand и связывает его с игровой доской 
    Hand(Board *board) : board(board)
    {
    }

    // метод get_cell(): обрабатывает ввод игрока и возвращает событие и координаты клетки
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent; // событие SDL (например, клик мыши или закрытие окна)
        Response resp = Response::OK; // изначальный статус ответа
        int x = -1, y = -1;           // координаты клика мыши 
        int xc = -1, yc = -1;         // координаты клетки на доске

        // основной цикл обработки событий
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    // пользователь закрыл окно — возвращаем статус QUIT
                    resp = Response::QUIT;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    // обработка клика мыши: определяем, какая клетка выбрана
                    x = windowEvent.motion.x; // координата X клика 
                    y = windowEvent.motion.y; // координата Y клика
                    xc = int(y / (board->H / 10) - 1); // координата строки на доске
                    yc = int(x / (board->W / 10) - 1); // координата столбца на доске

                    // определяем тип действия на основе координат клетки
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK; // игрок выбрал действие "откатить ход"
                    }
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY; // игрок выбрал "начать заново"
                    }
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL; // выбрана игровая клетка
                    }
                    else
                    {
                        // если выбор некорректен, сбрасываем координаты клетки
                        xc = -1;
                        yc = -1;
                    }
                    break;

                case SDL_WINDOWEVENT:
                    // обработка изменения размера окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size(); // пересчитываем размеры доски
                        break;
                    }
                }

                // если событие не является "OK", выходим из цикла обработки
                if (resp != Response::OK)
                    break;
            }
        }

        // возвращаем результат: тип события и координаты выбранной клетки
        return {resp, xc, yc};
    }

    // метод wait(): ожидает событие от игрока и возвращает тип действия
    Response wait() const
    {
        SDL_Event windowEvent; // событие SDL
        Response resp = Response::OK; // изначальный статус ответа

        // основной цикл ожидания события
        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    resp = Response::QUIT; // пользователь закрыл окно
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    board->reset_window_size(); // пересчитываем размеры доски
                    break;

                case SDL_MOUSEBUTTONDOWN: {
                    // обработка клика мыши: проверяем, выбрал ли игрок "начать заново"
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY; // игрок выбрал "начать заново"
                }
                break;
                }

                // если событие не является "OK", выходим из цикла
                if (resp != Response::OK)
                    break;
            }
        }

        // возвращаем тип события
        return resp;
    }

  private:
    Board *board; // указатель на объект доски, для взаимодействия с её состоянием
};
