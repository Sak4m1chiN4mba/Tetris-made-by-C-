# Tetris (C++)

A classic Tetris game that runs in your terminal, written in a single C++17 file with no external libraries.

```
  T E T R I S
            +--------------------+
  HOLD      | .  .  .  .  .  .  .|   NEXT
   ██       | .  .  .  .  .  .  .|    ██
   ██████   | .  .  .  .  .  .  .|    ██
            | .  .  .  .  .  .  .|
            | .  .  .  .  .  .  .|   SCORE
            | .  .  .  .  .  .  .|   1200
            | .  .  .  .  .  .  .|
            | .  .  .  .  .  .  .|   LINES
            | .  .  .  .  .  .  .|   8
            | .  .  .  .  .  .  .|
            | .  .  .  .  .  .  .|   LEVEL
            |██ .  .  .  .  .  .|   1
            +--------------------+
```

*(Illustration only. The real game draws colored blocks.)*

## Features

- Standard 10×20 board with all 7 tetrominoes (I, O, T, S, Z, J, L)
- 7-bag randomizer, so you never wait too long for a piece
- Ghost piece showing where the current piece will land
- Hold piece and next-piece preview
- Soft drop and hard drop
- Wall kicks, so you can rotate against walls and the floor
- Scoring that grows with your level, and gravity that speeds up as you clear lines
- Pause, quit, and instant restart after a game over
- Session best score
- Cross-platform: Linux, macOS, and Windows

## Requirements

- A C++17 compiler (g++, clang++, or MSVC)
- A terminal that supports ANSI colors (most modern terminals do, including Windows Terminal)
- A terminal window of at least 46 columns by 26 rows

No third-party libraries are needed. The game uses `termios` on Linux/macOS and `conio.h` on Windows.

## Build

**Linux / macOS**

```bash
g++ -std=c++17 -O2 -o tetris tetris.cpp
```

**Windows (MinGW)**

```bash
g++ -std=c++17 -O2 -o tetris.exe tetris.cpp
```

**Windows (MSVC, from a Developer Command Prompt)**

```bat
cl /std:c++17 /EHsc /O2 tetris.cpp
```

## Run

```bash
./tetris        # Linux / macOS
tetris.exe      # Windows
```

## Controls

| Key | Action |
|---|---|
| `←` `→` or `A` `D` | Move left / right |
| `↑`, `W`, or `X` | Rotate clockwise |
| `Z` | Rotate counter-clockwise |
| `↓` or `S` | Soft drop (one row down) |
| `Space` | Hard drop (instantly lock the piece) |
| `C` | Hold the current piece (once per piece) |
| `P` | Pause / resume |
| `Q` (or `Esc` on Linux/macOS) | Quit |
| `Enter` or `Space` | Play again after a game over |

## How to play

- Pieces fall from the top. Move and rotate them to complete full horizontal rows.
- Completed rows disappear and score points.
- The game ends when a new piece has no room to appear.

### Scoring

| Lines cleared at once | Points |
|---|---|
| 1 | 100 × level |
| 2 | 300 × level |
| 3 | 500 × level |
| 4 (a "Tetris") | 800 × level |

You also earn 1 point per row for a soft drop and 2 points per row for a hard drop.

### Levels and speed

- You level up every 10 lines cleared.
- At level 1 a piece falls once every 800 ms. Each level makes it 20% faster, down to a minimum of 60 ms.

### Good to know

- Pieces lock as soon as they land. There is no lock delay.
- The O piece (square) doesn't rotate.
- If a rotation is blocked, the game tries nudging the piece one or two cells sideways, or one cell up, before giving up.

## Project layout

```
tetris.cpp   # the whole game
README.md
```

Inside `tetris.cpp`:

| Part | What it does |
|---|---|
| `terminalInit` / `terminalRestore` | Switch the terminal to raw, non-blocking input and restore it on exit |
| `readKey` / `mapChar` | Wait for a key press and map arrows and letters to actions |
| `SHAPES`, `SIZES`, `COLORS` | Definitions of the 7 pieces and their colors |
| `Game::run` | Main loop: handles input and gravity timing |
| `Game::fits` / `rotate` / `doHold` | Collision checks, rotation with wall kicks, and the hold slot |
| `Game::lockPiece` | Places the piece, clears lines, updates score and level, spawns the next piece |
| `Game::draw` | Builds the whole frame as one string and prints it |

## Customizing

Edit the constants and functions near the top of `tetris.cpp`:

- **Board size:** change `W` and `H`. The side panels assume the default size, so very different dimensions may need layout tweaks in `Game::draw()`.
- **Speed curve:** adjust `Game::gravityMs()`. The start speed is `800`, each level multiplies it by `0.8`, and the floor is `60`.
- **Scoring:** edit the `points` table in `Game::lockPiece()`.
- **Colors:** change the `COLORS` array, which holds ANSI background color codes.

## Troubleshooting

- **Weird characters like `←[106m` appear:** your terminal doesn't support ANSI escape codes. Use Windows Terminal or another modern terminal.
- **Terminal looks broken after a crash:** run `reset` (Linux/macOS) to restore normal input.
- **The board is cut off or garbled:** make your terminal window at least 46 columns by 26 rows.
- **Arrow keys do nothing:** use `A` / `D` / `S` and `W` or `X` instead.

## License

Add a license of your choice here (for example MIT) before publishing.
