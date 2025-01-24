#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

   std::vector<move_pos> find_best_turns(const bool color) {
    next_best_state.clear(); // очищаем массив состояний
    next_move.clear(); // очищаем массив ходов

    // ищем первый лучший ход с использованием текущего состояния доски
    find_first_best_turn(board->get_board(), color, -1, -1, 0);

    std::vector<move_pos> result; // результативный вектор для хранения последовательности ходов
    int current_state = 0; // текущий индекс состояния

    // пока есть состояния и текущий ход не завершен
    while (current_state != -1 && next_move[current_state].x != -1) {
        result.emplace_back(next_move[current_state]); // добавляем ход в результат
        current_state = next_best_state[current_state]; // переходим к следующему состоянию
    }

    return result; // возвращаем последовательность лучших ходов
}


private:
    // выполняет ход, обновляя доску
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        // если есть сбитая фигура, очищаем соответствующую ячейку
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;

        // если шашка достигает последней линии, она становится дамкой
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2; // превращаем шашку в дамку

        // перемещаем фигуру на новое место
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        // очищаем исходную позицию фигуры
        mtx[turn.x][turn.y] = 0;

        // возвращаем обновленную доску
        return mtx;
    }


    // вычисляет оценку текущего состояния доски
double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
{
    // инициализация переменных для подсчета шашек и дамок белого и черного игрока
    double w = 0, wq = 0, b = 0, bq = 0;

    // проход по всей доске для подсчета количества шашек и дамок каждого цвета
    for (POS_T i = 0; i < 8; ++i)
    {
        for (POS_T j = 0; j < 8; ++j)
        {
            // подсчет шашек и дамок белого игрока
            w += (mtx[i][j] == 1);  // обычные шашки белого
            wq += (mtx[i][j] == 3); // дамки белого

            // подсчет шашек и дамок черного игрока
            b += (mtx[i][j] == 2);  // обычные шашки черного
            bq += (mtx[i][j] == 4); // дамки черного

            // добавление потенциала для шашек в зависимости от их положения (если включен режим подсчета потенциала)
            if (scoring_mode == "NumberAndPotential")
            {
                w += 0.05 * (mtx[i][j] == 1) * (7 - i); // потенциал для белых шашек
                b += 0.05 * (mtx[i][j] == 2) * (i);    // потенциал для черных шашек
            }
        }
    }

    // если первый бот играет черными, меняем местами значения белого и черного игрока
    if (!first_bot_color)
    {
        swap(b, w);
        swap(bq, wq);
    }

    // если у белого игрока не осталось фигур, возвращаем бесконечность (черный выигрывает)
    if (w + wq == 0)
        return INF;

    // если у черного игрока не осталось фигур, возвращаем 0 (белый выигрывает)
    if (b + bq == 0)
        return 0;

    // коэффициент веса дамок (по умолчанию 4, в режиме потенциала увеличен до 5)
    int q_coef = 4;
    if (scoring_mode == "NumberAndPotential")
    {
        q_coef = 5;
    }

    // возвращаем отношение количества фигур черного игрока к количеству фигур белого игрока, учитывая вес дамок
    return (b + bq * q_coef) / (w + wq * q_coef);
}

// функция для поиска первого лучшего хода, используя альфа-бета отсечение
double find_first_best_turn(const std::vector<std::vector<POS_T>>& mtx, 
                            const bool color, 
                            const POS_T x, 
                            const POS_T y, 
                            size_t state, 
                            double alpha = -1) {
    // инициализация нового состояния и хода
    next_best_state.push_back(-1); // индекс следующего лучшего состояния
    next_move.emplace_back(-1, -1, -1, -1); // добавляем "пустой" ход

    double best_score = -1; // начальная лучшая оценка
    
    // если это не начальное состояние, ищем возможные ходы
    if (state != 0) {
        find_turns(x, y, mtx);
    }

    // сохраняем текущие ходы и информацию об обязательных ударах
    const auto available_moves = turns; 
    const bool has_mandatory_beats = have_beats; 

    // если ударов нет, переходим к следующему игроку
    if (!has_mandatory_beats && state != 0) {
        return find_best_turns_rec(mtx, 1 - color, 0, alpha);
    }

    // итерация по всем возможным ходам
    for (const auto& move : available_moves) {
        size_t next_state_index = next_move.size(); // индекс следующего состояния
        double move_score; // оценка текущего хода

        if (has_mandatory_beats) {
            // выполняем обязательный удар и рекурсивно ищем дальнейшие ходы
            move_score = find_first_best_turn(make_turn(mtx, move), color, move.x2, move.y2, next_state_index, best_score);
        } else {
            // оцениваем ход для следующего игрока
            move_score = find_best_turns_rec(make_turn(mtx, move), 1 - color, 0, best_score);
        }

        // обновляем лучшую оценку, если нашли более выгодный ход
        if (move_score > best_score) {
            best_score = move_score;
            next_best_state[state] = (has_mandatory_beats ? static_cast<int>(next_state_index) : -1);
            next_move[state] = move;
        }
    }
    return best_score; // возвращаем лучшую оценку
}

