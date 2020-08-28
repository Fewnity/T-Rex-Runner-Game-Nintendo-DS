//Always include this
#include <nds.h>

//Debug print
#include <stdio.h>
#include <math.h>

//For random system
#include <time.h>

//File system
#include <fat.h>

//Sprites
#include <character32x32.h>
#include "Terrain.h"

//Sound system
#include <maxmod9.h>
#include "soundbank.h"
#include "soundbank_bin.h"

//
//All functions
void UpdateSprite(int xPos, int yPos, OamState *screen, u16 *gfx, int entry, int RotationId);
void AddNewObstacle(int Index, int Id);
void SetDayOrNightMode();
void AddNewDecor(int Index, int Id);
void ShowScore();
void SetHitBox(int SpriteDataIndex, int HitBoxIndex, int Corner1X, int Corner1Y, int Corner2X, int Corner2Y);

//
//All structs
struct VariablesToSave
{
    int HiScore;
} SaveFile;

typedef struct
{
    int x, y; //Prop x/y location

    int Type;      // Sprite Apparence :
    bool IsMoving; //Is obstacle moving
    bool WingsUp;  //Is bird has wings up?
    int NextAnimationIn;
    u16 *gfx[4]; // oam GFX
} Obstacle;

typedef struct
{
    float x, y, Speed; //Prop x/y location
    int Type;          //Sprite Apparence :
    bool IsMoving;     //Is decor moving
    u16 *gfx[2];       // oam GFX
} Decor;

typedef struct
{
    int Corner1X, Corner1Y, Corner2X, Corner2Y; //Corner 1 is the Top left corner of the 2d box, and corner 2 is the bottom right corner
} HitBox;

typedef struct
{
    int HitBoxCount;
    HitBox AllHitBox[3];
} SpriteData; //Contains sprite hit boxs

//
//Debug
PrintConsole TopScreen;
PrintConsole BottomScreen;

//
//Input
int pressedKey;
int releasedKey;
int heldKey;

//
//Player
#define ChangeAnimAt 7
u16 *PlayerGfx1;
u16 *PlayerGfx2;

int AnimSprite1Id = 2;
int AnimSprite2Id = 2;
int AnimCount = ChangeAnimAt;

bool IsDown;
bool IsDead;

int yJumpOffset = 0;
int JumpForce = 0;
int JumpTimer = 0;
bool IsJumping;

//
//All sprite ids from spritesheet
#define Idle1Sprite 0
#define Idle2Sprite 1

#define Walk1Sprite 2
#define Walk2Sprite 3

#define DeadSprite 4

#define Down1Sprite1 5
#define Down1Sprite2 6

#define Down2Sprite1 7
#define Down2Sprite2 8

#define CactusTop 10
#define CactusBottom 15

#define Cloud1 25

#define xPlayerPosition 0
#define yPlayerPosition 140

//
//Environment
Obstacle AllObstacles[3];
Decor AllDecor[5];
int BackGroundId;
bool NightMode;

//Obstacles and ground scrolling speed
int Speed = 3;
const int MaxSpeed = 6;
const int MinSpeed = 3;
int NextObstacle = 100; //Frame count for adding obstacle
int NextDecor = 1;      //Frame count for adding decor
int NextNight = 1500;   //Frame count for switching to night or day
float NightColorCoef = 1;

//
//Party values
int Score = 0;
int CurrentHiScore = 0;
int NextScore = 4; //Frame count for adding point
int NextPointSound = 100;
int BlinkCount = 0;
int NextBlinkAnim = 16;
int LastScoreForBlink;
bool IsPaused;
bool DebugMode = false;
int TimeBeforeRestart = 20;
bool PartyStated;

//
//File system
FILE *savefile;

//
//Collisions
SpriteData AllSpriteData[11]; //0 normal dino, 1 crouched dino, [2;4] normal cactus, 5 big cactus, [6;8] small cactus, [9;10] birds

