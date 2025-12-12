#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <cmath>
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>

#define SCREEN_X_PIXELS 1200.0f
#define SCREEN_Y_PIXELS 1200.0f

// Vertex shader source code
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform vec2 offset;
void main() {
    gl_Position = vec4(aPos.x + offset.x, aPos.y + offset.y, 0.0, 1.0);
}
)";

// Fragment shader source code
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 color;
void main() {
    FragColor = vec4(color, 1.0);
}
)";

// Text vertex shader
const char* textVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec4 vertex;
out vec2 TexCoords;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)";

// Text fragment shader
const char* textFragmentShaderSource = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;
uniform sampler2D text;
uniform vec3 textColor;
void main() {
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = vec4(textColor, 1.0) * sampled;
}
)";

struct Character {
    unsigned int TextureID;
    int SizeX, SizeY;
    int BearingX, BearingY;
    unsigned int Advance;
};

struct FallingText {
    std::string text;
    float x;
    float y;
    float speed;
    float r, g, b;
};

std::map<char, Character> Characters;
std::vector<FallingText> fallingTexts;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window, float& playerX, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float moveSpeed = 1.0f;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        playerX -= moveSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        playerX += moveSpeed * deltaTime;

    // Clamp player position to stay within bounds
    float maxX = 0.7f;
    if (playerX < -maxX) playerX = -maxX;
    if (playerX > maxX) playerX = maxX;
}

unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
}

