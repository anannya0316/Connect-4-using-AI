#include <limits.h>
#include <stdlib.h>
#include<stdbool.h>
#include <stdio.h>
#include <string.h>

#define OFF_BOARD -2  //off-board position to check boudary condition
#define EMPTY -1  //empty cell on the game board
#define LOOK_AHEAD 5   //depth of the game tree search
#define TABLE_SIZE 32000
#define TABLE_BIN_SIZE 10
//parameters for a transposition table used for optimization. this table is used to keep records of the previous game positions.
//Used for alpha beta pruning to skip sub tree evaluations

typedef struct {
	int width;
	int height;
	int* board;
	int last_move;
	int weight;

	int refs;
} GameState;
// represents the state of the game, including board dimensions, the game board, the last move made,
//a weight for heuristic evaluation, and a reference count for memory management.


//allocates memory to GameState
GameState* newGameState(int width, int height) {
	int i;
	GameState* toR = (GameState*) malloc(sizeof(GameState));      //memory allocation for new GameState

	if (toR == NULL)     //checks if memory allocation is successful
		return NULL;


	toR->width = width;
	toR->height = height;

	toR->weight = 0;      //stores heuristic value
	toR->refs = 1;        // number of references used for managing memory (number of ref to an object) and ensuring that resources are deallocated when not needed,
	toR->last_move = 0;   //keep track of players moves


	toR->board = (int*) malloc(sizeof(int) * width * height); //allocates memory for game board using array with width and height as dimensions
	if (toR->board == NULL) {
		free(toR);                     //free memory if board is null
		return NULL;
	}



	for (i = 0; i < width * height; i++) {       //initial state of board is empty for all elements in array
		toR->board[i] = EMPTY;
	}


	return toR;
}

//decrements the reference count of a game state and frees its memory when the reference count reaches zero. reference count is the lifetime of the GameState structure.
void freeGameState(GameState* gs) {
	gs->refs--;      //decrements ref indicating that one reference to the object has been released or no longer exists.
	if (gs->refs <= 0) {         //if no more references then free the board
		free(gs->board);
		free(gs);
	}
}


//increment ref count to indicate that another part of the program is now referencing the same game state
void retainGameState(GameState* gs) {
	gs->refs++;
}

//returns the position at which value (gs) is placed and checks for off board position
int at(GameState* gs, int x, int y) {
	if (x < 0 || y < 0)               //x y are the coordinates of the board
		return OFF_BOARD;

	if (x >= gs->width || y >= gs->height)
		return OFF_BOARD;

	return gs->board[x * gs->height + y];  //calculates a single index value within a one-dimensional array that corresponds to the two-dimensional coordinates
}


//places a players piece in a column and updates gamestate
void drop(GameState* gs, int column, int player) {
	int i;
	for (i = 0; i < gs->height; i++) {  //iterates through the rows
		if (at(gs, column, i) == EMPTY) {               //checks if r/c is empty in that row
			gs->board[column * gs->height + i] = player;   //calculates index of empty cell
			gs->last_move = column;        //updates last move using column as only that is required
			return;
		}
	}


}

//checks for win condition
int checkAt(GameState* gs, int x, int y) {
	// check across
	bool found = true;
	int curr = at(gs, x, y);
	int i;
	for (i = 0; i < 4; i++) {               //less than 4 is not win
		if (at(gs, x + i, y) != curr) {      //check for horizontal hence x+1, if others players piece is found then not win
			found = false;
			break;
		}
	}

 //makes sure current state of win is not invalid
	if (found && (curr != EMPTY && curr != OFF_BOARD))
		return curr;

	// check down
	found = true;
	for (i = 0; i < 4; i++) {
		if (at(gs, x, y + i) != curr) {        //checks for vertical
			found = false;
			break;
		}
	}

	if (found && (curr != EMPTY && curr != OFF_BOARD))
		return curr;

	// check diag +/+
	found = true;
	for (i = 0; i < 4; i++) {
		if (at(gs, x + i, y + i) != curr) {     //checks for diagonal from top left to bottom right
			found = false;
			break;
		}
	}

	if (found && (curr != EMPTY && curr != OFF_BOARD))
		return curr;

	// check diag -/+
	found = true;
	for (i = 0; i < 4; i++) {
		if (at(gs, x - i, y + i) != curr) {    //check for diagonal from top-right to bottom-left
			found = false;
			break;
		}
	}

	if (found && (curr != EMPTY && curr != OFF_BOARD))
		return curr;

	return 0;
}