//---------------------------------------------------------------------------------
int main(void)
{
    //Init file system for save High score
    fatInitDefault();

    //Init random
    srand(time(NULL));

    //Init and load sounds
    mmInitDefaultMem((mm_addr)soundbank_bin);
    mmLoadEffect(SFX_POINT);
    mmLoadEffect(SFX_DIE);
    mmLoadEffect(SFX_JUMP);

    //Read high score
    savefile = fopen("fat:/Google_Chrome_Score.sav", "rb");
    fread(&SaveFile, 1, sizeof(SaveFile), savefile);
    fclose(savefile);

    CurrentHiScore = SaveFile.HiScore;

    //Set main screen video mode to 5
    videoSetMode(MODE_5_2D);

    //Set sub screen video mode to 5 to get black screen
    videoSetModeSub(MODE_5_2D);

    //Load background
    BackGroundId = bgInit(0, BgType_Text8bpp, BgSize_T_512x256, 0, 4);
    dmaCopy(Terrain3Tiles, bgGetGfxPtr(BackGroundId), Terrain3TilesLen);
    dmaCopy(Terrain3Map, bgGetMapPtr(BackGroundId), Terrain3MapLen);
    dmaCopy(Terrain3Pal, BG_PALETTE, Terrain3PalLen);
    bgScroll(BackGroundId, 0, 65);

    //Set background layer priority
    bgSetPriority(BackGroundId, 2);

    //Init text printing on top screen
    consoleInit(&TopScreen, 1, BgType_Text4bpp, BgSize_T_256x256, 2, 6, true, true);
    consoleSelect(&TopScreen);

    //Init sprite system
    oamInit(&oamMain, SpriteMapping_1D_128, false);

    //Copy sprites sheet
    dmaCopy(character32x32Pal, SPRITE_PALETTE, character32x32PalLen);

    //Allow 2 GFX Id for player
    PlayerGfx1 = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);
    PlayerGfx2 = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);

    //Allow 4 GFX Id for each obstacle
    for (int i = 0; i < 3; i++)
        for (int i2 = 0; i2 < 4; i2++)
            AllObstacles[i].gfx[i2] = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);

    for (int i = 0; i < 5; i++)
        for (int i2 = 0; i2 < 2; i2++)
            AllDecor[i].gfx[i2] = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);

    //Main sprite/back sprite
    u8 *Sprite1Offset = ((u8 *)character32x32Tiles) + AnimSprite1Id * 32 * 32;
    dmaCopy(Sprite1Offset, PlayerGfx1, 32 * 32);

    //Apply Main sprite/back sprite scale
    oamRotateScale(&oamMain, 0, degreesToAngle(0), 256, 256);
    UpdateSprite(xPlayerPosition, yPlayerPosition, &oamMain, PlayerGfx1, 0, 0);

    SetDayOrNightMode();

    //Update screen
    oamUpdate(&oamMain);

    AllSpriteData[0].HitBoxCount = 3; //Normal player
    SetHitBox(0, 0, /*x,y*/ 8, 16, /*x,y*/ 19, 31);
    SetHitBox(0, 1, /*x,y*/ 16, 0, /*x,y*/ 31, 9);
    SetHitBox(0, 2, /*x,y*/ 0, 10, /*x,y*/ 15, 19);

    AllSpriteData[1].HitBoxCount = 1;
    SetHitBox(1, 0, /*x,y*/ 0, 11, /*x,y*/ 41, 27);

    AllSpriteData[2].HitBoxCount = 3;                 //Normal cactus 1
    SetHitBox(2, 0, /*x,y*/ 4, -4, /*x,y*/ 11, 15);   //Left
    SetHitBox(2, 1, /*x,y*/ 12, -16, /*x,y*/ 21, 31); //Center
    SetHitBox(2, 2, /*x,y*/ 22, -6, /*x,y*/ 28, 31);  //Right

    AllSpriteData[3].HitBoxCount = 3;                 //Normal cactus 2
    SetHitBox(3, 0, /*x,y*/ 4, -4, /*x,y*/ 11, 15);   //Left
    SetHitBox(3, 1, /*x,y*/ 12, -16, /*x,y*/ 21, 31); //Center
    SetHitBox(3, 2, /*x,y*/ 22, -6, /*x,y*/ 27, 31);  //Right

    AllSpriteData[4].HitBoxCount = 3;                 //Normal cactus 3
    SetHitBox(4, 0, /*x,y*/ 4, -11, /*x,y*/ 11, 15);  //Left
    SetHitBox(4, 1, /*x,y*/ 12, -16, /*x,y*/ 21, 31); //Center
    SetHitBox(4, 2, /*x,y*/ 22, -6, /*x,y*/ 28, 31);  //Right

    AllSpriteData[5].HitBoxCount = 3;                 //Big cactus
    SetHitBox(5, 0, /*x,y*/ 8, 0, /*x,y*/ 13, 15);    //Left
    SetHitBox(5, 1, /*x,y*/ 14, -16, /*x,y*/ 48, 31); //Center
    SetHitBox(5, 2, /*x,y*/ 32, -5, /*x,y*/ 38, 31);  //Right

    AllSpriteData[6].HitBoxCount = 3;               //Small cactus 1
    SetHitBox(6, 0, /*x,y*/ 0, 7, /*x,y*/ 4, 18);   //Left
    SetHitBox(6, 1, /*x,y*/ 5, 0, /*x,y*/ 26, 31);  //Center
    SetHitBox(6, 2, /*x,y*/ 27, 4, /*x,y*/ 31, 15); //Right

    AllSpriteData[7].HitBoxCount = 3;               //Small cactus 2
    SetHitBox(7, 0, /*x,y*/ 0, 4, /*x,y*/ 4, 15);   //Left
    SetHitBox(7, 1, /*x,y*/ 5, 0, /*x,y*/ 26, 31);  //Center
    SetHitBox(7, 2, /*x,y*/ 27, 4, /*x,y*/ 31, 15); //Right

    AllSpriteData[8].HitBoxCount = 3;               //Small cactus 3
    SetHitBox(8, 0, /*x,y*/ 0, 3, /*x,y*/ 4, 18);   //Left
    SetHitBox(8, 1, /*x,y*/ 5, 0, /*x,y*/ 26, 31);  //Center
    SetHitBox(8, 2, /*x,y*/ 27, 4, /*x,y*/ 31, 15); //Right

    AllSpriteData[9].HitBoxCount = 3;                //Bird 1
    SetHitBox(9, 0, /*x,y*/ 0, 4, /*x,y*/ 11, 13);   //Head
    SetHitBox(9, 1, /*x,y*/ 11, 10, /*x,y*/ 15, 27); //Center
    SetHitBox(9, 2, /*x,y*/ 16, 13, /*x,y*/ 31, 19); //Back

    AllSpriteData[10].HitBoxCount = 3;                //Bird 2
    SetHitBox(10, 0, /*x,y*/ 0, 4, /*x,y*/ 11, 13);   //Head
    SetHitBox(10, 1, /*x,y*/ 11, 0, /*x,y*/ 15, 17);  //Center
    SetHitBox(10, 2, /*x,y*/ 14, 13, /*x,y*/ 31, 20); //Back

    //Show start screen for first party
    iprintf("\x1b[30m"); //Black font color
    iprintf("\x1b[12;6HANY BUTTON  TO START");

    //Game loop
    while (1)
    {
        //Get key and touch informations
        scanKeys();

        pressedKey = keysDown(); // keys pressed this loop
        releasedKey = keysUp();  // keys released this loop
        heldKey = keysHeld();    // keys currently held

        //Wait until the party has started
        if (!PartyStated)
        {
            if (pressedKey != 0)
            {
                PartyStated = true;
                iprintf("\x1b[12;0H");
                iprintf("\x1b[2K");
            }
            continue;
        }

        //Wait until the party is paused
        if (IsPaused && !IsDead)
        {
            if ((pressedKey & KEY_START) == KEY_START)
            {
                IsPaused = false;
                iprintf("\x1b[10;0H");
                iprintf("\x1b[2K");
            }
            continue;
        }

        //Stop game if player is dead
        if (IsDead)
        {
            if (TimeBeforeRestart > 0)
                TimeBeforeRestart--;

            if (pressedKey != 0 && TimeBeforeRestart == 0)
            {
                //Reset all values to start a new party
                IsDead = false;
                Speed = MinSpeed;
                yJumpOffset = 0;
                JumpTimer = 0;
                NextScore = MaxSpeed - Speed + 1;
                NextObstacle = 100;
                Score = 0;
                AnimCount = 1;
                IsDown = false;
                NextPointSound = 100;
                NextNight = 1500;
                NightColorCoef = 1;
                TimeBeforeRestart = 20;

                //Remove moon
                if (NightMode)
                {
                    AllDecor[4].IsMoving = false;
                    int Index = 0 + 4 * (4 + 1) + 3 * 4;
                    oamClearSprite(&oamMain, Index);
                }

                NightMode = false;

                SetDayOrNightMode();

                //Reset player sprite
                AnimSprite1Id = Idle1Sprite;
                u8 *Sprite1Offset = ((u8 *)character32x32Tiles) + AnimSprite1Id * 32 * 32;
                dmaCopy(Sprite1Offset, PlayerGfx1, 32 * 32);

                //Clear game over screen
                iprintf("\x1b[10;0H");
                iprintf("\x1b[2K");
                iprintf("\x1b[12;0H");
                iprintf("\x1b[2K");

                //For all obstacles
                for (int i = 0; i < 3; i++)
                {
                    //Reset current obstacle values
                    AllObstacles[i].IsMoving = false;
                    AllObstacles[i].x = -64 - 16;

                    //Set current obstacle sprites new position
                    for (int i2 = 0; i2 < 4; i2++)
                    {
                        int Index = i2 + 4 * (i + 1);
                        UpdateSprite(AllObstacles[i].x, AllObstacles[i].y, &oamMain, AllObstacles[i].gfx[i2], Index, Index);
                    }
                }
                oamUpdate(&oamMain);
            }

            swiWaitForVBlank();
            continue;
        }

        //Reduce by 1 all counters
        NextScore--;
        NextObstacle--;
        NextDecor--;
        NextNight--;

        if (NextNight == 0)
        {
            NightMode = !NightMode;
            NextNight = 1500;
        }

        if (NightMode)
        {
            if (NightColorCoef >= 0.020)
            {
                NightColorCoef -= 0.020;
                SetDayOrNightMode();

                if (NightColorCoef <= 0.020)
                    AddNewDecor(4, 4 + rand() % 5); //4;5;6;7;8
            }
        }
        else
        {
            if (NightColorCoef < 1)
            {
                NightColorCoef += 0.020;
                SetDayOrNightMode();
            }
        }

        if (BlinkCount > 0)
            NextBlinkAnim--;

        if (NextBlinkAnim == 0)
        {
            BlinkCount--;
            NextBlinkAnim = 16;
        }

        //If NextObstacle counter is egal to 0, try to add new obstacle
        if (NextObstacle == 0)
        {
            for (int i = 0; i < 3; i++)
            {
                //Check for an obstacle that does not move
                if (!AllObstacles[i].IsMoving)
                {
                    if (Score >= 500) //Use birds when score is > than 500
                    {
                        if (rand() % 5 != 0)               //Add more chance to have birds
                            AddNewObstacle(i, rand() % 7); //Random [0;7[ (Exclude birds)
                        else
                            AddNewObstacle(i, 7); //Add bird
                    }
                    else if (Score >= 300)             //Or don't use birds
                        AddNewObstacle(i, rand() % 7); //Random [0;7[ (Exclude birds)
                    else                               //Or don't use birds and very bigs cactus
                    {
                        if (rand() % 2 == 0)
                            AddNewObstacle(i, rand() % 3); //Random [0;3[ (Exclude bigs/smalls cactus and birds)
                        else
                            AddNewObstacle(i, 4 + rand() % 3); //Random [4;7[ (Exclude normal/bigs cactus and birds)
                    }
                    break;
                }
            }
            //Set NextObstacle counter with 50 frames minus a growing number with higher speed, plus a random [0, 60[
            NextObstacle = 50 - 5 * (Speed - MinSpeed) + rand() % 60;
        }

        //If NextDecor counter is egal to 0, try to add new obstacle
        if (NextDecor == 0)
        {
            for (int i = 0; i < 4; i++)
            {
                //Check for an obstacle that does not move
                if (!AllDecor[i].IsMoving)
                {
                    if (NightMode)
                        AddNewDecor(i, 1 + rand() % 3);
                    else
                        AddNewDecor(i, 0);

                    break;
                }
            }
            //Set NextDecor counter with 50 frames plus a random [0, 60[
            NextDecor = 30 + rand() % 110;
        }

        //If NextScore counter is egal to 0, add + 1 to score
        if (NextScore == 0)
        {
            Score++;
            NextPointSound--;

            //Every 100/500/1000 of score, make sound
            if (NextPointSound == 0)
            {
                mmEffect(SFX_POINT);
                LastScoreForBlink = Score;

                if (Score < 1000)
                    NextPointSound = 100;
                else if (Score < 10000)
                    NextPointSound = 500;
                else
                    NextPointSound = 1000;

                BlinkCount = 6;
            }

            //Increase speed every 300 of score
            if (Score % 300 == 0 && Speed < MaxSpeed)
                Speed++;

            ShowScore();

            //Set NextScore counter (Higher speed, lower counter value)
            NextScore = MaxSpeed - Speed + 1;
        }

        //Scoll backgrouns
        bgScroll(BackGroundId, Speed, 0);
        bgUpdate();

        //For all decors
        for (int i = 0; i < 5; i++)
        {
            if (!AllDecor[i].IsMoving)
                continue;

            //Move forward decors
            AllDecor[i].x -= AllDecor[i].Speed;

            if (AllDecor[i].Type == 0)
            {
                if (AllDecor[i].x <= -64 - 16)
                    AllDecor[i].IsMoving = false;
            }
            else
            {
                if (AllDecor[i].x <= -32 - 16)
                    AllDecor[i].IsMoving = false;
            }

            int SpriteCount = 1;
            if (AllDecor[i].Type == 0)
                SpriteCount = 2;

            //Set decor sprite position
            for (int i2 = 0; i2 < SpriteCount; i2++)
            {
                int Index = i2 + 4 * (i + 1) + 3 * 4;
                if (i2 == 0)
                    UpdateSprite(AllDecor[i].x, AllDecor[i].y, &oamMain, AllDecor[i].gfx[i2], Index, Index);
                else if (i2 == 1)
                    UpdateSprite(AllDecor[i].x + 32, AllDecor[i].y, &oamMain, AllDecor[i].gfx[i2], Index, Index);
            }
            oamUpdate(&oamMain);
        }

        iprintf("\x1b[11;0H");
        iprintf("\x1b[2K");
        iprintf("\x1b[10;0H");
        iprintf("\x1b[2K");

        //For all obstacles
        for (int i = 0; i < 3; i++)
        {
            if (!AllObstacles[i].IsMoving)
                continue;

            //Move forward obstacles
            AllObstacles[i].x -= Speed;

            //Stop obstacle with obstacle is off screen
            if (AllObstacles[i].Type == 0 || AllObstacles[i].Type == 1 || AllObstacles[i].Type == 2 || AllObstacles[i].Type == 4 || AllObstacles[i].Type == 5 || AllObstacles[i].Type == 6 || AllObstacles[i].Type == 7)
            {
                if (AllObstacles[i].x <= -32 - 16)
                    AllObstacles[i].IsMoving = false;
            }
            else if (AllObstacles[i].Type == 3)
            {
                if (AllObstacles[i].x <= -64 - 16)
                    AllObstacles[i].IsMoving = false;
            }

            if (AllObstacles[i].Type == 7)
            {
                //Reduce by 1 NextAnimationIn counter for bird animation
                AllObstacles[i].NextAnimationIn--;

                //if NextAnimationIn is egal to 0, do next bird animation
                if (AllObstacles[i].NextAnimationIn == 0)
                {
                    //Set next animation in 12 frames
                    AllObstacles[i].NextAnimationIn = 12;
                    AllObstacles[i].WingsUp = !AllObstacles[i].WingsUp;

                    //Sprite sheet offset for bird
                    int SpriteIdOffset = 6;

                    if (AllObstacles[i].WingsUp)
                        SpriteIdOffset = 7;

                    //Set sprite
                    int CurrentSpriteId = CactusTop + AllObstacles[i].Type + SpriteIdOffset;
                    u8 *SpriteOffset = ((u8 *)character32x32Tiles) + CurrentSpriteId * 32 * 32;
                    dmaCopy(SpriteOffset, AllObstacles[i].gfx[0], 32 * 32);
                }
            }

            int PlayerHitBoxCount = 3;
            if (IsDown)
                PlayerHitBoxCount = 1;

            for (int CurrentObstacleHitBox = 0; CurrentObstacleHitBox < AllSpriteData[AllObstacles[i].Type + 2].HitBoxCount; CurrentObstacleHitBox++)
            {
                for (int CurrentPlayerHitBox = 0; CurrentPlayerHitBox < PlayerHitBoxCount; CurrentPlayerHitBox++)
                {
                    int BirdOffset = 0;
                    int PlayerOffset = 0;

                    if (AllObstacles[i].WingsUp)
                        BirdOffset = 1;

                    if (IsDown)
                        PlayerOffset = 1;

                    if (xPlayerPosition - AllObstacles[i].x + AllSpriteData[0 + PlayerOffset].AllHitBox[CurrentPlayerHitBox].Corner1X <= AllSpriteData[AllObstacles[i].Type + 2 + BirdOffset].AllHitBox[CurrentObstacleHitBox].Corner2X && xPlayerPosition - AllObstacles[i].x + AllSpriteData[0 + PlayerOffset].AllHitBox[CurrentPlayerHitBox].Corner2X >= AllSpriteData[AllObstacles[i].Type + 2 + BirdOffset].AllHitBox[CurrentObstacleHitBox].Corner1X)                                 //Left & right OK
                        if (yPlayerPosition + yJumpOffset - AllObstacles[i].y + AllSpriteData[0 + PlayerOffset].AllHitBox[CurrentPlayerHitBox].Corner1Y <= AllSpriteData[AllObstacles[i].Type + 2 + BirdOffset].AllHitBox[CurrentObstacleHitBox].Corner2Y && yPlayerPosition + yJumpOffset - AllObstacles[i].y + AllSpriteData[0 + PlayerOffset].AllHitBox[CurrentPlayerHitBox].Corner2Y >= AllSpriteData[AllObstacles[i].Type + 2 + BirdOffset].AllHitBox[CurrentObstacleHitBox].Corner1Y) //Top & Bottom ok
                        {
                            //IsDead = true;
                            //iprintf("Col at %d,%d ", AllObstacles[i].x, i);
                            iprintf("Col %d:%d ", CurrentPlayerHitBox, CurrentObstacleHitBox);
                        }
                }
            }

            //Check how many sprites current obstacle need
            int SpriteCount = 0;
            if (AllObstacles[i].Type == 0 || AllObstacles[i].Type == 1 || AllObstacles[i].Type == 2)
                SpriteCount = 2;
            else if (AllObstacles[i].Type == 3)
                SpriteCount = 4;
            else if (AllObstacles[i].Type == 4 || AllObstacles[i].Type == 5 || AllObstacles[i].Type == 6 || AllObstacles[i].Type == 7)
                SpriteCount = 1;

            //Set obstacle sprite position
            for (int i2 = 0; i2 < SpriteCount; i2++)
            {
                int Index = i2 + 4 * (i + 1);
                if (i2 == 0 && SpriteCount != 1)
                    UpdateSprite(AllObstacles[i].x, AllObstacles[i].y - 32, &oamMain, AllObstacles[i].gfx[i2], Index, Index);
                else if (i2 == 1 || SpriteCount == 1)
                    UpdateSprite(AllObstacles[i].x, AllObstacles[i].y, &oamMain, AllObstacles[i].gfx[i2], Index, Index);
                else if (i2 == 2)
                    UpdateSprite(AllObstacles[i].x + 32, AllObstacles[i].y - 32, &oamMain, AllObstacles[i].gfx[i2], Index, Index);
                else if (i2 == 3)
                    UpdateSprite(AllObstacles[i].x + 32, AllObstacles[i].y, &oamMain, AllObstacles[i].gfx[i2], Index, Index);
            }

            oamUpdate(&oamMain);

            //Is player is dead
            if (IsDead)
            {
                //Player die sounf effect
                mmEffect(SFX_DIE);

                BlinkCount = 0;
                ShowScore();

                iprintf("\x1b[10;8HG A M E  O V E R");
                iprintf("\x1b[12;5HANY BUTTON  TO RESTART");

                //UpdateSprite(xPlayerPosition, yPlayerPosition, &oamMain, PlayerGfx1, 0, 0);

                if (IsDown)
                {
                    //Set maint sprite/back sprite scale
                    oamRotateScale(&oamMain, 0, degreesToAngle(0), 256, 256);
                    UpdateSprite(xPlayerPosition, yPlayerPosition + yJumpOffset, &oamMain, PlayerGfx1, 0, 0);
                    //Hide secondary sprite/front sprite
                    oamClearSprite(&oamMain, 1);
                }

                //Set player sprite with dead dino
                AnimSprite1Id = DeadSprite;
                u8 *Sprite1Offset = ((u8 *)character32x32Tiles) + AnimSprite1Id * 32 * 32;
                dmaCopy(Sprite1Offset, PlayerGfx1, 32 * 32);
                oamUpdate(&oamMain);

                //Save new high score when high score is beated
                if (Score > CurrentHiScore)
                {
                    SaveFile.HiScore = Score;
                    savefile = fopen("fat:/Google_Chrome_Score.sav", "wb");
                    fwrite(&SaveFile, 1, sizeof(SaveFile), savefile);
                    fclose(savefile);
                    SaveFile.HiScore = 0;
                    CurrentHiScore = Score;
                }
            }
        }

        if (IsDead)
            continue;

        //Check crouch key (press)
        if ((pressedKey & KEY_DOWN) == KEY_DOWN)
        {
            IsDown = true;

            //Go down when dino is jumping
            if (IsJumping && JumpTimer < 27)
                JumpTimer = 27;

            //Set AnimCount frame count
            AnimCount = 1;

            //Apply Main sprite/back sprite scale
            oamRotateScale(&oamMain, 0, degreesToAngle(0), 390, 390);
            UpdateSprite(xPlayerPosition - 5, yPlayerPosition + 5, &oamMain, PlayerGfx1, 0, 0);

            //Apply Secondary sprite/front sprite scale
            oamRotateScale(&oamMain, 1, degreesToAngle(0), 390, 390);
            UpdateSprite(xPlayerPosition + 21 - 5, yPlayerPosition + 5, &oamMain, PlayerGfx2, 1, 1);
        }

        //Check input for Jump
        if ((heldKey & KEY_UP) == KEY_UP && !IsJumping && !IsDown)
        {
            //Set jump values
            JumpTimer = 0;
            JumpForce = 6;
            IsJumping = true;

            //Make jump sound effect
            mmEffect(SFX_JUMP);
        }

        //Debug mode, check input for night/day mode
        if ((pressedKey & KEY_X) == KEY_X && DebugMode)
        {
            NightMode = !NightMode;
            SetDayOrNightMode();
        }

        //Debug
        if ((pressedKey & KEY_B) == KEY_B && DebugMode)
        {
            yJumpOffset--;
            UpdateSprite(xPlayerPosition, yPlayerPosition + yJumpOffset, &oamMain, PlayerGfx1, 0, 0);
        }

        //Debug
        if ((pressedKey & KEY_A) == KEY_A && DebugMode)
        {
            yJumpOffset++;
            UpdateSprite(xPlayerPosition, yPlayerPosition + yJumpOffset, &oamMain, PlayerGfx1, 0, 0);
        }

        //Debug
        if ((pressedKey & KEY_LEFT) == KEY_LEFT && DebugMode)
        {
            if (Speed > 0)
                Speed--;
        }

        //Debug
        if ((pressedKey & KEY_RIGHT) == KEY_RIGHT && DebugMode)
        {
            if (Speed < MaxSpeed)
                Speed++;
        }

        //Check crouch key (release)
        if ((releasedKey & KEY_DOWN) == KEY_DOWN)
        {
            IsDown = false;

            //Set AnimCount frame count
            //AnimCount = ChangeAnimAt - 1;
            AnimCount = 1;

            //Apply Main sprite/back sprite scale
            oamRotateScale(&oamMain, 0, degreesToAngle(0), 256, 256);
            UpdateSprite(xPlayerPosition, yPlayerPosition, &oamMain, PlayerGfx1, 0, 0);

            //Hide Secondary sprite/front sprite
            oamClearSprite(&oamMain, 1);
        }

        if ((pressedKey & KEY_START) == KEY_START)
        {
            IsPaused = true;
            iprintf("\x1b[10;12HP A U S E");
        }

        if (IsJumping)
        {
            JumpTimer += 1;

            //Set jum force by a timer
            if (JumpTimer == 8)
                JumpForce = 5;
            else if (JumpTimer == 12)
                JumpForce = 4;
            else if (JumpTimer == 14)
                JumpForce = 3;
            else if (JumpTimer == 16)
                JumpForce = 2;
            else if (JumpTimer == 18)
                JumpForce = 1;
            else if (JumpTimer == 20)
                JumpForce = 0;
            else if (JumpTimer == 22)
                JumpForce = -1;
            else if (JumpTimer == 24)
                JumpForce = -2;
            else if (JumpTimer == 26)
                JumpForce = -3;
            else if (JumpTimer == 28)
                JumpForce = -4;
            else if (JumpTimer == 32)
                JumpForce = -5;
            else if (JumpTimer == 34)
                JumpForce = -6;

            //Add or reduce y offset for player
            yJumpOffset -= JumpForce;

            //Check if player is grounded
            if (yJumpOffset >= 0)
            {
                yJumpOffset = 0;
                IsJumping = false;
            }

            //If player is crouched, use 2 sprites and set sprites position
            if (IsDown)
            {
                UpdateSprite(xPlayerPosition - 5, yPlayerPosition + 5 + yJumpOffset, &oamMain, PlayerGfx1, 0, 0);
                UpdateSprite(xPlayerPosition + 21 - 5, yPlayerPosition + 5 + yJumpOffset, &oamMain, PlayerGfx2, 1, 1);
            }
            else //If player is not crouched, use 1 sprite and set sprite position
                UpdateSprite(xPlayerPosition, yPlayerPosition + yJumpOffset, &oamMain, PlayerGfx1, 0, 0);

            oamUpdate(&oamMain);
        }

        //Reduce by 1 AnimCount counter for dino animation
        AnimCount--;
        if (AnimCount == 0)
        {
            if (!IsDown) //Do normal animation
            {
                if (!IsJumping)
                {
                    //Swap animation
                    if (AnimSprite1Id == Walk1Sprite)
                        AnimSprite1Id = Walk2Sprite;
                    else
                        AnimSprite1Id = Walk1Sprite;
                }
                else
                    AnimSprite1Id = Idle1Sprite;
            }
            else //Do crouched animation
            {
                //Swap animation
                if (AnimSprite1Id == Down1Sprite1)
                {
                    AnimSprite1Id = Down2Sprite1;
                    AnimSprite2Id = Down2Sprite2;
                }
                else
                {
                    AnimSprite1Id = Down1Sprite1;
                    AnimSprite2Id = Down1Sprite2;
                }
            }

            //Main sprite/back sprite
            u8 *Sprite1Offset = ((u8 *)character32x32Tiles) + AnimSprite1Id * 32 * 32;
            dmaCopy(Sprite1Offset, PlayerGfx1, 32 * 32);

            //Secondary sprite/front sprite
            u8 *Sprite2Offset = ((u8 *)character32x32Tiles) + AnimSprite2Id * 32 * 32;
            dmaCopy(Sprite2Offset, PlayerGfx2, 32 * 32);

            oamUpdate(&oamMain);

            AnimCount = ChangeAnimAt;
        }
        swiWaitForVBlank();
    }

    return 0;
}

