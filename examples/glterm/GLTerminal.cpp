/**
 * This file is part of the "libterminal" project
 *   Copyright (c) 2019 Christian Parpart <christian@parpart.family>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "GLTerminal.h"
#include <terminal/Util.h>
#include <iostream>

using namespace std;
using namespace std::placeholders;
using namespace terminal;

auto const envvars = terminal::Process::Environment{
    {"TERM", "xterm-256color"},
    {"COLORTERM", "xterm"},
    {"COLORFGBG", "15;0"},
    {"LINES", ""},
    {"COLUMNS", ""},
    {"TERMCAP", ""}
};

#ifndef GLTERM_FONT_PATH
#define GLTERM_FONT_PATH "C:\\WINDOWS\\FONTS\\CONSOLA.TTF"
// Hmm, how'd that look like on Linux, again? :-D
#endif

GLTerminal::GLTerminal(unsigned _width, unsigned _height,
                       unsigned _fontSize, string const& _shell,
                       glm::mat4 const& _projectionMatrix) :
    width_{ _width },
    height_{ _height },
    textShaper_{ GLTERM_FONT_PATH , _fontSize, _projectionMatrix },
    cellBackground_{
        textShaper_.maxAdvance(),
        textShaper_.lineHeight(),
        _projectionMatrix
    },
    terminal_{
        terminal::WindowSize{
            static_cast<unsigned short>(width_ / textShaper_.maxAdvance()),
            static_cast<unsigned short>(height_ / textShaper_.lineHeight())
        },
        [this](auto const& msg) { /*TODO(traceLog) cout << "terminal: " << msg << '\n'; */ },
        bind(&GLTerminal::onScreenUpdateHook, this, _1),
    },
    process_{ terminal_, _shell, {_shell}, envvars },
    processExitWatcher_{ [this]() { wait(); }}
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

GLTerminal::~GLTerminal()
{
    wait();
}

bool GLTerminal::alive() const
{
    return alive_;
}

void GLTerminal::resize(unsigned _width, unsigned _height)
{
    width_ = _width;
    height_ = _height;

    auto const winSize = terminal::WindowSize{
        static_cast<unsigned short>(height_ / textShaper_.lineHeight()),
        static_cast<unsigned short>(width_ / textShaper_.maxAdvance())
    };
    auto const usedHeight = winSize.rows * textShaper_.lineHeight();
    auto const usedWidth = winSize.columns * textShaper_.maxAdvance();
    auto const freeHeight = _height - usedHeight;
    auto const freeWidth = _width - usedWidth;

    cout << fmt::format("Resized to {}x{} ({}x{}) (free: {}x{}) (CharBox: {}x{})\n",
        winSize.columns, winSize.rows,
        _width, _height,
        freeWidth, freeHeight,
        textShaper_.maxAdvance(), textShaper_.lineHeight()
    );

    terminal_.resize(winSize);
}

void GLTerminal::setProjection(glm::mat4 const& _projectionMatrix)
{
    cellBackground_.setProjection(_projectionMatrix);
    textShaper_.setProjection(_projectionMatrix);
}

void GLTerminal::render()
{
    auto const winSize = terminal_.size();
    auto const usedHeight = winSize.rows * textShaper_.lineHeight();
    auto const usedWidth = winSize.columns * textShaper_.maxAdvance();
    auto const freeHeight = height_ - usedHeight;
    auto const freeWidth = width_ - usedWidth;
    auto const bottomMargin = freeHeight / 2;
    auto const leftMargin = freeWidth / 2;

    using namespace terminal;

    auto constexpr defaultForegroundColor = RGBColor{ 255, 255, 255 };
    auto constexpr defaultBackgroundColor = RGBColor{ 0, 32, 32 };

    auto const makeCoords = [&](cursor_pos_t col, cursor_pos_t row) {
        return glm::ivec2{
            leftMargin + (col - 1) * textShaper_.maxAdvance(),
            bottomMargin + (winSize.rows - row) * textShaper_.lineHeight()
        };
    };

    terminal_.render([&](cursor_pos_t row, cursor_pos_t col, Screen::Cell const& cell) {
        cellBackground_.render(
            makeCoords(col, row),
            toRGB(cell.attributes.backgroundColor, defaultBackgroundColor)
        );

        RGBColor const fgColor = toRGB(cell.attributes.foregroundColor, defaultForegroundColor);
        //TODO: other SGRs

        if (cell.character && cell.character != ' ')
        {
            textShaper_.render(
                makeCoords(col, row),
                cell.character,
                fgColor.red / 255.0f,
                fgColor.green / 255.0f,
                fgColor.blue / 255.0f
            );
        }
    });
}

void GLTerminal::wait()
{
    using namespace terminal;
    while (true)
        if (visit(overloaded{[&](Process::NormalExit) { return true; },
                             [&](Process::SignalExit) { return true; },
                             [&](Process::Suspend) { return false; },
                             [&](Process::Resume) { return false; },
                  },
                  process_.wait()))
            break;

    terminal_.wait();
    processExitWatcher_.join();
    alive_ = false;
}

void GLTerminal::onScreenUpdateHook(std::vector<terminal::Command> const& _commands)
{
}
