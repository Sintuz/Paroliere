#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>

#define SCREEN_DEFAULT_WIDTH	1280
#define SCREEN_DEFAULT_HEIGHT   720

#define SCREEN_MIN_WIDTH		1280
#define SCREEN_MIN_HEIGHT	   720

#define BORDER_THICKNESS		10

#define MAX_GUESSED_WORDS	   64
#define MAX_WORDS			   130000
#define MAX_WORD_SIZE		   25

#define NUM_AVAILABLE_CHARS	 10

#define TIME_MAX				120

#define PRESSED_KEY event.key.keysym.sym

#define EXPAND_COLOR(color) color.r, color.g, color.b, color.a
#define COLOR_SET(renderer, color) SDL_SetRenderDrawColor(renderer, EXPAND_COLOR(color))


const SDL_Color COLOR_GAME_BACKGROUND = {0, 0, 150, 255};

const SDL_Color COLOR_GAME_LOADING_BORDER = {0, 0, 255, 255};
const SDL_Color COLOR_GAME_CHOOSING_LETTERS_BORDER = {0, 0, 255, 255};
const SDL_Color COLOR_GAME_RUNNING_NONE_BORDER = {0, 0, 255, 255};
const SDL_Color COLOR_GAME_RUNNING_VALID_BORDER = {0, 255, 0, 255};
const SDL_Color COLOR_GAME_RUNNING_INVALID_BORDER = {255, 0, 0, 255};
const SDL_Color COLOR_GAME_ENDED_BORDER = {0, 255, 0, 255};


const SDL_Color COLOR_TEXT_NUMBER = {255, 255, 0, 255};
const SDL_Color COLOR_TEXT_NORMAL = {0, 255, 255, 255};
const SDL_Color COLOR_TEXT_TITLE = {255, 255, 0, 255};
const SDL_Color COLOR_TEXT_CENTERED = {0, 255, 0, 255};

const char *vocals = "AEIOU";
const char *consonants = "BCDFGHLMNPQRSTVZ";

typedef enum {
	FONT_ROBOTO_LIGHT,
	FONT_ROBOTO_REGULAR,
	FONT_ROBOTO_BOLD,
	FONT_SEVEN_SEGMENTS,
	NUM_FONTS
} fonts_t;

typedef enum {
	TEXT_TYPE_MAIN_TITLE,
	TEXT_TYPE_SECONDARY_TITLE,
	TEXT_TYPE_NORMAL,
	TEXT_TYPE_CENTERED,
	TEXT_TYPE_BOTTOM,
	TEXT_TYPE_TIME,
	TEXT_TYPE_RULE
} textTypes_t;

typedef enum {
	TEXT_ALIGNMENT_LEFT,
	TEXT_ALIGNMENT_CENTER,
	TEXT_ALIGNMENT_RIGHT,
	TEXT_ALIGNMENT_CUSTOM
} alignment_t;

typedef enum {
	AUDIO_TIC,
	AUDIO_TAC,
	AUDIO_CORRECT,
	AUDIO_INCORRECT,
	NUM_AUDIO
} audios_t;

typedef enum {
	LAST_WORD_NONE,
	LAST_WORD_VALID,
	LAST_WORD_INVALID,
} lastWord_t;

typedef struct {
	TTF_Font *fonts[NUM_FONTS];
	int fontSizes[NUM_FONTS];

	Mix_Chunk *sounds[NUM_AUDIO];

	char words[MAX_WORDS][MAX_WORD_SIZE];
	size_t numWords;
} assets_t;

typedef enum {
	GAME_STATE_LOADING,
	GAME_STATE_CHOOSING_LETTERS,
	GAME_STATE_RUNNING,
	GAME_STATE_ENDED,
	GAME_STATE_CLOSE
} gameState_t;

typedef struct {
	gameState_t state;

	int points;
	char guessedWords[MAX_GUESSED_WORDS][MAX_WORD_SIZE];
	size_t guessedWordsNum;

	lastWord_t lastWordT;

	char currentWord[MAX_WORD_SIZE];
	size_t currentWordSize;

	char validCharList[NUM_AVAILABLE_CHARS+1];

	time_t timeLeft, lastTime;

	int screen_width, screen_height;

	assets_t assets;
} game_t;

char randomVocal(void);
char randomConsonant(void);