void AddNewDecor(int Index, int Id)
{
    //Set new decor position
    AllDecor[Index].x = 256;
    AllDecor[Index].y = -10 + rand() % 100;

    AllDecor[Index].IsMoving = true;
    AllDecor[Index].Speed = 0.2 + rand() % 7 / 10.0;

    //Set decor type
    AllDecor[Index].Type = Id;

    int SpriteCount = 1;
    int SpriteOffset = 0;

    //Check how many sprites current obstacle need
    if (Id == 0)
        SpriteCount = 2;
    else
        SpriteOffset = 1;

    //Show sprites
    for (int i = 0; i < SpriteCount; i++)
    {
        int CurrentSpriteId = 0;

        if (i == 0)
            CurrentSpriteId = Cloud1 + Id + SpriteOffset;
        else if (i == 1)
            CurrentSpriteId = Cloud1 + Id + 1;

        u8 *SpriteOffset = ((u8 *)character32x32Tiles) + CurrentSpriteId * 32 * 32;
        dmaCopy(SpriteOffset, AllDecor[Index].gfx[i], 32 * 32);
    }
}

void AddNewObstacle(int Index, int Id)
{
    //Set new obstacle position
    AllObstacles[Index].x = 237;
    AllObstacles[Index].y = 140;
    AllObstacles[Index].IsMoving = true;

    //Change Y position for bird with random system
    if (Id == 7)
    {
        int Random = rand() % 3;
        if (Random == 0)
            AllObstacles[Index].y -= 18;
        else if (Random == 1)
            AllObstacles[Index].y -= 45;

        AllObstacles[Index].NextAnimationIn = 12;
    }

    //Set obstacle type
    AllObstacles[Index].Type = Id;
    int SpriteCount = 0;
    int SpriteOffset = 0;

    //Check how many sprites current obstacle need
    if (Id == 0 || Id == 1 || Id == 2)
        SpriteCount = 2;
    else if (Id == 3)
        SpriteCount = 4;
    else if (Id == 4 || Id == 5 || Id == 6 || Id == 7)
    {
        SpriteCount = 1;
        SpriteOffset = 6;
    }

    //Show sprites
    for (int i = 0; i < SpriteCount; i++)
    {
        int CurrentSpriteId = 0;

        if (i == 0)
            CurrentSpriteId = CactusTop + Id + SpriteOffset;
        else if (i == 1)
            CurrentSpriteId = CactusBottom + Id + SpriteOffset;
        else if (i == 2)
            CurrentSpriteId = CactusTop + Id + 1 + SpriteOffset;
        else if (i == 3)
            CurrentSpriteId = CactusBottom + Id + 1 + SpriteOffset;

        u8 *SpriteOffset = ((u8 *)character32x32Tiles) + CurrentSpriteId * 32 * 32;
        dmaCopy(SpriteOffset, AllObstacles[Index].gfx[i], 32 * 32);
    }
}

