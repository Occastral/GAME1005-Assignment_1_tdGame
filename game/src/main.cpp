#include "raylib.h"
#include "raymath.h"
#include "raygui.h"
#include "time.h"

#include <cassert>
#include <array>
#include <vector>
#include <algorithm>

// SCREEN & TILE SETUP =====================================================

constexpr float SCREEN_SIZE = 800;
constexpr int TILE_COUNT = 20;
constexpr float TILE_SIZE = SCREEN_SIZE / TILE_COUNT;
float timer = 0.0f;

enum TileType : int
{
    STONE,      // Marks unoccupied space, can be overwritten 
    PATH,       // Marks the path, cannot be overwritten
    WAYPOINT,   // Marks where the path turns, cannot be overwritten
    COUNT
};

struct Cell
{
    int row;
    int col;
};

constexpr std::array<Cell, 4> DIRECTIONS{ Cell{ -1, 0 }, Cell{ 1, 0 }, Cell{ 0, -1 }, Cell{ 0, 1 } };

inline bool InBounds(Cell cell, int rows = TILE_COUNT, int cols = TILE_COUNT)
{
    return cell.col >= 0 && cell.col < cols && cell.row >= 0 && cell.row < rows;
}

//--------------------------------------------------------------------------

void DrawTile(int row, int col, Color color)
{
    DrawRectangle(col * TILE_SIZE, row * TILE_SIZE, TILE_SIZE, TILE_SIZE, color);
}

void DrawTile(int row, int col, int type)
{
    Color colors[3] =
    {
        GRAY,
        LIGHTGRAY,
        WHITE
    };
    DrawTile(row, col, colors[type]);
}

Vector2 TileCenter(int row, int col)
{
    float x = col * TILE_SIZE + TILE_SIZE * 0.5f;
    float y = row * TILE_SIZE + TILE_SIZE * 0.5f;
    return { x, y };
}

//--------------------------------------------------------------------------

// Returns a collection of adjacent cells that match the search value.
std::vector<Cell> FloodFill(Cell start, int tiles[TILE_COUNT][TILE_COUNT], TileType searchValue)
{
    std::vector<Cell> result;
    std::vector<Cell> open;                     // Places we want to search
    bool closed[TILE_COUNT][TILE_COUNT]{};      // Places we've already searched
    for (int row = 0; row < TILE_COUNT; row++)
    {
        for (int col = 0; col < TILE_COUNT; col++)
        {
            closed[row][col] = tiles[row][col] == 0;    // Avoid searching for Zero-Tiles
        }
    }

    // Add the starting cell to the exploration queue & search till there's nothing left!
    open.push_back(start);
    while (!open.empty())
    {
        // Remove from queue and prevent revisiting
        Cell cell = open.back();
        open.pop_back();
        closed[cell.row][cell.col] = true;

        // Add to result if explored cell has the desired value
        if (tiles[cell.row][cell.col] == searchValue)
            result.push_back(cell);

        // Search neighbours
        for (Cell dir : DIRECTIONS)
        {
            Cell adj = { cell.row + dir.row, cell.col + dir.col };
            if (InBounds(adj) && !closed[adj.row][adj.col] && tiles[adj.row][adj.col] != 0)
                open.push_back(adj);
        }
    }
    return result;
}

// ENEMY SETUP==============================================================

struct Enemy
{
    Cell cell;
    float health;
    bool destroy = false;
    Vector2 enemy_position;
    float enemy_speed = 250.0f;   // <-- 250 pixels per second
    float min_distance = (enemy_speed / 60.0f) * 1.1f;
    bool at_end = false;
    int current_waypoint = 0;
};

int enemy_count = 0;

// PROGRAM MAIN ENTRY POINT ================================================

int main()
{
    int levelOneTiles[TILE_COUNT][TILE_COUNT]
    {
        //col:0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19    row:
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 0
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 1
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 2
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 3
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 4
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 5
            { 0, 0, 0, 0, 0, 2, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 6
            { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 7
            { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 8
            { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 9
            { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 10
            { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 11
            { 0, 0, 0, 0, 0, 2, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 12
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 13
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 14
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 15
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 16
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 17
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 18
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 }  // 19
    };
    std::vector<Cell> waypoints = FloodFill({ 0, 10 }, levelOneTiles, WAYPOINT);

    std::vector<Enemy> enemies;
 
    // GAME WINDOW =========================================================

    InitWindow(SCREEN_SIZE, SCREEN_SIZE, "Tower Defense");
    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        timer = timer + dt;

        // ENEMY MOVEMENT ==================================================
        for (Enemy& enemy : enemies)
        {
            // Apply enemy movement and gameplay logic (such as flagging for removal when health is <= 0.0f) here
            if (enemy.health <= 0.0f)
            {
                enemy.destroy;
            }
            if (!enemy.at_end)
            {
                Vector2 from = TileCenter(waypoints[enemy.current_waypoint].row, waypoints[enemy.current_waypoint].col);
                Vector2 to = TileCenter(waypoints[enemy.current_waypoint+1].row, waypoints[enemy.current_waypoint+1].col);
                Vector2 direction = Vector2Normalize(to - from);
                enemy.enemy_position += direction * enemy.enemy_speed * dt;

                // Tolorance depends on enemy speed
                if (CheckCollisionPointCircle(enemy.enemy_position, to, enemy.min_distance))
                {
                    enemy.enemy_position = to;
                    enemy.current_waypoint++;
                    enemy.at_end = enemy.current_waypoint == waypoints.size() - 1;
                }
            }
        }

        // DESTROY LOGIC ===================================================
        std::erase_if(enemies, [](Enemy& enemy)
            {
                return enemy.destroy;
            });

        // DRAW GAME =======================================================
        BeginDrawing();
        ClearBackground(BLACK);

            // Draw Grid
        for (int row = 0; row < TILE_COUNT; row++)
        {
            for (int col = 0; col < TILE_COUNT; col++)
            {
                DrawTile(row, col, levelOneTiles[row][col]);
            }
        }

        // ENEMY SPAWNING ==================================================

        for (const Enemy& enemy : enemies)
        {
            DrawCircleV(enemy.enemy_position, 20.0f, RED);
        }

        if (timer < 1.0f)
        {

        }
        if (timer >= 1.0f && enemy_count < 10)
        {
            Enemy testEnemy;
            testEnemy.enemy_position = TileCenter(waypoints[0].row, waypoints[0].col);
            enemies.push_back(testEnemy);
            timer = timer - 1.0f;
            timer = timer + dt;
            enemy_count++;
        }

        EndDrawing();
    }
    CloseWindow();
    return 0;
}

//==========================================================================