int getFontWidth(TTF_Font *font, const char *text);

void checkWordAndPlay(game_t *game);
void findValidWord(const game_t *game, char *word);

void loadFont(assets_t * const assets, fonts_t font, const char *name, size_t size);
void loadAudio(assets_t * const assets, audios_t audioId, const char *name);
void loadText(assets_t * const assets, const char *name);

void loadAssets(assets_t * const assets);

void freeAudio(Mix_Chunk *sounds[NUM_AUDIO]);

void playSound(Mix_Chunk *sound);

void renderRect(SDL_Renderer *renderer, SDL_Color color, int x, int y, int width, int height);
void renderText(SDL_Renderer *renderer, const game_t *game, SDL_Color color, const char *text, TTF_Font *font, alignment_t alignment, int x, int y);
void renderTextType(SDL_Renderer *renderer, const game_t *game, textTypes_t type, const char *text);
void renderBorder(SDL_Renderer *renderer, const game_t *game, const SDL_Color color);
void renderTimeLeft(SDL_Renderer *renderer, const game_t *game);

void renderLoadingState(SDL_Renderer *renderer, const game_t *game);
void renderChoosingLettersState(SDL_Renderer *renderer, const game_t *game);
void renderRunningState(SDL_Renderer *renderer, const game_t *game);
void renderEndedState(SDL_Renderer *renderer, const game_t *game);
void renderGame(SDL_Renderer *renderer, const game_t *game);

