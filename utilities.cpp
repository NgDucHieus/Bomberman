#include "utilities.h"

// Global variable for block size
int block_size = 32;

SDL_Texture* loadTexture(const std::string& filePath, SDL_Renderer* renderer) {
    SDL_Surface* surface = SDL_LoadBMP(filePath.c_str());
    if (!surface) {
        logSDLError("Failed to load surface: " + filePath);
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        logSDLError("Failed to create texture: " + filePath);
    }
    return texture;
}

void logSDLError(const std::string& msg) {
    std::cerr << msg << " SDL_Error: " << SDL_GetError() << std::endl;
}
