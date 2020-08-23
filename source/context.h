// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdlib.h>

struct reParseState;

enum reType {
    reTypeNone,
    reTypeIdentifier,
    reTypeNumber,
    reTypeString,
    reTypeBoolean,
    reTypeNull
};

struct reID {
    enum reType type;
    int value;
};

#define RE_NONE (struct reID){reTypeNone, -1}

struct reLoc {
    size_t position;
    int line;
};

struct reParseContext {
    struct reParseState* state;
    char* source;
    size_t length;
    struct reLoc loc;
};

struct reID rePushIdentifier(struct reParseState* state, char const* text);
struct reID rePushString(struct reParseState* state, char const* text);
struct reID rePushNumber(struct reParseState* state, char const* text);
struct reID rePushBoolean(struct reParseState* state, int boolean);
struct reID rePushNull(struct reParseState* state);

void rePushField(struct reParseState* state, struct reID type, struct reID name, struct reID init, struct reLoc loc);
void rePushTypeDefinition(struct reParseState* state, struct reID name, struct reID base, struct reLoc loc);

void rePushAttributeArgument(struct reParseState* state, struct reID id);
void rePushAttribute(struct reParseState* state, struct reID name, struct reLoc loc);

void rePushAttributeParam(struct reParseState* state, struct reID type, struct reID name, struct reID init, struct reLoc loc);
void rePushAttributeDefinition(struct reParseState* state, struct reID name, struct reLoc loc);
void rePushAttributeSet(struct reParseState* state);

void reImport(struct reParseState* state, struct reID name, struct reLoc loc);
void reInclude(struct reParseState* state, struct reID path, struct reLoc loc);

void reError(struct reParseState* state, struct reLoc loc);

#if defined(__cplusplus)
} // extern "C"
#endif