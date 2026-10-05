#ifndef SCROLLER_H
#define SCROLLER_H

#include <stdbool.h>

// Longest text the scroller keeps.
#define SCROLLER_MAX_LEN 128

// C64-style sine scroller: text moves right to left along a moving wave.
void init_scroller(void);
bool render_scroller(void);

// Parse "[rrggbb]:text". An empty colour or text keeps the current one.
// Returns false if malformed.
bool scroller_parse_command(const char *line);

#endif /* SCROLLER_H */
