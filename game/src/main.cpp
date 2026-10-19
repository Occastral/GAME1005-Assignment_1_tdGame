#include "raylib.h"
#include "raymath.h"
#include "raygui.h"
#include "time.h"

#include <cassert>
#include <array>
#include <vector>
#include <algorithm>

// SCREEN & TILE SETUP =============================================================================

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

//DRAW TILES =======================================================================================

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

//FLOOD FILL =======================================================================================

std::vector<Cell> FloodFill(Cell start, int tiles[TILE_COUNT][TILE_COUNT], TileType searchValue)    // Returns a collection of adjacent cells that match the search value.
{
    std::vector<Cell> result;   
    std::vector<Cell> open;                                 // Places we want to search
    bool closed[TILE_COUNT][TILE_COUNT]{};                  // Places we've already searched
    for (int row = 0; row < TILE_COUNT; row++)
    {
        for (int col = 0; col < TILE_COUNT; col++)
            {
                closed[row][col] = tiles[row][col] == 0;    // Avoid searching for Zero-Tiles
        }
    }


    open.push_back(start);  // Add the starting cell to the exploration queue & search till there's nothing left!
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

//BULLET SETUP =====================================================================================

struct Bullet
{
    Vector2 position;
    Vector2 direction;
    float speed;
    float time;
    float radius;
    Color color;
    bool destroy;
};

//ENEMY SETUP ======================================================================================

struct Enemy
{
    Cell cell;
    Vector2 enemy_position;
    float enemy_speed = 250.0f;
    float min_distance = (enemy_speed / 60.0f) * 1.1f;
    float health;
    int curr = 0;
    bool at_end = false;
    bool destroy = false;
};

int enemy_count = 0;

//TURRET SETUP =====================================================================================

struct Turret
{
    Cell cell;
    Enemy* target;
};

//PROGRAM MAIN ENTRY POINT =========================================================================

int main()
{
    int tiles[TILE_COUNT][TILE_COUNT]
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
    std::vector<Cell> waypoints = FloodFill({ 0, 10 }, tiles, WAYPOINT);
    int curr = 0;
    int next = curr + 1;


    float bullet_time_current = 0.0f;
    float bullet_time_total = 0.25f;


    std::vector<Bullet> bullets;
    std::vector<Turret> turrets;
    std::vector<Enemy> enemies; 

    //TURRET CREATION ==================================================================================
    
        // Manual turret creation example
    turrets.resize(3);
    turrets[0].cell = { 3,  7 };
    turrets[1].cell = { 10, 7 };
    turrets[2].cell = { 15, 14 };

        // Search array for desired tiles (ie turrets)
    for (int row = 0; row < TILE_SIZE; row++)
    {
        for (int col = 0; col < TILE_SIZE; col++)
        {
            if (tiles[row][col] == 3)
            {
                // Construct turret here
                Turret turret{};
                turret.cell = { row, col };
                turrets.push_back(turret);
            }
        }
    }

    // GAME WINDOW =====================================================================================

    InitWindow(SCREEN_SIZE, SCREEN_SIZE, "Tower Defense");
    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        timer = timer + dt;

    // ENEMY LOGIC =====================================================================================
    
        for (Enemy& enemy : enemies)
        {
            if (enemy.health <= 0.0f)
            {
                enemy.destroy;
            }
            if (!enemy.at_end)
            {
                Vector2 from = TileCenter(waypoints[enemy.curr].row, waypoints[enemy.curr].col);
                Vector2 to = TileCenter(waypoints[enemy.curr + 1].row, waypoints[enemy.curr + 1].col);
                Vector2 direction = Vector2Normalize(to - from);
                enemy.enemy_position += direction * enemy.enemy_speed * dt;

                // Tolorance depends on enemy speed
                if (CheckCollisionPointCircle(enemy.enemy_position, to, enemy.min_distance))
                {
                    enemy.enemy_position = to;
                    enemy.curr++;
                    enemy.at_end = enemy.curr == waypoints.size() - 1;
                }
            }
        }

    //TURRET LOGIC =====================================================================================
           
        for (Turret& turret : turrets)
        {
            Vector2 turret_position = TileCenter(turret.cell.row, turret.cell.col);
            float max = FLT_MAX;
            for (Enemy& enemy : enemies)
            {
                Vector2 enemy_position = TileCenter(enemy.cell.row, enemy.cell.col);
                float distance = Vector2Distance(turret_position, enemy_position);
                if (distance < max)
                {
                    max = distance;
                    // Target nearest enemy
                }
            }
        }

    //BULLET CREATION ==================================================================================
      
        bullet_time_current += dt;
        if (bullet_time_current >= bullet_time_total && IsKeyDown(KEY_SPACE))
        {
            bullet_time_current = 0.0f;

            Bullet bullet{};
            bullet.position = GetMousePosition();
            /*bullet.direction = Vector2Normalize(enemy_position - bullet.position);*/

            bullet.speed = GetRandomValue(250, 500);
            bullet.radius = GetRandomValue(10, 50);

            bullet.color.r = GetRandomValue(0, 255);
            bullet.color.g = GetRandomValue(0, 255);
            bullet.color.b = GetRandomValue(0, 255);
            bullet.color.a = 255;

            bullets.push_back(bullet);
        }

        // Bullet update
        for (Bullet& bullet : bullets)
        {
            Vector2 bullet_velocity = bullet.direction * bullet.speed;
            bullet.position += bullet_velocity * dt;
            bullet.time += dt;

            // "Flag the current bullet to be destroyed if its been alive for 1 or more seconds"
            bullet.destroy |= bullet.time >= 1.0f;

            Rectangle screen_rec;
            screen_rec.x = 0.0f;
            screen_rec.y = 0.0f;
            screen_rec.width = GetScreenWidth();
            screen_rec.height = GetScreenHeight();

            bullet.destroy |= !CheckCollisionCircleRec(bullet.position, bullet.radius, screen_rec);
        }

        for (Bullet& bullet : bullets)
        {
            for (Enemy& enemy : enemies)
            {
                // Check if the current bullet is colliding with the current enemy
                // If so, damage the enemy
                // Do any on-bullet-enemy behaviour here such as playing sounds
            }
        }

    //DESTROY LOGIC ====================================================================================

        std::erase_if(bullets, [](Bullet& bullet)
            {
                return bullet.destroy;
            });

        std::erase_if(enemies, [](Enemy& enemy)
            {
                return enemy.destroy;
            });

    //DRAW GAME ========================================================================================
        
        BeginDrawing();
        ClearBackground(BLACK);

            // Draw grid
        for (int row = 0; row < TILE_COUNT; row++)
        {
            for (int col = 0; col < TILE_COUNT; col++)
            {
                DrawTile(row, col, tiles[row][col]);
            }
        }

    // ENEMY SPAWNING ==================================================================================
        
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


    //BULLET SPAWNING ==================================================================================

        for (const Bullet& bullet : bullets)
        {
            DrawCircleV(bullet.position, bullet.radius, bullet.color);
        }

    //TURRET SPAWNING ==================================================================================
        
        for (const Turret& turret : turrets)
        {
            DrawRectangleV(TileCenter(turret.cell.row, turret.cell.col), { TILE_SIZE, TILE_SIZE }, ORANGE);
        }

        EndDrawing();
    }
    CloseWindow();
    return 0;
}