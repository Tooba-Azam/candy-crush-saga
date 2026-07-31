// Candy Crush 

#include <SFML/Graphics.hpp> // SFML graphics module
#include <SFML/Window.hpp>   // SFML window module
#include <iostream>          // For input/output (cout, cerr)
#include <cstdlib>           // For rand(), srand()
#include <ctime>             // For time() used in seeding rand
#include <string>            // For std::string
#include <utility>           // For std::pair
#include <algorithm>         // For std::swap, std::min, std::max

using namespace std;

// --- Config ---
const int rows = 8;         // Number of rows on the board
const int columns = 8;      // Number of columns on the board
const int gems = 5;         // Number of different gem colors
const int Empty = 0;        // Represents empty cell
const int tileSize = 80;    // Pixel size of a single candy tile
const int windowW = 1200;   // Window width
const int windowH = 900;    // Window height

//Game States
enum GameState { STATE_IDLE, STATE_SWAPPING, STATE_RETURNING, STATE_CLEARING, STATE_FALLING, STATE_GAMEOVER };

// Candy special types
enum { CT_NORMAL = 0, CT_STRIPED_H = 1, CT_STRIPED_V = 2, CT_WRAPPED = 3, CT_COLORBOMB = 4 };

// Candy cell structure
struct CandyCell
{
    int gem = Empty;              // 1..gems or Empty
    int specialType = CT_NORMAL;  // Special candy type
    float yOffset = 0.f;          // Vertical offset for falling animation
    float xOffset = 0.f;          // Horizontal offset for swap animation
};

// Board arrays
CandyCell board[rows][columns];  //Main Board Array
bool MatchCheck[rows][columns];  // Tracks cells marked for clearing

// Textures and sprites
sf::Texture texNormal[6], texH[6], texV[6], texWrapped[6], texColorBomb, backgroundTexture;
sf::Sprite backgroundSprite;   //background sprite
sf::Font font;                  //font for UI

// State Variables
int score = 0;                     // Player score
int timeRemaining = 300;           // Countdown timer
int cursorR = 0, cursorC = 0;       // Cursor position
int selectedR = -1, selectedC = -1; // Currently selected candy

// Swap animation state
GameState state = STATE_IDLE;                           // Current game state
int swapR1 = -1, swapC1 = -1, swapR2 = -1, swapC2 = -1; // Swap positions
float animTimer = 0.f;                                  // Timer for animation
const float animDuration = 0.16f;                       // Duration of swap animation
bool returnAfterNoMatch = false;                        // Flag to return swap if no match found

// Utility Functions 
//checks if coordinates are inside the board
bool inBounds(int r, int c)
{
    return r >= 0 && r < rows && c >= 0 && c < columns;
}

// Swap two cells in the board
void swapCells(int r1, int c1, int r2, int c2)
{
    std::swap(board[r1][c1], board[r2][c2]);
}

// --- Forward declarations ---
bool findMatches();                         // Detect matches and spawn specials
void resolveMatches();                      // Clear matches and expand special effects
bool applyGravity(float dt);                // Make candies fall
void activateColorBombOnSwap(int r, int c, int targetColor); // Bomb + normal gem
void activateColorBombOnSwapAsStriped(int r, int c, int targetColor, int stripedType); // Bomb + striped
bool loadAssets();                            // Load textures/fonts
void initializeBoard();                       // Fill board with random candies
void startSwap(int r1, int c1, int r2, int c2); // Start swap animation
void finalizeSwapAndCheck(bool keepSelectionClear); // Check swap results
void clearBoard();                                // Empty the board for Game Over

// --- Implementation ---

// Clears the board completely
void clearBoard()
{
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            board[r][c].gem = Empty;              // Remove gem
            board[r][c].specialType = CT_NORMAL; // Reset special
            board[r][c].yOffset = 0.f;           // Reset animation
            board[r][c].xOffset = 0.f;
        }
    }
}

// Initialize board with random gems, avoiding immediate matches
void initializeBoard()
{
    srand((unsigned)time(0));           //seed random
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            int g;
            bool ok;
            do
            {
                ok = true;
                g = 1 + rand() % gems;           //random gems generation
                // Avoid horizontal match 3+
                if (c >= 2 && board[r][c - 1].gem == g && board[r][c - 2].gem == g) ok = false;
                // Avoid vertical match 3+
                if (r >= 2 && board[r - 1][c].gem == g && board[r - 2][c].gem == g) ok = false;
            } while (!ok);
            board[r][c].gem = g;                // Assign gem
            board[r][c].specialType = CT_NORMAL; // Normal candy
            board[r][c].yOffset = 0.f;          // Reset animation
            board[r][c].xOffset = 0.f;
        }
    }
    score = 0;             // Reset score
    timeRemaining = 300;   // Reset timer
    cursorR = cursorC = 0; // Reset cursor
    selectedR = selectedC = -1; // Reset selection
    state = STATE_IDLE;          // Ready state
}

