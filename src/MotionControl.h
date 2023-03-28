/// TODO: 
///
/// 1. Better SLEEP FUNCTION
/// SetCurrent() calls setTMC262Register from inside interrupt. 
/// (requestAwake ? is Awake ? some shared flags ?)
///
/// 2. Add status from M4 and M5
///
/// 3. refactor main StateMachine
///
/// Separate Configuration // Hardware setup to another file ?
///

#ifndef MOTIONCONTROL_H
#define MOTIONCONTROL_H

#include "RoboTimer.h"
#include "TMC_Registers.h"
#include <Arduino.h>

#define XMAX 1100000 // total X steps +- 1127620 | margin (13581)
#define YMAX 1568000 // total Y steps +- 1595169 | margin (13581)

/// @brief Union to convert int64 to bytearray.
union byte64
{
    int64_t value;
    byte bytes[8];
};

/// @brief Union to convert int32 to bytearray.
union byte32
{
    int32_t value;
    byte bytes[4];
};

struct DrawInstruction
{
    int64_t index;
    uint8_t type;
    int8_t dirX;
    int8_t dirY;
    int64_t startX;
    int64_t startY;
    int64_t endX;
    int64_t endY;
    int64_t deltaX;
    int64_t deltaY;
    int64_t deltaXX;
    int64_t deltaYY;
    int64_t deltaXY;
    int64_t error;
    int64_t steps;
};

struct moveInstruction
{
    int8_t dirX;
    int8_t dirY;    
    int64_t endX;
    int64_t endY;
    int64_t deltaX;
    int64_t deltaY;   
    int64_t error;
    double steps;
    double step; 
};

// ===================== Machines task switching & State Machines =====================

enum State
{
    state_none = 0,
    state_start = 1,
    state_home = 2,
    state_draw = 3,
    state_idle = 4,
    state_panic = 5,
    state_eof = 6,
    state_reset = 7,
    state_mapheight = 10,
    state_clearheight = 11,
};

enum class MachineState {
    None,
    Start,
    Home,
    Draw,
    Stop,
    Idle,
    Panic,
    EOL,
    Reset,
    MapHeight,
    ClearHeight
};

/// @brief State Machine used for drawing lines.
enum class DrawState {
    LineStart,
    Move,
    Wait,
    Done
};

/// @brief State Machine used for homeing.
enum class HomeState {
    Zero,
    Home,
    Done
};

/// @brief State Machine used for building a heigtMap.
enum class MapHeightState 
{
    None,
    Choose,
    MoveUp,
    StartMoveXY,
    MoveXY,
    MoveDown,
    Done
};

extern volatile State activeState;
extern volatile State requestedState;

// ===================== Circular buffer for draw instructions. =====================

/// @brief Draw Instruction circular buffer
/// @attention use power of 2 size so I can use & in stead of modulo.
/// e.g. tailIndex = (tailIndex + 1) & 63;
extern volatile DrawInstruction iBuffer[64];

/// @brief Write index for draw instruction buffer
extern volatile uint8_t iBufferWriteIndex;

/// @brief Read index for draw instruction buffer
extern volatile uint8_t iBufferReadIndex;

/// @brief Index for last requested Instruction
extern volatile int64_t requestedInstruction;

/// @brief Index for last received Instruction
extern volatile int64_t receivedInstruction;


// ===================== Height Mapping algorithm. =====================

#define HeightMapWidth 4
#define HeightMapHeight 6
#define HeightMapSize 24

extern volatile int64_t HeightMap[];

/// ===================== Limit switch states (pressed is true) =====================

extern volatile bool Limit_Y1_start;
extern volatile bool Limit_Y1_end;
extern volatile bool Limit_Y2_start;
extern volatile bool Limit_Y2_end;
extern volatile bool Limit_X_start;
extern volatile bool Limit_X_end;
extern volatile bool Limit_Z_start;
extern volatile bool Limit_Z_end;

