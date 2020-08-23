// sapc - by Sean Middleditch
// This is free and unencumbered software released into the public domain.
// See LICENSE.md for more details.

#include "state.hh"
#include "sapc_parse.h"
#include "context.h"
#include "overloaded.hh"

#include <string_view>
#include <iostream>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <cassert>

namespace {
	struct Jsonify {
		sapc::ParseState const& ast;
		reID id = sapc::reNone;

		friend void to_json(nlohmann::json& j, Jsonify const& value) {
			switch (value.id.type) {
			case reTypeIdentifier:
			case reTypeString: j = value.ast.strings.strings[value.id.value]; break;
			case reTypeNumber: j = value.id.value; break;
			case reTypeBoolean: j = value.id.value ? true : false; break;
			case reTypeNull: j = nullptr; break;
			default: break;
			}
		}
	};
}

std::ostream& sapc::operator<<(std::ostream& os, Error const& error) {
	return os << error.loc.filename.string() << '(' << error.loc.line << "): ERROR: " << error.message;
}

nlohmann::json sapc::ParseState::to_json() {
	using namespace nlohmann;

	auto doc = json::object();
	doc["$type"] = "sapc.module";
	doc["$version"] = 1;

	auto attrs_to_json = [this](std::vector<Attribute> const& attributes) -> nlohmann::json {
		auto attrs_json = json::object();
		for (auto const& attr : attributes) {
			auto const it = typeMap.find(attr.name);
			assert(it != typeMap.end());
			assert(it->second < this->types.size());

			auto const& def = *this->types[it->second];
			assert(def.attribute);

			auto args_json = json::object();
			for (size_t index = 0; index != attr.arguments.size(); ++index) {
				assert(def.fields.size() == attr.arguments.size());

				auto const& param = def.fields[index];

				json value(Jsonify{*this, attr.arguments[index]});
				args_json[param.name] = std::move(value);
			}

			attrs_json[def.name].push_back(std::move(args_json));
		}
		return attrs_json;
	};

	auto types_json = json::array();
	for (auto const& type : types) {
		auto type_json = json::object();
		type_json["name"] = type->name;
		type_json["imported"] = type->imported;
		if (!type->base.empty())
			type_json["base"] = type->base;
		if (!type->attributes.empty())
			type_json["attributes"] = attrs_to_json(type->attributes);

		auto fields = json::array();
		for (auto const& field : type->fields) {
			auto field_json = json::object();
			field_json["name"] = field.name;
			field_json["type"] = field.type;
			if (field.init != reNone)
				field_json["default"] = Jsonify{ *this, field.init };
			if (!field.attributes.empty())
				field_json["attributes"] = attrs_to_json(field.attributes);
			fields.push_back(std::move(field_json));
		}

		type_json["fields"] = std::move(fields);
		types_json.push_back(std::move(type_json));
	}
	doc["types"] = std::move(types_json);

	return doc;
}

std::filesystem::path sapc::ParseState::resolveInclude(std::filesystem::path include) {
	if (include.is_relative()) {
		auto tmp = pathStack.back();
		include = tmp.remove_filename() / include;
	}
	return include;
}

std::filesystem::path sapc::ParseState::resolveModule(std::string const& name) {
	auto tmp = pathStack.back();
	auto path = tmp.remove_filename() / name;
	path.replace_extension("sap");
	return path;
}

bool sapc::ParseState::compile(std::filesystem::path filename) {
	std::ifstream stream(filename);
	if (!stream)
		return false;

	std::ostringstream buffer;
	buffer << stream.rdbuf();
	stream.close();
	std::string text = buffer.str();

	dependencies.push_back(filename);
	pathStack.push_back(std::move(filename));
	bool const result = build(std::move(text));

	if (!result && errors.empty())
		error(location({ 0, 1 }), "unknown compile error");

	pathStack.pop_back();

	return !failed();
}

