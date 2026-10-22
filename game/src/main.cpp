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
bool levelSuccess = false;
bool levelFailure = false;

// MOUSE POSITION =================================================================================

Vector2 mousePositionWorld;
Vector2 mousePositionGrid;

enum GameScreen
{
    LEVEL_ONE,  // Mainly Basic, Some Speedsters, Some Tanks
    LEVEL_TWO,  //Few Basic, Mainly Speedsters & Tanks, One Trackstar
    LEVEL_THREE,    //Speedsters, Tanks, Few Trackstars, Few Bodybuilders
    LEVEL_FAILURE,
    LEVEL_SUCCESS,
    GAME_WON
};

enum TileType : int
{
    STONE,      // Marks unoccupied space, can be overwritten 
    PATH,       // Marks the path, cannot be overwritten
    WAYPOINT,   // Marks where the path turns, cannot be overwritten
    TURRET,
    COUNT
};

enum EnemyType : int
{
    BASIC,
    SPEEDSTER,
    TANK,
    TRACKSTAR,
    BODYBUILDER
};

struct Cell
{
    int row;
    int col;
};

constexpr std::array<Cell, 4> DIRECTIONS{ Cell{ -1, 0 }, Cell{ 1, 0 }, Cell{ 0, -1 }, Cell{ 0, 1 } };

Cell mouseCell;

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
    Color colors[4] =
    {
        GRAY,
        LIGHTGRAY,
        WHITE,
        ORANGE
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
    float speed = 2000.0f;
    float damage;
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
    EnemyType type;
    float enemy_speed = 250.0f;
    float radius = 10.0f;
    float min_distance = (enemy_speed / 60.0f) * 1.1f;
    float health = 100.0f;
    int curr = 0;
    bool at_end = false;
    bool destroy = false;
    Color colour = RED;
};

int enemy_count = 0;
int enemies_defeated = 0;

//TURRET SETUP =====================================================================================

struct Turret
{
    Cell cell;
    Enemy* target;
    float damage = 25.0f;
    float max_cooldown = 1.0f;
    float cooldown = max_cooldown;
    bool destroy = false;
};

Vector2 turret_position;
int turret_count = 0;

//LEVEL SETUP ======================================================================================

struct Level
{
    int tiles[TILE_COUNT][TILE_COUNT];
    int enemyTotal = 10;
    std::vector<Cell> waypoints;
    std::vector<Bullet> bullets;
    std::vector<Turret> turrets;
    std::vector<Enemy> enemies;
};

bool level1Complete = false;
bool level2Complete = false;
bool level3Complete = false;


//ENEMY TYPE DIFFERENCES ===========================================================================

Enemy SpawnEnemy(EnemyType type)
{
    Enemy testEnemy{};
    switch (type)
    {
    case BASIC: //Default Enemy
        testEnemy.health = 100;
        testEnemy.enemy_speed = 250.0f;
        testEnemy.radius = 12.0f;
        testEnemy.colour = RED;
        break;
    case SPEEDSTER: //Faster Speed & Less Health
        testEnemy.health = 50;
        testEnemy.enemy_speed = 500.0f;
        testEnemy.radius = 10.0f;
        testEnemy.colour = PINK;
        break;
    case TANK:  //Slower Speed & More Health
        testEnemy.health = 200;
        testEnemy.radius = 18.0f;
        testEnemy.enemy_speed = 200.0f;
        testEnemy.colour = DARKBLUE;
        break;
    case TRACKSTAR:   //Faster Speed
        testEnemy.health = 100;
        testEnemy.radius = 14.0f;
        testEnemy.enemy_speed = 500.0f;
        testEnemy.colour = YELLOW;
        break;
    case BODYBUILDER:   //More Health
        testEnemy.health = 200.0f;
        testEnemy.radius = 20.0f;
        testEnemy.enemy_speed = 250.0f;
        testEnemy.colour = MAROON;
        break;
    default:
        break;
    }

    return testEnemy;
}

//PROGRAM MAIN ENTRY POINT =========================================================================

