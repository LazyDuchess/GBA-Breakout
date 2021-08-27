#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 160
#define SCREEN_HEIGHT_MIN 12

#define MEM_IO   0x04000000
#define MEM_PAL  0x05000000
#define MEM_VRAM 0x06000000
#define MEM_OAM  0x07000000



#define DCNT_MODE3      0x0003
#define DCNT_BG2        0x0400

#define REG_DISPLAY        (*((volatile uint32 *)(MEM_IO)))
#define REG_BG2CNT        (*((volatile uint32 *)(MEM_IO + 0x000C)))
#define REG_DISPLAY_VCOUNT (*((volatile uint32 *)(MEM_IO + 0x0006)))
#define REG_KEY_INPUT      (*((volatile uint32 *)(MEM_IO + 0x0130)))

#define KEY_UP   0x0040
#define KEY_A 0x1
#define KEY_DOWN 0x0080
#define KEY_RIGHT   0x0010
#define KEY_LEFT 0x0020
#define KEY_ANY  0x03FF

#define OBJECT_ATTR0_Y_MASK 0x0FF
#define OBJECT_ATTR1_X_MASK 0x1FF
#define OBJECT_ATTR2_PALETTE_MASK 0xF000
#define OBJECT_ATTR2_PALETTE_SHIFT	12
#define OBJECT_ATTR2_PALETTE(n)	((n)<<OBJECT_ATTR2_PALETTE_SHIFT)

#define PLAYER_START_X SCREEN_WIDTH/2
#define PLAYER_START_Y SCREEN_HEIGHT-4

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;
typedef uint16 rgb15;
typedef struct obj_attrs {
	uint16 attr0;
	uint16 attr1;
	uint16 attr2;
	uint16 pad;
} __attribute__((packed, aligned(4))) obj_attrs;
typedef uint32    tile_4bpp[8];
typedef tile_4bpp tile_block[512];

//! Screen entry conceptual typedef
typedef uint16 SCR_ENTRY;

//! Affine parameter struct for backgrounds, covered later
typedef struct BG_AFFINE
{
    short pa, pb;
    short pc, pd;
    int dx, dy;
} __attribute__((packed, aligned(4))) BG_AFFINE;

//! Regular map offsets
typedef struct BG_POINT
{
    short x, y;
} __attribute__((packed, aligned(4))) BG_POINT;

//! Screenblock struct
typedef SCR_ENTRY   SCREENBLOCK[1024];


// === Memory map #defines (tonc_memmap.h) ============================

//! Screen-entry mapping: se_mem[y][x] is SBB y, entry x
#define se_mem          ((SCREENBLOCK*)MEM_VRAM)

#define oam_mem            ((volatile obj_attrs *)MEM_OAM)
#define tile_mem           ((volatile tile_block *)MEM_VRAM)

#define bg_palette_mem ((volatile rgb15 *)(MEM_PAL))
#define object_palette_mem ((volatile rgb15 *)(MEM_PAL + 0x200))


#define BLOCK_OAM_OFFSET 5
#define BLOCK_ROWS 6
#define BLOCK_AMOUNT_PER_ROW 10
#define BLOCK_HEIGHT 8
#define BLOCK_WIDTH 16
#define BLOCK_SPRITE_WIDTH 3
#define BLOCK_SPACE 2
#define BLOCK_POSITION_Y 32

#define BLOCK_MEMORY 70

typedef struct Block {
	int enabled;
	int hp;
	int hardened;
	int maxHP;
	int posX;
	int posY;
}__attribute__((packed, aligned(4))) Block;

#define LEVEL_AMOUNT 14
#define MAX_LIVES 3

#define LIVES_OAM_OFFSET BLOCK_OAM_OFFSET+BLOCK_MEMORY+1

int currentLevel = 0;

typedef struct Level {
	int rows;
	int amount_per_row;
	int hardened_rows;
}__attribute__((packed, aligned(4))) Level;

