#include <fstream>
#include <SDL.h>
#include <vector>
#include <random>
#include <iostream>
#include <cstdint>
#include <array>

const std::array<uint8_t, 80> FONTSET = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

class Screen {
private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
public:
    Screen(int scale = 10) {
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow("CHIP-8 Screen", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 64 * scale, 32 * scale, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 64, 32);
    }

    ~Screen() {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void Draw(const std::array<uint8_t, 64 * 32>& vram) {
        std::array<uint32_t, 64 * 32> pixels{};
        for (size_t i = 0; i < vram.size(); ++i) {
            pixels[i] = vram[i] ? 0xFFFFFFFF : 0xFF000000;
        }

        SDL_UpdateTexture(texture, nullptr, pixels.data(), 64 * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
};

struct Chip8CPU {
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<int> dist{0, 255};

    void ExecuteCXNN(uint8_t X, uint8_t NN) {
        uint8_t randomByte = static_cast<uint8_t>(dist(gen));

        V[X] = randomByte & NN;
    }

    std::array<uint8_t, 4096> ram{}; // RAM memory

    std::array<uint8_t, 64 * 32> vram{}; // Video RAM (screen buffer)

    std::array<uint8_t, 16> V{}; // General-purpose registers (V[0xF] is the flag register, should not be modified directly by the user)

    uint16_t I = 0; // Index register (points to data in RAM)
    uint16_t pc = 0x200; // Program Counter (points to the current instruction; ROM starts at 0x200)

    std::array<uint16_t, 16> stack{}; // Call stack
    uint8_t sp = 0; //stack pointer

    uint8_t delayTimer = 0; // Delay timer (decrements at 60Hz)
    uint8_t soundTimer = 0; // Sound timer (decrements at 60Hz, plays sound when > 0)

    std::array<uint8_t, 16> keypad{}; // Keypad state: 1 if pressed, 0 if released

    Chip8CPU() {
        std::copy(FONTSET.begin(), FONTSET.end(), ram.begin());
    }

    void Step() {
        uint16_t opcode = (ram[pc] << 8) | ram[pc + 1];
        pc += 2;

        uint8_t opType = (opcode & 0xF000) >> 12;
        uint8_t X = (opcode & 0x0F00) >> 8;
        uint8_t Y = (opcode & 0x00F0) >> 4;
        uint8_t N = (opcode & 0x000F);
        uint8_t NN = (opcode & 0x00FF);
        uint16_t NNN = (opcode & 0x0FFF);

        switch(opType) {
        case 0:
            switch(N) {
                case 0:
                //00E0 
                //CLEAN DISPLAY
                    vram.fill(0);
                    break;
                case 0xE:
                //OOEE 
                //RETURN
                    sp--;
                    pc = stack[sp];
                    break;
            }
            break;

        case 1:
            //1NNN
            //jmp(JP)
            pc = NNN;
            break;

        case 2:
            //2NNN
            //CALL 
            stack[sp] = pc;
            sp++;
            pc = NNN;
            break;

        case 3:
        // 3XNN 
        //SKIP NEXT INSTRUCTION IF V[X] == NN
            if (V[X] == NN) {
                pc += 2;
            }
            break;

        case 4: {
        //4XNN
        //SKIP NEXT INSTRUCTION IF V[X] != NN
            if (V[X] != NN) {
                pc += 2;
            }
            break;
        }

        case 5: {
        //5XY0
        //SKIP NEXT INSTRUCTION IF V[X] == V[Y]
            if (V[X] == V[Y]) {
                pc += 2;
            }
            break;
        }

        case 6: {
        //6XNN
        // MOV V[X], NN
            V[X] = NN;
            break;
        }

        case 7: {
        //7XNN
        //ADD V[X], NN
            V[X] += NN;
            break;
        }

        case 8:
            switch(N) {
            case 0:
            //8XY0
            //MOV V[X], V[Y]
                V[X] = V[Y];
                break;

            case 1:
            //8XY1
            // V[X] |= V[Y]
                V[X] |= V[Y];
                break;

            case 2:
            //8XY2
            //AND V[X], V[Y]
                V[X] &= V[Y];
                break;

            case 3:
                V[X] ^= V[Y];
                break;

            case 4: {
            //8XY4
            //ADD V[X], V[Y]
                uint16_t sum = V[X] + V[Y];
                V[X] = sum & 0xFF;
                if (sum > 255) {
                    V[0xF] = 1;
                } else {
                    V[0xF] = 0; 
                }
                break;
            }

            case 5:
            //8XY5
            //SUB V[X], V[Y]
                if (V[X] >= V[Y]) {
                    V[0xF] = 1;
                } else {
                    V[0xF] = 0; 
                }
                V[X] -= V[Y];
                break;

            case 6: {
            //8XY6
            //V[X] >> 1
                uint8_t bit = V[X] & 1;
                V[0xF] = bit;
                V[X] = V[X] >> 1;
                break;
            }

            case 7:
            //8XY7 
            //SUB V[Y], V[X]
            //MOV V[X], V[Y]
                if (V[Y] >= V[X]) {
                    V[0xF] = 1;
                } else {
                    V[0xF] = 0; 
                }
                V[X] = V[Y] - V[X];
                break;
            
            case 0xE: {
                //8XYE
                uint8_t bit = (V[X] >> 7) & 1;
                V[0xF] = bit;
                V[X] = V[X] << 1;
                break;
            }
            }
            break;

        case 9:
            if (V[X] != V[Y]) {
                pc += 2;
            }
            break;

        case 0xA:
            I = NNN;
            break;
            
        case 0xB:
            pc = NNN + V[0];
            break;

        case 0XC:
            ExecuteCXNN(X, NN);    
            break;   

        case 0XD: {
            V[0xF] = 0;
            uint16_t saved_I = I;
            uint8_t start_x = V[X];
            uint8_t start_y = V[Y];
            for (int i = 0; i < N; ++i) {
                uint8_t sprite = ram[saved_I + i];
                uint8_t y = (start_y + i) % 32;
                for (int j = 0; j < 8; ++j) {
                    uint8_t x = (start_x + j) % 64;
                    uint16_t pixelIdx = x + (y * 64);
                    if (vram[pixelIdx] == 1 && ((sprite >> (7 - j)) & 1) == 1) {
                        V[0xF] = 1;
                    }
                    vram[pixelIdx] ^= (sprite >> (7 - j)) & 1;
                }
            } 
            break;
        }

        case 0xE: {
            uint8_t key = V[X];
            switch(NN) {
                case 0x9E:
                    //EX9E
                    // 
                    if (keypad[key] == 1) {
                        pc += 2;
                    }
                    break;

                case 0xA1:
                    //EXA1
                    if (keypad[key] == 0) {
                        pc += 2;
                    }
                    break;
            }
            break;
        }
        
        case 0xF:
            switch(NN) {
            case 0x07:
            //FX07
            //MOV V[X], delayTimer
                V[X] = delayTimer;
                break;
            
            case 0x0A: {
            // FX0A
                bool keyPressed = false;
                for (int i = 0; i < 16; ++i) {
                    if (keypad[i] == 1) {
                        keyPressed = true;
                        V[X] = i;
                        break;
                    }
                }

                if (!keyPressed) {
                    pc -= 2;
                }
                break;
            }
            case 0x15:
            //FX15
                delayTimer = V[X];
                break;

            case 0x18:
            // FX18
                soundTimer = V[X];
                break;

            case 0x1E:
            //FX1E
                I += V[X];
                break;

            case 0x29: {
            //FX29
                uint8_t digit = V[X] & 0x0F;
                I = digit * 5;
                break;
            }
            case 0x33:{
            //FX33
                uint8_t value = V[X];
                ram[I]     = value / 100;
                ram[I + 1] = (value / 10) % 10;
                ram[I + 2] = value % 10;
                break;
            }
            case 0x55:
            //FX55
                for (int i = 0; i <= X; ++i) {
                    ram[I + i] = V[i];
                }
                break;

            case 0x65:
            //FX65
                for (int i = 0; i <= X; ++i) {
                    V[i] = ram[I + i];
                }
            }
            break;
        }
    }

    bool LoadROM(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Failed to open ROM: " << path << std::endl;
            return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (size > (4096 - 0x200)) {
            std::cerr << "ROM is too large" << std::endl;
            return false;
        }

        file.read(reinterpret_cast<char*>(&ram[0x200]), size);
        pc = 0x200;
        return true;
    }
};

int MapKey(SDL_Keycode sym) {
    switch (sym) {
        // 1-я строка: 1, 2, 3, 4 -> 0x1, 0x2, 0x3, 0xC
        case SDLK_1: return 0x1;
        case SDLK_2: return 0x2;
        case SDLK_3: return 0x3;
        case SDLK_4: return 0xC;

        // 2-я строка: Q, W, E, R -> 0x4, 0x5, 0x6, 0xD
        case SDLK_q: return 0x4;
        case SDLK_w: return 0x5;
        case SDLK_e: return 0x6;
        case SDLK_r: return 0xD;

        // 3-я строка: A, S, D, F -> 0x7, 0x8, 0x9, 0xE
        case SDLK_a: return 0x7;
        case SDLK_s: return 0x8;
        case SDLK_d: return 0x9;
        case SDLK_f: return 0xE;

        // 4-я строка: Z, X, C, V -> 0xA, 0x0, 0xB, 0xF
        case SDLK_z: return 0xA;
        case SDLK_x: return 0x0;
        case SDLK_c: return 0xB;
        case SDLK_v: return 0xF;

        default: return -1;
    }
}

int main(int argc, char* argv[]) {
    Screen screen(23);
    Chip8CPU cpu;
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <rom.ch8>\n";
        return 1;
    }

    std::string romPath = argv[1];

    if (!cpu.LoadROM(romPath)) {
        return 1;
    }

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (event.type == SDL_KEYDOWN) {
                int key = MapKey(event.key.keysym.sym);
                if (key != -1) {
                    cpu.keypad[key] = 1;
                }
            }

            if (event.type == SDL_KEYUP) {
                int key = MapKey(event.key.keysym.sym);
                if (key != -1) {
                    cpu.keypad[key] = 0;
                }
            }
        }

        for (int i = 0; i < 9; ++i) {
            cpu.Step();
        }

        if (cpu.delayTimer > 0) {
        cpu.delayTimer--;
        }
        if (cpu.soundTimer > 0) {
            cpu.soundTimer--;
        }

        screen.Draw(cpu.vram);
        SDL_Delay(16);
    }
    return 0;
}