int main()
{
    
    // INITIALIZATION ==============================================================================

    GameScreen currentScreen = LEVEL_ONE;

    // LEVEL LAYOUTS ===============================================================================

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
    Level level1;
    level1.enemyTotal = 10;

    for (int row = 0; row < TILE_COUNT; row++)
    {
        for (int col = 0; col < TILE_COUNT; col++)
        {
            level1.tiles[row][col] = levelOneTiles[row][col];
        }
    }

    int levelTwoTiles[TILE_COUNT][TILE_COUNT]
    {
        //col:0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19    row:
            { 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 0
            { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 1
            { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 2
            { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 3
            { 0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0 }, // 4
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 5
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 6
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 7
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 8
            { 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0 }, // 9
            { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 10
            { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 11
            { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 12
            { 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0 }, // 13
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 14
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 15
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 16
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 17
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, // 18
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0 }  // 19
    };
    Level level2;
    level2.enemyTotal = 20;

    for (int row = 0; row < TILE_COUNT; row++)
    {
        for (int col = 0; col < TILE_COUNT; col++)
        {
            level2.tiles[row][col] = levelTwoTiles[row][col];
        }
    }

    int levelThreeTiles[TILE_COUNT][TILE_COUNT]
    {
        //col:0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19    row:
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0 }, // 0
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 }, // 1
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 }, // 2
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 2, 0, 0 }, // 3
            { 0, 0, 0, 2, 1, 1, 1, 1, 1, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 }, // 4
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 }, // 5
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 }, // 6
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0 }, // 7
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 8
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 9
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, // 10
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0 }, // 11
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 12
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 13
            { 0, 0, 0, 2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 14
            { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 15
            { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // 16
            { 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0 }, // 17
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 }, // 18
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0 }  // 19
    };
    Level level3;
    level3.enemyTotal = 30;

    for (int row = 0; row < TILE_COUNT; row++)
    {
        for (int col = 0; col < TILE_COUNT; col++)
        {
            level3.tiles[row][col] = levelThreeTiles[row][col];
        }
    }
    

    Level levels[3];
    levels[0] = level1;
    levels[1] = level2;
    levels[2] = level3;


    Level* currentLevel;
    currentLevel = &levels[0];

    //==================================================================================================

    float bullet_time_current = 0.0f;
    float bullet_time_total = 0.25f;

    // GAME WINDOW =====================================================================================

    InitWindow(SCREEN_SIZE, SCREEN_SIZE, "Tower Defense");
    SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

            // MOUSE POSITION
        mousePositionWorld = GetMousePosition();
        mousePositionGrid = Vector2(floorf(mousePositionWorld.x / TILE_SIZE), floorf(mousePositionWorld.y / TILE_SIZE));
        mouseCell = Cell(mousePositionGrid.y, mousePositionGrid.x);
        
