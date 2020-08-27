// sapc - by Sean Middleditch
// This is free and unencumbered software released reIDo the public domain.
// See LICENSE.md for more details.

#include "context.h"
#include "state.hh"

#include <charconv>
#include <cassert>
#include <memory>

namespace {
    auto get_attributes(reParseState* state) -> std::vector<sapc::Attribute> {
        assert(!state->attributeSetStack.empty());

        auto attrs = std::move(state->attributeSetStack.back());
        state->attributeSetStack.pop_back();
        return attrs;
    }

    auto get_string(reParseState* state, reID id) -> std::string {
        if (id.value < 0)
            return {};
        switch (id.type) {
        case reTypeIdentifier:
        case reTypeString:
            return state->strings.strings[id.value];
        default:
            return {};
        }
    }
}

reID rePushIdentifier(reParseState* state, char const* text) {
    return state->strings.pushString(text, reTypeIdentifier);
}

reID rePushString(reParseState* state, char const* text) {
    return state->strings.pushString(text);
}

reID rePushNumber(reParseState* state, char const* text) {
    int value = 0;
    std::from_chars(text, text + std::strlen(text), value);
    return { reTypeNumber, value };
}

reID rePushBoolean(struct reParseState* state, int boolean) {
    return { reTypeBoolean, boolean & 0x1 };
}

reID rePushNull(struct reParseState* state) {
    return { reTypeNull, 0 };
}

void rePushField(reParseState* state, reID type, reID name, reID init, reLoc loc) {
    using namespace sapc;

    state->fieldStack.push_back(Field{ get_string(state, type), get_string(state, name), init, get_attributes(state), state->location(loc) });
}

namespace {
    void rePushType(struct reParseState* state, std::string name, std::string base, std::vector<sapc::Field> fields, std::vector<sapc::Attribute> attributes, sapc::Location loc, bool isAttribute = false) {
        using namespace sapc;

        auto it = state->typeMap.find(name);
        if (it != state->typeMap.end()) {
            auto const& previous = state->types[it->second];

            if (previous->loc == loc)
                return;

            state->error(loc, "Entity '", name, "' is a redefinition of entity at ", previous->loc.filename.string(), '(', previous->loc.line, ')');
            return;
        }

        auto const index = state->types.size();
        state->typeMap[name] = index;

        state->types.push_back(std::make_unique<Type>(Type{ std::move(name), std::move(base), std::move(attributes), std::move(fields), std::move(loc), isAttribute }));
    }
}

void rePushTypeDefinition(struct reParseState* state, reID name, reID base, reLoc loc) {
    rePushType(state, get_string(state, name), get_string(state, base), std::move(state->fieldStack), get_attributes(state), state->location(loc));
}

void rePushAttributeArgument(reParseState* state, reID id) {
    state->argumentStack.push_back(id);
}

void rePushAttribute(reParseState* state, reID name, reLoc loc) {
    using namespace sapc;

    state->attributeStack.push_back(Attribute{ get_string(state, name), std::move(state->argumentStack), state->location(loc) });
}

void rePushAttributeParam(reParseState* state, reID type, reID name, reID init, reLoc loc) {
    using namespace sapc;

    state->attributeParamStack.push_back(Field{ get_string(state, type), get_string(state, name), init, {}, state->location(loc) });
}

void rePushAttributeDefinition(struct reParseState* state, reID name, reLoc loc) {
    rePushType(state, get_string(state, name), {}, std::move(state->attributeParamStack), {}, state->location(loc), true);
}

void rePushAttributeSet(struct reParseState* state) {
    state->attributeSetStack.push_back(std::move(state->attributeStack));
}

void reModuleName(struct reParseState* state, struct reID name, struct reLoc loc) {
    auto moduleName = get_string(state, name);

    // module name should match filename
    auto const expectedModule = state->pathStack.back().filename().replace_extension().string();
    if (moduleName != expectedModule)
        state->error(state->location(loc), "module name '", moduleName, "' does not match filename '", state->pathStack.back().string(), '\'');

    // ignore duplicate module name declarations
    if (!state->moduleName.empty())
        return;

    state->moduleName = std::move(moduleName);
}

void reImport(reParseState* state, reID name, reLoc loc) {
    state->importModule(get_string(state, name), loc);
}

void reInclude(reParseState* state, reID path, reLoc loc) {
    state->includeFile(get_string(state, path), loc);
}

void reError(struct reParseState* state, reLoc loc) {
    state->error(state->location(loc), "unknown parse error");
}
