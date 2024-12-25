#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <SDL2/SDL_ttf.h>




// Game state enumeration
enum GameState {
    MENU,
    GAME,
    EXIT
};


int block_size = 32; // Kích thước mỗi ô mặc định ban đầu
const int MAP_WIDTH = 13; // Độ rộng bản đồ
const int MAP_HEIGHT = 13; // Chiều cao bản đồ
int playerX = 0; // Vị trí ban đầu của nhân vật 1
int playerY = 0;
int player2X = 12; // Vị trí ban đầu của nhân vật 2
int player2Y = 12;
bool isPlayer1Dead = false;
bool isPlayer2Dead = false;
bool bothDead = false;
bool isMoving = false; // trạng thái di chuyển cho nhân vật 1
bool isMoving2 = false; // trạng thái di chuyển cho nhân vật 2



// // Ma trận bản đồ
// std::vector<std::vector<int>> map = {
//     {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
//     {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1},
//     {1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1},
//     {1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1},
//     {1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1},
//     {1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1},
//     {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
//     {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1},
//     {1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1},
//     {1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1},
//     {1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1},
//     {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1},
//     {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
// };
// Function to generate a random map
void generateRandomMap(std::vector<std::vector<int>>& map) {
    // Initialize the map with 1s (walkable paths)
    for (int i = 0; i < MAP_WIDTH; ++i) {
        for (int j = 0; j < MAP_WIDTH; ++j) {
            map[i][j] = 1; // Set all to walkable
        }
    }

    // Randomly set some cells to 0 (obstacles)
    int numberOfObstacles = (MAP_WIDTH * MAP_WIDTH) / 4; // Set 25% of the cells as obstacles

    srand(static_cast<unsigned int>(time(0))); // Seed for random number generation

    for (int i = 0; i < numberOfObstacles; ++i) {
        int x = rand() % MAP_WIDTH; // Random x coordinate
        int y = rand() % MAP_WIDTH; // Random y coordinate

        // Ensure that we don't overwrite an existing obstacle
        while (map[y][x] == 0) {
            x = rand() % MAP_WIDTH;
            y = rand() % MAP_WIDTH;
        }

        map[y][x] = 0; // Set the cell to 0 (obstacle)
    }
}


std::vector<std::vector<int>> map(MAP_WIDTH, std::vector<int>(MAP_WIDTH));
// Cấu trúc cho bom
struct Bomb {
    int x;
    int y;
    float timer; // Thời gian cho bom nổ
    std::chrono::steady_clock::time_point startTime; // Thời điểm bom được đặt
};

// Danh sách các quả bom
std::vector<Bomb> bombs; 

// Cấu trúc cho hiệu ứng nổ
struct Explosion {
    int x;
    int y;
    std::chrono::steady_clock::time_point startTime; // Thời điểm bắt đầu hiển thị
};
std::vector<Explosion> explosions; // Danh sách các hiệu ứng nổ

// Hướng di chuyển của nhân vật
enum Direction { UP, DOWN, LEFT, RIGHT };
Direction playerDirection = DOWN; // Hướng mặc định là đi xuống
Direction player2Direction = DOWN; // Hướng mặc định cho nhân vật 2

// Biến cho hoạt ảnh
int playerFrame = 0; // Khung hình hiện tại cho nhân vật 1
int player2Frame = 0; // Khung hình hiện tại cho nhân vật 2
float animationTimer = 0.0f; // Thời gian đã trôi qua
const float ANIMATION_DELAY = 0.2f; // Thời gian giữa các khung hình (0.2 giây)

// Tải texture
SDL_Texture* loadTexture(const char* filePath, SDL_Renderer* renderer) {
    SDL_Surface* surface = SDL_LoadBMP(filePath);
    if (!surface) {
        std::cerr << "Failed to load image: " << SDL_GetError() << std::endl;
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

// Vẽ bản đồ
void renderMap(SDL_Renderer* renderer, SDL_Texture* solidTexture, SDL_Texture* stoneTexture) {
    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            SDL_Texture* texture = nullptr;
            if (map[y][x] == 1) {
                texture = stoneTexture;
            } else if (map[y][x] == 0) {
                texture = solidTexture;
            }
            if (texture) {
                SDL_Rect dstRect = { x * block_size, y * block_size, block_size, block_size };
                SDL_RenderCopy(renderer, texture, nullptr, &dstRect);
            }
        }
    }
}

