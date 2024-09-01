#pragma once
/**
 **   [NOTE]: Since I chose to not overcomplicate this project by adding 
 **           extra files/folders, every struct/method must stay in the 
 **           top-down order below to prevent compiler whining.
 **           If you do add more files, be sure to set their Relative Path
 **           and add them to the CMakeLists.txt "add_executable(...)".
 **           https://github.com/GuildOfCalamity?tab=repositories
 **/
#include <random>
#include <filesystem>
#include <SFML/Graphics.hpp>
#include <SFML/System/Vector2.hpp>
#include <iostream>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#pragma region [Constants]
sf::Vector2u const window_size = { 1920u, 1080u };
sf::Vector2f const window_size_f = static_cast<sf::Vector2f>(window_size);
uint32_t const max_framerate = 29;
uint32_t const max_count = 30000;
uint32_t first = 0;
bool const useTexture = true;
float speed = 0.85f;
float const radius = 40.0f;
float const farDist = 11.0f;
float const nearDist = 0.1f; // originally 1.0, but was adjusted to account for the star-free-zone
float const dt = 1.0f / static_cast<float>(max_framerate);
#pragma endregion

#pragma region [Model]
// Structs
struct Star
{
    sf::Vector2f position;
    float z = 1.0f;
    //sf::Color color;
};
#pragma endregion

#pragma region [Generation]
/// <summary>
/// Returns a vector of stars based on max_count.
/// </summary>
std::vector<Star> createStars(uint32_t count, float scale)
{
    std::vector<Star> stars;
    stars.reserve(count);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    // Define star-free zone in middle camera view .
    sf::Vector2f const window_world_size = window_size_f * nearDist;
    sf::FloatRect const star_free_zone = { -window_world_size * 0.5f, window_world_size};

    // Generate randomly distributed stars on the screen.
    for (uint32_t i{ count }; i--;)
    {
        float const x = (dis(gen) - 0.5f) * window_size_f.x * scale;
        float const y = (dis(gen) - 0.5f) * window_size_f.y * scale;
        float const z = dis(gen) * (farDist - nearDist) + nearDist;
        
        // Discard anything that falls in the star-free-zone.
        if (star_free_zone.contains(x, y))
        {
            ++i;
            continue;
        }

        stars.push_back({ {x, y}, z });
    }

    // Sort the depth to prevent stars from being drawn over each other.
    std:sort(stars.begin(), stars.end(), [](Star const& s_1, Star const& s_2)
    {
        return s_1.z > s_2.z;
    });

    return stars;
}
#pragma endregion

/// <summary>
/// For improving draw call performance when large amounts of stars are required.
/// </summary>
void updateGeometry(uint32_t idx, Star const& s, sf::VertexArray& va)
{
    // Determine size based on distance.
    float const scale = 1.0f / s.z;
    
    // Distant stars should be dimmer.
    float const depth_ratio = (s.z - nearDist) / (farDist - nearDist);
    float const color_ratio = 1.0f - depth_ratio;

    sf::Vector2f const p = s.position * scale;
    float const r = radius * scale;
    uint32_t const i = 4 * idx;
    // Calculate each vertex.
    va[i + 0].position = { p.x - r, p.y - r };
    va[i + 1].position = { p.x + r, p.y - r };
    va[i + 2].position = { p.x + r, p.y + r };
    va[i + 3].position = {p.x - r, p.y + r};

    #pragma region [Color]
    auto const clr = static_cast<uint8_t>(color_ratio * 255.0f);
    auto const clr2 = static_cast<uint8_t>(clr / 1.15);
    sf::Color color{ clr2, clr2, clr };
    switch (rand() % 3)
    {
        case 0: color = { clr2, clr2, clr };
            break;
        case 1: color = { clr, clr2, clr2 };
            break;
        case 2: color = { clr, clr, clr2 };
            break;
        default:
            break;
    }
    va[i + 0].color = color;
    va[i + 1].color = color;
    va[i + 2].color = color;
    va[i + 3].color = color;
    #pragma endregion
}

/// <summary>
/// Don't run multiple instances.
/// </summary>
bool isAlreadyRunning() 
{
    // Create a named mutex
    HANDLE hMutex = CreateMutex(nullptr, FALSE, "Star-Demo-SFML");
    if (hMutex == nullptr) {
        std::cerr << "CreateMutex failed, error: " << GetLastError() << std::endl;
        return true; // Assume running if mutex creation fails
    }
    // Check if another instance exists
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return true;
    }
    return false;
}

