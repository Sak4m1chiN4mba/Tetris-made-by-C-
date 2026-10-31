// Classic Tetris - terminal game in C++17
// Works on Linux, macOS (termios) and Windows (conio). No external libraries.
//
// Build:  g++ -std=c++17 -O2 -o tetris tetris.cpp
//
// Controls
//   Left/Right or A/D : move          Up / W / X : rotate clockwise
//   Down or S         : soft drop     Z          : rotate counter-clockwise
//   Space             : hard drop     C          : hold piece
//   P                 : pause         Q          : quit

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
  #include <conio.h>
  #include <windows.h>
#else
  #include <sys/select.h>
  #include <termios.h>
  #include <unistd.h>
#endif

using Clock = std::chrono::steady_clock;

constexpr int W = 10;
constexpr int H = 20;

enum class Key { None, Left, Right, SoftDrop, RotCW, RotCCW, HardDrop, Hold, Pause, Quit, Enter };

struct Point { int x, y; };

// ---------------------------------------------------------------- terminal
#ifndef _WIN32
static termios g_origTerm;
#endif

void terminalInit() {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(h, &mode);
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    tcgetattr(STDIN_FILENO, &g_origTerm);
    termios raw = g_origTerm;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
#endif
    std::cout << "\033[?25l\033[2J" << std::flush;  // hide cursor, clear screen
}

void terminalRestore() {
    std::cout << "\033[?25h\033[0m\n" << std::flush;
#ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSANOW, &g_origTerm);
#endif
}

Key mapChar(int c) {
    switch (c) {
        case 'a': case 'A': return Key::Left;
        case 'd': case 'D': return Key::Right;
        case 's': case 'S': return Key::SoftDrop;
        case 'w': case 'W': case 'x': case 'X': return Key::RotCW;
        case 'z': case 'Z': return Key::RotCCW;
        case ' ': return Key::HardDrop;
        case 'c': case 'C': return Key::Hold;
        case 'p': case 'P': return Key::Pause;
        case 'q': case 'Q': return Key::Quit;
        case '\r': case '\n': return Key::Enter;
    }
    return Key::None;
}

// Waits up to `ms` milliseconds for a key. Returns Key::None on timeout.
Key readKey(int ms) {
#ifdef _WIN32
    auto end = Clock::now() + std::chrono::milliseconds(ms);
    do {
        if (_kbhit()) {
            int c = _getch();
            if (c == 0 || c == 224) {
                switch (_getch()) {
                    case 72: return Key::RotCW;
                    case 80: return Key::SoftDrop;
                    case 75: return Key::Left;
                    case 77: return Key::Right;
                }
                return Key::None;
            }
            return mapChar(c);
        }
        Sleep(2);
    } while (Clock::now() < end);
    return Key::None;
#else
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    timeval tv{ms / 1000, (ms % 1000) * 1000};
    if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0) return Key::None;

    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return Key::None;
    if (c == '\033') {  // arrow keys arrive as ESC [ A/B/C/D
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return Key::Quit;  // bare ESC
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return Key::None;
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return Key::RotCW;
                case 'B': return Key::SoftDrop;
                case 'C': return Key::Right;
                case 'D': return Key::Left;
            }
        }
        return Key::None;
    }
    return mapChar(c);
#endif
}

// ------------------------------------------------------------------ pieces
// Order: I, O, T, S, Z, J, L
constexpr int SIZES[7] = {4, 2, 3, 3, 3, 3, 3};
constexpr int SHAPES[7][4][2] = {
    {{0, 1}, {1, 1}, {2, 1}, {3, 1}},  // I
    {{0, 0}, {1, 0}, {0, 1}, {1, 1}},  // O
    {{1, 0}, {0, 1}, {1, 1}, {2, 1}},  // T
    {{1, 0}, {2, 0}, {0, 1}, {1, 1}},  // S
    {{0, 0}, {1, 0}, {1, 1}, {2, 1}},  // Z
    {{0, 0}, {0, 1}, {1, 1}, {2, 1}},  // J
    {{2, 0}, {0, 1}, {1, 1}, {2, 1}},  // L
};
const char* COLORS[7] = {
    "\033[106m", "\033[103m", "\033[105m", "\033[102m",
    "\033[101m", "\033[104m", "\033[48;5;208m",
};

struct Piece {
    int type = 0;
    int size = 3;
    std::array<Point, 4> c{};
    int x = 0, y = 0;
};

Piece makePiece(int type) {
    Piece p;
    p.type = type;
    p.size = SIZES[type];
    for (int i = 0; i < 4; ++i) p.c[i] = {SHAPES[type][i][0], SHAPES[type][i][1]};
    p.x = (W - p.size) / 2;
    p.y = 0;
    return p;
}

std::string pad(const std::string& s, int w) {
    return (int)s.size() >= w ? s.substr(0, w) : s + std::string(w - s.size(), ' ');
}