//This function calculates an increment value for an array of pieces in a row, which is used for heuristic evaluation.
int getIncrementForArray(int* arr, int player) {        //arr represents a sequence of game pieces.  player is for player 1 or 2
	int toR = 0;
	int i;
	for (i = 0; i < 4; i++) {           // iterate the array assuming at least 4 elements
		if (arr[i] == player) {         //if 4 consecutive pieces of player in array then return 1 else 0 and checks if element is array is of the player
			toR = 1;
			continue;
		}

		if (arr[i] != player && arr[i] != EMPTY) {        //element in array isnt of player hence no win for that player
			return 0;
		}
	}

	return toR;
}

// counts the number of pieces in the board for a specific player at a given position
int countAt(GameState* gs, int x, int y, int player) {

	// check across
	int found = 0;       //keep track of the number of sequences found.
	int buf[4];          //it checks the 3 next positions to x,y and returns values in this array
	int i;

	for (i = 0; i < 4; i++) {
		buf[i] = at(gs, x + i, y);      //checks the next 3 positions hence x+1
	}

	found += getIncrementForArray(buf, player);

	// check down
	for (i = 0; i < 4; i++) {              //checks the next 3 positions hence y+1
		buf[i] = at(gs, x, y - i);
	}
	found += getIncrementForArray(buf, player);

	// check diag +/+
	for (i = 0; i < 4; i++) {
		buf[i] = at(gs, x + i, y + i);           //checks the next 3 positions from top-left to bottom-right hence x+1 and y+1
	}
	found += getIncrementForArray(buf, player);

	// check diag -/+
	for (i = 0; i < 4; i++) {
		buf[i] = at(gs, x - i, y + i);        ////checks the next 3 positions from  top-right to bottom-left hence x+1 and y+1
	}
	found += getIncrementForArray(buf, player);

	return found;
}

// iterates through all positions on the game board to check if there is a winner
bool getWinner(GameState* gs) {
	int x, y;
	bool res;
	for (x = 0; x < gs->width; x++) {
		for (y = 0; y < gs->height; y++) {
			res = (bool) checkAt(gs, x, y);
			if (res)
				return res;
		}
	}

	return 0;
}

//checks if the game has ended in a draw
int isDraw(GameState* gs) {
	int x, y;
	for (x = 0; x < gs->width; x++) {
		for (y = 0; y < gs->height; y++) {
			if (at(gs, x, y) == EMPTY)      //if cell is empty then not a draw
				return 0;
		}
	}

	return 1;    //no more moves left hence draw
}

// calculates a heuristic value for a game state to help evaluate its desirability for the player.
//it calculates the number of sequences (consecutive pieces in a row) for the specified player and subtracts the number of sequences for the opponent player.
//This is done using the countAt function.
int getHeuristic(GameState* gs, int player, int other_player) {
    int player1_wins = 0;
    int player2_wins = 0;
    int x, y;

    // Calculate the number of ways player 1 and player 2 can win
    for (x = 0; x < gs->width; x++) {
        for (y = 0; y < gs->height; y++) {
            int winner = checkAt(gs, x, y);
            if (winner == player) {
                player1_wins++;     // Increment if player 1 can win at this position
            } else if (winner == other_player) {
                player2_wins++;     // Increment if player 2 can win at this position
            }
        }
    }

    // Calculate the heuristic score based on the difference
    int heuristic_score = player1_wins - player2_wins;
    return heuristic_score;
}

//creates a new game state that represents the state of the game after a player makes a move to find the best move
GameState* stateForMove(GameState* orig, int column, int player) {
    GameState* toR; // Declare a pointer to the new GameState

    // Check if the original GameState or its board is invalid
    if (orig == NULL || orig->board == NULL)
        return NULL;

    // Create a new GameState with the same dimensions as the original
    toR = newGameState(orig->width, orig->height);
    if (toR == NULL)
        return NULL;

    // Copy the game board from the original GameState to the new GameState
    // This is done using the memcpy function to duplicate the board state
    memcpy(toR->board, orig->board, sizeof(int) * orig->width * orig->height);

    drop(toR, column, player);    // Drop the player's piece into the specified column

    return toR;
}

//prints the board
void printGameState(GameState* gs) {
    int i, x, y, toP;

    // Print column numbers for reference
    for (i = 0; i < gs->width; i++) {
        printf("%d ", i);
    }
    printf("\n");

    // Iterate through the game board in reverse order (from top to bottom)
    for (y = gs->height - 1; y >= 0; y--) {
        for (x = 0; x < gs->width; x++) {
            toP = at(gs, x, y); // Get the value (player or empty) at the current cell
            if (toP == EMPTY) {
                printf("  "); // Empty cell, print two spaces
            } else if (toP == 1) {
                printf("X ");
            } else if (toP == 2) {
                printf("O ");
            }
        }
        printf("\n"); // Move to the next row after printing a row
    }

    // Print column numbers again for reference
    for (i = 0; i < gs->width; i++) {
        printf("%d ", i);
    }
    printf("\n\n");
}

