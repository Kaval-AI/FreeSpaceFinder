# FreeSpaceFinder

FreeSpaceFinder is a Qt 6 desktop application for finding out where your disk space went. Point it at one or more folders, let it scan, and explore the results in an interactive tree view alongside a summary of the biggest space hogs. An optional built-in AI assistant (powered by the Claude API) can discuss the scan results with you and suggest what is safe to clean up.

## Features

- **Folder scanning** — add any number of folders and scan them in a background thread with live progress and cancellation support.
- **Size tree view** — browse the scanned hierarchy with per-item sizes; open a file's folder or copy its path straight from the view.
- **Analysis summary** — tabbed overview of the scan, including:
  - largest files and largest directories
  - space usage broken down by file type
  - old files (not accessed in over a year) and very old files (over three years)
  - empty directories
- **AI chat assistant** — a chat panel that sends the analysis as context to Claude, with streamed responses, so you can ask things like "what can I safely delete?"

## Requirements

- CMake ≥ 3.16 and Ninja
- A C++17 compiler
- Qt 6 (Widgets, Network, Concurrent, Test modules)
- Optional: an Anthropic API key for the AI assistant

## Building

The project ships with CMake presets (`debug`, `release`, `relwithdebinfo`). If Qt is not on your default prefix path, copy `CMakeUserPresets.json.sample` to `CMakeUserPresets.json` and set `CMAKE_PREFIX_PATH` to your Qt installation (e.g. `/path/to/Qt/6.x.x/gcc_64`).

```sh
cmake --preset release
cmake --build --preset release
./build/release/FreeSpaceFinder
```

## Running the tests

Unit tests cover the core (non-GUI) library: the file tree, scanner, analyzer, and file model.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug
```

## AI assistant setup

The chat panel needs an Anthropic API key. Either:

- enter it in the app under **Settings** (it is stored obfuscated in the application settings), or
- set the `ANTHROPIC_API_KEY` environment variable before launching.

Without a key, the rest of the application works normally; only the chat panel is disabled.

## Project layout

```
src/
  scanner.*        Recursive directory scanner (runs on a worker thread)
  filenode.h       In-memory file tree built by the scanner
  filemodel.*      Qt item model exposing the tree to the view
  analyzer.*       Derives the analysis (largest files, by type, old files, ...)
  aiclient.*       Streaming client for the Anthropic Messages API
  summarypanel.*   Tabbed summary UI
  aichatwidget.*   Chat UI for the AI assistant
  mainwindow.*     Main window wiring it all together
tests/             Qt Test unit tests for the core library
resources/         Application icons
```

The core logic is built as a separate static library (`FreeSpaceFinderCore`, no GUI dependencies) that both the application and the unit tests link against.

## License

FreeSpaceFinder is licensed under the GNU Affero General Public License v3.0 — see [LICENSE.txt](LICENSE.txt).