/// =====================  Stepper motor hardware, config and status  =====================

/// Status

/// @brief Used to display status.
/// Indicating current function of the machine.
extern volatile uint8_t drawFunction;

/// @brief Used to display status.
/// Current line being drawn.
extern volatile int32_t drawIndex;

/// @brief union for Motor 1 status register.
extern TMC262::STATUS status_M1;

/// @brief union for Motor 2 status register.
extern TMC262::STATUS status_M2;

/// @brief union for Motor 3 status register.
extern TMC262::STATUS status_M3;

/// ===================== Motion Control =====================

/// motor position (global for status display)

extern volatile int32_t M1_pos;
extern volatile int32_t M2_pos;
extern volatile int32_t M3_pos;
extern volatile int32_t M4_pos;
extern volatile int32_t M5_pos;

/// ===================== FUNCTIONS that are part of the intterrupt loop (FASTRUN) =====================

/// @brief The main motion control loop. Called via interrupt timer. To move as
/// continuous as possible, prepared motor stepping is performed at beginning of each
/// loop. After that actions for the do next iteration are calculated, which can take
/// variable amount of time. Following this pattern, movement is controled at a fixed
/// interval as close to the interrupt call as possible.
/// @return
FASTRUN void MachineLoop();

/// @brief The homing algorithm
FASTRUN void CalculateHomeSteps();

/// @brief The drawing algorithm.
// Calculate steps & directions for next iteration.
FASTRUN void CalculateDrawSteps();

/// @brief The height mapping algorithm.
FASTRUN void MapHeight();

/// @brief Perform Motor Steps that are calculated in previous iteration
/// and updates position variables.
/// @return
FASTRUN void StepMotors();

/// @brief Set PRE-STEP Directions and check LIMIT switches,
/// disables steps if limits are hit.
/// @attention FASTRUN in interrupt
/// @return
FASTRUN void SetDirectionsAndLimits();

/// @brief Debouncing of Limit switches.
/// Fast press and slow release debounce.
/// Press debounce is shifted in 8 StepLoops
/// Release debounce is performed every 300000 cycles
FASTRUN void DebounceSwitches();

/// @brief Prepare a step in X direction
/// @param dir positive or negative X direction
FASTRUN void StepX(int8_t dir);

/// @brief Prepare a step in Y direction
/// @param dir positive or negative Y direction
FASTRUN void StepY(int8_t dir);

/// @brief Prepare a step in Z direction
/// @param dir positive or negative Z direction
FASTRUN void StepZ(int8_t dir);

/// @brief Manually set current scale on Stepper Motors
/// @param cur 0 - 31 current scale
FASTRUN void setCurrent(int cur);

/// @brief Draw / Step in a straight Line
FASTRUN void CalculateStraightLine();

/// @brief Draw / Step along a Quadratic Bezier
FASTRUN void CalculateQuadBezier();

/// ===================== FUNCTIONS not part of the intterrupt loop =====================

/// @brief Startup procedure, sets up interrupt timer and
/// enables stepper motors.
void StartUp();

/// @brief Configure hardware pins as input for switches
void configureSwitches();

/// @brief Configure hardware pins as output for stepper motors and configure
/// all the stepper driver registers.
void configureStepperDrivers();

/// @brief Reads status registers from Stepper drivers
void updateStepperStatus();

/// @brief Function to set TMC262 Stepper Driver register
/// @param bytes Register data (24bits)
/// @param CSPIN SPI pin the driver is attached to
/// @return A status union with various driver flags and driver information
TMC262::STATUS setTMC262Register(uint8_t bytes[3], int CSPIN);

/// @brief Function to set TMC263 Stepper Driver register
/// @param address Register address (8bits)
/// @param data Register data (32bits)
/// @param CSPIN SPI pin the driver is attached to
/// @return A status union with various driver flags and driver information
TMC2130::SPI_STATUS setTMC2130Register(uint8_t address, uint32_t data, int CSPIN);

#endif