bool sapc::ParseState::build(std::string contents) {
	if (!parse(std::move(contents)))
		return false;

	if (!checkTypes())
		return false;

	if (!expandAttributes())
		return false;

	return true;
}

bool sapc::ParseState::parse(std::string contents) {
	reParseContext parse_ctx;
	parse_ctx.state = static_cast<reParseState*>(this);
	parse_ctx.source = contents.data();
	parse_ctx.length = contents.size();
	parse_ctx.loc.position = 0;
	parse_ctx.loc.line = 1;

	sapc_context_t* ctx = sapc_create(&parse_ctx);
	int const rs = sapc_parse(ctx, nullptr);
	sapc_destroy(ctx);

	return rs == 0;
}

bool sapc::ParseState::importModule(std::filesystem::path path, reLoc loc) {
	dependencies.push_back(path);

	sapc::ParseState parser{strings};
	auto const parsed = parser.compile(path);

	for (auto& error : parser.errors)
		errors.push_back(std::move(error));

	if (!parsed)
		error(loc, "errors encountered during import '", path.string(), '\'');

	for (auto& type : parser.types) {
		auto it = typeMap.find(type->name);
		if (it != typeMap.end()) {
			auto const& previous = types[it->second];
			error(loc, "imported type '", type->name, "' conflicts type defined at ", previous->loc.filename.string(), '(', previous->loc.line, ')');
			continue;
		}

		type->imported = true;
		typeMap[type->name] = types.size();
		types.push_back(std::move(type));
	}

	return parsed;
}

bool sapc::ParseState::includeFile(std::filesystem::path path, reLoc loc) {
	std::ifstream stream(path);
	if (!stream) {
		error(loc, "could not open file '", path.string(), '\'');
		return false;
	}

	std::ostringstream buffer;
	buffer << stream.rdbuf();
	stream.close();
	std::string text = buffer.str();

	dependencies.push_back(path);
	pathStack.push_back(path);
	bool const result = parse(std::move(text));
	pathStack.pop_back();

	if (!result)
		error(loc, "errors encountered during include '", path.string(), '\'');

	return result;
}

bool sapc::ParseState::checkTypes() {
	bool ok = true;

	for (auto const& type : types) {
		if (type->imported)
			continue;

		if (!type->base.empty() && typeMap.find(type->base) == typeMap.end()) {
			error(type->loc, "unknown type '", type->base, '\'');
			ok = false;
		}

		for (auto const& field : type->fields) {
			if (typeMap.find(field.type) == typeMap.end()) {
				error(field.loc, "unknown type '", field.type, '\'');
				ok = false;
			}
		}
	}

	return ok;
}

bool sapc::ParseState::expandAttributes() {
	auto expand = [this](Attribute& attr) {
		auto const it = typeMap.find(attr.name);
		if (it == typeMap.end()) {
			error(attr.loc, "unknown attribute '", attr.name, '\'');
			return false;
		}

		auto const argc = attr.arguments.size();
		auto const& def = types[it->second];
		auto const& params = def->fields;

		if (!def->attribute) {
			error(attr.loc, "attribute type '", attr.name, "' is declared as a regular type (not attribute) at ", def->loc.filename.string(), '(', def->loc.line, ')');
			return false;
		}

		attr.type = def.get();

		if (argc > params.size()) {
			error(attr.loc, "too many arguments to attribute '", attr.name, '\'');
			return false;
		}

		bool ok = true;
		for (size_t index = argc; index != params.size(); ++index) {
			auto const& param = params[index];

			if (param.init.type == reTypeNone) {
				error(attr.loc, "missing required argument '", param.name, "' to attribute '", attr.name, '\'');
				ok = false;
			}

			attr.arguments.push_back(param.init);
		}
		return ok;
	};

	bool ok = true;
	for (auto& type : types) {
		for (auto& attr : type->attributes) {
			ok = expand(attr) && ok;
		}

		for (auto& field : type->fields) {
			for (auto& attr : field.attributes)
				ok = expand(attr) && ok;
		}
	}

	return ok;
}