// Load textures and fonts while handling errors
bool loadAssets()
{
    const char* names[6] = { "", "blue", "green", "purple", "red", "yellow" };     //gem names
    for (int g = 1; g <= 5; ++g)
    {
        string b = names[g];
        string n = "assets/" + b + ".png";   //normal gems 
        if (!texNormal[g].loadFromFile(n)) { cerr << "Failed to load " << n << endl; return false; }
        string h = "assets/" + b + "Hstriped.png"; // Horizontal striped
        if (!texH[g].loadFromFile(h)) { cerr << "Failed to load " << h << endl; return false; }
        string v = "assets/" + b + "Vstriped.png"; // Vertical striped
        if (!texV[g].loadFromFile(v)) { cerr << "Failed to load " << v << endl; return false; }
        string w = "assets/" + b + "Wrapped.png"; // Wrapped candy
        if (!texWrapped[g].loadFromFile(w)) { cerr << "Failed to load " << w << endl; return false; }
    }
    if (!texColorBomb.loadFromFile("assets/ColorBomb.png")) { cerr << "Failed to load assets/ColorBomb.png\n"; return false; }
    if (backgroundTexture.loadFromFile("assets/background.png")) {
        backgroundSprite.setTexture(backgroundTexture); // Assign background
    }
    else
    {
        cout << "Warning: assets/background.png not found -> using fallback background color\n";
    }
    if (!font.loadFromFile("assets/Arial.ttf"))
    {
        cerr << "Failed to load assets/Arial.ttf\n";
        return false;
    }
    return true;
}

// findMatches: detecting hor/vert lengths, spawning specials, marking matches.
//special candy spawning priority: Color Bomb > Wrapped > Striped.
// The priority is handled by checking for 5+ (Color Bomb) first, then L/T (Wrapped), then 4 (Striped).

