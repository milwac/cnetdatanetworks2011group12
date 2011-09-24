#include "definitions.h"

typedef struct {
CnetAddr dest;
CnetAddr via;
long cost;
int link;
} ROUTING_TABLE;

ROUTING_TABLE table[MAX_NODES];


