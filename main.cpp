#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <SDL2/SDL_ttf.h>
#include <cstdlib> // For rand()
#include <ctime>   // For seeding with time()
#include <set> // Để sử dụng std::set cho lịch sử vị trí

std::set<std::pair<int, int>> visitedPositions; // Lưu các vị trí đã đi qua




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


void renderMenu(SDL_Renderer* renderer, TTF_Font* font, SDL_Rect& botRect, SDL_Rect& humanRect) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Color white = {255, 255, 255};

    // Option 1: Play with Bot
    SDL_Surface* botSurface = TTF_RenderText_Solid(font, "Play with Bot", white);
    SDL_Texture* botTexture = SDL_CreateTextureFromSurface(renderer, botSurface);
    int botWidth = botSurface->w;
    int botHeight = botSurface->h;
    SDL_FreeSurface(botSurface);

    botRect = { (MAP_WIDTH * block_size - botWidth) / 2, MAP_HEIGHT * block_size / 4, botWidth, botHeight };
    SDL_RenderCopy(renderer, botTexture, NULL, &botRect);

    // Option 2: Play with Human
    SDL_Surface* humanSurface = TTF_RenderText_Solid(font, "Play with Human", white);
    SDL_Texture* humanTexture = SDL_CreateTextureFromSurface(renderer, humanSurface);
    int humanWidth = humanSurface->w;
    int humanHeight = humanSurface->h;
    SDL_FreeSurface(humanSurface);

    humanRect = { (MAP_WIDTH * block_size - humanWidth) / 2, MAP_HEIGHT * block_size / 2, humanWidth, humanHeight };
    SDL_RenderCopy(renderer, humanTexture, NULL, &humanRect);

    // Present the menu
    SDL_RenderPresent(renderer);

    SDL_DestroyTexture(botTexture);
    SDL_DestroyTexture(humanTexture);
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

const int BOT_UPDATE_INTERVAL = 500; // Bot updates every 200ms
Uint32 lastBotUpdateTime = 0;        // Time of the last bot update


struct Bot {
    int x, y;           // Current position
    int lastX, lastY;   // Previous position
    Direction direction;
    bool isMoving;
    bool isDead;

    // Constructor to initialize all fields
    Bot(int startX, int startY, Direction startDirection, bool startMoving, bool startDead)
        : x(startX), y(startY), lastX(startX), lastY(startY),
          direction(startDirection), isMoving(startMoving), isDead(startDead) {}
};



bool isWalkable(int x, int y) {
    return x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT && map[y][x] == 1;
}

Bot aiBot = {6, 6, DOWN, false, false}; // Initialize bot at position (6, 6)

void shuffleDirections(Direction directions[], int size) {
    for (int i = size - 1; i > 0; --i) {
        int j = rand() % (i + 1); // Random index from 0 to i
        std::swap(directions[i], directions[j]);
    }
}
bool isPlayerNear(int botX, int botY, int playerX, int playerY, int range = 1) {
    return (std::abs(botX - playerX) + std::abs(botY - playerY)) <= range;
}


void aiBotLogic() {
    // Lấy thời gian hiện tại
    Uint32 currentTime = SDL_GetTicks();

    // Chỉ cập nhật bot nếu đủ thời gian từ lần cập nhật trước
    if (currentTime - lastBotUpdateTime < BOT_UPDATE_INTERVAL) {
        return; // Bỏ qua lần cập nhật này
    }

    // Cập nhật thời gian cho lần tiếp theo
    lastBotUpdateTime = currentTime;

    // Nếu bot gần người chơi, đặt bom tại vị trí trước đó
    if (isPlayerNear(aiBot.x, aiBot.y, player2X, player2Y)) {
        placeBomb(aiBot.lastX, aiBot.lastY); // Đặt bom ở vị trí trước đó
        return; // Không di chuyển trong lượt này
    }

    // Lưu vị trí hiện tại trước khi di chuyển
    aiBot.lastX = aiBot.x;
    aiBot.lastY = aiBot.y;

    // Các hướng di chuyển khả dụng
    Direction possibleDirections[4] = {UP, DOWN, LEFT, RIGHT};
    shuffleDirections(possibleDirections, 4); // Trộn ngẫu nhiên các hướng

    // Duyệt qua các hướng để tìm vị trí mới
    for (Direction dir : possibleDirections) {
        int newX = aiBot.x, newY = aiBot.y;
        switch (dir) {
            case UP:    newY--; break;
            case DOWN:  newY++; break;
            case LEFT:  newX--; break;
            case RIGHT: newX++; break;
        }

        // Kiểm tra nếu vị trí chưa được ghé qua và có thể đi được
        if (isWalkable(newX, newY) && visitedPositions.find({newX, newY}) == visitedPositions.end()) {
            aiBot.x = newX;
            aiBot.y = newY;
            aiBot.direction = dir;
            aiBot.isMoving = true;

            // Lưu vị trí vào lịch sử
            visitedPositions.insert({newX, newY});
            return; // Kết thúc di chuyển
        }
    }

    // Nếu không có vị trí mới, bot có thể quay lại một vị trí cũ
    for (Direction dir : possibleDirections) {
        int newX = aiBot.x, newY = aiBot.y;
        switch (dir) {
            case UP:    newY--; break;
            case DOWN:  newY++; break;
            case LEFT:  newX--; break;
            case RIGHT: newX++; break;
        }

        if (isWalkable(newX, newY)) { // Di chuyển đến vị trí cũ nếu không còn lựa chọn
            aiBot.x = newX;
            aiBot.y = newY;
            aiBot.direction = dir;
            aiBot.isMoving = true;
            return; // Kết thúc di chuyển
        }
    }

    // Nếu không di chuyển được, bot đứng yên
    aiBot.isMoving = false;
}




