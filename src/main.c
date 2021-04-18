#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>

#include "texture.h"
#include "game.h"

#define MAX_PLAYERS 256
#define TURN_ACTIONS_NUM 3

int is_running;
SDL_Window *window;
SDL_Renderer *renderer;

enum state_e
{
	menu,
	move,
	attack,
	pass,
};
enum state_e state;

typedef struct vec2i_t
{
	int x;
	int y;
} vec2i_t;

typedef struct game_memory_t
{
	int is_initialized;
	unsigned long long permanent_storage_size;
	void *permanent_storage;
	unsigned long long transient_storage_size;
	void *transient_storage;
} game_memory_t;

typedef struct game_state_t
{
	int test_val;
} game_state_t;

typedef struct player_t
{
	char *name;
	int hp;
	int att;
	int dead;
	int party;
	vec2i_t pos;
	SDL_Rect src_rect;
	SDL_Rect dst_rect;
	SDL_Texture *texture;
	int animation_timer;
	int animation_frame;
} player_t;

typedef struct cursor_t
{
	SDL_Texture *texture;
	SDL_Rect dst_rect;
	vec2i_t pos;
} cursor_t;

int menu_cursor_index;
int size;
int scale;
int offset_x;
int offset_y;

player_t paladin;
player_t black_mage;
player_t white_mage;
player_t slime_1;
player_t slime_2;

player_t *players[MAX_PLAYERS] = {0};
int players_num;
int players_index;

player_t *player_current;




cursor_t cursor;



SDL_Texture *move_area;
SDL_Rect move_area_dst_rect;

SDL_Texture *attack_area;
SDL_Rect attack_area_dst_rect;


TTF_Font *font;

void input_handler();


int turn_start_pos_x;
int turn_start_pos_y;
int turn_actions[TURN_ACTIONS_NUM] = {0};

Mix_Music *bgm;
Mix_Chunk *sound_select;
Mix_Chunk *sound_miss;
Mix_Chunk *sound_move;

void turn_actions_reset()
{
	for(int i = 0; i < TURN_ACTIONS_NUM; i++)
	{
		turn_actions[i] = 0;
	}
}

int abs(int val)
{
	return (val * (-1));
}

void set_rect(SDL_Rect *rect, int x, int y)
{
	rect->x = x * size * scale + offset_x;
	rect->y = y * size * scale + offset_y;
	rect->w = size * scale;
	rect->h = size * scale;
}

void check_game_over()
{
	int i = 0;
	int is_game_over = 1;
	while(players[i])
	{
		if(players[i]->party == 1)
		{
			if(players[i]->dead == 0)
			{
				is_game_over = 0;
			}
		}
		i++;
	}

	if(is_game_over)
		is_running = 0;
}

// ------------------------------------------------------------------------------------------------------------------
// CURSOR
// ------------------------------------------------------------------------------------------------------------------
/*
static void 
cursor_move_to(cursor_t *cursor, int x, int y)
{
	cursor->pos.x = x;
	cursor->pos.y = y;
	cursor->dst_rect.x = cursor->pos.x * size * scale + offset_x;
	cursor->dst_rect.y = cursor->pos.y * size * scale + offset_y;
}
*/

static void 
cursor_move_by(cursor_t *cursor, int x, int y)
{
	int absolute_pos_x = cursor->pos.x + x;
	int absolute_pos_y = cursor->pos.y + y;

	if(absolute_pos_x < 0 || absolute_pos_x > 8 - 1 ||
		absolute_pos_y < 0 || absolute_pos_y > 8 - 1) return;

	cursor->pos.x += x;
	cursor->pos.y += y;
}

// ------------------------------------------------------------------------------------------------------------------
// AREA
// ------------------------------------------------------------------------------------------------------------------
static void 
move_area_render(SDL_Renderer *renderer)
{
	for(int y = 0; y < 8; y++)
	{
		for(int x = 0; x < 8; x++)
		{
			int tmp_x = abs(player_current->pos.x - x);
			int tmp_y = abs(player_current->pos.y - y);

			if(tmp_x + tmp_y < 2 + 1)
			{	
				int i = 0;
				int found = 0;
				while(players[i])
				{
					if(players[i]->pos.x == x && players[i]->pos.y == y)
					{
						found = 1;
						break;
					}
					i++;
				}

				if(!found)
				{
					move_area_dst_rect.x = x * size * scale + offset_x;
					move_area_dst_rect.y = y * size * scale + offset_y;
					if((x + y) % 2)
					{
						SDL_Rect src_rect = {1 * size, 0 * size, size, size};
						SDL_RenderCopy(renderer, move_area, &src_rect, &move_area_dst_rect);
					}
					else
					{
						SDL_Rect src_rect = {0 * size, 0 * size, size, size};
						SDL_RenderCopy(renderer, move_area, &src_rect, &move_area_dst_rect);
					}
				}
			}
		}
	}
}

