#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <filesystem>
#include <vector>
#include <string>
#include <chrono>
#include <ctime>
#include <sys/stat.h>

namespace fs = std::filesystem;
using namespace std;

// Convert file_time_type to time_t
time_t to_time_t(const fs::file_time_type& ftime) {
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + system_clock::now()
    );
    return system_clock::to_time_t(sctp);
}

uintmax_t getDirectorySize(const fs::path& dir) {
    uintmax_t size = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (fs::is_regular_file(entry)) {
                size += fs::file_size(entry);
            }
        }
    } catch (...) {
        // Ignore errors (e.g., permission denied)
    }
    return size;
}


// Get UNIX-style file permissions
string getPermissions(const fs::path& path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) return "??????????";

    string prefix = "---";
    if (fs::is_symlink(path)) prefix[0] = 'l';
    if (info.st_mode & S_ISUID) prefix[1] = 'u';
    if (info.st_mode & S_ISGID) prefix[2] = 'g';

    string perms;
    perms += (S_ISDIR(info.st_mode)) ? 'd' : '-';
    perms += (info.st_mode & S_IRUSR) ? 'r' : '-';
    perms += (info.st_mode & S_IWUSR) ? 'w' : '-';
    perms += (info.st_mode & S_IXUSR) ? 'x' : '-';
    perms += (info.st_mode & S_IRGRP) ? 'r' : '-';
    perms += (info.st_mode & S_IWGRP) ? 'w' : '-';
    perms += (info.st_mode & S_IXGRP) ? 'x' : '-';
    perms += (info.st_mode & S_IROTH) ? 'r' : '-';
    perms += (info.st_mode & S_IWOTH) ? 'w' : '-';
    perms += (info.st_mode & S_IXOTH) ? 'x' : '-';

    return prefix + " " + perms;
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        cerr << "SDL_Init error: " << SDL_GetError() << endl;
        return 1;
    }

    if (TTF_Init() != 0) {
        cerr << "TTF_Init error: " << TTF_GetError() << endl;
        SDL_Quit();
        return 1;
    }

    TTF_Font* font = TTF_OpenFont("2.ttf", 28); // Smaller font
    if (!font) {
        cerr << "Font load error: " << TTF_GetError() << endl;
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_CreateWindowAndRenderer(1920, 1080, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS, &window, &renderer);

    string path = "/home/pale/prog";
    vector<string> columnLabels = {"Name", "File size", "Creation date", "Access rights"};
    vector<vector<string>> tableData;

    SDL_Color backgroundColor = {30, 30, 30, 255};
    SDL_Color lineColor = {200, 200, 200, 255};
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Color redColor = {255, 0, 0, 255};

    // Load file data recursively
    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        vector<string> row;
        row.push_back(entry.path().filename().string()); // Just the name

        uintmax_t size = 0;
        if (fs::is_regular_file(entry)) {
            size = fs::file_size(entry);
        } else if (fs::is_directory(entry)) {
            size = getDirectorySize(entry.path());
        }
        row.push_back(to_string(size) + " bytes");


        auto ftime = fs::last_write_time(entry);
        time_t cftime = to_time_t(ftime);
        string dateStr = std::asctime(std::localtime(&cftime));
        dateStr.pop_back();
        row.push_back(dateStr);

        row.push_back(getPermissions(entry.path())); // UNIX-style rights

        tableData.push_back(row);
    }

    int lines = 4;
    int rows = tableData.size() + 1; // +1 for header

    int inLineThickness = 2;
    int outLineThickness = 4;

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        int screenWidth, screenHeight;
        SDL_GetRendererOutputSize(renderer, &screenWidth, &screenHeight);
        int cellWidth = screenWidth / lines;
        int cellHeight = screenHeight / rows;

        SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, lineColor.r, lineColor.g, lineColor.b, lineColor.a);
        for (int i = 1; i < lines; ++i) {
            int x = i * cellWidth;
            SDL_Rect vLine = {x, 0, inLineThickness, screenHeight};
            SDL_RenderFillRect(renderer, &vLine);
        }
        for (int j = 1; j < rows; ++j) {
            int y = j * cellHeight;
            SDL_Rect hLine = {0, y, screenWidth, inLineThickness};
            SDL_RenderFillRect(renderer, &hLine);
        }

        SDL_SetRenderDrawColor(renderer, redColor.r, redColor.g, redColor.b, redColor.a);
        SDL_Rect leftBorder   = {0, 0, outLineThickness, screenHeight};
        SDL_Rect topBorder    = {0, 0, screenWidth, outLineThickness};
        SDL_Rect rightBorder  = {screenWidth - outLineThickness, 0, outLineThickness, screenHeight};
        SDL_Rect bottomBorder = {0, screenHeight - outLineThickness, screenWidth, outLineThickness};

        SDL_RenderFillRect(renderer, &leftBorder);
        SDL_RenderFillRect(renderer, &topBorder);
        SDL_RenderFillRect(renderer, &rightBorder);
        SDL_RenderFillRect(renderer, &bottomBorder);

        // Draw column headers
        for (int i = 0; i < lines; ++i) {
            string label = (i < columnLabels.size()) ? columnLabels[i] : "";

            SDL_Surface* textSurface = TTF_RenderText_Solid(font, label.c_str(), textColor);
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

            int textW, textH;
            SDL_QueryTexture(textTexture, nullptr, nullptr, &textW, &textH);

            int x = i * cellWidth + (cellWidth - textW) / 2;
            int y = (cellHeight - textH) / 2;

            SDL_Rect textRect = {x, y, textW, textH};
            SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);

            SDL_FreeSurface(textSurface);
            SDL_DestroyTexture(textTexture);
        }

        // Draw file data
        for (int row = 0; row < tableData.size(); ++row) {
            for (int col = 0; col < lines && col < tableData[row].size(); ++col) {
                string cellText = tableData[row][col];

                SDL_Surface* textSurface = TTF_RenderText_Solid(font, cellText.c_str(), textColor);
                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

                int textW, textH;
                SDL_QueryTexture(textTexture, nullptr, nullptr, &textW, &textH);

                int x = col * cellWidth + (cellWidth - textW) / 2;
                int y = (row + 1) * cellHeight + (cellHeight - textH) / 2;

                SDL_Rect textRect = {x, y, textW, textH};
                SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);

                SDL_FreeSurface(textSurface);
                SDL_DestroyTexture(textTexture);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