bool findMatches()
{
    // reset match table for this check
    for (int r = 0; r < rows; ++r)
        std::fill(MatchCheck[r], MatchCheck[r] + columns, false);

    // Arrays to track horizontal and vertical run lengths
    static int horLen[rows][columns];
    static int verLen[rows][columns];
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < columns; ++c)
            horLen[r][c] = verLen[r][c] = 0;

    bool found = false;          // Flag to indicate if any matches were found

    // horizontal runs
    for (int r = 0; r < rows; ++r)
    {
        int c = 0;
        while (c < columns)
        {
            if (board[r][c].gem == Empty)
            {
                ++c;
                continue;  //skip empty cells 
            }
            int g = board[r][c].gem;       //current gemn color
            int start = c;                  // start of potential run 
            while (c < columns && board[r][c].gem == g)
                ++c;                                   // count consecutive runs
            int len = c - start;                      //length of run
            if (len >= 3)
            {
                found = true;                     // valid match
                for (int x = start; x < c; ++x)
                {
                    horLen[r][x] = len;            // Store run length
                    MatchCheck[r][x] = true; // Mark cells for clearing
                }
            }
        }
    }

    // vertical runs
    for (int c = 0; c < columns; ++c)
    {
        int r = 0;
        while (r < rows)
        {
            if (board[r][c].gem == Empty)
            {
                ++r;
                continue;                 //skip empty cells
            }
            int g = board[r][c].gem;
            int start = r;
            while (r < rows && board[r][c].gem == g) ++r;
            int len = r - start;
            if (len >= 3)
            {
                found = true;
                for (int y = start; y < r; ++y)
                {
                    verLen[y][c] = len;              // Store vertical run length
                    MatchCheck[y][c] = true;      // Mark cells for clearing
                }
            }
        }
    }

    if (!found)
        return false;       // No matches found, return early

    // Special Candy Creation Priority 

    // 1. Color Bomb (5+ match) - This must be prioritized

    // Helper to find the best spawn location among the matched cells (prefer the swapped cell)
    auto getSpawnPos = [&](int rStart, int cStart, int rLen, int cLen, int& spawnR, int& spawnC)
        {
            spawnR = rStart + rLen / 2;            // Default center
            spawnC = cStart + cLen / 2;
            // Prefer the swapped cell for special spawn if it is within the matched region
            if (swapR1 >= rStart && swapR1 < rStart + rLen && swapC1 >= cStart && swapC1 < cStart + cLen)
            {
                spawnR = swapR1; spawnC = swapC1;
            }
            else if (swapR2 >= rStart && swapR2 < rStart + rLen && swapC2 >= cStart && swapC2 < cStart + cLen)
            {
                spawnR = swapR2; spawnC = swapC2;
            }
        };

    // Color Bomb from horizontal 5+
    for (int r = 0; r < rows; ++r)
    {
        int c = 0;
        while (c < columns)
        {
            if (horLen[r][c] < 5)
            {
                c += horLen[r][c] > 0 ? horLen[r][c] : 1;
                continue;
            }
            int len = horLen[r][c];
            int start = c;
            int spawnR, spawnC;
            getSpawnPos(r, start, 1, len, spawnR, spawnC); // rLen=1 for horizontal

            board[spawnR][spawnC].specialType = CT_COLORBOMB;
            // set gem to target color for future activation; it can be anything from 1..gems
            // keeping the gem color for the 5-match color, but ensuring it's NOT cleared immediately.
            board[spawnR][spawnC].gem = board[r][c].gem;
            MatchCheck[spawnR][spawnC] = false;                // Don't clear the new bomb
            // Erasing horizontal length for this span so Wrapped/Striped isn't created on top
            for (int x = start; x < start + len; ++x)
                horLen[r][x] = 0;
            c = start + len;          //move past this run
        }
    }

    // Color Bomb from vertical 5+
    for (int c = 0; c < columns; ++c)
    {
        int r = 0;
        while (r < rows)
        {
            if (verLen[r][c] < 5)
            {
                r += verLen[r][c] > 0 ? verLen[r][c] : 1;
                continue;
            }
            int len = verLen[r][c];
            int start = r;
            int spawnR, spawnC;
            getSpawnPos(start, c, len, 1, spawnR, spawnC); // cLen=1 for vertical(spawn position in vertical run )

            board[spawnR][spawnC].specialType = CT_COLORBOMB;
            board[spawnR][spawnC].gem = board[r][c].gem;
            MatchCheck[spawnR][spawnC] = false; // Don't clear the new bomb
            // Erase vertical length for this span
            for (int y = start; y < start + len; ++y) verLen[y][c] = 0;  //clear vertical length
            r = start + len;
        }
    }

    // 2. Wrapped Candy (L/T match) - Check remaining intersections

    // This handles intersection of remaining horizontal/vertical runs of length 3+
    bool wrappedCreated = false;
    //  First try to create on swapped cells if possible
    for (int i = 0; i < 2; ++i)
    {
        int r = (i == 0) ? swapR1 : swapR2;
        int c = (i == 0) ? swapC1 : swapC2;
        if (r >= 0 && c >= 0 && horLen[r][c] >= 3 && verLen[r][c] >= 3)
        {
            board[r][c].specialType = CT_WRAPPED;             // set wrapped
            MatchCheck[r][c] = false;                          //donot clear
            wrappedCreated = true;
            // Erase the runs so striped isn't created
            for (int x = c - (horLen[r][c] - 1); x <= c + (horLen[r][c] - 1); ++x) if (inBounds(r, x)) horLen[r][x] = 0;
            for (int y = r - (verLen[r][c] - 1); y <= r + (verLen[r][c] - 1); ++y) if (inBounds(y, c)) verLen[y][c] = 0;
            break;
        }
    }

    // If no wrapped was created on swap cells, check general intersections
    if (!wrappedCreated)
    {
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < columns; ++c)
            {
                if (horLen[r][c] >= 3 && verLen[r][c] >= 3)
                {
                    board[r][c].specialType = CT_WRAPPED;
                    MatchCheck[r][c] = false;
                    // Erase the runs so striped isn't created
                    for (int x = c - (horLen[r][c] - 1); x <= c + (horLen[r][c] - 1); ++x) if (inBounds(r, x)) horLen[r][x] = 0;
                    for (int y = r - (verLen[r][c] - 1); y <= r + (verLen[r][c] - 1); ++y) if (inBounds(y, c)) verLen[y][c] = 0;
                    break;
                }
            }
        }
    }

    // 3. Striped Candy (4 match) - Check remaining runs of length 4

    // Horizontal Striped from remaining runs of length 4
    for (int r = 0; r < rows; ++r)
    {
        int c = 0;
        while (c < columns)
        {
            if (horLen[r][c] < 4)
            {
                c += horLen[r][c] > 0 ? horLen[r][c] : 1;
                continue;
            }
            int len = horLen[r][c]; // Must be 4 here
            int start = c;
            int spawnR, spawnC;
            getSpawnPos(r, start, 1, len, spawnR, spawnC);

            board[spawnR][spawnC].specialType = CT_STRIPED_H;   // Set horizontal striped
            MatchCheck[spawnR][spawnC] = false;
            c = start + len;
        }
    }

    // Vertical Striped from remaining runs of length 4
    for (int c = 0; c < columns; ++c)
    {
        int r = 0;
        while (r < rows)
        {
            if (verLen[r][c] < 4)
            {
                r += verLen[r][c] > 0 ? verLen[r][c] : 1;
                continue;
            }
            int len = verLen[r][c]; // Must be 4 here
            int start = r;
            int spawnR, spawnC;
            getSpawnPos(start, c, len, 1, spawnR, spawnC);

            board[spawnR][c].specialType = CT_STRIPED_V;         // Set vertical striped
            MatchCheck[spawnR][c] = false;
            r = start + len;
        }
    }

    return true;          // Matches found and processed
}

