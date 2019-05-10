/*
    Configuration file for ArenaDB. 5.2
*/

/*  Note that apart from configs in this file, you can also specify a config file whose configs
*   will overwite the CONFIG_PARAM_XXX in this file. As a last resort, you can specify paramters
*   to server when starting it, which will overwite both CONFIG_PARAM_XXX configs in this file
*   and configs in the configuration file.
*
*   Basically, it looks like this:
*   ./ArenaDB -p db_num=64 (specify when it starts up)
*       OVERWRITES
*   db_num=32 (in user-provided configuration file)
*       OVERWRITES
*   CONFIG_PARAM_DB_NUM 16 (in this file)
*
*   So the final number of databases in the running server is 64, not 32 or 16.
*/


// ---------------------------CONPILE TIME CONFIGURATIONS---------------------------------------

// CONFIG_BUILD_XXX build and integrate the part of code needed to the functionality
// CONFIG_BUILD_XXX can be commented out if its functionaty is not desired.
#define CONFIG_BUILD_TEST
#define CONFIG_BUILD_BENCHMARK

// CONFIGs below are consant parameters that you'd better not moditfy them for perfomance issues
#define CONFIG_MAX_DB_NUM       16

// CONFIG_PARAM_XXX are configurable parameters that you can adjust for customized building
#define CONFIG_PARAM_DB_NUM     CONFIG_MAX_DB_NUM




// Function declarations
void config_init();