//calculates a hash value for a game state to be used in hash table lookups.
//used for optimization like avoiding redundant evaluations of the same state.
unsigned long long hashGameState(GameState* gs) {
    unsigned long long hash = 14695981039346656037Lu; // Initialize the hash with a constant value

    // Iterate through the elements of the game board
    int i;
    for (i = 0; i < gs->width * gs->height; i++) {
        // Update the hash by XOR-ing it with the value of the game board at position 'i'
        hash ^= gs->board[i];

        // Multiply the hash by a large prime number to further mix the bits
        hash *= 1099511628211Lu;
    }

    return hash; // Return the computed hash value
}

//This function checks if two game states are equal in terms of board configuration.
//The comparison occurs when the program needs to determine if the current game state is equivalent to a previously stored game state
// This comparison may happen during the search for the best move or when checking if a particular game position has been encountered before.
int isGameStateEqual(GameState* gs1, GameState* gs2) {
    int i;

    // Check if the dimensions (width and height) of the two game states are equal.
    if (gs1->width != gs2->width || gs1->height != gs2->height)
        return 0;

    // Iterate through the elements of the game board and compare them.
    for (i = 0; i < gs1->width * gs1->height; i++) {
        if (gs1->board[i] == gs2->board[i])
            continue; // Continue comparing the next element if they are equal.

        return 0; // Return 0 if any elements are different.
    }

    return 1; // Return 1 if all dimensions and elements are equal.
}

typedef struct {
    GameState*** bins; // Array of bins for storing game states.  it efficiently manages and looks up these states during a search algorithm.
} TranspositionTable;     //store game state information for optimization.

//create table and initialize bins
TranspositionTable* newTable() {
	int i, j;
	TranspositionTable* toR = (TranspositionTable*) malloc(sizeof(TranspositionTable));
	toR->bins = (GameState***) malloc(sizeof(GameState**) * TABLE_SIZE);
	for (i = 0; i < TABLE_SIZE; i++) {
		toR->bins[i] = (GameState**) malloc(sizeof(GameState*) * TABLE_BIN_SIZE);
		for (j = 0; j < TABLE_BIN_SIZE; j++) {
			toR->bins[i][j] = NULL;         // Initialize each bin with NULL
		}
	}

	return toR;
}


//looks up a game state in the transposition table and returns it if found
// Search for a game state 'k' in the transposition table 't'
GameState* lookupInTable(TranspositionTable* t, GameState* k) {

    // Calculate a hash value for the game state 'k'
    int hv = hashGameState(k) % TABLE_SIZE;
    int i;

    // Get the bin (list of game states) at the hash value 'hv'
    GameState** bin = t->bins[hv];

    for (i = 0; i < TABLE_BIN_SIZE; i++) {
        // If the bin entry is NULL, there are no more game states in this bin, so return NULL
        if (bin[i] == NULL) {
            return NULL;
        }

        // Check if the game state 'k' matches the game state in the current bin entry
        if (isGameStateEqual(k, bin[i])) {
            // If there is a match, return a pointer to the matching game state
            return bin[i];
        }
    }

    // If no match is found in the bin, return NULL to indicate that 'k' is not in the table
    return NULL;
}

// Add a game state 'k' to the transposition table 't'
void addToTable(TranspositionTable* t, GameState* k) {
    // Calculate a hash value for the game state 'k'
    int hv = hashGameState(k) % TABLE_SIZE;
    int i;

    // Get the bin (list of game states) at the hash value 'hv'
    GameState** bin = t->bins[hv];

    for (i = 0; i < TABLE_BIN_SIZE; i++) {
        // If the bin entry is NULL, it means there's space to store 'k' in this bin
        if (bin[i] == NULL) {
            // Store 'k' in the bin
            bin[i] = k;
            // Increment the reference count of 'k' to indicate it's referenced in the table
            retainGameState(k);
            return; // Exit the function after storing 'k'
        }
    }

    // If all entries in the bin are occupied, print an error message indicating overflow
    fprintf(stderr, "Overflow in hash bin %d, won't store GameState\n", hv);
}