unsigned int createShaderProgram() {
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

unsigned int createTextShaderProgram() {
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, textVertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, textFragmentShaderSource);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::TEXT_SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

std::vector<std::string> readAllLines(const std::string& filename) {
    std::ifstream file(filename);
    std::vector<std::string> lines;
    std::string line;

    while (std::getline(file, line)) {
        if (!line.empty() && line.find("//") != 0) {
            lines.push_back(line);
        }
    }

    return lines;
}

std::string getNextLine(const std::vector<std::string>& lines, int& currentIndex) {
    if (lines.empty()) return "";

    if (currentIndex >= lines.size()) return "";

    std::string result = lines[currentIndex];

    currentIndex = (currentIndex + 1);
    return result;
}

float getTextWidth(const std::string& text, float scale) {
    float width = 0.0f;
    for (char c : text) {
        if (Characters.find(c) != Characters.end()) {
            Character ch = Characters[c];
            width += (ch.Advance >> 6) * scale;
        }
    }
    return width;
}

float getTextHeight(float scale) {
    return 48.0f * scale;
}

bool checkCollision(float playerX, float playerY, float playerSize,
                    float textX, float textY, const std::string& text, float textScale) {
    // Convert player square from normalized coords to pixel coords
    float playerPixelX = (playerX * SCREEN_Y_PIXELS/2.0f) + SCREEN_Y_PIXELS/2.0f;
    float playerPixelY = (playerY * SCREEN_Y_PIXELS/2.0f) + SCREEN_Y_PIXELS/2.0f;
    float playerPixelSize = playerSize * SCREEN_Y_PIXELS/2.0f;

    // Player square bounds in pixel coords
    float playerLeft = playerPixelX - playerPixelSize;
    float playerRight = playerPixelX + playerPixelSize;
    float playerTop = playerPixelY - playerPixelSize;
    float playerBottom = playerPixelY + playerPixelSize;

    // Text bounds in pixel coords
    float textWidth = getTextWidth(text, textScale);
    float textHeight = getTextHeight(textScale);
    float textLeft = textX;
    float textRight = textX + textWidth;
    float textTop = textY;
    float textBottom = textY + textHeight;

    // AABB collision detection
    return playerLeft < textRight &&
           playerRight > textLeft &&
           playerTop < textBottom &&
           playerBottom > textTop;
}

void renderText(unsigned int shader, unsigned int VAO, unsigned int VBO,
                const std::string& text, float x, float y, float scale,
                float r, float g, float b) {
    glUseProgram(shader);
    glUniform3f(glGetUniformLocation(shader, "textColor"), r, g, b);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    float currentX = x;
    for (char c : text) {
        if (Characters.find(c) == Characters.end()) continue;

        Character ch = Characters[c];

        float xpos = currentX + ch.BearingX * scale;
        float ypos = y - (ch.SizeY - ch.BearingY) * scale;
        float w = ch.SizeX * scale;
        float h = ch.SizeY * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        currentX += (ch.Advance >> 6) * scale;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <red_text_file> <green_text_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " test_text.cpp test_text2.cpp" << std::endl;
        return -1;
    }

    std::string redTextFile = argv[1];
    std::string greenTextFile = argv[2];

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
    GLFWwindow* window = glfwCreateWindow(SCREEN_X_PIXELS, SCREEN_Y_PIXELS, "ClaudeGame - Border Renderer", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Enable blending for text rendering
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Initialize FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return -1;
    }

    // Load font
    FT_Face face;
    if (FT_New_Face(ft, "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 0, &face)) {
        std::cerr << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return -1;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);

    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Load first 128 ASCII characters
    for (unsigned char c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "ERROR::FREETYPE: Failed to load Glyph " << c << std::endl;
            continue;
        }

        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            static_cast<int>(face->glyph->bitmap.width),
            static_cast<int>(face->glyph->bitmap.rows),
            face->glyph->bitmap_left,
            face->glyph->bitmap_top,
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Create shader programs
    unsigned int shaderProgram = createShaderProgram();
    unsigned int textShaderProgram = createTextShaderProgram();

    // Setup text rendering VAO and VBO
    unsigned int textVAO, textVBO;
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Setup orthographic projection for text
    float projection[16] = {
        2.0f/SCREEN_X_PIXELS, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f/SCREEN_Y_PIXELS, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f
    };
    glUseProgram(textShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(textShaderProgram, "projection"), 1, GL_FALSE, projection);

    // Define vertices for outer square (border)
    float outerSize = 0.8f;
    float outerVertices[] = {
        -outerSize, -outerSize,  // bottom left
         outerSize, -outerSize,  // bottom right
         outerSize,  outerSize,  // top right
        -outerSize,  outerSize   // top left
    };

    // Define vertices for inner square (black center)
    float borderThickness = 0.05f;
    float innerSize = outerSize - borderThickness;
    float innerVertices[] = {
        -innerSize, -innerSize,  // bottom left
         innerSize, -innerSize,  // bottom right
         innerSize,  innerSize,  // top right
        -innerSize,  innerSize   // top left
    };

    // Indices for drawing squares
    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    // Setup outer square VAO and VBO
    unsigned int outerVAO, outerVBO, outerEBO;
    glGenVertexArrays(1, &outerVAO);
    glGenBuffers(1, &outerVBO);
    glGenBuffers(1, &outerEBO);

    glBindVertexArray(outerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, outerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(outerVertices), outerVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, outerEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Setup inner square VAO and VBO
    unsigned int innerVAO, innerVBO, innerEBO;
    glGenVertexArrays(1, &innerVAO);
    glGenBuffers(1, &innerVBO);
    glGenBuffers(1, &innerEBO);

    glBindVertexArray(innerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, innerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(innerVertices), innerVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, innerEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Define vertices for player square (small grey square)
    float playerSize = 0.05f;
    float playerVertices[] = {
        -playerSize, -playerSize,  // bottom left
         playerSize, -playerSize,  // bottom right
         playerSize,  playerSize,  // top right
        -playerSize,  playerSize   // top left
    };

    // Setup player square VAO and VBO
    unsigned int playerVAO, playerVBO, playerEBO;
    glGenVertexArrays(1, &playerVAO);
    glGenBuffers(1, &playerVBO);
    glGenBuffers(1, &playerEBO);

    glBindVertexArray(playerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, playerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(playerVertices), playerVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, playerEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Define vertices for health bar background (at top of screen)
    float healthBarBgWidth = 0.6f;
    float healthBarBgHeight = 0.05f;
    float healthBarBgY = 0.85f;
    float healthBarBgVertices[] = {
        -healthBarBgWidth, healthBarBgY - healthBarBgHeight,  // bottom left
         healthBarBgWidth, healthBarBgY - healthBarBgHeight,  // bottom right
         healthBarBgWidth, healthBarBgY + healthBarBgHeight,  // top right
        -healthBarBgWidth, healthBarBgY + healthBarBgHeight   // top left
    };

    // Setup health bar background VAO and VBO
    unsigned int healthBgVAO, healthBgVBO, healthBgEBO;
    glGenVertexArrays(1, &healthBgVAO);
    glGenBuffers(1, &healthBgVBO);
    glGenBuffers(1, &healthBgEBO);

    glBindVertexArray(healthBgVAO);
    glBindBuffer(GL_ARRAY_BUFFER, healthBgVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(healthBarBgVertices), healthBarBgVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, healthBgEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Define vertices for health bar foreground (dynamic)
    unsigned int healthFgVAO, healthFgVBO, healthFgEBO;
    glGenVertexArrays(1, &healthFgVAO);
    glGenBuffers(1, &healthFgVBO);
    glGenBuffers(1, &healthFgEBO);

    glBindVertexArray(healthFgVAO);
    glBindBuffer(GL_ARRAY_BUFFER, healthFgVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, healthFgEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // Get uniform locations
    int colorLoc = glGetUniformLocation(shaderProgram, "color");
    int offsetLoc = glGetUniformLocation(shaderProgram, "offset");

    // Load text lines from both files
    std::vector<std::string> file1Lines = readAllLines(redTextFile);
    std::vector<std::string> file2Lines = readAllLines(greenTextFile);
    int file1Index = 0;
    int file2Index = 0;

    // Player position, health, and timing
    float playerX = 0.0f;
    float playerY = -0.7f;
    float playerHealth = 100.0f;
    const float maxHealth = 100.0f;
    bool isGameOver = false;
    float lastFrame = 0.0f;
    float textSpawnTimer = 0.0f;
    const float textSpawnInterval = 0.5f;
    float fileReloadTimer = 0.0f;
    const float fileReloadInterval = 10.0f;
    float damageTimer = 0.0f;
    const float damageInterval = 0.5f;  // Take damage every 0.5 seconds while colliding
    const float damageAmount = 1.0f;
    bool useFirstFile = true;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Calculate delta time
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window, playerX, deltaTime);

        // Reload text files every 10 seconds
        fileReloadTimer += deltaTime;
        if (fileReloadTimer >= fileReloadInterval) {
            fileReloadTimer = 0.0f;
            file1Lines = readAllLines(redTextFile);
            file2Lines = readAllLines(greenTextFile);
            // Clamp indices to prevent out-of-bounds if files got shorter
            if (file1Index >= file1Lines.size() && !file1Lines.empty()) {
                file1Index = file1Lines.size();
            }
            if (file2Index >= file2Lines.size() && !file2Lines.empty()) {
                file2Index = file2Lines.size();
            }
        }

        // Spawn new falling text every textSpawnInterval seconds, alternating between files
        if (!isGameOver) {
            textSpawnTimer += deltaTime;
            if (textSpawnTimer >= textSpawnInterval) {
                textSpawnTimer = 0.0f;

                std::string nextLine;
                if (useFirstFile) {
                    nextLine = getNextLine(file1Lines, file1Index);
                } else {
                    nextLine = getNextLine(file2Lines, file2Index);
                }

            if (!nextLine.empty()) {
                FallingText newText;
                newText.text = nextLine;
                newText.x = 150.0f;
                newText.y = 850.0f;
                newText.speed = 50.0f;

                // Red for first file, green for second file
                if (useFirstFile) {
                    newText.r = 1.0f;
                    newText.g = 0.0f;
                    newText.b = 0.0f;
                } else {
                    newText.r = 0.0f;
                    newText.g = 1.0f;
                    newText.b = 0.0f;
                }

                fallingTexts.push_back(newText);
            }

            useFirstFile = !useFirstFile;
            }
        }

        // Update falling texts
        for (auto& text : fallingTexts) {
            text.y -= text.speed * deltaTime;
        }

        // Remove texts that have fallen off screen
        fallingTexts.erase(
            std::remove_if(fallingTexts.begin(), fallingTexts.end(),
                [](const FallingText& text) { return text.y < 0.0f; }),
            fallingTexts.end()
        );

        // Check for collisions with player
        bool isColliding = false;
        bool isCollidingWithRed = false;
        for (const auto& text : fallingTexts) {
            if (checkCollision(playerX, playerY, 0.05f, text.x, text.y, text.text, 0.5f)) {
                isColliding = true;
                // Check if this is red text (from first file)
                if (text.r == 1.0f && text.g == 0.0f && text.b == 0.0f) {
                    isCollidingWithRed = true;
                }
            }
        }

        // Apply damage from red text collisions
        if (isCollidingWithRed && !isGameOver) {
            damageTimer += deltaTime;
            if (damageTimer >= damageInterval) {
                damageTimer = 0.0f;
                playerHealth -= damageAmount;
                if (playerHealth <= 0.0f) {
                    playerHealth = 0.0f;
                    isGameOver = true;
                }
            }
        } else {
            damageTimer = 0.0f;  // Reset timer when not colliding
        }

        // Clear screen
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Draw outer square (white border) - no offset
        glUniform2f(offsetLoc, 0.0f, 0.0f);
        glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
        glBindVertexArray(outerVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Draw inner square (black center) - no offset
        glUniform2f(offsetLoc, 0.0f, 0.0f);
        glUniform3f(colorLoc, 0.0f, 0.0f, 0.0f);
        glBindVertexArray(innerVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Draw player square (changes color based on collision)
        glUniform2f(offsetLoc, playerX, playerY);
        if (isColliding) {
            glUniform3f(colorLoc, 1.0f, 0.0f, 0.0f); // Red when colliding
        } else {
            glUniform3f(colorLoc, 0.5f, 0.5f, 0.5f); // Grey normally
        }
        glBindVertexArray(playerVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Draw health bar
        // Background (dark grey)
        glUniform2f(offsetLoc, 0.0f, 0.0f);
        glUniform3f(colorLoc, 0.2f, 0.2f, 0.2f);
        glBindVertexArray(healthBgVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Foreground (color based on health level)
        float healthPercent = playerHealth / maxHealth;
        float healthFgWidth = healthBarBgWidth * healthPercent;
        float healthFgVertices[] = {
            -healthBarBgWidth, healthBarBgY - healthBarBgHeight,  // bottom left
            -healthBarBgWidth + (healthFgWidth * 2.0f), healthBarBgY - healthBarBgHeight,  // bottom right
            -healthBarBgWidth + (healthFgWidth * 2.0f), healthBarBgY + healthBarBgHeight,  // top right
            -healthBarBgWidth, healthBarBgY + healthBarBgHeight   // top left
        };

        glBindBuffer(GL_ARRAY_BUFFER, healthFgVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(healthFgVertices), healthFgVertices);

        glUniform2f(offsetLoc, 0.0f, 0.0f);
        // Color based on health level: green > 66%, yellow > 33%, red <= 33%
        if (healthPercent > 0.66f) {
            glUniform3f(colorLoc, 0.0f, 1.0f, 0.0f); // Green
        } else if (healthPercent > 0.33f) {
            glUniform3f(colorLoc, 1.0f, 1.0f, 0.0f); // Yellow
        } else {
            glUniform3f(colorLoc, 1.0f, 0.0f, 0.0f); // Red
        }
        glBindVertexArray(healthFgVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Draw falling texts with their respective colors
        for (const auto& text : fallingTexts) {
            renderText(textShaderProgram, textVAO, textVBO, text.text,
                      text.x, text.y, 0.5f, text.r, text.g, text.b);
        }

        // Draw "Git Gud" message if game over
        if (isGameOver) {
            renderText(textShaderProgram, textVAO, textVBO, "Git Gud",
                      450.0f, SCREEN_X_PIXELS/2.0f, 1.5f, 1.0f, 0.0f, 0.0f);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &outerVAO);
    glDeleteVertexArrays(1, &innerVAO);
    glDeleteVertexArrays(1, &playerVAO);
    glDeleteVertexArrays(1, &healthBgVAO);
    glDeleteVertexArrays(1, &healthFgVAO);
    glDeleteBuffers(1, &outerVBO);
    glDeleteBuffers(1, &innerVBO);
    glDeleteBuffers(1, &playerVBO);
    glDeleteBuffers(1, &healthBgVBO);
    glDeleteBuffers(1, &healthFgVBO);
    glDeleteBuffers(1, &outerEBO);
    glDeleteBuffers(1, &innerEBO);
    glDeleteBuffers(1, &playerEBO);
    glDeleteBuffers(1, &healthBgEBO);
    glDeleteBuffers(1, &healthFgEBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
