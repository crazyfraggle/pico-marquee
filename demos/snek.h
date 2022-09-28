#ifndef SNEK_H
#define SNEK_H

// Exported functions
void init_snek();
bool render_snek();
bool snek_keyboard(char);

static char *score_text[] = {
    " ## ",
    "#     ##   ##  # #   ##",
    "#    #  # #  # ## # #  #",
    " ##  #    #  # #    ###",
    "   # #    #  # #    #",
    "#  # #  # #  # #    #  #",
    " ##   ##   ##  #     ##",
    NULL};

static char *high_text[] = {
    "#  #        #",
    "#  # #      #",
    "#  #    ##  ###",
    "#### # #  # #  #",
    "#  # # #  # #  #",
    "#  # #  ### #  #",
    "#  # #    # #  #",
    "       ###",
    NULL};

static char *nums[][6] = {
    {" ## ",
     "#  #",
     "#  #",
     "#  #",
     " ## ",
     NULL},

    {"  ##",
     "   #",
     "   #",
     "   #",
     "   #",
     NULL},

    {" ## ",
     "#  #",
     "  # ",
     " #  ",
     "####",
     NULL},

    {" ## ",
     "#  #",
     "  ##",
     "#  #",
     " ## ",
     NULL},

    {"  ##",
     " # #",
     "####",
     "   #",
     "   #",
     NULL},

    {"####",
     "#   ",
     "### ",
     "   #",
     "### ",
     NULL},

    {" ## ",
     "#   ",
     "### ",
     "#  #",
     " ## ",
     NULL},

    {"####",
     "   #",
     "  # ",
     " #  ",
     "#   ",
     NULL},

    {" ## ",
     "#  #",
     " ## ",
     "#  #",
     " ## ",
     NULL},

    {" ## ",
     "#  #",
     " ###",
     "   #",
     " ## ",
     NULL},

};

#endif /* SNEK_H */