//releases memory for the transposition table and its bins.
void freeTranspositionTable(TranspositionTable* t) {
    int i, j;

    for (i = 0; i < TABLE_SIZE; i++) {
        // Iterate through each entry (game state) in the bin
        for (j = 0; j < TABLE_BIN_SIZE; j++) {
            if (t->bins[i][j] != NULL)
                // Free the memory associated with the stored game state
                freeGameState(t->bins[i][j]);
        }
        // Free the memory associated with the bin itself
        free(t->bins[i]);
    }
    // Free the memory associated with the array of bins
    free(t->bins);
    // Free the memory associated with the transposition table itself
    free(t);
}


typedef struct {
	GameState* gs;
	int player;
	int other_player;
	int turn;               //current turn or depth
	int alpha;              //alpha beta pruning
	int beta;
	int best_move;      // Best move found at this node

	TranspositionTable* ht;
} GameTreeNode;       //representing nodes in the game tree during AI search.

GameTreeNode* newGameTreeNode(GameState* gs, int player, int other, int turn, int alpha, int beta, TranspositionTable* ht) {
	GameTreeNode* toR = (GameTreeNode*) malloc(sizeof(GameTreeNode));
	toR->gs = gs;
	toR->player = player;
	toR->other_player = other;
	toR->turn = turn;
	toR->alpha = alpha;
	toR->beta = beta;
	toR->best_move = -1;     // Initialize the best move to an invalid value
	toR->ht = ht;
	return toR;
}


//calculates a heuristic value for a game state, taking into account the player's and opponent's positions.
int heuristicForState(GameState* gs, int player, int other) {
	if (isDraw(gs))
		return 0;

	int term_stat = getWinner(gs);
	if (term_stat == player)
		return 1000;     //If the user wins, return a high positive score (1000)

	if (term_stat)
		return -1000;    // If the AI wins, return a high negative score (-1000)

	// If the game is still ongoing, return a heuristic score based on the board evaluation
	return getHeuristic(gs, player, other);

}

//This declares a global variable g_node, which is used for sorting game states in certain order.
GameTreeNode* g_node = NULL;

// comparison function used for sorting game states in ascending order based on their heuristic values.
int ascComp(const void* a, const void* b) {
	GameTreeNode* node = g_node;      // used to access the player and other_player fields for heuristic calculations.

	return heuristicForState(*(GameState**) a, node->player, node->other_player) - heuristicForState(*(GameState**) b, node->player, node->other_player);
	//returns +ve if AI value is lesser and -ve if greater

}


//same but in descending
int desComp(const void* a, const void* b) {
	GameTreeNode* node = g_node;
	return heuristicForState(*(GameState**) b, node->player, node->other_player) -
		heuristicForState(*(GameState**) a, node->player, node->other_player);

}
//both asc and desc so that if you want to take heuristic from lowest or higest score.
//This can be useful if you are trying to maximize your chances of winning or searching for the most favorable game states.


// performs a depth-limited search of the game tree to find the best move for the AI player while considering alpha-beta pruning to minimize
//the number of nodes that need to be explored.