// UPDATE GAME SCREEN ==================================================================================

        switch (currentScreen)
        {
        case LEVEL_ONE:
            level1Complete = false;
            level2Complete = false;
            level3Complete = false;

                    // SET WAYPOINTS
            currentLevel->waypoints = FloodFill({ 0, 10 }, currentLevel->tiles, WAYPOINT);

            if (enemies_defeated >= currentLevel->enemyTotal)
            {
                levelSuccess = true;
            }
            if (levelFailure)
            {
                currentScreen = LEVEL_FAILURE;
            }
            if (levelSuccess)
            {
                level1Complete = true;
                currentScreen = LEVEL_SUCCESS;
            }

        break;

        case LEVEL_FAILURE:
       
            for (Enemy& enemy : currentLevel->enemies)
            {
                enemy.destroy = true;
            }
            for (Turret& turret : currentLevel->turrets)
            {
                turret.destroy = true;
            }
            for (Bullet& bullet : currentLevel->bullets)
            {
                bullet.destroy = true;
            }
            if (IsKeyPressed(KEY_SPACE))
            {
                levelFailure = false;
                timer = 0.0f;   // Reset Timer
                enemy_count = 0;     // Reset Enemy Counter
                enemies_defeated = 0;    // Reset Enemies Defeated
                turret_count = 0.0f;    // Reset Turret Counter
                for (int row = 0; row < TILE_COUNT; row++)
                {
                    for (int col = 0; col < TILE_COUNT; col++)
                    {
                        if (currentLevel->tiles[row][col] == TURRET)
                        {
                            currentLevel->tiles[row][col] = STONE;
                        }
                    }
                }
                if (!level1Complete && !level2Complete && !level3Complete)
                {
                    currentLevel = &levels[0];
                    currentScreen = LEVEL_ONE;
                }
                if (level1Complete && !level2Complete && !level3Complete)
                {
                    currentLevel = &levels[1];
                    currentScreen = LEVEL_TWO;
                }
                if (level1Complete && level2Complete && !level3Complete)
                {
                    currentLevel = &levels[2];
                    currentScreen = LEVEL_THREE;
                }
            }

        break;

        case LEVEL_SUCCESS:
       
            for (Enemy& enemy : currentLevel->enemies)
            {
                enemy.destroy = true;
            }
            for (Turret& turret : currentLevel->turrets)
            {
                turret.destroy = true;
            }
            for (Bullet& bullet : currentLevel->bullets)
            {
                bullet.destroy = true;
            }
            if (IsKeyPressed(KEY_SPACE))
            {
                levelSuccess = false;
                timer = 0.0f;   // Reset Timer
                enemy_count = 0;     // Reset Enemy Counter
                enemies_defeated = 0;    // Reset Enemies Defeated
                turret_count = 0.0f;    // Reset Turret Counter
                if (level1Complete && !level2Complete && !level3Complete)
                {
                    currentLevel = &levels[1];
                    currentScreen = LEVEL_TWO;
                }
                if (level1Complete && level2Complete && !level3Complete)
                {
                    currentLevel = &levels[2];
                    currentScreen = LEVEL_THREE;
                }
            }
        break;

        case LEVEL_TWO:
            level1Complete = true;
            level2Complete = false;
            level3Complete = false;

                // SET WAYPOINTS
            currentLevel->waypoints = FloodFill({ 0, 4 }, currentLevel->tiles, WAYPOINT);

            if (enemies_defeated >= currentLevel->enemyTotal)
            {
                levelSuccess = true;
            }
            if (levelFailure)
            {
                currentScreen = LEVEL_FAILURE;
            }
            if (levelSuccess)
            {
                level2Complete = true;
                currentScreen = LEVEL_SUCCESS;
            }
        break;

        case LEVEL_THREE:
            level1Complete = true;
            level2Complete = true;
            level3Complete = false;

                // SET WAYPOINTS
            currentLevel->waypoints = FloodFill({ 0, 17 }, currentLevel->tiles, WAYPOINT);

            if (enemies_defeated >= currentLevel->enemyTotal)
            {
                levelSuccess = true;
            }
            if (levelFailure)
            {
                currentScreen = LEVEL_FAILURE;
            }
            if (levelSuccess)
            {
                level3Complete = true;
                currentScreen = GAME_WON;
            }
        break;

        case GAME_WON:
        
            for (Enemy& enemy : currentLevel->enemies)
            {
                enemy.destroy = true;
            }
            for (Turret& turret : currentLevel->turrets)
            {
                turret.destroy = true;
            }
            for (Bullet& bullet : currentLevel->bullets)
            {
                bullet.destroy = true;
            }
            if (IsKeyPressed(KEY_BACKSPACE))
            {
                levelSuccess = false;
                timer = 0.0f;   // Reset Timer
                enemy_count = 0;     // Reset Enemy Counter
                enemies_defeated = 0;    // Reset Enemies Defeated
                turret_count = 0.0f;    // Reset Turret Counter

                level1Complete = false;
                level2Complete = false;
                level3Complete = false;
                currentLevel = &levels[0];
                currentScreen = LEVEL_ONE;
            }
        break;
        default: break;
        }

    // ENEMY LOGIC =====================================================================================
    
        for (Enemy& enemy : currentLevel->enemies)
        {
            if (enemy.health <= 0.0f)
            {
                enemy.destroy = true;
            }
            if (!enemy.at_end)
            {
                Vector2 from = TileCenter(currentLevel->waypoints[enemy.curr].row, currentLevel->waypoints[enemy.curr].col);
                Vector2 to = TileCenter(currentLevel->waypoints[enemy.curr + 1].row, currentLevel->waypoints[enemy.curr + 1].col);
                Vector2 direction = Vector2Normalize(to - from);
                enemy.enemy_position += direction * enemy.enemy_speed * dt;

                // Tolorance depends on enemy speed
                if (CheckCollisionPointCircle(enemy.enemy_position, to, enemy.min_distance))
                {
                    enemy.enemy_position = to;
                    enemy.curr++;
                    enemy.at_end = enemy.curr == currentLevel->waypoints.size() - 1;
                }
            }
            if (enemy.at_end)
            {
                levelFailure = true;
            }
        }

    //TURRET LOGIC =====================================================================================
           
        for (Turret& turret : currentLevel->turrets)
        {
            turret.cooldown += dt;
            
            if (turret.cooldown >= turret.max_cooldown)
            {
                turret_position = TileCenter(turret.cell.row, turret.cell.col);
                Vector2 enemy_position = turret_position;
                float max = FLT_MAX;
                for (Enemy& enemy : currentLevel->enemies)
                {
                    /*enemy_position = TileCenter(enemy.cell.row, enemy.cell.col);*/
                    float distance = Vector2Distance(turret_position, enemy.enemy_position);
                    if (distance < max)
                    {
                        max = distance;
                        // Target nearest enemy
                        enemy_position = enemy.enemy_position;
                    }
                }
                if (enemy_position != turret_position)
                {
        //BULLET CREATION ==================================================================================

                    turret.cooldown = 0.0f;

                    Bullet bullet{};
                    bullet.damage = turret.damage;
                    bullet.position = turret_position;
                    bullet.direction = Vector2Normalize(enemy_position - bullet.position);

                    bullet.radius = 7;

                    bullet.color = BLACK;

                    currentLevel->bullets.push_back(bullet);
                }
            }
        }

        // Bullet update
        for (Bullet& bullet : currentLevel->bullets)
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

        for (Bullet& bullet : currentLevel->bullets)
        {
            for (Enemy& enemy : currentLevel->enemies)
            {
                // Check if the current bullet is colliding with the current enemy
                if (CheckCollisionCircles(bullet.position, bullet.radius, enemy.enemy_position, 20))
                {
                    enemy.health -= bullet.damage;
                    bullet.destroy = true;
                    if (enemy.health <= 0)
                    {
                        enemy.destroy = true;
                        enemies_defeated++;
                    }
                }
                // Do any on-bullet-enemy behaviour here such as playing sounds
            }
        }

    //DESTROY LOGIC ====================================================================================

        std::erase_if(currentLevel->bullets, [](Bullet& bullet)
            {
                return bullet.destroy;
            });

        std::erase_if(currentLevel->enemies, [](Enemy& enemy)
            {
                return enemy.destroy;
            });

        std::erase_if(currentLevel->turrets, [](Turret& turret)
            {
                return turret.destroy;
            });

    //DRAW GAME ========================================================================================
        
        BeginDrawing();
        ClearBackground(BLACK);

        switch (currentScreen)
        {
        case LEVEL_ONE:
        
            for (int row = 0; row < TILE_COUNT; row++)
            {
                for (int col = 0; col < TILE_COUNT; col++)
                {
                    DrawTile(row, col, currentLevel->tiles[row][col]);
                }
            }
        break;
        case LEVEL_TWO:

            for (int row = 0; row < TILE_COUNT; row++)
            {
                for (int col = 0; col < TILE_COUNT; col++)
                {
                    DrawTile(row, col, currentLevel->tiles[row][col]);
                }
            }
            break;
        case LEVEL_THREE:

            for (int row = 0; row < TILE_COUNT; row++)
            {
                for (int col = 0; col < TILE_COUNT; col++)
                {
                    DrawTile(row, col, currentLevel->tiles[row][col]);
                }
            }
            break;
        case LEVEL_FAILURE:
        
            ClearBackground(RED);
            DrawText("DEFEAT", 190, 320, 100, WHITE);
            if (!level1Complete && !level2Complete && !level3Complete)
            {
                DrawText("Press SPACE to Retry Level One", 200, 750, 23, RAYWHITE);
            }
            if (level1Complete && !level2Complete && !level3Complete)
            {
                DrawText("Press SPACE to Retry Level Two", 200, 750, 23, RAYWHITE);
            }
            if (level1Complete && level2Complete && !level3Complete)
            {
                DrawText("Press SPACE to Retry Level Three", 200, 750, 23, RAYWHITE);
            }
        break;
        case LEVEL_SUCCESS:
        
            ClearBackground(GREEN);
            DrawText("LEVEL COMPLETE", 120, 320, 60, WHITE);
            DrawText("Press SPACE to Continue", 240, 750, 23, RAYWHITE);
        break;
        case GAME_WON:
        
            ClearBackground(GOLD);
            DrawText("YOU WIN!!!", 150, 300, 100, WHITE);
            DrawText("Press ESC to Close the Game, or Backspace to Start Again", 50, 450, 23, RAYWHITE);
        break;
        default: break;
        }


    // ENEMY SPAWNING ==================================================================================
        
        for (const Enemy& enemy : currentLevel->enemies)
        {
            DrawCircleV(enemy.enemy_position, enemy.radius, enemy.colour);
        }

        if (turret_count == 5)  // Don't start spawning enemies until 5 turrets are placed
        {
            if (timer < 1.0f)
            {

            }
            if (timer >= 1.0f && enemy_count < currentLevel->enemyTotal)
            {
                if (currentScreen == LEVEL_ONE)
                {
                    Enemy testEnemy = SpawnEnemy(SPEEDSTER);
                    testEnemy.enemy_position = TileCenter(currentLevel->waypoints[0].row, currentLevel->waypoints[0].col);
                    currentLevel->enemies.push_back(testEnemy);
                }
                if (currentScreen == LEVEL_TWO)
                {
                    Enemy testEnemy = SpawnEnemy(BASIC);
                    testEnemy.enemy_position = TileCenter(currentLevel->waypoints[0].row, currentLevel->waypoints[0].col);
                    currentLevel->enemies.push_back(testEnemy);
                }
                if (currentScreen == LEVEL_THREE)
                {
                    Enemy testEnemy = SpawnEnemy(BASIC);
                    testEnemy.enemy_position = TileCenter(currentLevel->waypoints[0].row, currentLevel->waypoints[0].col);
                    currentLevel->enemies.push_back(testEnemy);
                }
                timer = timer - 1.0f;
                timer = timer + dt;
                enemy_count++;
            }
        }
        

    //BULLET SPAWNING ==================================================================================

        for (const Bullet& bullet : currentLevel->bullets)
        {
            DrawCircleV(bullet.position, bullet.radius, bullet.color);
        }

    //TURRET SPAWNING ==================================================================================
        
        for (const Turret& turret : currentLevel->turrets)
        {
            DrawRectangleV(Vector2(turret.cell.col * TILE_SIZE, turret.cell.row * TILE_SIZE), { TILE_SIZE, TILE_SIZE }, ORANGE);
        }

        if (turret_count < 5)   // can place turrets
        {
            DrawRectangleV(mousePositionGrid* TILE_SIZE, { TILE_SIZE, TILE_SIZE }, ORANGE);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                if (currentLevel->tiles[mouseCell.row][mouseCell.col] == STONE)
                {
                    currentLevel->tiles[mouseCell.row][mouseCell.col] = TURRET;
                    Turret testTurret{};
                    testTurret.cell = mouseCell;
                    currentLevel->turrets.push_back(testTurret);
                    turret_count++;
                }
            };
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            {
                if (currentLevel->tiles[mouseCell.row][mouseCell.col] == TURRET)
                {
                    std::erase_if(currentLevel->turrets, [](Turret turret)
                    {
                            return turret.cell.row == mouseCell.row && turret.cell.col == mouseCell.col;
                    });
                    currentLevel->tiles[mouseCell.row][mouseCell.col] = STONE;
                    turret_count--;
                }
            };
        }
        if (turret_count == 5)  // can no longer place turrets
        {
            timer = timer + dt;
        }

    // =================================================================================================

        EndDrawing();
    }
    CloseWindow();
    return 0;
}