// Expand special effects and clear matches
void resolveMatches()
{
    // Expand effects (striped, wrapped, colorbomb) until no new cells are affected
    bool changed = true; // Flag to repeat until no more expansions
    while (changed)
    {
        changed = false;

        std::pair<int, int> cellsToActivate[rows * columns]; // Fixed-size array
        int activationCount = 0;                              // Counter for active cells

        // Collect all cells that are marked for clearing and are special candies
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < columns; ++c)
            {
                // If cell is marked for clearing AND is special, it triggers effect
                if (MatchCheck[r][c] && board[r][c].specialType != CT_NORMAL)
                {
                    if (activationCount < rows * columns)
                    {
                        cellsToActivate[activationCount] = { r, c };
                        activationCount++;
                    }
                }
            }
        }

        // Activate each special candy
        // Iterate using the counter 'activationCount'
        for (int i = 0; i < activationCount; ++i)
        {
            int r = cellsToActivate[i].first;
            int c = cellsToActivate[i].second;

            int st = board[r][c].specialType; // Special type
            int target = board[r][c].gem;     // Gem color for color bomb

            // Reset special type before expansion to avoid recursive double-activation
            board[r][c].specialType = CT_NORMAL;

            // Priority: Color Bomb > Wrapped > Striped
            if (st == CT_COLORBOMB)
            {
                // Clear all candies of the same color
                if (target >= 1 && target <= gems)
                {
                    for (int rr = 0; rr < rows; ++rr)
                        for (int cc = 0; cc < columns; ++cc)
                            if (board[rr][cc].gem == target && !MatchCheck[rr][cc])
                            {
                                MatchCheck[rr][cc] = true; // Mark for clearing
                                changed = true;            // Trigger another expansion loop
                            }
                }
            }
            else if (st == CT_WRAPPED)
            {
                // Clear 3x3 area around wrapped candy
                for (int dr = -1; dr <= 1; ++dr)
                    for (int dc = -1; dc <= 1; ++dc)
                    {
                        int nr = r + dr, nc = c + dc;
                        if (inBounds(nr, nc) && !MatchCheck[nr][nc])
                        {
                            MatchCheck[nr][nc] = true;
                            changed = true;
                        }
                    }
            }
            else if (st == CT_STRIPED_H)
            {
                // Clear entire row
                for (int cc = 0; cc < columns; ++cc)
                {
                    if (!MatchCheck[r][cc])
                    {
                        MatchCheck[r][cc] = true;
                        changed = true;
                    }
                }
            }
            else if (st == CT_STRIPED_V)
            {
                // Clear entire column
                for (int rr = 0; rr < rows; ++rr)
                {
                    if (!MatchCheck[rr][c])
                    {
                        MatchCheck[rr][c] = true;
                        changed = true;
                    }
                }
            }
        }
    }

    // After expanding special effects, clear marked cells
    int cleared = 0; // Count cleared candies for scoring
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            if (MatchCheck[r][c])
            {
                MatchCheck[r][c] = false;   // Reset match flag
                if (board[r][c].gem != Empty)
                {
                    cleared++;               // Increment cleared count
                    board[r][c].gem = Empty; // Clear gem
                    board[r][c].specialType = CT_NORMAL; // Reset special
                    board[r][c].yOffset = 0.f; // Reset animation offsets
                    board[r][c].xOffset = 0.f;
                }
            }
        }
    }
    if (cleared > 0) score += cleared * 10; // Add points
}