// -------------------------------------------------------------------- game
class Game {
public:
    Game() : rng_(std::random_device{}()) {}

    // Returns true if the player wants to play again.
    bool run() {
        reset();
        draw();
        auto nextDrop = Clock::now() + std::chrono::milliseconds(gravityMs());

        while (true) {
            int rem = 100;
            if (!paused_) {
                rem = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                          nextDrop - Clock::now()).count();
                if (rem < 0) rem = 0;
            }

            Key k = readKey(rem);
            bool alive = true;

            if (k == Key::Quit) return false;
            if (k == Key::Pause) {
                paused_ = !paused_;
                if (!paused_) nextDrop = Clock::now() + std::chrono::milliseconds(gravityMs());
                draw();
                continue;
            }
            if (paused_) continue;

            bool locked = false;
            switch (k) {
                case Key::Left:  tryMove(-1, 0); break;
                case Key::Right: tryMove(1, 0);  break;
                case Key::RotCW:  rotate(true);  break;
                case Key::RotCCW: rotate(false); break;
                case Key::SoftDrop:
                    if (fits(cur_, 0, 1)) { cur_.y++; score_ += 1; }
                    else { alive = lockPiece(); locked = true; }
                    break;
                case Key::HardDrop:
                    while (fits(cur_, 0, 1)) { cur_.y++; score_ += 2; }
                    alive = lockPiece();
                    locked = true;
                    break;
                case Key::Hold:
                    alive = doHold();
                    locked = true;
                    break;
                default: break;
            }

            if (alive && Clock::now() >= nextDrop) {
                if (fits(cur_, 0, 1)) cur_.y++;
                else alive = lockPiece();
                locked = true;
            }
            if (locked) nextDrop = Clock::now() + std::chrono::milliseconds(gravityMs());

            draw();
            if (!alive) break;
        }

        // Game over
        std::cout << "\033[" << (3 + H / 2) << ";" << 17 << "H\033[1;97;41m  GAME OVER  \033[0m"
                  << "\033[" << (H + 5) << ";1H\033[K Enter = play again, Q = quit" << std::flush;
        while (true) {
            Key k = readKey(1000);
            if (k == Key::Enter || k == Key::HardDrop) return true;
            if (k == Key::Quit) return false;
        }
    }