int ball_x;
int ball_y;
int blockAmount = 0;
int gameStateBlockAmount = 0;
int ball_velocity_x = 0,
	    ball_velocity_y = 0;

Block blockArray[BLOCK_MEMORY];

Level levelArray[LEVEL_AMOUNT];

// Form a 16-bit BGR GBA colour from three component values
static inline rgb15 RGB15(int r, int g, int b)
{
	return r | (g << 5) | (b << 10);
}

int heldBall = 1;

// Set the position of an object to specified x and y coordinates
static inline void set_object_position(volatile obj_attrs *object, int x,
                                       int y)
{
	object->attr0 = (object->attr0 & ~OBJECT_ATTR0_Y_MASK) |
	                (y & OBJECT_ATTR0_Y_MASK);
	object->attr1 = (object->attr1 & ~OBJECT_ATTR1_X_MASK) |
	                (x & OBJECT_ATTR1_X_MASK);
}

// Set the position of an object to specified x and y coordinates
static inline void set_object_palette(volatile obj_attrs *object, int palette)
{
	object->attr2 = (object->attr2 & ~OBJECT_ATTR2_PALETTE_MASK) |
	                (OBJECT_ATTR2_PALETTE(palette) & OBJECT_ATTR2_PALETTE_MASK);
}

int lives = MAX_LIVES;

static inline void process_lives(){
	for(unsigned int i=0;i<MAX_LIVES;i++)
	{
		volatile obj_attrs *heart_attrs = &oam_mem[LIVES_OAM_OFFSET+i];
		if (lives > i)
		{
			heart_attrs->attr0 = 0; // 4bpp tiles, SQUARE shape
			heart_attrs->attr1 = 0; // 8x8 size when using the SQUARE shape
			heart_attrs->attr2 = 10; // Start at the fifth tile in tile block four,
	                       // use color palette zero
						   set_object_position(heart_attrs,2+i*10,2);
		}
		else
		{
			heart_attrs->attr0 = 0; // 4bpp tiles, SQUARE shape
			heart_attrs->attr1 = 0; // 8x8 size when using the SQUARE shape
			heart_attrs->attr2 = 0; // Start at the fifth tile in tile block four,
	                       // use color palette zero
		}
	}
}

static inline void process_blocks(){
	for(unsigned int i = 0;i<BLOCK_MEMORY;i++){
		Block *thisBlock = &blockArray[i];
		if (thisBlock->enabled == 1)
		{
			volatile obj_attrs *block_attrs = &oam_mem[BLOCK_OAM_OFFSET+i];
			block_attrs->attr0 = 0x4000; // 4bpp tiles, TALL shape
			block_attrs->attr1 = 0x3000; // 8x32 size when using the TALL shape
			if (thisBlock->hp == thisBlock->maxHP)
			block_attrs->attr2 = 6;      // Start at the first tile in tile
		else
			block_attrs->attr2 = 8;      // Start at the first tile in tile
									// block four, use color palette zero
								  set_object_position(block_attrs,thisBlock->posX,thisBlock->posY);
								  if (thisBlock->hardened == 1)
								  set_object_palette(block_attrs,1);
		}
		else
		{
			volatile obj_attrs *block_attrs = &oam_mem[BLOCK_OAM_OFFSET+i];
			block_attrs->attr0 = 0; // 4bpp tiles, TALL shape
			block_attrs->attr1 = 0; // 8x32 size when using the TALL shape
			block_attrs->attr2 = 0;      // Start at the first tile in tile
	                              // block four, use color palette zero
		}
	}
}

static inline int game_won(){
	if (gameStateBlockAmount <= 0)
		return 1;
	return 0;
}