// Apply gravity; dt for animation timing
bool applyGravity(float dt)
{
    bool moved = false; // Flag to check if anything moved
    // Process each column from bottom to top
    for (int c = 0; c < columns; ++c)
    {
        for (int r = rows - 1; r >= 0; --r)
        {
            if (board[r][c].gem == Empty)
            {
                int n = r - 1;
                while (n >= 0 && board[n][c].gem == Empty) --n; // Find nearest gem above
                if (n >= 0)
                {
                    // Bring gem down
                    swap(board[r][c], board[n][c]);
                    board[r][c].yOffset = -(float)(r - n) * tileSize; // Set falling animation
                    moved = true;
                }
                else
                {
                    // Spawn new gem at top
                    board[r][c].gem = 1 + rand() % gems;
                    board[r][c].specialType = CT_NORMAL;
                    board[r][c].yOffset = -(float)(r + 1) * tileSize; // Animate drop from above
                    moved = true;
                }
            }
        }
    }

    // Animate yOffset back to 0 for smooth falling
    float speed = 800.0f; // Pixels per second
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < columns; ++c)
        {
            if (board[r][c].yOffset < 0.0f)
            {
                board[r][c].yOffset += speed * dt;
                if (board[r][c].yOffset > 0.0f) board[r][c].yOffset = 0.0f;
                moved = true;
            }
        }
    return moved;
}

// Color bomb + normal gem swap: clear all of target color
void activateColorBombOnSwap(int r, int c, int targetColor)
{
    if (!inBounds(r, c) || targetColor == Empty)
        return;
    MatchCheck[r][c] = true;                            // Clear bomb itself
    for (int rr = 0; rr < rows; ++rr)
        for (int cc = 0; cc < columns; ++cc)
            if (board[rr][cc].gem == targetColor)
                MatchCheck[rr][cc] = true;                   // Clear all target gems
}

// Color bomb + striped candy swap: convert all target gems to striped type
void activateColorBombOnSwapAsStriped(int r, int c, int targetColor, int stripedType)
{
    if (!inBounds(r, c) || targetColor == Empty)
        return;
    MatchCheck[r][c] = true;                      // Clear bomb itself
    for (int rr = 0; rr < rows; ++rr)
    {
        for (int cc = 0; cc < columns; ++cc)
        {
            if (board[rr][cc].gem == targetColor)
            {
                board[rr][cc].specialType = stripedType; // Convert to striped
                MatchCheck[rr][cc] = true;                  // Mark for clearing/expansion
            }
        }
    }
}

// Start swap animation and update state
void startSwap(int r1, int c1, int r2, int c2)
{
    swapR1 = r1; swapC1 = c1; swapR2 = r2; swapC2 = c2; // Store swap positions
    animTimer = 0.f;             // Reset timer
    state = STATE_SWAPPING;      // Set state
    board[r1][c1].xOffset = 0.f; board[r2][c2].xOffset = 0.f; // Reset offsets
}

// Finalize swap after animation
void finalizeSwapAndCheck(bool keepSelectionClear)
{
    int st1 = board[swapR1][swapC1].specialType; // First candy type
    int st2 = board[swapR2][swapC2].specialType; // Second candy type
    int g1 = board[swapR1][swapC1].gem;         // First gem color
    int g2 = board[swapR2][swapC2].gem;         // Second gem color

    bool resolvedSpecialCombo = false; // Flag for special interaction

    // Only handle Color Bomb + Normal gem interaction
    if (st1 == CT_COLORBOMB && st2 == CT_NORMAL)
    {
        activateColorBombOnSwap(swapR1, swapC1, g2);
        MatchCheck[swapR2][swapC2] = true; // Clear normal gem
        resolvedSpecialCombo = true;
    }
    else if (st2 == CT_COLORBOMB && st1 == CT_NORMAL)
    {
        activateColorBombOnSwap(swapR2, swapC2, g1);
        MatchCheck[swapR1][swapC1] = true;
        resolvedSpecialCombo = true;
    }

    if (resolvedSpecialCombo)
    {
        // Clear the bombs immediately
        board[swapR1][swapC1].gem = Empty; board[swapR2][swapC2].gem = Empty;
        board[swapR1][swapC1].specialType = CT_NORMAL; board[swapR2][swapC2].specialType = CT_NORMAL;
        resolveMatches();           // Apply effects
        state = STATE_FALLING;      // Trigger falling
    }
    else {
        // Check normal matches
        if (!findMatches())
        {
            swapCells(swapR1, swapC1, swapR2, swapC2); // Return swap if no match
            returnAfterNoMatch = true;
            state = STATE_RETURNING; // Animate return
            animTimer = 0.f;
        }
        else
        {
            state = STATE_CLEARING;  // Matches found -> clear
        }
    }

    if (keepSelectionClear)
    {
        selectedR = selectedC = -1;
    } // Reset selection
}