// Vẽ nhân vật
void renderPlayer(SDL_Renderer* renderer, SDL_Texture* playerTextures[4][2], int posX, int posY, Direction direction, int& frame) {
    SDL_Texture* currentTexture = playerTextures[direction][frame]; // Chọn texture dựa trên hướng và khung hình
    SDL_Rect playerRect = { posX * block_size, posY * block_size, block_size, block_size };
    SDL_RenderCopy(renderer, currentTexture, nullptr, &playerRect); // Vẽ hình ảnh nhân vật
}

// Vẽ bom
void renderBombs(SDL_Renderer* renderer, SDL_Texture* bombTexture) {
    for (const Bomb& bomb : bombs) {
        SDL_Rect bombRect = { bomb.x * block_size, bomb.y * block_size, block_size, block_size };
        SDL_RenderCopy(renderer, bombTexture, nullptr, &bombRect); // Vẽ bom
    }
}

// Vẽ hiệu ứng nổ
void renderExplosions(SDL_Renderer* renderer, SDL_Texture* explosionTexture) {
    auto now = std::chrono::steady_clock::now();
    for (const Explosion& explosion : explosions) {
        // Vẽ hiệu ứng nổ
        SDL_Rect explosionRect = { explosion.x * block_size, explosion.y * block_size, block_size, block_size };
        SDL_RenderCopy(renderer, explosionTexture, nullptr, &explosionRect); // Vẽ hiệu ứng nổ

        // Kiểm tra thời gian hiển thị (ví dụ: 1 giây)
        if (std::chrono::duration_cast<std::chrono::seconds>(now - explosion.startTime).count() >= 1) {
            // Xóa hiệu ứng nổ sau 1 giây
            explosions.erase(std::remove_if(explosions.begin(), explosions.end(),
                [&](const Explosion& e) { return e.x == explosion.x && e.y == explosion.y; }), explosions.end());
        }
    }
}

// Kiểm tra xem có thể di chuyển không
bool canMove(int newX, int newY) {
    return (newX >= 0 && newX < MAP_WIDTH && newY >= 0 && newY < MAP_HEIGHT && map[newY][newX] == 1);
}
void initializeSDL(SDL_Window*& window, SDL_Renderer*& renderer, int& block_size) {
    SDL_DisplayMode win_size;
    SDL_GetCurrentDisplayMode(0, &win_size);
    int win_height = win_size.h;
    block_size = win_height / (MAP_HEIGHT + 2);

    window = SDL_CreateWindow("Bomberman", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, MAP_WIDTH * block_size, MAP_HEIGHT * block_size, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        exit(-1);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        exit(-1);
    }
}
void loadIcon(SDL_Window* window) {
    SDL_Surface* iconSurface = IMG_Load("./materials/icon.png");
    if (!iconSurface) {
        std::cerr << "Load icon failed! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        exit(-1);
    }
    SDL_SetWindowIcon(window, iconSurface);
    SDL_FreeSurface(iconSurface);
}
TTF_Font* loadFont(const char* filePath, int fontSize) {
    TTF_Font* font = TTF_OpenFont(filePath, fontSize);
    if (!font) {
        std::cerr << "Error loading font: " << TTF_GetError() << std::endl;
        SDL_Quit();
        exit(-1);
    }
    return font;
}
void destroyTextures(SDL_Texture* textures[4][2]) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 2; ++j) {
            SDL_DestroyTexture(textures[i][j]);
        }
    }
}