static inline void create_blocks(Level *level){
	for(unsigned int i = 0;i<BLOCK_MEMORY;i++)
	{
		Block *thisBlock = &blockArray[i];
		thisBlock->enabled = 0;
	}
	int baseXPos = 1+(SCREEN_WIDTH/2)-(((BLOCK_WIDTH+BLOCK_SPACE)*level->amount_per_row)/2);
	int baseYPos = 30;
	int cBlockID = 0;
	for(unsigned int i = 0;i<level->rows;i++){
		for(unsigned int n = 0;n<level->amount_per_row;n++){
			Block *thisBlock = &blockArray[cBlockID];
			thisBlock->enabled = 1;
			thisBlock->posX = baseXPos + (n*(BLOCK_WIDTH+BLOCK_SPACE));
			thisBlock->posY = baseYPos + (i*(BLOCK_HEIGHT+BLOCK_SPACE));
			thisBlock->hardened = 0;
			if (level->hardened_rows > i)
			{
				thisBlock->hp = 2;
				thisBlock->maxHP = 2;
				thisBlock->hardened = 1;
			}
			else
			{
				thisBlock->hp = 1;
				thisBlock->maxHP = 1;
			}
			cBlockID += 1;
		}
	}
	blockAmount = cBlockID;
	gameStateBlockAmount = blockAmount;
}

static inline void restart_game(){
	create_blocks(&levelArray[currentLevel]);
	ball_x = 0;
	ball_y = 0;
	heldBall = 1;
	ball_velocity_x = 0;
	ball_velocity_y = 0;
	process_blocks();
}

static inline void next_level(){
	currentLevel += 1;
	if (currentLevel >= LEVEL_AMOUNT)
		currentLevel = 0;
	restart_game();
}



static inline int process_block_breaking(){
	int updated = 0;
	for(unsigned int i=0;i<blockAmount;i++){
		Block *thisBlock = &blockArray[i];
		if (thisBlock->enabled == 1)
		{
			if (ball_x+8 >= thisBlock->posX && ball_y+8 >= thisBlock->posY && ball_x <= thisBlock->posX+BLOCK_WIDTH && ball_y <= thisBlock->posY+BLOCK_HEIGHT)
			{
				thisBlock->hp = thisBlock->hp - 1;
				if (thisBlock->hp <= 0)
				{
					thisBlock->enabled = 0;
					gameStateBlockAmount = gameStateBlockAmount - 1;
				}
				updated = 1;
				if (ball_y+8 <= thisBlock->posY && ball_velocity_y > 0)
					ball_velocity_y = -ball_velocity_y;
				else if (ball_y >= thisBlock->posY+BLOCK_HEIGHT && ball_velocity_y < 0)
					ball_velocity_y = -ball_velocity_y;
				else if (ball_x+8 <= thisBlock->posX && ball_velocity_x > 0)
					ball_velocity_x = -ball_velocity_x;
				else if (ball_x >= thisBlock->posX+BLOCK_WIDTH && ball_velocity_x < 0)
					ball_velocity_x = -ball_velocity_x;
				break;
			}
		}
	}
	if (updated == 1)
	{
		process_blocks();
		if (game_won() == 1)
			next_level();
	}
	return updated;
}



// Clamp 'value' in the range 'min' to 'max' (inclusive)
static inline int clamp(int value, int min, int max)
{
	return (value < min ? min
	                    : (value > max ? max : value));
}

static inline void makeHeartSprite(volatile uint16 *mem_loc){
	mem_loc[1] = 0x0330; mem_loc[0] = 0x0330;
	mem_loc[3] = 0x3333; mem_loc[2] = 0x3333;
	mem_loc[5] = 0x3333; mem_loc[4] = 0x3333;
	mem_loc[7] = 0x3333; mem_loc[6] = 0x3333;
	mem_loc[9] = 0x3333; mem_loc[8] = 0x3333;
	mem_loc[11] = 0x0333; mem_loc[10] = 0x3330;
	mem_loc[13] = 0x0033; mem_loc[12] = 0x3300;
	mem_loc[15] = 0x0003; mem_loc[14] = 0x3000;
}

