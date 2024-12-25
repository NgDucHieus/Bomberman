#ifndef UTILITIES_H
#define UTILITIES_H

#include <SDL2/SDL.h>
#include <iostream>
#include <string>

extern int block_size; // Block size used for scaling the game grid

// Enum for player directions
enum Direction {
    UP,
    DOWN,
    LEFT,
    RIGHT
};

// Helper function to load textures
SDL_Texture* loadTexture(const std::string& filePath, SDL_Renderer* renderer);

// Utility function to print SDL error messages
void logSDLError(const std::string& msg);

#endif