int main(void) {
	srand(time(NULL));

	// initializing general library
	if(SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	// initializing font library
	if(TTF_Init() != 0) {
		fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
		return EXIT_FAILURE;
	}

	// initialize audio library
	if(Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) != 0) {
		fprintf(stderr, "Mix_OpenAudio Error: %s\n", Mix_GetError());
		return EXIT_FAILURE;
	}

	// set max volume
	Mix_Volume(-1, MIX_MAX_VOLUME/10);

	// creating main object
	game_t game = {0};
	game.timeLeft = TIME_MAX;
	game.screen_width = SCREEN_DEFAULT_WIDTH;
	game.screen_height = SCREEN_DEFAULT_HEIGHT;

	// loading assets
	loadAssets(&game.assets);

	// creating window
	SDL_Window *window = SDL_CreateWindow("Paroliere",
										  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
										  game.screen_width, game.screen_height,
										  SDL_WINDOW_SHOWN |
										  SDL_WINDOW_RESIZABLE);
	if (window == NULL) {
		printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	// creating window renderer
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
												SDL_RENDERER_ACCELERATED |
												SDL_RENDERER_PRESENTVSYNC);
	if(renderer == NULL) {
		SDL_DestroyWindow(window);
		printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	// set minimum dimensions for the window
	SDL_SetWindowMinimumSize(window, SCREEN_MIN_WIDTH, SCREEN_MIN_HEIGHT);

	SDL_Event event;
	time_t before, actual;

	// main cicle
	while(game.state != GAME_STATE_CLOSE) {

		if(game.state == GAME_STATE_RUNNING) {

			// decrement the time every second
			actual = time(NULL);
			game.timeLeft -= (actual-before);
			before = actual;
			// if the time has ended, end the game
			if(game.timeLeft < 1) {
				game.state = GAME_STATE_ENDED;
			}

			// play the sound, once the "tic" and once the "tac"
			if(game.lastTime > game.timeLeft) {
				if(game.lastTime % 2 == 0) {
					playSound(game.assets.sounds[AUDIO_TIC]);
				} else {
					playSound(game.assets.sounds[AUDIO_TAC]);
				}
			}

			game.lastTime = game.timeLeft;

		}

		// repeat for every event
		while(SDL_PollEvent(&event)) {

			switch(event.type) {

			case SDL_QUIT: {
				// set state when window is closed
				game.state = GAME_STATE_CLOSE;
			} break;

			case SDL_WINDOWEVENT: {

				switch(event.window.event) {

				case SDL_WINDOWEVENT_RESIZED: {
					// update game window size in case of resize
					SDL_GetWindowSize(window, &game.screen_width, &game.screen_height);
				} break;

				default: {
				} break;

				}

			} break;

			case SDL_KEYDOWN: {

				switch(game.state) {

				case GAME_STATE_LOADING: {
					switch(PRESSED_KEY) {

					// if user has pressed enter the game goes to next phase
					case SDLK_RETURN: {
						game.state = GAME_STATE_CHOOSING_LETTERS;
					} break;

					// if the user has pressed q the game exits
					case SDLK_q: {
						game.state = GAME_STATE_CLOSE;
					} break;

					default: {
					} break;

					}
				} break;

				case GAME_STATE_CHOOSING_LETTERS: {

					switch(PRESSED_KEY) {

					// 1 and 2 to choose types of letters
					case SDLK_1:
					case SDLK_2: {
						char ch;

						// generate random char based on the pressed character
						if(PRESSED_KEY == SDLK_1) {
							ch = randomVocal();
						} else {
							ch = randomConsonant();
						}
						// add the generated character to the list
						game.validCharList[strlen(game.validCharList)] = ch;

						// if the user has selected the type for every letter
						if(strlen(game.validCharList) == NUM_AVAILABLE_CHARS) {
							// initialize timer
							before = time(NULL);
							// pass to next fase
							game.state = GAME_STATE_RUNNING;
						}

					} break;

					default: {
					} break;

					}
				} break;

				case GAME_STATE_RUNNING: {

					switch(PRESSED_KEY) {

					case SDLK_RETURN: {
						checkWordAndPlay(&game);
					} break;

					// delete last inserted char if there is any
					case SDLK_BACKSPACE: {
						if(game.currentWordSize > 0) {
							game.currentWord[--game.currentWordSize] = '\0';
						}
					} break;

					default: {
						int ch = PRESSED_KEY;
						// check if ch is a letter
						if(ch < 127 && isalpha(ch)) {
							// verify that the inserted word hasn't reached the max length
							if(game.currentWordSize < MAX_WORD_SIZE) {
								// check that the charachter is in the available list
								if(strrchr(game.validCharList, toupper(ch)) != NULL) {
									// add character to end of word
									game.currentWord[game.currentWordSize++] = toupper(ch);
								} else {
									playSound(game.assets.sounds[AUDIO_INCORRECT]);
								}
							} else {
								playSound(game.assets.sounds[AUDIO_INCORRECT]);
							}
						}
					} break;

					}

				} break;

				// close the game if the player presses enter in the final window
				case GAME_STATE_ENDED: {
					if(PRESSED_KEY == SDLK_RETURN) {
						game.state = GAME_STATE_CLOSE;
					}
				} break;

				case GAME_STATE_CLOSE: {
				} break;

				default: {
				} break;

				}
			} break;

			default: {
			} break;

			}
		}

		// clear renderer queue
		SDL_RenderClear(renderer);

		// render all game parts in base of the game state
		renderGame(renderer, &game);

		// call to render the frame
		SDL_RenderPresent(renderer);

	}


	// free window resources
	SDL_DestroyWindow(window);

	// free renderer resources
	SDL_DestroyRenderer(renderer);

	// free audio resources
	freeAudio(game.assets.sounds);

	// closing and cleaning TTF, Mixer and SDL libraries 
	TTF_Quit();
	Mix_CloseAudio();
	SDL_Quit();

	return EXIT_SUCCESS;
}


char randomVocal(void)
{
	return vocals[rand()%5];
}

char randomConsonant(void)
{
	return consonants[rand()%strlen(consonants)];
}

int getFontWidth(TTF_Font *font, const char *text)
{
	int width;
	TTF_SizeText(font, text, &width, NULL);
	return width;
}

void checkWordAndPlay(game_t *game)
{
	// check the word is at least 2 characters long
	if(strlen(game->currentWord) > 1) {
		char word[MAX_WORD_SIZE];
		// copy the word in a local backup
		strcpy(word, game->currentWord);

		// reset the word
		memset(game->currentWord, 0, MAX_WORD_SIZE);
		game->currentWordSize = 0;

		// check that the word is a valid word
		bool valid = false;
		for(size_t i = 0; i < game->assets.numWords; i++) {
			if(strcmp(word, game->assets.words[i]) == 0) {
				valid = true;
				break;
			}
		}

		if(valid) {
			// if it is valid check that it hasn't been already used
			bool used = false;
			for(size_t i = 0; i < game->guessedWordsNum; i++) {
				if(strcmp(word, game->guessedWords[i]) == 0) {
					used = true;
					break;
				}
			}

			// if it isn't used
			if(!used) {
				// play the correct sound
				playSound(game->assets.sounds[AUDIO_CORRECT]);

				// add the word to the used words list
				strcpy(game->guessedWords[game->guessedWordsNum], word);
				game->guessedWordsNum++;

				// calculate the points of the word based on the length
				switch(strlen(word)) {

				case 2:
				case 3:
				case 4: {
					game->points += 1;
				} break;

				case 5: {
					game->points += 2;
				} break;

				case 6: {
					game->points += 3;
				} break;

				case 7: {
					game->points += 5;
				} break;

				default: {
					game->points += 11;
				} break;

				}

				// set last word state
				game->lastWordT = LAST_WORD_VALID;
			} else {
				playSound(game->assets.sounds[AUDIO_INCORRECT]);
				game->lastWordT = LAST_WORD_INVALID;
			}
		} else {
			playSound(game->assets.sounds[AUDIO_INCORRECT]);
			game->lastWordT = LAST_WORD_INVALID;
		}
	}
}

void findValidWord(const game_t *game, char *word)
{
	size_t lenM = 0;

	// scan all the valid words
	for(size_t i = 0; i < game->assets.numWords; i++) {
		bool valid = true;

		// if the length of the current word is longer
		size_t len = strlen(game->assets.words[i]);
		if(len > lenM) {
			// check that all the characters of the word are valid
			for(size_t j = 0; j < len; j++) {
				bool charFound = false;
				for(size_t k = 0; k < NUM_AVAILABLE_CHARS; k++) {
					if(game->assets.words[i][j] == game->validCharList[k]) {
						charFound = true;
						break;
					}
				}

				if(!charFound) {
					valid = false;
					break;
				}
			}

			// if the word is still valid, set it as the longest word
			if(valid) {
				lenM = len;
				strcpy(word, game->assets.words[i]);
			}
		}
	}
}

void loadFont(assets_t * const assets,
			  fonts_t fontId,
			  const char *name,
			  size_t size)
{
	// creating path
	char path[100] = "./assets/fonts/";
	strcat(path, name);
	fprintf(stdout, "Loading font: %s ...\n",path);
	// loading font
	TTF_Font *font = TTF_OpenFont(path, size);
	if(font == NULL) {
		fprintf(stderr, "Error: %s", TTF_GetError());
		exit(EXIT_FAILURE);
	}
	fprintf(stdout, "Font loaded.\n");
	assets->fonts[fontId] = font;
	assets->fontSizes[fontId] = size;
}

void loadAudio(assets_t * const assets,
			  audios_t audioId,
			  const char *name)
{
	// creating path
	char path[100] = "./assets/sounds/";
	strcat(path, name);
	fprintf(stdout, "Loading sound: %s ...\n",path);
	// loading audio
	Mix_Chunk *sound = Mix_LoadWAV(path);
	if(sound == NULL)
	{
		fprintf(stderr, "Error: %s", Mix_GetError());
		exit(EXIT_FAILURE);
	}
	assets->sounds[audioId] = sound;
	fprintf(stdout, "Sound loaded.\n");
}

void loadText(assets_t * const assets, const char *name)
{
	size_t num = assets->numWords;
	// creating path
	char path[100] = "./assets/texts/";
	strcat(path, name);
	fprintf(stdout, "Loading texts: %s ...\n",path);
	// opening file
	FILE *fp = fopen(path, "r");
	if(fp == NULL) {
		fprintf(stderr, "Error: couldn't load file: %s", path);
		exit(EXIT_FAILURE);
	}
	// loading word until the end of file, or until max num of words reached
	char word[MAX_WORD_SIZE];
	while(assets->numWords < MAX_WORDS && fgets(word, MAX_WORD_SIZE-1, fp)) {
		// removing any line terminator
		char *ptr = strchr(word, '\r');
		if(ptr != NULL) {
			*ptr = '\0';
		}
		ptr = strchr(word, '\n');
		if(ptr != NULL) {
			*ptr = '\0';
		}
		strcpy(assets->words[assets->numWords++], word);
	}
	fprintf(stdout, "Loaded %zu words.\n", assets->numWords - num);
}

void loadAssets(assets_t * const assets)
{
	// loading all assets

	loadFont(assets, FONT_ROBOTO_LIGHT, "Roboto-Light.ttf", 35);
	loadFont(assets, FONT_ROBOTO_REGULAR, "Roboto-Regular.ttf", 50);
	loadFont(assets, FONT_ROBOTO_BOLD, "Roboto-Bold.ttf", 70);
	loadFont(assets, FONT_SEVEN_SEGMENTS, "Seven-Segment.ttf", 100);

	loadAudio(assets, AUDIO_TIC, "tic.wav");
	loadAudio(assets, AUDIO_TAC, "tac.wav");
	loadAudio(assets, AUDIO_CORRECT, "correct.wav");
	loadAudio(assets, AUDIO_INCORRECT, "incorrect.wav");

	loadText(assets, "words.txt");
	loadText(assets, "words2.txt");
}

void freeAudio(Mix_Chunk *sounds[NUM_AUDIO]) {
	for(size_t i = 0; i < NUM_AUDIO; i++) {
		Mix_FreeChunk(sounds[i]);
	}
}

void playSound(Mix_Chunk *sound) {
	// play sound in the default channel
	Mix_PlayChannel(-1, sound, 0);
}

void renderRect(SDL_Renderer *renderer,
				SDL_Color color,
				int x, int y,
				int width, int height)
{
	// set color
	COLOR_SET(renderer, color);
	// declare rectangle
	SDL_Rect b = {x, y, width, height};
	// render rectangle
	SDL_RenderFillRect(renderer, &b);
}


void renderText(SDL_Renderer *renderer,
				const game_t *game, 
				SDL_Color color,
				const char *text,
				TTF_Font *font,
				alignment_t alignment,
				int x, int y)
{
	// create text surface
	SDL_Surface* messageSurface = TTF_RenderText_Solid(font, text, color);

	// create texture from surface
	SDL_Texture* messageTexture = SDL_CreateTextureFromSurface(renderer, messageSurface);

	int textWidth;
	TTF_SizeText(font, text, &textWidth, NULL);

	// calculating x based on the alignment
	switch(alignment) {

	case TEXT_ALIGNMENT_LEFT: {
		x = 4*BORDER_THICKNESS;
	} break;

	case TEXT_ALIGNMENT_CENTER: {
		x = (game->screen_width-textWidth)/2;
	} break;

	case TEXT_ALIGNMENT_RIGHT: {
		x = game->screen_width-textWidth-4*BORDER_THICKNESS;
	} break;

	// x remains the same for custom alignment
	case TEXT_ALIGNMENT_CUSTOM: {
	} break;

	default: {
	} break;

	}

	// declaring text position and size from the surface
	SDL_Rect messagePosSize = {x, y,
							   messageSurface->w,
							   messageSurface->h};

	// render the texture with the 
	SDL_RenderCopy(renderer, messageTexture, NULL, &messagePosSize);

	// free texture and surface
	SDL_FreeSurface(messageSurface);
	SDL_DestroyTexture(messageTexture);
}

void renderBorder(SDL_Renderer *renderer,
				  const game_t *game,
				  const SDL_Color color)
{
	// fill everithing with the color of the border
	renderRect(renderer, color,
			   0, 0,
			   game->screen_width,
			   game->screen_height);

	// fill the center with the color of the background
	renderRect(renderer, COLOR_GAME_BACKGROUND,
			   BORDER_THICKNESS, BORDER_THICKNESS,
			   game->screen_width-2*BORDER_THICKNESS,
			   game->screen_height-2*BORDER_THICKNESS);
}

void renderTextType(SDL_Renderer *renderer,
					const game_t *game,
					textTypes_t type,
					const char *text)
{
	static int num, totHeight;

	// render text based on the type, incrementing the heigth reached by the other texts
	switch(type) {

	case TEXT_TYPE_MAIN_TITLE: {
		num = 1;
		renderText(renderer,
				   game,
				   COLOR_TEXT_TITLE,
				   text,
				   game->assets.fonts[FONT_ROBOTO_BOLD],
				   TEXT_ALIGNMENT_CENTER,
				   0, 3*BORDER_THICKNESS);
		totHeight = game->assets.fontSizes[FONT_ROBOTO_BOLD] + 6*BORDER_THICKNESS;
	} break;

	case TEXT_TYPE_SECONDARY_TITLE: {
		renderText(renderer,
				   game,
				   COLOR_TEXT_TITLE,
				   text,
				   game->assets.fonts[FONT_ROBOTO_REGULAR],
				   TEXT_ALIGNMENT_CENTER,
				   0, totHeight + 3*BORDER_THICKNESS);
		totHeight += game->assets.fontSizes[FONT_ROBOTO_REGULAR] + 3*BORDER_THICKNESS;
	} break;

	case TEXT_TYPE_NORMAL: {
		renderText(renderer,
				   game,
				   COLOR_TEXT_NORMAL,
				   text,
				   game->assets.fonts[FONT_ROBOTO_REGULAR],
				   TEXT_ALIGNMENT_LEFT,
				   0, totHeight + 3*BORDER_THICKNESS);
		totHeight += game->assets.fontSizes[FONT_ROBOTO_REGULAR] + 3*BORDER_THICKNESS;
	} break;

	case TEXT_TYPE_CENTERED: {
		renderText(renderer,
				   game,
				   COLOR_TEXT_CENTERED,
				   text,
				   game->assets.fonts[FONT_ROBOTO_BOLD],
				   TEXT_ALIGNMENT_CENTER,
				   0, (game->screen_height-game->assets.fontSizes[FONT_ROBOTO_BOLD])/2);
	} break;

	case TEXT_TYPE_BOTTOM: {
		renderText(renderer,
				   game,
				   COLOR_TEXT_NUMBER,
				   text,
				   game->assets.fonts[FONT_ROBOTO_REGULAR],
				   TEXT_ALIGNMENT_CENTER,
				   0, game->screen_height-game->assets.fontSizes[FONT_ROBOTO_BOLD]-3*BORDER_THICKNESS);
	} break;

	case TEXT_TYPE_TIME: {
		SDL_Color color = {(TIME_MAX - game->timeLeft) * 2, game->timeLeft*2, 0, 255};
		renderText(renderer,
				   game,
				   color,
				   text,
				   game->assets.fonts[FONT_SEVEN_SEGMENTS],
				   TEXT_ALIGNMENT_CENTER,
				   0, totHeight + 3*BORDER_THICKNESS);
		totHeight += game->assets.fontSizes[FONT_SEVEN_SEGMENTS] + 3*BORDER_THICKNESS;
	} break;

	case TEXT_TYPE_RULE: {
		char numS[10];
		sprintf(numS, "%d)", num);
		renderText(renderer,
				   game,
				   COLOR_TEXT_NUMBER,
				   numS,
				   game->assets.fonts[FONT_ROBOTO_REGULAR],
				   TEXT_ALIGNMENT_LEFT,
				   0, totHeight + 3*BORDER_THICKNESS);

		int width = getFontWidth(game->assets.fonts[FONT_ROBOTO_REGULAR], numS);
		renderText(renderer,
				   game,
				   COLOR_TEXT_NORMAL,
				   text,
				   game->assets.fonts[FONT_ROBOTO_LIGHT],
				   TEXT_ALIGNMENT_CUSTOM,
				   6*BORDER_THICKNESS + width, totHeight + 4*BORDER_THICKNESS);
		totHeight += game->assets.fontSizes[FONT_ROBOTO_LIGHT] + 3*BORDER_THICKNESS;
		num++;
	} break;

	}
}

void renderTimeLeft(SDL_Renderer *renderer, const game_t *game)
{
	// getting minutes and seconds
	int minutes = (int) game->timeLeft / 60;
	int second = (int) game->timeLeft % 60;

	char time[6];
	// pretty printing (MM:SS) string
	sprintf(time, "%02d:%02d", minutes, second);

	// get time text width
	int textWidth;
	TTF_SizeText(game->assets.fonts[FONT_SEVEN_SEGMENTS],
				 time, &textWidth, NULL);
	// render time
	renderTextType(renderer,
			   game,
			   TEXT_TYPE_TIME,
			   time);
}


void renderLoadingState(SDL_Renderer *renderer, const game_t *game)
{
	renderBorder(renderer, game, COLOR_GAME_LOADING_BORDER);

	// render welcome message
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_MAIN_TITLE,
				   "Benvenuto a Paroliere");

	// render rule title
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_NORMAL,
				   "Regole:");

	// rule list
	const char *rules[] = {
		"- Scegli 10 tra vocali e consonanti;",
		"- Devi formare parole con le lettere generate;",
		"- Puoi utilizzare le lettere varie volte;",
		"- Hai 120 secondi.",
	};
	// calculating number of rules
	size_t numRules = sizeof(rules)/sizeof(char*);
	// renderering every rule
	for(size_t i=0;i<numRules;i++) {
		renderTextType(renderer,
					   game,
					   TEXT_TYPE_NORMAL,
					   rules[i]);
	}

	// render instruction for next game state and exit
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_BOTTOM,
				   "Premi [INVIO] per continuare, [Q] per uscire");

}

