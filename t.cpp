#include <chrono>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

struct Point {
    int x;
    int y;

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

enum class Direction {
    Up,
    Down,
    Left,
    Right
};

class TerminalRawMode {
public:
    TerminalRawMode() {
#ifdef _WIN32
        // 启用 Windows 终端 ANSI 支持（较新系统有效）
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (out != INVALID_HANDLE_VALUE) {
            DWORD mode = 0;
            if (GetConsoleMode(out, &mode)) {
                SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
#else
        tcgetattr(STDIN_FILENO, &old_);
        termios raw = old_;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
#endif
    }

    ~TerminalRawMode() {
#ifndef _WIN32
        tcsetattr(STDIN_FILENO, TCSANOW, &old_);
#endif
    }

private:
#ifndef _WIN32
    termios old_{};
#endif
};

class SnakeGame {
public:
    SnakeGame(int width, int height)
        : width_(width), height_(height), rng_seed_(static_cast<unsigned int>(std::time(nullptr))) {
        std::srand(rng_seed_);
        reset();
    }

    void run() {
        TerminalRawMode guard;
        printHelp();

        while (!game_over_) {
            processInput();
            update();
            draw();
            std::this_thread::sleep_for(std::chrono::milliseconds(speed_ms_));
        }

        std::cout << "\n游戏结束！最终得分: " << score_ << "\n";
    }

private:
    void reset() {
        snake_.clear();
        snake_.push_back({width_ / 2, height_ / 2});
        snake_.push_back({width_ / 2 - 1, height_ / 2});
        snake_.push_back({width_ / 2 - 2, height_ / 2});

        dir_ = Direction::Right;
        pending_dir_ = dir_;
        score_ = 0;
        game_over_ = false;
        speed_ms_ = 120;

        spawnFood();
    }

    void printHelp() const {
        std::cout << "\033[2J\033[H";
        std::cout << "================ 贪吃蛇（C++ 终端版） ================\n";
        std::cout << "控制: W/A/S/D 或方向键移动，Q 退出\n";
        std::cout << "规则: 撞到自己会失败，穿墙会从另一边出现\n";
        std::cout << "按任意键开始...\n";

        while (readKey() < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    static int readKey() {
#ifdef _WIN32
        if (!_kbhit()) {
            return -1;
        }
        return _getch();
#else
        char ch = '\0';
        if (read(STDIN_FILENO, &ch, 1) <= 0) {
            return -1;
        }
        return static_cast<unsigned char>(ch);
#endif
    }

    void processInput() {
        int ch = readKey();
        if (ch < 0) {
            return;
        }

#ifdef _WIN32
        if (ch == 0 || ch == 224) {
            int arrow = readKey();
            switch (arrow) {
                case 72: setDirection(Direction::Up); break;
                case 80: setDirection(Direction::Down); break;
                case 75: setDirection(Direction::Left); break;
                case 77: setDirection(Direction::Right); break;
                default: break;
            }
            return;
        }
#else
        if (ch == '\033') {
            int c1 = readKey();
            int c2 = readKey();
            if (c1 == '[') {
                switch (c2) {
                    case 'A': setDirection(Direction::Up); break;
                    case 'B': setDirection(Direction::Down); break;
                    case 'C': setDirection(Direction::Right); break;
                    case 'D': setDirection(Direction::Left); break;
                    default: break;
                }
            }
            return;
        }
#endif

        if (ch == 'q' || ch == 'Q') {
            game_over_ = true;
            return;
        }

        switch (ch) {
            case 'w':
            case 'W': setDirection(Direction::Up); break;
            case 's':
            case 'S': setDirection(Direction::Down); break;
            case 'a':
            case 'A': setDirection(Direction::Left); break;
            case 'd':
            case 'D': setDirection(Direction::Right); break;
            default: break;
        }
    }

    void setDirection(Direction next) {
        if ((dir_ == Direction::Up && next == Direction::Down) ||
            (dir_ == Direction::Down && next == Direction::Up) ||
            (dir_ == Direction::Left && next == Direction::Right) ||
            (dir_ == Direction::Right && next == Direction::Left)) {
            return;
        }
        pending_dir_ = next;
    }

    void update() {
        dir_ = pending_dir_;
        Point next = snake_.front();

        switch (dir_) {
            case Direction::Up: --next.y; break;
            case Direction::Down: ++next.y; break;
            case Direction::Left: --next.x; break;
            case Direction::Right: ++next.x; break;
        }

        if (next.x < 0) next.x = width_ - 1;
        if (next.x >= width_) next.x = 0;
        if (next.y < 0) next.y = height_ - 1;
        if (next.y >= height_) next.y = 0;

        for (const auto& s : snake_) {
            if (next == s) {
                game_over_ = true;
                return;
            }
        }

        snake_.push_front(next);
        if (next == food_) {
            score_ += 10;
            if (speed_ms_ > 60) {
                speed_ms_ -= 2;
            }
            spawnFood();
        } else {
            snake_.pop_back();
        }
    }

    void spawnFood() {
        Point p;
        bool overlap;

        do {
            p.x = std::rand() % width_;
            p.y = std::rand() % height_;
            overlap = false;
            for (const auto& s : snake_) {
                if (p == s) {
                    overlap = true;
                    break;
                }
            }
        } while (overlap);

        food_ = p;
    }

    void draw() const {
        std::string board;
        board.reserve((width_ + 3) * (height_ + 3));

        board += "\033[H";
        board += "得分: " + std::to_string(score_) + "  速度(ms): " + std::to_string(speed_ms_) + "\n";

        for (int x = 0; x < width_ + 2; ++x) board += '#';
        board += '\n';

        for (int y = 0; y < height_; ++y) {
            board += '#';
            for (int x = 0; x < width_; ++x) {
                Point p{x, y};
                if (p == snake_.front()) {
                    board += '@';
                } else if (p == food_) {
                    board += '*';
                } else if (isBody(p)) {
                    board += 'o';
                } else {
                    board += ' ';
                }
            }
            board += "#\n";
        }

        for (int x = 0; x < width_ + 2; ++x) board += '#';
        board += '\n';

        std::cout << board << std::flush;
    }

    bool isBody(const Point& p) const {
        for (size_t i = 1; i < snake_.size(); ++i) {
            if (snake_[i] == p) {
                return true;
            }
        }
        return false;
    }

private:
    int width_;
    int height_;
    unsigned int rng_seed_;
    std::deque<Point> snake_;
    Point food_{};
    Direction dir_{Direction::Right};
    Direction pending_dir_{Direction::Right};
    int score_{0};
    int speed_ms_{120};
    bool game_over_{false};
};

int main() {
    SnakeGame game(30, 20);
    game.run();
    return 0;
}