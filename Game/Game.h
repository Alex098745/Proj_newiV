#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // to start checkers

    int play()
{
    // засекаем время начала игры.
    auto start = chrono::steady_clock::now();

    if (is_replay)
    {
        // если включён режим повтора, перезагружаем логику и настройки и обновляем доску  
        logic = Logic(&board, &config);
        config.reload();
        board.redraw();
    }
    else
    {
        // если это новый запуск, рисуем начальное состояние доски
        board.start_draw();
    }

    // сбрасываем флаг повтора игры
    is_replay = false;

    int turn_num = -1;  // текущий номер хода
    bool is_quit = false;  // флаг выхода из игры
    const int Max_turns = config("Game", "MaxNumTurns");  // максимальное количество ходов

    while (++turn_num < Max_turns) // цикл обработки ходов
    {  
        beat_series = 0; // сбрасываем счётчик серии ходов
        
        // поиск доступных ходов для текущего игрока (0 — белый, 1 — чёрный)
        logic.find_turns(turn_num % 2);
        if (logic.turns.empty())
            break; // если ходов нет, выходим из цикла

        // устанавливаем максимальную глубину анализа для бота
        logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
        
        // проверяем, является ли текущий игрок ботом
        if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
        {
            // ход игрока и обработка возможных ответов (выход, повтор игры, возврат)
            auto resp = player_turn(turn_num % 2);
            if (resp == Response::QUIT)
            {
                is_quit = true; // игрок завершил игру
                break;
            }
            else if (resp == Response::REPLAY)
            {
                is_replay = true; // перезапуск игры
                break;
            }
            else if (resp == Response::BACK)
            {
                // обработка отката хода. Если предыдущий игрок — бот, откат выполняется для двух последних ходов
                if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                    !beat_series && board.history_mtx.size() > 2)
                {
                    board.rollback();
                    --turn_num;
                }
                if (!beat_series)
                    --turn_num;

                board.rollback();
                --turn_num;
                beat_series = 0; // сброс серии
            }
        }
        else
        {
            // ход бота
            bot_turn(turn_num % 2);
        }
    }

    // засекаем время окончания игры
    auto end = chrono::steady_clock::now();

    // записываем время игры в лог
    ofstream fout(project_path + "log.txt", ios_base::app);
    fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
    fout.close();

    if (is_replay)
        return play(); // повтор игры, если установлен соответствующий флаг
    
    if (is_quit)
        return 0; // завершение игры

    int res = 2; // результат игры (по умолчанию ничья)
    if (turn_num == Max_turns)
    {
        res = 0; // ничья из-за достижения максимального числа ходов
    }
    else if (turn_num % 2)
    {
        res = 1; // победа чёрного игрока
    }

    // отображение итогового результата игры
    board.show_final(res);

    // ожидание ответа от игрока после завершения игры (например, перезапуск игры)
    auto resp = hand.wait();
    if (resp == Response::REPLAY)
    {
        is_replay = true;
        return play();
    }

    return res; // возвращаем результат игры
}


  private:
    void bot_turn(const bool color) {

    auto start = chrono::steady_clock::now(); // начало отсчета времени выполнения хода бота
    auto delay_ms = config("Bot", "BotDelayMS");  // получение настройки задержки для хода бота в миллисекундах
    
    thread th(SDL_Delay, delay_ms); // создание нового потока для обеспечения равномерной задержки перед ходом

    auto turns = logic.find_best_turns(color);  // определение лучших ходов для бота на основе указанного цвета

    th.join(); // ожидание завершения потока задержки
    bool is_first = true; // флаг для проверки, является ли это первый ход в последовательности

    for (auto turn : turns){ // выполнение каждого хода из найденной последовательности лучших ходов
    
        if (!is_first) // если это не первый ход, добавляется задержка перед его выполнением
        {
            SDL_Delay(delay_ms);
        }
        is_first = false;
        beat_series += (turn.xb != -1);    // обновление счетчика серии ударов, если захвачена фигура

        board.move_piece(turn, beat_series);  // выполнение хода на игровом поле
    }

    auto end = chrono::steady_clock::now();   // завершение отсчета времени и расчет времени выполнения хода

    // запись времени выполнения хода бота в файл лога
    ofstream fout(project_path + "log.txt", ios_base::app);
    fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
    fout.close();
}