static inline void makeBallSprite(volatile uint16 *mem_loc){
	mem_loc[1] = 0x0044; mem_loc[0] = 0x4400;
	mem_loc[3] = 0x0422; mem_loc[2] = 0x2240;
	mem_loc[5] = 0x4222; mem_loc[4] = 0x2124;
	mem_loc[7] = 0x4222; mem_loc[6] = 0x2224;
	mem_loc[9] = 0x4222; mem_loc[8] = 0x2224;
	mem_loc[11] = 0x4222; mem_loc[10] = 0x2224;
	mem_loc[13] = 0x0422; mem_loc[12] = 0x2240;
	mem_loc[15] = 0x0044; mem_loc[14] = 0x4400;
}

static inline void makeBlockStartSprite(volatile uint16 *mem_loc){
	mem_loc[1] = 0x5555; mem_loc[0] = 0x5550;
	mem_loc[3] = 0x3333; mem_loc[2] = 0x3335;
	mem_loc[5] = 0x3333; mem_loc[4] = 0x3335;
	mem_loc[7] = 0x3333; mem_loc[6] = 0x3335;
	mem_loc[9] = 0x3333; mem_loc[8] = 0x3335;
	mem_loc[11] = 0x3333; mem_loc[10] = 0x3335;
	mem_loc[13] = 0x3333; mem_loc[12] = 0x3335;
	mem_loc[15] = 0x5555; mem_loc[14] = 0x5550;
}

static inline void makeBlockEndSprite(volatile uint16 *mem_loc){
	mem_loc[1] = 0x0555; mem_loc[0] = 0x5555;
	mem_loc[3] = 0x5333; mem_loc[2] = 0x3333;
	mem_loc[5] = 0x5333; mem_loc[4] = 0x3333;
	mem_loc[7] = 0x5333; mem_loc[6] = 0x3333;
	mem_loc[9] = 0x5333; mem_loc[8] = 0x3333;
	mem_loc[11] = 0x5333; mem_loc[10] = 0x3333;
	mem_loc[13] = 0x5333; mem_loc[12] = 0x3333;
	mem_loc[15] = 0x0555; mem_loc[14] = 0x5555;
}

static inline void makeBlockBreakStartSprite(volatile uint16 *mem_loc){
	mem_loc[1] = 0x5555; mem_loc[0] = 0x6550;
	mem_loc[3] = 0x3336; mem_loc[2] = 0x3335;
	mem_loc[5] = 0x6663; mem_loc[4] = 0x3335;
	mem_loc[7] = 0x3363; mem_loc[6] = 0x3335;
	mem_loc[9] = 0x6633; mem_loc[8] = 0x3335;
	mem_loc[11] = 0x3663; mem_loc[10] = 0x3335;
	mem_loc[13] = 0x3336; mem_loc[12] = 0x6335;
	mem_loc[15] = 0x5555; mem_loc[14] = 0x5650;
}

static inline void makeBlockBreakEndSprite(volatile uint16 *mem_loc){
	mem_loc[1] = 0x0555; mem_loc[0] = 0x5555;
	mem_loc[3] = 0x5633; mem_loc[2] = 0x3333;
	mem_loc[5] = 0x5633; mem_loc[4] = 0x3366;
	mem_loc[7] = 0x5363; mem_loc[6] = 0x3633;
	mem_loc[9] = 0x5336; mem_loc[8] = 0x6666;
	mem_loc[11] = 0x5333; mem_loc[10] = 0x3633;
	mem_loc[13] = 0x5336; mem_loc[12] = 0x6333;
	mem_loc[15] = 0x0565; mem_loc[14] = 0x5555;
}

