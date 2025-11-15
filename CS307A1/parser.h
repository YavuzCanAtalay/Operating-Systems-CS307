// parser.h

#ifndef sparser_h
#define sparser_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "mpc.h"

// ===================================================================================
// PUBLIC DATA STRUCTURES FOR COMMAND PIPELINES
// ===================================================================================

// CmdVec: Holds an array of argv-style arrays for a command pipeline.
// e.g., { {"ls", "-l", NULL}, {"wc", "-l", NULL} }
typedef struct {
    char ***argvs; // Array of argument vectors
    size_t n;      // Number of commands in the pipeline
} CmdVec;

// compiledCmd: The primary structure containing parsed commands and metadata.
// This is the main structure students interact with.
typedef struct {
    CmdVec before;    // Commands before a loop
    CmdVec inLoop;    // Commands within a loop
    CmdVec after;     // Commands after a loop

    size_t loopLen;   // Number of loop iterations
    char *inFile;     // Input redirection filename (or NULL)
    char *outFile;    // Output redirection filename (or NULL)
    int isQuit;       // 1 if ':q' was entered, 0 otherwise
} compiledCmd;


typedef struct {
    mpc_parser_t *Fname, *Redirin, *Redirout, *Redir, *Cmd, *Pipe, *Loop,
                 *Composite, *RedirCmd, *Quit, *Line;
} sparser_t;

// ===================================================================================
// PUBLIC FUNCTIONS FOR STUDENT USEGE
// ===================================================================================


// Initializes the parser engine. Must be called once at the start of your program.
// Returns 1 on success, 0 on failure.
int initParser(sparser_t *p);

// Cleans up all resources used by the parser engine. Must be called once at the end of your program.
void freeParser(sparser_t *p);

// --- Command Compilation and Management ---

// Parses a command string and populates a compiledCmd struct.
// The compiledCmd struct is initialized internally by this function.
// Returns 1 on successful parsing, 0 on error.
int compileCommand(sparser_t *sp, char *input, compiledCmd* cmd);

// Initializes a compiledCmd struct's members to safe defaults (e.g., NULL pointers, zero counts).
// While compileCommand calls this internally, students might use it if manually creating a compiledCmd.
void initCompiledCmd(compiledCmd *cmd);

// Frees all memory associated with a compiledCmd struct, including its internal CmdVecs
// and any allocated filenames. Call after you have finished executing a command.
void freeCompiledCmd(compiledCmd *cmd);
// --- Debugging and Inspection ---

// A debugging function to print the contents of a compiledCmd struct.
void printCompiledCmd(compiledCmd *c);

#endif