// --- Main ---
int main()
{
    // Load all textures and font assets
    if (!loadAssets())
    {
        cerr << "Missing textures or font. Make sure assets/ folder exists with required files.\n";
        return 1;
    }
    initializeBoard();              // Fill board with initial random gems

    //create sfml window
    sf::RenderWindow win(sf::VideoMode(windowW, windowH), "Candy Crush SFML - Specials + Animation", sf::Style::Close);
    win.setFramerateLimit(60);               // Limit FPS for smooth animation

    sf::Clock clock;           // delta time clock
    sf::Clock gameTimer; // game timer for countdown (for timeRemaining)

    while (win.isOpen())
    {
        //main game loop
        float dt = clock.restart().asSeconds(); // Time elapsed since last frame

        //Game Over Logic
        if (state != STATE_GAMEOVER)
        {
            timeRemaining = max(0, 300 - (int)gameTimer.getElapsedTime().asSeconds());
            if (timeRemaining == 0)
            {
                state = STATE_GAMEOVER;             //trigger gameover
                clearBoard();
            }
        }


        sf::Event e;
        while (win.pollEvent(e))
        {                  //process events
            if (e.type == sf::Event::Closed)
                win.close();                          //close window
            if (e.type == sf::Event::KeyPressed)
            {
                if (e.key.code == sf::Keyboard::Q)
                    win.close();                        //quit
                if (state == STATE_GAMEOVER && e.key.code == sf::Keyboard::Enter)
                {
                    // Restart game on enter
                    initializeBoard();
                    gameTimer.restart();           //restart timer
                }
                else if (state == STATE_IDLE)                  // Only allow movement when idle 
                {
                    //cursor movement
                    if (e.key.code == sf::Keyboard::Left && cursorC > 0)
                        cursorC--;
                    if (e.key.code == sf::Keyboard::Right && cursorC < columns - 1)
                        cursorC++;
                    if (e.key.code == sf::Keyboard::Up && cursorR > 0)
                        cursorR--;
                    if (e.key.code == sf::Keyboard::Down && cursorR < rows - 1)
                        cursorR++;
                    if (e.key.code == sf::Keyboard::Enter || e.key.code == sf::Keyboard::Space) {
                        if (selectedR == -1)
                        {            //1st selection
                            selectedR = cursorR;
                            selectedC = cursorC;
                        }
                        else
                        {
                            // attempt swap if adjacent
                            if (abs(selectedR - cursorR) + abs(selectedC - cursorC) == 1)
                            {
                                // perform immediate logical swap then animate
                                swapCells(selectedR, selectedC, cursorR, cursorC);
                                startSwap(selectedR, selectedC, cursorR, cursorC);
                                // keep selected until resolved (so findMatches can inspect swap positions)
                            }
                            else
                            {
                                // not adjacent,  select new cell
                                selectedR = cursorR; selectedC = cursorC;
                            }
                        }
                    }
                }
            }
        }

        // State machine
        if (state == STATE_SWAPPING)
        {
            animTimer += dt;                  // Update animation timer
            float t = min(animTimer / animDuration, 1.0f);

            // Compute offset for both swapping cells for rendering
            float sx1 = (swapC2 - swapC1) * tileSize * t;
            float sy1 = (swapR2 - swapR1) * tileSize * t;
            float sx2 = (swapC1 - swapC2) * tileSize * t;
            float sy2 = (swapR1 - swapR2) * tileSize * t;

            // set xOffset temporarily for render
            board[swapR1][swapC1].xOffset = sx1;
            board[swapR2][swapC2].xOffset = sx2;
            board[swapR1][swapC1].yOffset = sy1;
            board[swapR2][swapC2].yOffset = sy2;

            if (animTimer >= animDuration)
            {
                // finalize logical positions (they're already swapped logically)
                board[swapR1][swapC1].xOffset = board[swapR1][swapC1].yOffset = 0.f;
                board[swapR2][swapC2].xOffset = board[swapR2][swapC2].yOffset = 0.f;
                // finalize and check for matches
                finalizeSwapAndCheck(true);
                animTimer = 0.f;
            }
        }
        else if (state == STATE_RETURNING)
        {
            // animate the return of the cells
            animTimer += dt;
            float t = min(animTimer / animDuration, 1.0f);

            // Simple visual "rejection" pulse
            float pulseScale = 0.2f * (1.0f - t);
            board[swapR1][swapC1].xOffset = (float)(swapC1 - swapC2) * tileSize * pulseScale;
            board[swapR1][swapC1].yOffset = (float)(swapR1 - swapR2) * tileSize * pulseScale;
            board[swapR2][swapC2].xOffset = (float)(swapC2 - swapC1) * tileSize * pulseScale;
            board[swapR2][swapC2].yOffset = (float)(swapR2 - swapR1) * tileSize * pulseScale;


            if (animTimer >= animDuration)
            {
                // End of return animation
                animTimer = 0.f;
                state = STATE_IDLE;
                returnAfterNoMatch = false;
                selectedR = selectedC = -1;
                // Reset offsets for good measure
                board[swapR1][swapC1].xOffset = board[swapR1][swapC1].yOffset = 0.f;
                board[swapR2][swapC2].xOffset = board[swapR2][swapC2].yOffset = 0.f;
                // Reset swap positions
                swapR1 = swapC1 = swapR2 = swapC2 = -1;
            }
        }
        else if (state == STATE_CLEARING)
        {
            resolveMatches();                //expand and clear matches
            state = STATE_FALLING;
        }
        else if (state == STATE_FALLING)
        {
            if (!applyGravity(dt))
            {
                // after gravity stops, find new matches
                if (findMatches())
                {
                    state = STATE_CLEARING;
                }
                else
                {
                    // done resolving cascades
                    state = STATE_IDLE;
                    // reset swap positions
                    swapR1 = swapC1 = swapR2 = swapC2 = -1;
                    //reset selection
                    selectedR = selectedC = -1;
                }
            }
        }

        // Render
        win.clear();

        // Draw background (if loaded) stretched to window or fallback color
        if (backgroundTexture.getSize().x > 0)
        {
            float sx = (float)windowW / backgroundTexture.getSize().x;
            float sy = (float)windowH / backgroundTexture.getSize().y;
            float s = max(sx, sy);
            backgroundSprite.setScale(s, s);
            backgroundSprite.setPosition(0, 0);
            win.draw(backgroundSprite);
        }
        else {
            // fallback color
            sf::RectangleShape whole(sf::Vector2f(windowW, windowH));
            whole.setFillColor(sf::Color(40, 40, 60));            //dark background
            win.draw(whole);
        }

        // Compute board position
        float boardW = columns * tileSize;
        float boardH = rows * tileSize;
        float boardX = (windowW - boardW) / 2.f;
        float boardY = (windowH - boardH) / 2.f;

        // Board background
        sf::RectangleShape boardBg(sf::Vector2f(boardW + 20, boardH + 20));
        boardBg.setPosition(boardX - 10, boardY - 10);
        boardBg.setFillColor(sf::Color(10, 10, 10, 200));
        boardBg.setOutlineThickness(6);
        boardBg.setOutlineColor(sf::Color(200, 160, 180, 200));
        win.draw(boardBg);

        // Draw tiles & candies
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < columns; ++c)
            {
                float x = boardX + c * tileSize;
                float y = boardY + r * tileSize;

                // grid cell background
                sf::RectangleShape cell(sf::Vector2f(tileSize - 6, tileSize - 6));
                cell.setPosition(x + 3, y + 3);
                cell.setFillColor(sf::Color(20, 20, 20, 80));
                cell.setOutlineThickness(1);
                cell.setOutlineColor(sf::Color(255, 255, 255, 30));
                win.draw(cell);

                if (board[r][c].gem == Empty)
                    continue;         //skip empty

                sf::Sprite sp;
                int st = board[r][c].specialType;
                int g = board[r][c].gem;
                // Assign texture based on special type
                //when a gem is 0 (ColorBomb created from 5-match, using original gem color) and st is CT_COLORBOMB, use texture
                if (st == CT_COLORBOMB) sp.setTexture(texColorBomb);
                else if (st == CT_STRIPED_H) sp.setTexture(texH[g]);
                else if (st == CT_STRIPED_V) sp.setTexture(texV[g]);
                else if (st == CT_WRAPPED) sp.setTexture(texWrapped[g]);
                else sp.setTexture(texNormal[g]);

                // Scale sprite to fit tile
                sf::FloatRect b = sp.getLocalBounds();
                float scale = (tileSize - 14) / max(b.width, b.height);
                sp.setScale(scale, scale);

                // compute render offsets: yOffset + possible swap animation offsets
                float renderX = x + tileSize / 2.f;
                float renderY = y + tileSize / 2.f + board[r][c].yOffset;
                // swap animation offsets for the two cells involved in a swap
                if (state == STATE_SWAPPING || state == STATE_RETURNING)
                {
                    if ((r == swapR1 && c == swapC1) || (r == swapR2 && c == swapC2))
                    {
                        // if in swapping we set xOffset/yOffset directly earlier
                        renderX += board[r][c].xOffset;
                        renderY += board[r][c].yOffset;
                    }
                }

                sp.setOrigin(b.width / 2.f, b.height / 2.f);
                sp.setPosition(renderX, renderY);
                win.draw(sp);
            }
        }

        // Cursor & selection highlight
        if (state != STATE_GAMEOVER)
        {
            sf::RectangleShape highlight(sf::Vector2f(tileSize - 8, tileSize - 8));
            highlight.setFillColor(sf::Color::Transparent);
            highlight.setOutlineThickness(4);
            // selection highlighting
            if (selectedR != -1)
            {
                highlight.setOutlineColor(sf::Color::Red);
                highlight.setPosition(boardX + selectedC * tileSize + 4, boardY + selectedR * tileSize + 4);
                win.draw(highlight);
            }
            // cursor highlighting
            highlight.setOutlineColor(sf::Color::Yellow);
            highlight.setPosition(boardX + cursorC * tileSize + 4, boardY + cursorR * tileSize + 4);
            win.draw(highlight);
        }

        // UI boxes
        auto drawBox = [&](float x, float y, float w, float h, const string& text)
            {
                // shadow
                sf::RectangleShape sh(sf::Vector2f(w, h));
                sh.setPosition(x + 6, y + 6);
                sh.setFillColor(sf::Color(80, 20, 60));
                win.draw(sh);
                sf::RectangleShape body(sf::Vector2f(w, h));
                body.setPosition(x, y);
                body.setFillColor(sf::Color(230, 100, 150));
                body.setOutlineThickness(3);
                body.setOutlineColor(sf::Color(255, 200, 220));
                win.draw(body);
                sf::Text t(text, font, 20);
                t.setFillColor(sf::Color::White);
                sf::FloatRect tb = t.getLocalBounds();
                t.setOrigin(tb.left + tb.width / 2.f, tb.top + tb.height / 2.f);
                t.setPosition(x + w / 2.f, y + h / 2.f);
                win.draw(t);
            };

        drawBox(20, 20, 200, 60, "Score: " + to_string(score));
        drawBox(windowW - 240, 20, 200, 60, "Time: " + to_string(timeRemaining));
        drawBox(windowW - 240, windowH - 90, 200, 60, "Quit (Q)");

        //  Game Over Message
        if (state == STATE_GAMEOVER)
        {
            // Dark Overlay
            sf::RectangleShape overlay(sf::Vector2f(windowW, windowH));
            overlay.setFillColor(sf::Color(0, 0, 0, 180));
            win.draw(overlay);

            // Game Over Text
            sf::Text gameOverText("TIME'S UP!", font, 80);
            gameOverText.setFillColor(sf::Color::Red);
            sf::FloatRect bounds = gameOverText.getLocalBounds();
            gameOverText.setOrigin(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
            gameOverText.setPosition(windowW / 2.f, windowH / 2.f - 60);
            win.draw(gameOverText);

            // Final Score
            sf::Text scoreText("Final Score: " + to_string(score), font, 40);
            scoreText.setFillColor(sf::Color::White);
            bounds = scoreText.getLocalBounds();
            scoreText.setOrigin(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
            scoreText.setPosition(windowW / 2.f, windowH / 2.f + 20);
            win.draw(scoreText);

            // Restart Hint
            sf::Text restartText("Press ENTER to Restart", font, 24);
            restartText.setFillColor(sf::Color::Yellow);
            bounds = restartText.getLocalBounds();
            restartText.setOrigin(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
            restartText.setPosition(windowW / 2.f, windowH / 2.f + 80);
            win.draw(restartText);
        }


        win.display();          //present frame
    }

    return 0;    // exit program
}