void renderMenu(SDL_Renderer* renderer, TTF_Font* font) {
    // Clear the screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Render "Start Game" text
    SDL_Color white = {255, 255, 255};
    SDL_Surface* startSurface = TTF_RenderText_Solid(font, "Start Game", white);
    SDL_Texture* startTexture = SDL_CreateTextureFromSurface(renderer, startSurface);
    int startWidth = startSurface->w;
    int startHeight = startSurface->h;
    SDL_FreeSurface(startSurface);

    SDL_Rect startRect = {(MAP_WIDTH * block_size - startWidth) / 2, MAP_HEIGHT * block_size / 3, startWidth, startHeight};
    SDL_RenderCopy(renderer, startTexture, NULL, &startRect);

    // Render "Quit" text
    SDL_Surface* quitSurface = TTF_RenderText_Solid(font, "Quit", white);
    SDL_Texture* quitTexture = SDL_CreateTextureFromSurface(renderer, quitSurface);
    int quitWidth = quitSurface->w;
    int quitHeight = quitSurface->h;
    SDL_FreeSurface(quitSurface);

    SDL_Rect quitRect = {(MAP_WIDTH * block_size - quitWidth) / 2, MAP_HEIGHT * block_size / 2, quitWidth, quitHeight};
    SDL_RenderCopy(renderer, quitTexture, NULL, &quitRect);

    // Present the menu
    SDL_RenderPresent(renderer);

    // Clean up
    SDL_DestroyTexture(startTexture);
    SDL_DestroyTexture(quitTexture);
}
// Nổ bom
void explodeBomb(int x, int y, SDL_Texture* explosionTexture, SDL_Renderer* renderer) {
    // Danh sách các vị trí bị ảnh hưởng bởi vụ nổ
    std::vector<std::pair<int, int>> affectedPositions;

    // Thêm vị trí bom vào danh sách
    affectedPositions.push_back({x, y});

    // Nổ lên trên
    for (int i = 1; ; ++i) {
        if (y - i >= 0) {
            if (map[y - i][x] == 0) {
                map[y - i][x] = 1; // Chuyển đổi thành stone
                break;
            }
            map[y - i][x] = 1; // Chuyển đổi thành stone
            affectedPositions.push_back({x, y - i});
        } else {
            break;
        }
    }

    // Nổ xuống dưới
    for (int i = 1; ; ++i) {
        if (y + i < MAP_HEIGHT) {
            if (map[y + i][x] == 0) {
                map[y + i][x] = 1; // Chuyển đổi thành stone
                break;
            }
            map[y + i][x] = 1; // Chuyển đổi thành stone
            affectedPositions.push_back({x, y + i});
        } else {
            break;
        }
    }

    // Nổ sang trái
    for (int i = 1; ; ++i) {
        if (x - i >= 0) {
            if (map[y][x - i] == 0) {
                map[y][x - i] = 1; // Chuyển đổi thành stone
                break;
            }
            map[y][x - i] = 1; // Chuyển đổi thành stone
            affectedPositions.push_back({x - i, y});
        } else {
            break;
        }
    }

    // Nổ sang phải
    for (int i = 1; ; ++i) {
        if (x + i < MAP_WIDTH) {
            if (map[y][x + i] == 0) {
                map[y][x + i] = 1; // Chuyển đổi thành stone
                break;
            }
            map[y][x + i] = 1; // Chuyển đổi thành stone
            affectedPositions.push_back({x + i, y});
        } else {
            break;
        }
    }

    // Thêm hiệu ứng nổ vào danh sách
    for (const auto& pos : affectedPositions) {
        explosions.push_back({pos.first, pos.second, std::chrono::steady_clock::now()});
    }

    // Kiểm tra xem có nhân vật nào bị ảnh hưởng bởi vụ nổ không
    bool player1Caught = false;
    bool player2Caught = false;

    for (const auto& pos : affectedPositions) {
        if (playerX == pos.first && playerY == pos.second) {
            player1Caught = true;
        }
        if (player2X == pos.first && player2Y == pos.second) {
            player2Caught = true;
        }
    }

    if (player1Caught && player2Caught) {
        std::cout << "Game Over! Both players were caught in the explosion!" << std::endl;
        isPlayer1Dead = true;
        isPlayer2Dead = true;
        bothDead = true;
        isMoving = false;
        isMoving2 = false;
    } else {
        if (player1Caught) {
            std::cout << "Game Over! Player 1 was caught in the explosion!" << std::endl;
            isPlayer1Dead = true;
            isMoving = false;
        }
        if (player2Caught) {
            std::cout << "Game Over! Player 2 was caught in the explosion!" << std::endl;
            isPlayer2Dead = true;
            isMoving2 = false;
        }
    }
}