private:
    int grid_[H][W] = {};     // 0 = empty, otherwise piece type + 1
    Piece cur_;
    int next_ = 0;
    int hold_ = -1;
    bool canHold_ = true;
    bool paused_ = false;
    int score_ = 0, lines_ = 0, level_ = 0, best_ = 0;
    std::vector<int> bag_;
    std::mt19937 rng_;

    void reset() {
        for (auto& row : grid_) for (int& v : row) v = 0;
        score_ = lines_ = level_ = 0;
        hold_ = -1;
        canHold_ = true;
        paused_ = false;
        bag_.clear();
        next_ = drawFromBag();
        spawn();
        std::cout << "\033[2J" << std::flush;
    }

    int drawFromBag() {  // 7-bag randomizer
        if (bag_.empty()) {
            for (int i = 0; i < 7; ++i) bag_.push_back(i);
            std::shuffle(bag_.begin(), bag_.end(), rng_);
        }
        int t = bag_.back();
        bag_.pop_back();
        return t;
    }

    int gravityMs() const {
        double ms = 800;
        for (int i = 0; i < level_; ++i) ms *= 0.8;
        return ms < 60 ? 60 : (int)ms;
    }

    bool fits(const Piece& p, int dx, int dy) const {
        for (const Point& q : p.c) {
            int nx = p.x + q.x + dx, ny = p.y + q.y + dy;
            if (nx < 0 || nx >= W || ny >= H) return false;
            if (ny >= 0 && grid_[ny][nx]) return false;
        }
        return true;
    }

    bool spawn() {
        cur_ = makePiece(next_);
        next_ = drawFromBag();
        canHold_ = true;
        return fits(cur_, 0, 0);
    }

    void tryMove(int dx, int dy) {
        if (fits(cur_, dx, dy)) { cur_.x += dx; cur_.y += dy; }
    }

    void rotate(bool cw) {
        if (cur_.type == 1) return;  // O piece never changes
        Piece r = cur_;
        for (Point& q : r.c) {
            Point o = q;
            q = cw ? Point{r.size - 1 - o.y, o.x} : Point{o.y, r.size - 1 - o.x};
        }
        // Simple wall kicks: try shifting sideways, then up.
        static const int kicks[6][2] = {{0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {0, -1}};
        for (const auto& k : kicks) {
            if (fits(r, k[0], k[1])) {
                r.x += k[0];
                r.y += k[1];
                cur_ = r;
                return;
            }
        }
    }

    bool doHold() {
        if (!canHold_) return true;
        if (hold_ < 0) {
            hold_ = cur_.type;
            bool ok = spawn();
            canHold_ = false;
            return ok;
        }
        int t = cur_.type;
        cur_ = makePiece(hold_);
        hold_ = t;
        canHold_ = false;
        return fits(cur_, 0, 0);
    }

    // Lock the current piece, clear lines, spawn next. Returns false on game over.
    bool lockPiece() {
        for (const Point& q : cur_.c) {
            int nx = cur_.x + q.x, ny = cur_.y + q.y;
            if (ny < 0) return false;  // locked above the board
            grid_[ny][nx] = cur_.type + 1;
        }

        int cleared = 0;
        for (int y = H - 1; y >= 0;) {
            bool full = true;
            for (int x = 0; x < W; ++x) if (!grid_[y][x]) { full = false; break; }
            if (full) {
                for (int yy = y; yy > 0; --yy)
                    for (int x = 0; x < W; ++x) grid_[yy][x] = grid_[yy - 1][x];
                for (int x = 0; x < W; ++x) grid_[0][x] = 0;
                ++cleared;
            } else {
                --y;
            }
        }

        static const int points[5] = {0, 100, 300, 500, 800};
        score_ += points[cleared] * (level_ + 1);
        lines_ += cleared;
        level_ = lines_ / 10;
        if (score_ > best_) best_ = score_;

        return spawn();
    }

    // 8 visible characters showing row `row` (0 or 1) of a piece preview.
    static std::string previewRow(int type, int row) {
        std::string s;
        for (int cx = 0; cx < 4; ++cx) {
            bool filled = false;
            if (type >= 0)
                for (int i = 0; i < 4; ++i)
                    if (SHAPES[type][i][0] == cx && SHAPES[type][i][1] == row) filled = true;
            s += filled ? std::string(COLORS[type]) + "  \033[0m" : "  ";
        }
        return s;
    }

    void draw() const {
        // Build what to display on the board: 0 empty, 1..7 filled, -1 ghost.
        int disp[H][W];
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) disp[y][x] = grid_[y][x];

        Piece ghost = cur_;
        while (fits(ghost, 0, 1)) ghost.y++;
        for (const Point& q : ghost.c) {
            int nx = ghost.x + q.x, ny = ghost.y + q.y;
            if (ny >= 0 && ny < H) disp[ny][nx] = -1;
        }
        for (const Point& q : cur_.c) {
            int nx = cur_.x + q.x, ny = cur_.y + q.y;
            if (ny >= 0 && ny < H) disp[ny][nx] = cur_.type + 1;
        }

        // Side panels: each entry is exactly 12 visible characters.
        std::vector<std::string> left(H, std::string(12, ' '));
        std::vector<std::string> right(H, std::string(12, ' '));
        left[0] = pad(" HOLD", 12);
        left[1] = "  " + previewRow(hold_, 0) + "  ";
        left[2] = "  " + previewRow(hold_, 1) + "  ";
        right[0] = pad(" NEXT", 12);
        right[1] = "  " + previewRow(next_, 0) + "  ";
        right[2] = "  " + previewRow(next_, 1) + "  ";
        right[4] = pad(" SCORE", 12);
        right[5] = pad(" " + std::to_string(score_), 12);
        right[7] = pad(" LINES", 12);
        right[8] = pad(" " + std::to_string(lines_), 12);
        right[10] = pad(" LEVEL", 12);
        right[11] = pad(" " + std::to_string(level_ + 1), 12);
        right[13] = pad(" BEST", 12);
        right[14] = pad(" " + std::to_string(best_), 12);

        std::string out = "\033[H";
        out += "\033[1;36m  T E T R I S\033[0m";
        out += paused_ ? "   \033[33m[PAUSED]\033[0m\033[K\n" : "\033[K\n";

        std::string border = "+" + std::string(W * 2, '-') + "+";
        out += std::string(12, ' ') + border + "\n";
        for (int y = 0; y < H; ++y) {
            out += left[y] + "|";
            for (int x = 0; x < W; ++x) {
                int v = disp[y][x];
                if (v > 0)       out += std::string(COLORS[v - 1]) + "  \033[0m";
                else if (v < 0)  out += "\033[90m[]\033[0m";
                else             out += "\033[38;5;238m .\033[0m";
            }
            out += "|" + right[y] + "\n";
        }
        out += std::string(12, ' ') + border + "\n";
        out += " \xe2\x86\x90\xe2\x86\x92 move  \xe2\x86\x91/X rotate  Z rotate back  \xe2\x86\x93 soft drop\033[K\n";
        out += " Space hard drop  C hold  P pause  Q quit\033[K\n";
        std::cout << out << std::flush;
    }
};

int main() {
    terminalInit();
    {
        Game game;
        while (game.run()) {}
    }
    terminalRestore();
    return 0;
}