/// <summary>
/// Entry point.
/// </summary>
int main()
{
    if (isAlreadyRunning())
        return 1;

    // Basic init
    auto window = sf::RenderWindow{ { window_size.x, window_size.y }, "Stars", sf::Style::Fullscreen };
    window.setFramerateLimit(max_framerate);
    window.setMouseCursorVisible(false);

    sf::Texture texture;
    if (useTexture)
    {
        // Look for texture asset in current folder.
        std::filesystem::path asset = std::filesystem::current_path() / "star.png";
        texture.loadFromFile(asset.generic_string());
        if (texture.getSize().x > 0)
        {
            texture.setSmooth(true);
            texture.generateMipmap();
        }
        else // the pathing can be squirelly when launched from the OS
        {
            texture.loadFromFile("C:\\Windows\\System32\\star.png");
            texture.setSmooth(true);
            texture.generateMipmap();
        }
    }

    std::vector<Star> stars = createStars(max_count, farDist);

    // For draw call performance, setup an array to hold 4 vertices per star.
    sf::VertexArray va{sf::PrimitiveType::Quads, 4 * max_count};

    if (useTexture)
    {
        // Fill texture coords by defining each vertex association.
        auto const texture_size_f = static_cast<sf::Vector2f>(texture.getSize());
        for (uint32_t idx{ max_count }; idx--;)
        {
            uint32_t const i = 4 * idx;
            va[i + 0].texCoords = { 0.0f, 0.0f };
            va[i + 1].texCoords = { texture_size_f.x, 0.0f };
            va[i + 2].texCoords = { texture_size_f.x, texture_size_f.y };
            va[i + 3].texCoords = { 0.0f, texture_size_f.y };
        }
    }

    // Main loop
    while (window.isOpen())
    {
        #pragma region [Events]
        // Listens for event as long as the main window is open.
        for (auto event = sf::Event{}; window.pollEvent(event);)
        {
            if (event.type == sf::Event::Closed)
            {
                window.close();
            }
            else if (event.type == sf::Event::KeyPressed)
            {
                if (event.key.code == sf::Keyboard::Escape)
                {
                    window.close();
                }
                else if (event.key.code == sf::Keyboard::PageUp)
                {
                    speed += 0.3f;
                }
                else if (event.key.code == sf::Keyboard::PageDown)
                {
                    if (speed > 0.3f)
                        speed -= 0.3f;
                }
            }
            else if (event.type == sf::Event::MouseButtonPressed)
            {
                window.close();
            }
        }
        #pragma endregion

        #pragma region [Calculation]
        // Adjust the travel distance and recycle if past camera.
        // Process from back to front to prevent z-figthing on boundary.
        for (uint32_t i{ max_count }; i--;)
        {
            Star& s = stars[i];

            s.z -= speed * dt;

            // Re-use old stars by offsetting the over-shoot to keep spacing 
            // constant and prevent having to generate more objects.
            if (s.z < nearDist)
            {
                s.z = farDist - (nearDist - s.z);

                // The star is now the first we need to draw because it's further away.
                first = i;
            }
        }
        #pragma endregion

        #pragma region [Rendering]
        window.clear();

        //sf::CircleShape shape { radius };
        //shape.setOrigin(radius, radius);
        for (uint32_t i{0}; i < max_count; ++i)
        {
            uint32_t const idx = (i + first) % max_count;
            Star const& s = stars[idx];

            updateGeometry(i, s, va);
            /* 
             *  This was the old method of draw call once per star, but this is costly. 
             */
            //float const scale = 1.0f / s.z;
            //shape.setPosition(s.position * scale + window_size_f * 0.5f);
            //shape.setScale(scale, scale);
            //float const depth_ratio = (s.z - near) / (farDist - nearDist);
            //float const color_ratio = 1.0f - depth_ratio;
            //auto const clr = static_cast<uint8_t>(color_ratio * 255.0f);
            //auto const clr2 = static_cast<uint8_t>(clr / 1.21);
            //shape.setFillColor({ clr2, clr2, clr });
            //window.draw(shape);
        }

        if (useTexture)
        {
            sf::RenderStates states;
            states.transform.translate(window_size_f * 0.5f);
            states.texture = &texture;
            window.draw(va, states);
        }
        else
        {
            sf::Transform tf;
            tf.translate(window_size_f * 0.5f);
            window.draw(va, tf);
        }

        window.display();
        #pragma endregion
    }
}