int getWeight(GameTreeNode* node, int movesLeft) {
    int toR, move, best_weight;

    // Base case: If the game is over, return the heuristic value.
    if (getWinner(node->gs) || isDraw(node->gs) || movesLeft == 0)
        return heuristicForState(node->gs, node->player, node->other_player);

    // Create an array to store possible future game states.
    GameState** possibleMoves = (GameState**) malloc(sizeof(GameState*) * node->gs->width);
    int validMoves = 0;

    // Generate possible future game states based on available moves.
    for (int possibleMove = 0; possibleMove < node->gs->width; possibleMove++) {
        if (!canMove(node->gs, possibleMove)) {
            continue;
        }
        possibleMoves[validMoves] = stateForMove(node->gs, possibleMove, (node->turn ? node->player : node->other_player));
        validMoves++;
    }

    // Order possibleMoves by the heuristic.
    g_node = node;
    if (node->turn) {
        qsort(possibleMoves, validMoves, sizeof(GameState*), ascComp);
    } else {
        qsort(possibleMoves, validMoves, sizeof(GameState*), desComp);
    }

    // Initialize the best_weight based on whether it's a max or min node.
    best_weight = (node->turn ? INT_MIN : INT_MAX);

    // Loop through possible future game states.
    for (move = 0; move < validMoves; move++) {
        // Check if the game state is already in the hash table.
        GameState* inTable = lookupInTable(node->ht, possibleMoves[move]);
        int child_weight;
        int child_last_move;

        if (inTable != NULL) {
            // If the game state is in the hash table, use the stored weight.
            child_weight = inTable->weight;
            child_last_move = possibleMoves[move]->last_move;
        } else {
            // If not in the hash table, recursively calculate the weight.
            GameTreeNode* child = newGameTreeNode(possibleMoves[move], node->player, node->other_player, !(node->turn),
                                                  node->alpha, node->beta, node->ht);
            child_weight = getWeight(child, movesLeft - 1);
            child_last_move = child->gs->last_move;
            free(child);
        }

        // Store the child's weight in the game state and hash table.
        possibleMoves[move]->weight = child_weight;
        addToTable(node->ht, possibleMoves[move]);

        if (movesLeft == LOOK_AHEAD)
            printf("Move %d has weight %d\n", child_last_move, child_weight);

        // Alpha-beta pruning for maximizing and minimizing nodes.
        if (!node->turn) {
            if (child_weight <= node->alpha) {
                toR = child_weight;
                goto done;
            }
            node->beta = (node->beta < child_weight ? node->beta : child_weight);
        } else {
            if (child_weight >= node->beta) {
                toR = child_weight;
                goto done;
            }
            node->alpha = (node->alpha > child_weight ? node->alpha : child_weight);
        }

        // Update the best_weight based on whether it's a max or min node.
        if (!(node->turn)) {
            if (best_weight > child_weight) {
                best_weight = child_weight;
                node->best_move = child_last_move;
            }
        } else {
            if (best_weight < child_weight) {
                best_weight = child_weight;
                node->best_move = child_last_move;
            }
        }
    }
    toR = best_weight;

done:
    // Free allocated memory.
    for (int i = 0; i < validMoves; i++) {
        freeGameState(possibleMoves[i]);
    }
    free(possibleMoves);

    return toR;
}


int getBestMove(GameTreeNode* node, int movesLeft) {
	getWeight(node, movesLeft);     //how many moves ahead the AI should look
	return node->best_move;
}

// This function checks if the game has ended due to a win or a draw.
void checkWin(GameState* gs) {
    // Check if there is a winner in the current game state.
    int win = getWinner(gs);

    // If there is a winner, display the winning player and exit.
    if (win) {
        printf("Game over! Player %d wins!\n", win);
        printGameState(gs);
        exit(0);
    }

    // If there is no winner, check if the game is a draw.
    if (isDraw(gs)) {
        printf("Game over! Draw!\n");
        printGameState(gs);
        exit(0);
    }
}

// Given a game state, this function determines the best move for a player using the minimax algorithm with alpha-beta pruning.
int bestMoveForState(GameState* gs, int player, int other_player, int look_ahead) {

    // Create a new transposition table for caching game states.
    TranspositionTable* t1 = newTable();

    // Create a new game tree node with initial values.
    GameTreeNode* n = newGameTreeNode(gs, player, other_player, 1, INT_MIN, INT_MAX, t1);

    // Get the best move using the minimax algorithm with alpha-beta pruning.
    int move = getBestMove(n, look_ahead);

    // Free memory allocated for the game tree node and transposition table.
    free(n);
    freeTranspositionTable(t1);

    // Return the best move found.
    return move;
}

int canMove(GameState* gs, int column) {
    int y;
    for (y = 0; y < gs->height; y++) {
        if (at(gs, column, y) == EMPTY)
            return 1;
    }

    return 0;
}

// a couple of ease-of-use functions that will run a game in global state
GameState* globalState;

void startNewGame() {
	globalState = newGameState(7, 6);
}

void playerMove(int move) {
	drop(globalState, move, 1);
}

void computerMove(int look_ahead) {
	int move = bestMoveForState(globalState, 2, 1, look_ahead);
	drop(globalState, move, 2);
}

int isGameWon() {
	return getWinner(globalState);
}

int isGameDraw() {
	return isDraw(globalState);
}

int isEmpty(int x, int y) {
	return at(globalState, x, y) == EMPTY;
}


int main(int argc, char** argv) {
	startNewGame();

	while (1) {
		// Print the empty board before asking for user input
		printGameState(globalState);

		int move;
		printf("You can start from column 0 to 6. Choose which column you want to start with: ");
		scanf("%d", &move);

		if (move < 0 || move >= globalState->width || !canMove(globalState, move)) {
			printf("Invalid move. Please choose a valid column.\n");
			continue;
		}

		playerMove(move);

		printGameState(globalState);

		checkWin(globalState);

		computerMove(LOOK_AHEAD);

		printGameState(globalState);

		checkWin(globalState);
	}

	freeGameState(globalState);

	return 0;
}