void renderChoosingLettersState(SDL_Renderer *renderer, const game_t *game)
{
	renderBorder(renderer, game, COLOR_GAME_CHOOSING_LETTERS_BORDER);

	// render title
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_MAIN_TITLE,
				   "Scegli i tipi delle lettere");

	// render instruction
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_NORMAL,
				   "Premi:");

	// render keys for choosing types
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_RULE,
				   "Vocali");
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_RULE,
				   "Consonanti");

	// if the user has choosen at least one char render the generated chars
	if(strlen(game->validCharList) > 0) {
		renderTextType(renderer,
					   game,
					   TEXT_TYPE_NORMAL,
					   "Lettere generate:");

		renderTextType(renderer,
					   game,
					   TEXT_TYPE_SECONDARY_TITLE,
					   game->validCharList);
	}
}

void renderRunningState(SDL_Renderer *renderer, const game_t *game)
{
	// change the border color based on the last word entered
	switch(game->lastWordT) {

	case LAST_WORD_NONE: {
		renderBorder(renderer, game, COLOR_GAME_RUNNING_NONE_BORDER);
	} break;

	case LAST_WORD_VALID: {
		renderBorder(renderer, game, COLOR_GAME_RUNNING_VALID_BORDER);
	} break;

	case LAST_WORD_INVALID: {
		renderBorder(renderer, game, COLOR_GAME_RUNNING_INVALID_BORDER);
	} break;

	}

	// render title
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_MAIN_TITLE,
				   "Crea parole con le lettere date");

	renderTimeLeft(renderer, game);

	// render available letters
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_NORMAL,
				   "Lettere disponibili:");
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_SECONDARY_TITLE,
				   game->validCharList);

	// render points
	char phrase[100];
	sprintf(phrase, "Punti: %d", game->points);
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_NORMAL,
				   phrase);

	// render writing buffer
	char currentWordS[100];
	sprintf(currentWordS, " > %s", game->currentWord);
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_NORMAL,
				   currentWordS);

}