static void 
attack_area_render(SDL_Renderer *renderer)
{
	for(int y = 0; y < 8; y++)
	{
		for(int x = 0; x < 8; x++)
		{
			int tmp_x = (player_current->pos.x - x);
			if(tmp_x < 0) tmp_x *= (-1);
			int tmp_y = (player_current->pos.y - y);
			if(tmp_y < 0) tmp_y *= (-1);

			if(tmp_x + tmp_y < 1 + 1)
			{	
				attack_area_dst_rect.x = x * size * scale + offset_x;
				attack_area_dst_rect.y = y * size * scale + offset_y;
				if((x + y) % 2)
				{
					SDL_Rect src_rect = {1 * size, 0 * size, size, size};
					SDL_RenderCopy(renderer, attack_area, &src_rect, &attack_area_dst_rect);
				}
				else
				{
					SDL_Rect src_rect = {0 * size, 0 * size, size, size};
					SDL_RenderCopy(renderer, attack_area, &src_rect, &attack_area_dst_rect);
				}
			}
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------
// SDL
// ------------------------------------------------------------------------------------------------------------------
static int
sdl_init()
{
	SDL_Init(SDL_INIT_EVERYTHING);

	window = SDL_CreateWindow("Oliark", 100, 100, 800, 600, SDL_WINDOW_RESIZABLE);
	if(!window)
	{
		printf("%s\n", SDL_GetError());
		return 0;
	}

	renderer = SDL_CreateRenderer(window, -1, 0);
	if(!renderer)
	{
		printf("%s\n", SDL_GetError());
		return 0;
	}

	return 1;
}

static void 
sdl_quit()
{
	Mix_FreeMusic(bgm);
	Mix_FreeChunk(sound_select);
	Mix_FreeChunk(sound_move);
	Mix_FreeChunk(sound_miss);
	Mix_Quit();

	SDL_Quit();
}

// ------------------------------------------------------------------------------------------------------------------
// FONT
// ------------------------------------------------------------------------------------------------------------------
static int 
font_init()
{
	if(TTF_Init() != 0)
	{
		printf("%s\n", SDL_GetError());
		return 0;
	}

	font = TTF_OpenFont("./assets/fonts/opensans.ttf", 32);
	if(!font)
	{
		printf("%s\n", SDL_GetError());
		return 0;
	}
	
	return 1;
}

static void 
ui_font_render(char *text, int pos_x, int pos_y)
{
	SDL_Color color = {255, 255, 255, 255};
	SDL_Surface *surface; 
	SDL_Texture *texture;
	SDL_Rect dst_rect;

	surface = TTF_RenderText_Solid(font, text, color);
	if(!surface)
	{
		printf("%s\n", SDL_GetError());
		return;
	}

	texture = SDL_CreateTextureFromSurface(renderer, surface);
	if(!texture)
	{
		SDL_FreeSurface(surface);
		printf("%s\n", SDL_GetError());
		return;
	}

	dst_rect.x = pos_x;
	dst_rect.y = pos_y;
	SDL_QueryTexture(texture, NULL, NULL, &dst_rect.w, &dst_rect.h);

	SDL_RenderCopy(renderer, texture, NULL, &dst_rect);

	SDL_FreeSurface(surface);
	SDL_DestroyTexture(texture);
}

static void 
ui_font_int_render(int val, int pos_x, int pos_y)
{
	char buff[8];
	sprintf(buff, "%d", val);
	ui_font_render(buff, pos_x, pos_y);
}

static void 
ui_font_str_render(char *text, int pos_x, int pos_y)
{
	ui_font_render(text, pos_x, pos_y);
}

// ------------------------------------------------------------------------------------------------------------------
// MIX
// ------------------------------------------------------------------------------------------------------------------
static int 
mix_init()
{
	if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
	{
		printf("%s\n", SDL_GetError());
	}
	bgm = Mix_LoadMUS("./assets/sounds/battle.mp3");
	if(!bgm)
	{
		printf("%s\n", SDL_GetError());
	}
	else
	{
		if(!Mix_PlayingMusic())
			Mix_PlayMusic(bgm, -1);
	}

	sound_select = Mix_LoadWAV("./assets/sounds/select.wav");
	sound_miss = Mix_LoadWAV("./assets/sounds/miss.wav");
	sound_move = Mix_LoadWAV("./assets/sounds/move.wav");

	return 1;
}

// ------------------------------------------------------------------------------------------------------------------
// PLAYERS
// ------------------------------------------------------------------------------------------------------------------
static void 
player_init(player_t *player, char *name, int hp, int att, int party, SDL_Texture *texture, int pos_x, int pos_y)
{
	player->name = name;
	player->hp = hp;
	player->att = att;
	player->dead = 0;
	player->party = party;
	player->texture = texture;
	player->pos.x = pos_x;
	player->pos.y = pos_y;

	set_rect(&player->dst_rect, player->pos.x, player->pos.y);
	player->src_rect.x = 0;
	player->src_rect.y = 0;
	player->src_rect.w = size;
	player->src_rect.h = size;

	player->animation_timer = 0;
	player->animation_frame = 0;

	players[players_num] = player;
	players_num++;
}

static void 
player_animate(player_t *player)
{
	if(!player->dead)
	{
		if((SDL_GetTicks() - player->animation_timer) > 400)
		{
			player->animation_timer = SDL_GetTicks();

			player->animation_frame++;
			player->animation_frame %= 2;
			player->src_rect.x = player->animation_frame * size;
		}
	}
}

static void 
player_change()
{
	if(players[0])
	{
		do
		{
			players_index++;
			players_index %= players_num;
		} while(players[players_index]->dead != 0);

		player_current = players[players_index];
		cursor.pos.x = player_current->pos.x;
		cursor.pos.y = player_current->pos.y;
	}
}

static int 
player_move_by(player_t *player, int x, int y)
{
	int relative_pos_x = abs(player_current->pos.x - x);
	int relative_pos_y = abs(player_current->pos.y - y);
	if(relative_pos_x + relative_pos_y > 2) 
		return 0;

	int i = 0;
	while(players[i])
	{
		if(players[i]->pos.x == x && players[i]->pos.y == y)
			return 0;
		i++;
	}

	player->pos.x = x;
	player->pos.y = y;
	player->dst_rect.x = x * size * scale + offset_x;
	player->dst_rect.y = y * size * scale + offset_y;

	return 1;
}

static void 
player_die(player_t *player)
{
	player->dead = 1;
	player->src_rect.x = 2 * size;
	check_game_over();
}

static void 
player_hit(player_t *player)
{
	player->hp -= player_current->att;
	if(player->hp <= 0)
		player_die(player);
}

static int 
player_attack(player_t *player)
{
	player_t *target;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		target = players[i];
		if(target == 0) break;

		if(target->pos.x == cursor.pos.x && target->pos.y == cursor.pos.y)
		{
			if(target->dead == 0)
			{
				player_hit(target);
				return 1;
			}
		}
	}
	
	return 0;
}

static void 
players_render(SDL_Renderer *renderer)
{
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(!players[i]) break;
		SDL_RenderCopy(renderer, players[i]->texture, &players[i]->src_rect, &players[i]->dst_rect);
	}
}

// ------------------------------------------------------------------------------------------------------------------
// MAP
// ------------------------------------------------------------------------------------------------------------------
static void 
map_render(SDL_Renderer *renderer, SDL_Texture *tileset)
{
	for(int y = 0; y < 8; y++)
	{
		for(int x = 0; x < 8; x++)
		{
			SDL_Rect dst_rect = {x * size * scale + offset_x, y * size * scale + offset_y, size * scale, size * scale};
			if((x + y) % 2)
			{
				SDL_Rect src_rect = {0 * size, 0 * size, size, size};
				SDL_RenderCopy(renderer, tileset, &src_rect, &dst_rect);
			}
			else
			{
				SDL_Rect src_rect = {1 * size, 0 * size, size, size};
				SDL_RenderCopy(renderer, tileset, &src_rect, &dst_rect);
			}
		}
	}
}


void render_target_stats()
{
	int i = 0;
	while(players[i])
	{
		if(cursor.pos.x == players[i]->pos.x && cursor.pos.y == players[i]->pos.y)
		{
			ui_font_str_render("NAME: ", 532, 32);
			ui_font_str_render(players[i]->name, 644, 32);

			ui_font_str_render("HP: ", 532, 64);
			ui_font_int_render(players[i]->hp, 596, 64);

			ui_font_str_render("ATT: ", 532, 96);
			ui_font_int_render(players[i]->att, 612, 96);

			break;
		}
		i++;
	}
}

int main()
{

	if(!sdl_init()) return 0;
	if(!font_init()) return 0;
	if(!mix_init()) return 0;

	game_memory_t game_memory = {0};
	game_state_t *game_state;

	game_memory.permanent_storage_size = (64 * 1024 * 1024);
	game_memory.transient_storage_size = (512 * 1024 * 1024);

	game_memory.permanent_storage = (game_state_t*)malloc((size_t)game_memory.permanent_storage);
	game_memory.transient_storage = (char*)game_memory.permanent_storage + game_memory.permanent_storage_size;

	if(game_memory.permanent_storage && game_memory.transient_storage)
		is_running = 1;
	else
		is_running = 0;

	game_state = (game_state_t*)game_memory.permanent_storage;
	if(!game_memory.is_initialized)
	{
		game_state->test_val = 0;

		game_memory.is_initialized = 1;
	}

	menu_cursor_index = 0;
	size = 8;
	scale = 8;
	offset_x = 2 * size * scale;
	offset_y = 1 * size * scale;
	players_num = 0;
	players_index = 0;

	player_init(&paladin, "CECIL", 48, 12, 0, texture_load(renderer, "./assets/paladin.png"), 2, 3);
	player_init(&black_mage, "RYDIA", 32, 6, 0, texture_load(renderer, "./assets/black_mage.png"), 1, 2);
	player_init(&white_mage, "ROSA", 36, 4, 0, texture_load(renderer, "./assets/white_mage.png"), 1, 4);
	player_init(&slime_1, "SLIME 1", 18, 8, 1, texture_load(renderer, "./assets/slime.png"), 5, 1);
	player_init(&slime_2, "SLIME 2", 18, 8, 1, texture_load(renderer, "./assets/slime.png"), 6, 3);

	cursor.texture = texture_load(renderer, "./assets/cursor.png");

	move_area = texture_load(renderer, "./assets/move_area.png");
	set_rect(&move_area_dst_rect, 0, 0);

	attack_area = texture_load(renderer, "./assets/attack_area.png");
	set_rect(&attack_area_dst_rect, 0, 0);

	SDL_Texture *tileset = texture_load(renderer, "./assets/tileset.png");

	set_rect(&cursor.dst_rect, cursor.pos.x, cursor.pos.y);

	SDL_Texture *ui_move_en = texture_load(renderer, "./assets/ui_move_en.png");
	SDL_Texture *ui_move_dis = texture_load(renderer, "./assets/ui_move_dis.png");
	SDL_Rect ui_move_dst_rect = {32, 320, 32 * scale, 8 * scale};

	SDL_Texture *ui_att_en = texture_load(renderer, "./assets/ui_att_en.png");
	SDL_Texture *ui_att_dis = texture_load(renderer, "./assets/ui_att_dis.png");
	SDL_Rect ui_att_dst_rect = {32, 400, 32 * scale, 8 * scale};

	SDL_Texture *ui_pass_en = texture_load(renderer, "./assets/ui_pass_en.png");
	SDL_Texture *ui_pass_dis = texture_load(renderer, "./assets/ui_pass_dis.png");
	SDL_Rect ui_pass_dst_rect = {32, 480, 32 * scale, 8 * scale};


	player_current = players[players_index];
	cursor.pos.x = player_current->pos.x;
	cursor.pos.y = player_current->pos.y;

	turn_start_pos_x = player_current->pos.x;
	turn_start_pos_y = player_current->pos.y;

	state = menu;

	while(is_running)
	{
		input_handler();

		SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
		SDL_RenderClear(renderer);

		map_render(renderer, tileset);
		if(state == move)
		{
			move_area_render(renderer);
			set_rect(&cursor.dst_rect, cursor.pos.x, cursor.pos.y);
			SDL_RenderCopy(renderer, cursor.texture, NULL, &cursor.dst_rect);
		}
		if(state == attack)
		{
			attack_area_render(renderer);
			set_rect(&cursor.dst_rect, cursor.pos.x, cursor.pos.y);
			SDL_RenderCopy(renderer, cursor.texture, NULL, &cursor.dst_rect);
		}

		player_animate(&paladin);
		player_animate(&black_mage);
		player_animate(&white_mage);
		player_animate(&slime_1);
		player_animate(&slime_2);
		players_render(renderer);

		if(state == menu)
		{
			SDL_Texture *ui_move_tmp = ui_move_dis;
			SDL_Texture *ui_att_tmp = ui_att_dis;
			SDL_Texture *ui_pass_tmp = ui_pass_dis;

			if(menu_cursor_index == 0) ui_move_tmp = ui_move_en;
			else if(menu_cursor_index == 1) ui_att_tmp = ui_att_en;
			else if(menu_cursor_index == 2) ui_pass_tmp = ui_pass_en;

			if(turn_actions[0] == 0) SDL_RenderCopy(renderer, ui_move_tmp, NULL, &ui_move_dst_rect);
			if(turn_actions[1] == 0) SDL_RenderCopy(renderer, ui_att_tmp, NULL, &ui_att_dst_rect);
			SDL_RenderCopy(renderer, ui_pass_tmp, NULL, &ui_pass_dst_rect);
		}

		ui_font_str_render("NAME: ", 32, 32);
		ui_font_str_render(player_current->name, 144, 32);

		ui_font_str_render("HP: ", 32, 64);
		ui_font_int_render(player_current->hp, 96, 64);

		ui_font_str_render("ATT: ", 32, 96);
		ui_font_int_render(player_current->att, 112, 96);

		render_target_stats();

		SDL_RenderPresent(renderer);
	}

	sdl_quit();

	return 0;
}

void input_handler()
{
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_QUIT:
				is_running = 0;
				break;
			case SDL_KEYDOWN:
				switch(event.key.keysym.sym)
				{
					case SDLK_ESCAPE:
						is_running = 0;
						break;
					case SDLK_UP:
						if(state == menu)
						{
							if(menu_cursor_index > 0)
							{
								menu_cursor_index--;
								Mix_PlayChannel(-1, sound_move, 0);
							}
						}
						else if(state == move)
						{
							cursor_move_by(&cursor, 0, -1);
						}
						else if(state == attack)
						{
							cursor_move_by(&cursor, 0, -1);
						}
						break;
					case SDLK_DOWN:
						if(state == menu)
						{
							if(menu_cursor_index < 2)
							{
								menu_cursor_index++;
								Mix_PlayChannel(-1, sound_move, 0);
							}
						}
						else if(state == move)
						{
							cursor_move_by(&cursor, 0, 1);
						}
						else if(state == attack)
						{
							cursor_move_by(&cursor, 0, 1);
						}
						break;
					case SDLK_LEFT:
						if(state == move)
						{
							cursor_move_by(&cursor, -1, 0);
						}
						else if(state == attack)
						{
							cursor_move_by(&cursor, -1, 0);
						}
						break;
					case SDLK_RIGHT:
						if(state == move)
						{
							cursor_move_by(&cursor, 1, 0);
						}
						else if(state == attack)
						{
							cursor_move_by(&cursor, 1, 0);
						}
						break;
					case SDLK_z:
						if(state == menu)
						{
							if(menu_cursor_index == 0 && turn_actions[0] == 0)
							{
								state = move;
								cursor.pos.x = player_current->pos.x;
								cursor.pos.y = player_current->pos.y;
							}
							if(menu_cursor_index == 1 && turn_actions[1] == 0)
							{
								state = attack;
								cursor.pos.x = player_current->pos.x;
								cursor.pos.y = player_current->pos.y;
							}
							if(menu_cursor_index == 2 && turn_actions[2] == 0)
							{
								player_change();
								turn_start_pos_x = player_current->pos.x;
								turn_start_pos_y = player_current->pos.y;
								menu_cursor_index = 0;
								turn_actions_reset();
							}
							Mix_PlayChannel(-1, sound_select, 0);
						}
						else if(state == move)
						{
							int success = player_move_by(player_current, cursor.pos.x, cursor.pos.y);
							if(success)
							{
								turn_actions[0] = 1;
								state = menu;
								Mix_PlayChannel(-1, sound_select, 0);
							}
						}
						else if(state == attack)
						{
							int success = player_attack(player_current);
							if(success)
							{
								turn_actions[1] = 1;
								state = menu;
								Mix_PlayChannel(-1, sound_select, 0);
							}
						}
						break;
					case SDLK_x:
						if(state == move)
						{
							state = menu;
							Mix_PlayChannel(-1, sound_miss, 0);
						}
						else if(state == attack)
						{
							state = menu;
							Mix_PlayChannel(-1, sound_miss, 0);
						}
						break;
					case SDLK_q:
						if(state == move)
						{
						}
						break;
					default:
						break;
				}
				break;
			default:
				break;
		}
	}
}