// рекурсивная функция для поиска лучшего хода (альфа-бета отсечение)
double find_best_turns_rec(const std::vector<std::vector<POS_T>>& mtx, 
                           const bool color, 
                           const size_t depth, 
                           double alpha = -1, 
                           double beta = INF + 1, 
                           const POS_T x = -1, 
                           const POS_T y = -1) {
    // проверка глубины рекурсии
    if (depth == Max_depth) {
        return calc_score(mtx, (depth % 2 == color)); // оцениваем доску
    }

    // определяем возможные ходы
    if (x != -1) {
        find_turns(x, y, mtx); // ищем ходы для конкретной позиции
    } else {
        find_turns(color, mtx); // ищем ходы для текущего игрока
    }

    const auto available_moves = turns; // список доступных ходов
    const bool has_mandatory_beats = have_beats; // проверяем наличие обязательных ударов

    // если нет обязательных ударов, переходим к следующему игроку
    if (!has_mandatory_beats && x != -1) {
        return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
    }

    // если ходов больше нет, возвращаем значение на основе игрока
    if (available_moves.empty()) {
        return (depth % 2 == 0) ? INF : 0; // выигрыш для одного игрока и проигрыш для другого
    }

    // инициализируем оценки для альфа-бета отсечения
    double min_score = INF + 1;
    double max_score = -1;

    // перебираем возможные ходы
    for (const auto& move : available_moves) {
        double move_score;

        if (!has_mandatory_beats && x == -1) {
            // оцениваем ход для следующего игрока
            move_score = find_best_turns_rec(make_turn(mtx, move), 1 - color, depth + 1, alpha, beta);
        } else {
            // выполняем ход с ударом и продолжаем искать
            move_score = find_best_turns_rec(make_turn(mtx, move), color, depth, alpha, beta, move.x2, move.y2);
        }
        // обновляем минимальную и максимальную оценки
        min_score = std::min(min_score, move_score);
        max_score = std::max(max_score, move_score);

        // выполняем альфа-бета отсечение
        if (depth % 2 == 0) {
            beta = std::min(beta, min_score); // минимизируем для текущего игрока
        } else {
            alpha = std::max(alpha, max_score); // максимизируем для текущего игрока
        }

        if (alpha >= beta) {
            return (depth % 2 == 0) ? min_score : max_score; // раннее завершение
        }
    }

    // возвращаем результат в зависимости от хода
    return (depth % 2 == 0) ? min_score : max_score;
}


public:
    // вызывает метод для поиска возможных ходов, используя цвет
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    // вызывает метод для поиска возможных ходов для конкретной позиции
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // ищет все возможные ходы для заданного цвета на доске
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    // ищет все возможные ходы для конкретной позиции
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // проверяет бьющие ходы
        switch (type)
        {
        case 1:
        case 2:
            // проверяет шашки
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // проверяет дамки
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // проверяет другие ходы
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // проверяет шашки
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // проверяет дамки
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }


  public:
    vector<move_pos> turns;
    bool have_beats;
    int Max_depth;

  private:
    default_random_engine rand_eng;
    string scoring_mode;
    string optimization;
    vector<move_pos> next_move;
    vector<int> next_best_state;
    Board *board;
    Config *config;
};