enum GameMode {
    WITH_BOT,
    WITH_HUMAN
};

GameMode currentGameMode = WITH_BOT; // Default mode

int main(int argc, char* argv[]) {
    SDL_Rect botRect, humanRect;

    srand(static_cast<unsigned int>(time(0))); // Seed random generator

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
    if (!iconSurface) {
        std::cerr << "Load icon failed! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetWindowIcon(window, iconSurface);
    SDL_FreeSurface(iconSurface);

    // Load textures and materials
    SDL_Texture* playerTextures[4][2];
    SDL_Texture* player2Textures[4][2];
    SDL_Texture* solidTexture = loadAndCheckTexture("./materials/solid.bmp", renderer);
    SDL_Texture* stoneTexture = loadAndCheckTexture("./materials/stone.bmp", renderer);
    SDL_Texture* bombTexture = loadAndCheckTexture("./materials/bomb.bmp", renderer);
    SDL_Texture* explosionTexture = loadAndCheckTexture("./materials/explosion.bmp", renderer);
    loadPlayerTextures(playerTextures, "./materials/player", renderer);
    loadPlayerTextures(player2Textures, "./materials/player2", renderer);

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

    // Initialize game state and variables
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

    // Main game loop
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (gameState == MENU) {
                // Handle mouse clicks for menu options
                if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                    int mouseX = event.button.x;
                    int mouseY = event.button.y;

                    if (mouseX >= botRect.x && mouseX <= botRect.x + botRect.w &&
                        mouseY >= botRect.y && mouseY <= botRect.y + botRect.h) {
                        currentGameMode = WITH_BOT;
                        gameState = GAME;
                    } else if (mouseX >= humanRect.x && mouseX <= humanRect.x + humanRect.w &&
                               mouseY >= humanRect.y && mouseY <= humanRect.y + humanRect.h) {
                        currentGameMode = WITH_HUMAN;
                        gameState = GAME;
                    }
                }
            } else if (gameState == GAME) {
                if (event.type == SDL_KEYDOWN) {
                    int newX2 = player2X;
                    int newY2 = player2Y;

                    switch (event.key.keysym.sym) {
                        case SDLK_UP:    newY2--; player2Direction = UP; isMoving2 = true; break;
                        case SDLK_DOWN:  newY2++; player2Direction = DOWN; isMoving2 = true; break;
                        case SDLK_LEFT:  newX2--; player2Direction = LEFT; isMoving2 = true; break;
                        case SDLK_RIGHT: newX2++; player2Direction = RIGHT; isMoving2 = true; break;
                        case 109:
                            if (!isPlayer2Dead) {
                                placeBomb(player2X, player2Y);
                            }
                            break;
                    }

                    if (canMove(newX2, newY2)) {
                        player2X = newX2;
                        player2Y = newY2;
                    }
                } else if (event.type == SDL_KEYUP) {
                    if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_DOWN ||
                        event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT) {
                        isMoving2 = false;
                    }
                }
            }
        }

        if (gameState == GAME) {
            aiBotLogic();
            // placeBomb(aiBot.x, aiBot.y);

        }

        // Rendering
        if (gameState == MENU) {
            renderMenu(renderer, font, botRect, humanRect);
        } else if (gameState == GAME) {
            updateBombs(renderer, explosionTexture); // Cập nhật trạng thái bom

            SDL_RenderClear(renderer);
            renderMap(renderer, solidTexture, stoneTexture);

            if (!aiBot.isDead) {
                renderPlayer(renderer, playerTextures, aiBot.x, aiBot.y, aiBot.direction, playerFrame);
            }
            if (!isPlayer2Dead) {
                renderPlayer(renderer, player2Textures, player2X, player2Y, player2Direction, player2Frame);
            }
            renderBombs(renderer, bombTexture);
            renderExplosions(renderer, explosionTexture);

            SDL_RenderPresent(renderer);
        }
    }

    // Cleanup
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