// Đặt bom
void placeBomb(int x, int y) {
    Bomb newBomb = { x, y, 1.125, std::chrono::steady_clock::now() }; // Đặt thời gian nổ là 1.125 giây
    bombs.push_back(newBomb);
}

// Hàm vẽ văn bản
void renderText(SDL_Renderer* renderer, const char* text, int x, int y) {
    // Tạo font
    TTF_Font* font = TTF_OpenFont("./materials/font/arial.ttf", 24); // Đảm bảo đã tải SDL_ttf
    if (!font) {
        std::cerr << "Error loading font: " << TTF_GetError() << std::endl;
        return;
    }
    
    // Tạo surface từ văn bản
    SDL_Color color = {255, 0, 0}; // Màu đỏ
    SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, text, color);
    SDL_Texture* message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
    
    // Vẽ văn bản
    SDL_Rect messageRect = {x, y, surfaceMessage->w, surfaceMessage->h};
    SDL_RenderCopy(renderer, message, NULL, &messageRect);
    
    // Giải phóng
    SDL_DestroyTexture(message);
    SDL_FreeSurface(surfaceMessage);
    TTF_CloseFont(font);
}


// Cập nhật trạng thái bom
void updateBombs(SDL_Renderer* renderer, SDL_Texture* explosionTexture) {
    auto now = std::chrono::steady_clock::now();
    for (size_t i = 0; i < bombs.size(); ) {
        // Kiểm tra thời gian nổ
        if (std::chrono::duration_cast<std::chrono::seconds>(now - bombs[i].startTime).count() >= bombs[i].timer) {
            explodeBomb(bombs[i].x, bombs[i].y, explosionTexture, renderer);
            bombs.erase(bombs.begin() + i); // Xóa bom sau khi nổ
        } else {
            ++i; // Chỉ tăng chỉ số nếu không xóa
        }
    }
}

void renderMenu(SDL_Renderer* renderer, TTF_Font* font, int mapWidth, int mapHeight, int blockSize) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Color white = {255, 255, 255};
    SDL_Surface* startSurface = TTF_RenderText_Solid(font, "Start Game", white);
    SDL_Texture* startTexture = SDL_CreateTextureFromSurface(renderer, startSurface);
    int startWidth = startSurface->w;
    int startHeight = startSurface->h;
    SDL_FreeSurface(startSurface);

    SDL_Rect startRect = {(mapWidth * blockSize - startWidth) / 2, mapHeight * blockSize / 3, startWidth, startHeight};
    SDL_RenderCopy(renderer, startTexture, NULL, &startRect);

    SDL_Surface* quitSurface = TTF_RenderText_Solid(font, "Quit", white);
    SDL_Texture* quitTexture = SDL_CreateTextureFromSurface(renderer, quitSurface);
    int quitWidth = quitSurface->w;
    int quitHeight = quitSurface->h;
    SDL_FreeSurface(quitSurface);

    SDL_Rect quitRect = {(mapWidth * blockSize - quitWidth) / 2, mapHeight * blockSize / 2, quitWidth, quitHeight};
    SDL_RenderCopy(renderer, quitTexture, NULL, &quitRect);

    SDL_RenderPresent(renderer);

    SDL_DestroyTexture(startTexture);
    SDL_DestroyTexture(quitTexture);
}

SDL_Texture* loadAndCheckTexture(const char* filePath, SDL_Renderer* renderer);

void loadPlayerTextures(SDL_Texture* textures[4][2], const std::string& basePath, SDL_Renderer* renderer) {
    textures[UP][0] = loadAndCheckTexture((basePath + "/up/1.bmp").c_str(), renderer);
    textures[UP][1] = loadAndCheckTexture((basePath + "/up/2.bmp").c_str(), renderer);
    textures[DOWN][0] = loadAndCheckTexture((basePath + "/down/1.bmp").c_str(), renderer);
    textures[DOWN][1] = loadAndCheckTexture((basePath + "/down/2.bmp").c_str(), renderer);
    textures[LEFT][0] = loadAndCheckTexture((basePath + "/left/1.bmp").c_str(), renderer);
    textures[LEFT][1] = loadAndCheckTexture((basePath + "/left/2.bmp").c_str(), renderer);
    textures[RIGHT][0] = loadAndCheckTexture((basePath + "/right/1.bmp").c_str(), renderer);
    textures[RIGHT][1] = loadAndCheckTexture((basePath + "/right/2.bmp").c_str(), renderer);
}

