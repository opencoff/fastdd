/*
 * Automatically generated by /home/sherle/scripts/mkgetopt.py [v1.0.2]
 * Input file: opts.in
 *
 * DO NOT EDIT THIS FILE!
 *
 * Make all changes in opts.in.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>


#include "getopt_long.h"
#include "error.h"

#include "opts.h"

static const struct option Long_options[] =
{
      {"help",                            no_argument,       0, 300}
    , {"quiet",                           no_argument,       0, 302}

    , {0, 0, 0, 0}
};

static const char Short_options[] = "hq";

static void show_help(void);



/*
 * Parse command line options.
 * Return:
 *    0 on success
 *    > 0 on error (number of errors encountered)
 */
int
opt_parse(opt_option * opt, int argc, char * const *argv)
{
    int c,
        errs = 0;

    /*
     * Assume that getopt library has been used before this call;
     * and reset explicitly.
     */
    optind = 0;

    opt->help = 0;
    opt->quiet = 0;

    opt->help_present = 0;
    opt->quiet_present = 0;


    opt->argv_inputs = 0;
    opt->argv_count  = 0;

    if (argc == 0)
        return 0;

    if (argc < 0 || !argv || !argv[0])
        return 0;

    while ((c = getopt_long(argc, argv, Short_options, Long_options, 0)) != EOF)
    {
        switch (c)
        {
        case 300:  /* help */
        case 'h':  /* help */
            opt->help = 1;
            opt->help_present = 1;
            fflush(stdout);
            fflush(stderr);
            show_help();
            break;

        case 302:  /* quiet */
        case 'q':  /* quiet */
            opt->quiet = 1;
            opt->quiet_present = 1;
            break;



        default:
            ++errs;
            break;
        }
    }

    opt->argv_inputs = &argv[optind];
    opt->argv_count  = argc - optind;

    /*
     * Reset getopt library for next use.
     */
    optind = 0;

    return errs;
}


static void
show_help(void)
{
    const char* desc =
"Fast diskcopy.\n\n"
;
    const char* usage = opt_usage();
    const char* options = 
"\nOptions (defaults within '[ ]'):\n"
"    --help, -h    Print this help and exit [false]\n"
"    --quiet, -q   Be silent; don't print progress messages [false]\n"
;

    fflush(stdout);
    fflush(stderr);
    printf("%s: %s", program_name, desc);
    fputs(usage, stdout);
    fputs(options, stdout);
    fflush(stdout);
    fflush(stderr);
    exit(0);
}