void renderEndedState(SDL_Renderer *renderer, const game_t *game)
{
	renderBorder(renderer, game, COLOR_GAME_ENDED_BORDER);

	// render points
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_MAIN_TITLE,
				   "Gioco terminato");
	char phrase[100];
	sprintf(phrase, "Hai ottenuto %d punti", game->points);
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_CENTERED,
				   phrase);

	// find longest word possible
	char word[MAX_WORD_SIZE]="";
	findValidWord(game, word);
	// if there isn't any, display the fact
	if(strlen(word) == 0) {
		renderTextType(renderer,
					   game,
					   TEXT_TYPE_NORMAL,
					   "Non c'erano parole disponibili");
	} else { // else show the longest word
		renderTextType(renderer,
					   game,
					   TEXT_TYPE_NORMAL,
					   "Parola di massima lunghezza possibile:");
		renderTextType(renderer,
					   game,
					   TEXT_TYPE_SECONDARY_TITLE,
					   word);
	}

	// render instruction to exit
	renderTextType(renderer,
				   game,
				   TEXT_TYPE_BOTTOM,
				   "Premi [INVIO] per uscire");
}

void renderGame(SDL_Renderer *renderer, const game_t *game)
{
	// render the correct state of the game
	switch(game->state) {

	case GAME_STATE_LOADING: {
		renderLoadingState(renderer, game);
	} break;

	case GAME_STATE_CHOOSING_LETTERS: {
		renderChoosingLettersState(renderer, game);
	} break;

	case GAME_STATE_RUNNING: {
		renderRunningState(renderer, game);
	} break;

	case GAME_STATE_ENDED: {
		renderEndedState(renderer, game);
	} break;

	default: {
	} break;

	}
}