void ShowScore()
{
    //Set Hi score text color
    if (!NightMode)
        iprintf("\x1b[40m"); //Grey
    else
        iprintf("\x1b[37m"); //Light grey

    //Write Hi score
    if (CurrentHiScore < 10)
        iprintf("\x1b[0;17HHI 0000%d", CurrentHiScore);
    else if (CurrentHiScore < 100)
        iprintf("\x1b[0;17HHI 000%d", CurrentHiScore);
    else if (CurrentHiScore < 1000)
        iprintf("\x1b[0;17HHI 00%d", CurrentHiScore);
    else if (CurrentHiScore < 10000)
        iprintf("\x1b[0;17HHI 0%d", CurrentHiScore);
    else
        iprintf("\x1b[0;17HHI %d", CurrentHiScore);

    //Set score text color
    if (!NightMode)
        iprintf("\x1b[30m"); //Black
    else
        iprintf("\x1b[47m"); //White

    int ScoreToShow = Score;

    if (BlinkCount % 2 == 1 && BlinkCount != 0)
        ScoreToShow = LastScoreForBlink;

    if (BlinkCount % 2 == 0 && BlinkCount != 0)
        iprintf("\x1b[0;26H     ");
    else
    {
        //Write score
        if (ScoreToShow < 10)
            iprintf("\x1b[0;26H0000%d", ScoreToShow);
        else if (ScoreToShow < 100)
            iprintf("\x1b[0;26H000%d", ScoreToShow);
        else if (ScoreToShow < 1000)
            iprintf("\x1b[0;26H00%d", ScoreToShow);
        else if (ScoreToShow < 10000)
            iprintf("\x1b[0;26H0%d", ScoreToShow);
        else
            iprintf("\x1b[0;26H%d", ScoreToShow);
    }
}