Response player_turn(const bool color)
{
    // функция отвечает за выполнение хода игрока (не бота)
    // параметр "color" определяет текущий цвет игрока (true — чёрный, false — белый)
    // возвращает "Response", который сигнализирует о статусе хода

    vector<pair<POS_T, POS_T>> cells;
    // собираем список возможных ходов для текущего игрока
    for (auto turn : logic.turns)
    {
        cells.emplace_back(turn.x, turn.y); // добавляем стартовые координаты хода
    }

    // подсвечиваем клетки, куда игрок может сделать ход
    board.highlight_cells(cells);

    move_pos pos = {-1, -1, -1, -1}; // переменная для хранения выбранного хода
    POS_T x = -1, y = -1;           // координаты активной клетки

    
    while (true)
    {
        auto resp = hand.get_cell(); // получаем выбор игрока
        if (get<0>(resp) != Response::CELL)
            return get<0>(resp); // если выбор не клетка, возвращаем соответствующий ответ (например, QUIT или BACK)

        pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)}; // координаты выбранной клетки

        bool is_correct = false; // флаг корректности выбранного хода

        // проверяем, соответствует ли выбор игрока возможному ходу
        for (auto turn : logic.turns)
        {
            if (turn.x == cell.first && turn.y == cell.second)
            {
                is_correct = true; // игрок выбрал корректную клетку для начала хода
                break;
            }
            if (turn == move_pos{x, y, cell.first, cell.second})
            {
                pos = turn; // игрок завершил ход
                break;
            }
        }

        // если ход завершён, выходим из цикла
        if (pos.x != -1)
            break;

        // если выбор некорректен, сбрасываем активные клетки
        if (!is_correct)
        {
            if (x != -1)
            {
                board.clear_active();    // убираем активную клетку
                board.clear_highlight(); // убираем подсветку
                board.highlight_cells(cells); // осстанавливаем подсветку доступных клеток
            }
            x = -1;
            y = -1;
            continue;
        }

        // если выбор корректен, обновляем активную клетку и подсвечиваем возможные продолжения
        x = cell.first;
        y = cell.second;
        board.clear_highlight();
        board.set_active(x, y);

        vector<pair<POS_T, POS_T>> cells2;
        for (auto turn : logic.turns)
        {
            if (turn.x == x && turn.y == y)
            {
                cells2.emplace_back(turn.x2, turn.y2); // добавляем возможные клетки для продолжения хода
            }
        }
        board.highlight_cells(cells2); // подсвечиваем возможные клетки для продолжения
    }

    // очистка подсветки и активных клеток
    board.clear_highlight();
    board.clear_active();

    // выполняем ход и проверяем, есть ли возможность продолжения битья
    board.move_piece(pos, pos.xb != -1);
    if (pos.xb == -1)
        return Response::OK; // если битье не продолжается, возвращаем успешный статус

    // продолжаем серию битья, если это возможно
    beat_series = 1; // инициализируем счётчик серии битья
    while (true)
    {
        // находим доступные ходы для продолжения битья
        logic.find_turns(pos.x2, pos.y2);
        if (!logic.have_beats)
            break; // если битья больше нет, выходим из цикла

        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x2, turn.y2); // добавляем клетки для продолжения
        }

        // подсвечиваем доступные клетки для продолжения битья
        board.highlight_cells(cells);
        board.set_active(pos.x2, pos.y2); // устанавливаем текущую активную клетку

        // пытаемся сделать следующий ход
        while (true)
        {
            auto resp = hand.get_cell(); // получаем выбор игрока
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp); // если выбор не клетка, возвращаем соответствующий ответ

            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

            bool is_correct = false; // флаг корректности выбранного хода
            for (auto turn : logic.turns)
            {
                if (turn.x2 == cell.first && turn.y2 == cell.second)
                {
                    is_correct = true; // выбранная клетка корректна
                    pos = turn;        // сохраняем текущий ход
                    break;
                }
            }
            if (!is_correct)
                continue; // если ход некорректен, запрашиваем новый выбор

            // если выбор корректен, выполняем ход и продолжаем серию
            board.clear_highlight();
            board.clear_active();
            beat_series += 1; // увеличиваем счётчик серии
            board.move_piece(pos, beat_series);
            break;
        }
    }

    return Response::OK; // возвращаем успешный статус
}


  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