SDL_Texture* loadAndCheckTexture(const char* filePath, SDL_Renderer* renderer) {
    SDL_Texture* texture = loadTexture(filePath, renderer);
    if (!texture) {
        std::cerr << "Error loading texture: " << filePath << " SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        exit(-1);
    }
    return texture;
}


struct Player {
    int x, y;
    Direction direction;
    bool isMoving;
};


void handlePlayerMovement(Player& player, SDL_Keycode key, bool isKeyDown) {
    int newX = player.x, newY = player.y;

    if (isKeyDown) {
        switch (key) {
            case SDLK_w: case SDLK_UP:    newY--; player.direction = UP; break;
            case SDLK_s: case SDLK_DOWN:  newY++; player.direction = DOWN; break;
            case SDLK_a: case SDLK_LEFT:  newX--; player.direction = LEFT; break;
            case SDLK_d: case SDLK_RIGHT: newX++; player.direction = RIGHT; break;
        }
        player.isMoving = true;
    } else {
        player.isMoving = false;
    }

    if (canMove(newX, newY)) {
        player.x = newX;
        player.y = newY;
    }
}




int main(int argc, char* argv[]) {
    // Initialize SDL map
    generateRandomMap(map);
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        std::cerr << "TTF could not initialize! TTF_Error: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }




    SDL_DisplayMode win_size;
    SDL_GetCurrentDisplayMode(0, &win_size);
    int win_height = win_size.h;
    block_size = win_height / (MAP_HEIGHT + 2);

    SDL_Window* window = SDL_CreateWindow("Bomberman", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, MAP_WIDTH * block_size, MAP_HEIGHT * block_size, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    SDL_Surface* iconSurface = IMG_Load("./materials/icon.png");
    if (iconSurface == NULL) {
        std::cerr << "Load icon failed! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }


    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetWindowIcon(window, iconSurface);
    SDL_FreeSurface(iconSurface);
    // load material
    SDL_Texture* playerTextures[4][2];
    SDL_Texture* player2Textures[4][2];
    SDL_Texture* solidTexture = loadAndCheckTexture("./materials/solid.bmp", renderer);
    SDL_Texture* stoneTexture = loadAndCheckTexture("./materials/stone.bmp", renderer);
    SDL_Texture* bombTexture = loadAndCheckTexture("./materials/bomb.bmp", renderer);
    SDL_Texture* explosionTexture = loadAndCheckTexture("./materials/explosion.bmp", renderer);
    loadPlayerTextures(playerTextures, "./materials/player", renderer);
    loadPlayerTextures(player2Textures, "./materials/player2", renderer);
    //

    if (!solidTexture || !stoneTexture || !bombTexture || !explosionTexture) {
        std::cerr << "Error loading textures" << std::endl;
        SDL_DestroyTexture(solidTexture);
        SDL_DestroyTexture(stoneTexture);
        SDL_DestroyTexture(bombTexture);
        SDL_DestroyTexture(explosionTexture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    GameState gameState = MENU;
    bool running = true;
    SDL_Event event;

    TTF_Font* font = TTF_OpenFont("./materials/font/arial.ttf", 24);
    if (!font) {
        std::cerr << "Error loading font: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (gameState == MENU) {
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_RETURN) {
                        gameState = GAME;
                    } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                }
            } else if (gameState == GAME) {
                if (event.type == SDL_KEYDOWN) {
                    int newX = playerX;
                    int newY = playerY;
                    int newX2 = player2X;
                    int newY2 = player2Y;

                    switch (event.key.keysym.sym) {
                        case SDLK_w:    newY--; playerDirection = UP; isMoving = true; break;
                        case SDLK_s:    newY++; playerDirection = DOWN; isMoving = true; break;
                        case SDLK_a:    newX--; playerDirection = LEFT; isMoving = true; break;
                        case SDLK_d:    newX++; playerDirection = RIGHT; isMoving = true; break;
                        case SDLK_SPACE:
                            if (!isPlayer1Dead && !isPlayer2Dead) {
                                placeBomb(playerX, playerY);
                            }
                            break;
                    }

                    switch (event.key.keysym.sym) {
                        case SDLK_UP:    newY2--; player2Direction = UP; isMoving2 = true; break;
                        case SDLK_DOWN:  newY2++; player2Direction = DOWN; isMoving2 = true; break;
                        case SDLK_LEFT:  newX2--; player2Direction = LEFT; isMoving2 = true; break;
                        case SDLK_RIGHT: newX2++; player2Direction = RIGHT; isMoving2 = true; break;
                        case SDLK_KP_ENTER:
                            if (!isPlayer2Dead && !isPlayer1Dead) {
                                placeBomb(player2X, player2Y);
                            }
                            break;
                    }

                    if (canMove(newX, newY)) {
                        playerX = newX;
                        playerY = newY;
                    }

                    if (canMove(newX2, newY2)) {
                        player2X = newX2;
                        player2Y = newY2;
                    }
                } else if (event.type == SDL_KEYUP) {
                    if (event.key.keysym.sym == SDLK_w || event.key.keysym.sym == SDLK_s || 
                        event.key.keysym.sym == SDLK_a || event.key.keysym.sym == SDLK_d) {
                        isMoving = false;
                    }
                    if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_DOWN || 
                        event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT) {
                        isMoving2 = false;
                    }
                }
            }
        }

        if (gameState == MENU) {
            renderMenu(renderer, font);
        } else if (gameState == GAME) {
            if (isMoving) {
                animationTimer += 0.016f;
                if (animationTimer >= ANIMATION_DELAY) {
                    playerFrame = (playerFrame + 1) % 2;
                    animationTimer = 0.0f;
                }
            } else {
                playerFrame = 0;
            }

            if (isMoving2) {
                animationTimer += 0.016f;
                if (animationTimer >= ANIMATION_DELAY) {
                    player2Frame = (player2Frame + 1) % 2;
                    animationTimer = 0.0f;
                }
            } else {
                player2Frame = 0;
            }

            updateBombs(renderer, explosionTexture);

            SDL_RenderClear(renderer);
            renderMap(renderer, solidTexture, stoneTexture);
            if (!isPlayer1Dead) {
                renderPlayer(renderer, playerTextures, playerX, playerY, playerDirection, playerFrame);
            }
            if (!isPlayer2Dead) {
                renderPlayer(renderer, player2Textures, player2X, player2Y, player2Direction, player2Frame);
            }
            renderBombs(renderer, bombTexture);
            renderExplosions(renderer, explosionTexture);

            if (bothDead) {
                int textWidth, textHeight;
                TTF_SizeText(font, "Both players are dead!", &textWidth, &textHeight);
                int centerX = (MAP_WIDTH * block_size - textWidth) / 2;
                int centerY = (MAP_HEIGHT * block_size - textHeight) / 2;
                renderText(renderer, "Both players are dead!", centerX, centerY);
            } else if (isPlayer1Dead && !isPlayer2Dead) {
                int textWidth, textHeight;
                TTF_SizeText(font, "Player 1 is dead!", &textWidth, &textHeight);
                int centerX = (MAP_WIDTH * block_size - textWidth) / 2;
                int centerY = (MAP_HEIGHT * block_size - textHeight) / 2;
                renderText(renderer, "Player 1 is dead!", centerX, centerY);
            } else if (isPlayer2Dead && !isPlayer1Dead) {
                int textWidth, textHeight;
                TTF_SizeText(font, "Player 2 is dead!", &textWidth, &textHeight);
                int centerX = (MAP_WIDTH * block_size - textWidth) / 2;
                int centerY = (MAP_HEIGHT * block_size - textHeight) / 2;
                renderText(renderer, "Player 2 is dead!", centerX, centerY);
            }

            SDL_RenderPresent(renderer);
        }
    }

    TTF_CloseFont(font);
    SDL_DestroyTexture(solidTexture);
    SDL_DestroyTexture(stoneTexture);
    SDL_DestroyTexture(bombTexture);
    SDL_DestroyTexture(explosionTexture);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 2; ++j) {
            SDL_DestroyTexture(playerTextures[i][j]);
            SDL_DestroyTexture(player2Textures[i][j]);
        }
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