void SetHitBox(int SpriteDataIndex, int HitBoxIndex, int Corner1X, int Corner1Y, int Corner2X, int Corner2Y)
{
    AllSpriteData[SpriteDataIndex].AllHitBox[HitBoxIndex].Corner1X = Corner1X;
    AllSpriteData[SpriteDataIndex].AllHitBox[HitBoxIndex].Corner1Y = Corner1Y;
    AllSpriteData[SpriteDataIndex].AllHitBox[HitBoxIndex].Corner2X = Corner2X;
    AllSpriteData[SpriteDataIndex].AllHitBox[HitBoxIndex].Corner2Y = Corner2Y;
}

void SetDayOrNightMode()
{
    int GreyValue = 10 + 11 * (1 - NightColorCoef);
    BG_PALETTE[0] = RGB15(GreyValue, GreyValue, GreyValue);
    SPRITE_PALETTE[3] = RGB15(GreyValue, GreyValue, GreyValue);

    int WhiteValue = 31 * NightColorCoef;
    BG_PALETTE[1] = RGB15(WhiteValue, WhiteValue, WhiteValue);

    SPRITE_PALETTE[2] = RGB15(WhiteValue, WhiteValue, WhiteValue);
    SPRITE_PALETTE[4] = RGB15(WhiteValue, WhiteValue, WhiteValue);
    SPRITE_PALETTE[5] = RGB15(WhiteValue, WhiteValue, WhiteValue);
    SPRITE_PALETTE[6] = RGB15(WhiteValue, WhiteValue, WhiteValue);
    SPRITE_PALETTE[7] = RGB15(WhiteValue, WhiteValue, WhiteValue);
    SPRITE_PALETTE[8] = RGB15(WhiteValue, WhiteValue, WhiteValue);

    int BlackValue = 27 - 21 * (1 - NightColorCoef);
    SPRITE_PALETTE[1] = RGB15(BlackValue, BlackValue, BlackValue);
}

void UpdateSprite(int xPos, int yPos, OamState *screen, u16 *gfx, int entry, int RotationId)
{
    oamSet(screen,                     // which display
           entry,                      // the oam entry to set
           xPos, yPos,                 // x & y location
           1,                          // priority
           0,                          // palette for 16 color sprite or alpha for bmp sprite
           SpriteSize_32x32,           // size
           SpriteColorFormat_256Color, // color type
           gfx,                        // the oam gfx
           RotationId,                 //Rotation index
           true,                       //double the size of rotated sprites
           false,                      //hide the sprite
           false, false,               //vflip, hflip
           false                       //apply mosaic
    );
}