int main(void)
{
	bg_palette_mem[0] = RGB15(3,0,7);
	/*
	volatile uint16 *addr = ((uint16 *)MEM_VRAM);
	for (unsigned int i=0;i<40;i++)
	{
		addr[i] = 0x1001;
	}*/
	heldBall = 1;
	
	//Init Levels
	levelArray[0].rows = 4;
	levelArray[0].amount_per_row = 3;
	levelArray[0].hardened_rows = 0;
	
	levelArray[1].rows = 4;
	levelArray[1].amount_per_row = 5;
	levelArray[1].hardened_rows = 0;
	
	levelArray[2].rows = 4;
	levelArray[2].amount_per_row = 5;
	levelArray[2].hardened_rows = 1;
	
	levelArray[3].rows = 4;
	levelArray[3].amount_per_row = 6;
	levelArray[3].hardened_rows = 1;
	
	levelArray[4].rows = 6;
	levelArray[4].amount_per_row = 5;
	levelArray[4].hardened_rows = 0;
	
	levelArray[5].rows = 6;
	levelArray[5].amount_per_row = 5;
	levelArray[5].hardened_rows = 1;
	
	levelArray[6].rows = 6;
	levelArray[6].amount_per_row = 10;
	levelArray[6].hardened_rows = 0;
	
	levelArray[7].rows = 6;
	levelArray[7].amount_per_row = 10;
	levelArray[7].hardened_rows = 1;
	
	levelArray[8].rows = 6;
	levelArray[8].amount_per_row = 10;
	levelArray[8].hardened_rows = 2;
	
	levelArray[9].rows = 6;
	levelArray[9].amount_per_row = 10;
	levelArray[9].hardened_rows = 3;
	
	levelArray[10].rows = 7;
	levelArray[10].amount_per_row = 10;
	levelArray[10].hardened_rows = 0;
	
	levelArray[11].rows = 7;
	levelArray[11].amount_per_row = 10;
	levelArray[11].hardened_rows = 1;
	
	levelArray[12].rows = 7;
	levelArray[12].amount_per_row = 10;
	levelArray[12].hardened_rows = 2;
	
	levelArray[13].rows = 7;
	levelArray[13].amount_per_row = 10;
	levelArray[13].hardened_rows = 3;
	
	// Write the tiles for our sprites into the fourth tile block in VRAM.
	// Four tiles for an 8x32 paddle sprite, and one tile for an 8x8 ball
	// sprite. Using 4bpp, 0x1111 is four pixels of colour index 1, and
	// 0x2222 is four pixels of colour index 2.
	//
	// NOTE: We're using our own memory writing code here to avoid the
	// byte-granular writes that something like 'memset' might make (GBA
	// VRAM doesn't support byte-granular writes).
	volatile uint16 *paddle_tile_mem = (uint16 *)tile_mem[4][1];
	volatile uint16 *ball_tile_mem   = (uint16 *)tile_mem[4][5];
	volatile uint16 *block_tile_mem  = (uint16 *)tile_mem[4][6];
	volatile uint16 *block_break_tile_mem  = (uint16 *)tile_mem[4][8];
	volatile uint16 *heart_tile_mem  = (uint16 *)tile_mem[4][10];
	for (int i = 0; i < 4 * (sizeof(tile_4bpp) / 2); ++i)
		paddle_tile_mem[i] = 0x1111; // 0b_0001_0001_0001_0001
	makeBallSprite(ball_tile_mem);
	/*
	for (int i = 0; i < (sizeof(tile_4bpp) / 2); ++i)
		ball_tile_mem[i] = 0x2222;   // 0b_0002_0002_0002_0002*/
	/*
	for (int i = 0; i < BLOCK_SPRITE_WIDTH * (sizeof(tile_4bpp) / 2); ++i)
		block_tile_mem[i] = 0x3333; // 0b_0001_0001_0001_0001*/
	makeBlockStartSprite(block_tile_mem);
	makeBlockEndSprite(block_tile_mem+(sizeof(tile_4bpp) / 2));
	
	makeBlockBreakStartSprite(block_break_tile_mem);
	makeBlockBreakEndSprite(block_break_tile_mem+(sizeof(tile_4bpp) / 2));
	
	makeHeartSprite(heart_tile_mem);
	// Write the colour palette for our sprites into the first palette of
	// 16 colours in colour palette memory (this palette has index 0)

	object_palette_mem[1] = RGB15(31, 31, 31); // White
	object_palette_mem[2] = RGB15(0, 20, 31); // Lightish Blue
	object_palette_mem[3] = RGB15(31, 0, 0); // Red
	object_palette_mem[4] = RGB15(0, 10, 25); // Darker Blue
	object_palette_mem[5] = RGB15(15, 0, 0); // Darker Red ( outline )
	object_palette_mem[6] = RGB15(5, 0, 0); // Darker Red ( breaking )
	
	object_palette_mem[16+3] = RGB15(20, 0, 31); // Purple
	object_palette_mem[16+5] = RGB15(10, 0, 20); // Darker Purple ( outline )
	object_palette_mem[16+6] = RGB15(2, 0, 5); // Darker Purple ( Breaking )

	// Create our sprites by writing their object attributes into OAM
	// memory
	volatile obj_attrs *paddle_attrs = &oam_mem[0];
	paddle_attrs->attr0 = 0x4000; // 4bpp tiles, TALL shape
	paddle_attrs->attr1 = 0x4000; // 8x32 size when using the TALL shape
	paddle_attrs->attr2 = 1;      // Start at the first tile in tile
	                              // block four, use color palette zero
								  
	volatile obj_attrs *ball_attrs = &oam_mem[1];
	ball_attrs->attr0 = 0; // 4bpp tiles, SQUARE shape
	ball_attrs->attr1 = 0; // 8x8 size when using the SQUARE shape
	ball_attrs->attr2 = 5; // Start at the fifth tile in tile block four,
	                       // use color palette zero

	// Initialize variables to keep track of the state of the paddle and
	// ball, and set their initial positions (by modifying their
	// attributes in OAM)
	const int player_width = 32,
	          player_height = 8;
	const int ball_width = 8,
	          ball_height = 8;
	int player_velocity = 2;
	ball_velocity_x = 0;
	ball_velocity_y = 0;
	int player_x = PLAYER_START_X,
	    player_y = PLAYER_START_Y;
	ball_x = player_x+(player_width/2)-(ball_width/2);
	ball_y = player_y-player_height;
	set_object_position(paddle_attrs, player_x, player_y);
	set_object_position(ball_attrs, ball_x, ball_y);
	create_blocks(&levelArray[currentLevel]);
	process_blocks();


	// Set the display parameters to enable objects, and use a 1D
	// object->tile mapping
	REG_DISPLAY = 0x1000 | 0x0040;

	// The main game loop
uint32 key_states = 0;
process_lives();
	while (1) {
		// Skip past the rest of any current V-Blank, then skip past
		// the V-Draw
		while(REG_DISPLAY_VCOUNT >= 160);
		while(REG_DISPLAY_VCOUNT <  160);
		// Get current key states (REG_KEY_INPUT stores the states
		// inverted)
		key_states = ~REG_KEY_INPUT & KEY_ANY;

		// Note that our physics update is tied to the framerate,
		// which isn't generally speaking a good idea. Also, this is
		// really terrible physics and collision handling code.
		int player_max_clamp_x = SCREEN_WIDTH - player_width;
		if (key_states & KEY_UP && heldBall == 1)
		{
			heldBall = 0;
			//ball_y = player_y-player_height-ball_height;
			ball_velocity_y = -2;
			ball_velocity_x = 1;
		}
		int velocity_multi = 1;
		if (key_states & KEY_A)
			velocity_multi = 2;
		if (key_states & KEY_LEFT)
			player_x = clamp(player_x - (player_velocity * velocity_multi), 0,
			                 player_max_clamp_x);
		if (key_states & KEY_RIGHT)
			player_x = clamp(player_x + (player_velocity * velocity_multi), 0,
			                 player_max_clamp_x);
		if (key_states & KEY_RIGHT || key_states & KEY_LEFT)
		{
			set_object_position(paddle_attrs, player_x, player_y);
		}

		int ball_max_clamp_x = SCREEN_WIDTH  - ball_width;
			/*
		if ((ball_x >= player_x &&
		     ball_x <= player_x + player_width) &&
		    (ball_y >= player_y &&
		     ball_y <= player_y + player_height)) {
			ball_x = player_x + player_width;
			ball_velocity_x = -ball_velocity_x;
		} else {
			if (ball_x == 0 || ball_x == ball_max_clamp_x)
				ball_velocity_x = -ball_velocity_x;
			if (ball_y == 0 || ball_y == ball_max_clamp_y)
				ball_velocity_y = -ball_velocity_y;
		}*/
		if (heldBall == 0)
		{
		int ball_velocity_y_abs = ball_velocity_y;
		int ball_velocity_x_abs = ball_velocity_x;
		int ball_velocity_x_sign = 1;
		int ball_velocity_y_sign = 1;
		if (ball_velocity_y < 0)
		{
			ball_velocity_y_abs = -ball_velocity_y;
			ball_velocity_y_sign = -1;
		}
		if (ball_velocity_x < 0)
		{
			ball_velocity_x_abs = -ball_velocity_x;
			ball_velocity_x_sign = -1;
		}
		int largestVel = ball_velocity_x_abs;
		if (ball_velocity_y_abs > ball_velocity_x_abs)
			largestVel = ball_velocity_y_abs;
		if (largestVel >= 1)
		{
		for(unsigned int i=1;i<=largestVel;i++){
			if (i <= ball_velocity_y_abs)
			{
				ball_y = ball_y + ball_velocity_y_sign;
			}
			if (i <= ball_velocity_x_abs)
			{
				ball_x = ball_x + ball_velocity_x_sign;
			}
			int blockBr = process_block_breaking();
			if (blockBr == 1)
				break;
		}
		}
		if (ball_x <= 0 || ball_x >= ball_max_clamp_x)
			{
				ball_velocity_x = -ball_velocity_x;
			}
			if (ball_y <= SCREEN_HEIGHT_MIN)
			{
				ball_velocity_y = -ball_velocity_y;
			}
			if (ball_y >= SCREEN_HEIGHT)
			{
				heldBall = 1;
				ball_velocity_x = 0;
				ball_velocity_y = 0;
				lives -= 1;
				if (lives <= 0)
				{
					lives = MAX_LIVES;
					currentLevel = 0;
					restart_game();
					
				}
				process_lives();
			}
			if (ball_x+ball_width >= player_x && ball_x <= player_x + player_width && ball_y >= player_y-ball_height && ball_velocity_y > 0)
			{
				int bVelAbs = ball_velocity_y;
				if (bVelAbs < 0)
					bVelAbs = -bVelAbs;
				bVelAbs = bVelAbs*5;
				ball_velocity_x = ((ball_x+(ball_width/2))-(player_x+(player_width/2)))/bVelAbs;
				int bVelAbsX = ball_velocity_x;
				if (bVelAbsX < 0)
					bVelAbsX = -bVelAbsX;
				ball_velocity_y = (-ball_velocity_y)+bVelAbsX;
				if (ball_velocity_y >= 0)
					ball_velocity_y = -1;
				if (ball_velocity_x == 0)
				{
					int halfPlayer = player_x+(player_width/2);
					int halfBall = ball_x+(ball_width/2);
					int diff = halfBall-halfPlayer;
					ball_velocity_y = -2;
					if (diff >= 0)
						ball_velocity_x = 1;
					else
						ball_velocity_x = -1;
				}
				if ((ball_velocity_x == 1 || ball_velocity_x == -1) && (ball_velocity_y == 1 || ball_velocity_y == -1))
				{
					ball_velocity_x *= 2;
					ball_velocity_y *= 2;
				}
			}
		}
		//process_block_breaking();
		ball_x = clamp(ball_x/* + ball_velocity_x*/, 0, ball_max_clamp_x);
		ball_y = clamp(ball_y/* + ball_velocity_y*/, 0, SCREEN_HEIGHT);
		if (heldBall == 1)
		{
			ball_x = player_x+(player_width/2)-(ball_width/2);
			ball_y = player_y-player_height;
		}
		set_object_position(ball_attrs, ball_x, ball_y);
		//process_block_breaking();
	}

	return 0;
}